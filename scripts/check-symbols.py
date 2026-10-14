#!/usr/bin/env python3
"""Offline check of src/core/symbols.def.h against a Factorio build.

Usage (WSL, needs llvm-pdbutil):
    python3 -I scripts/check-symbols.py "/mnt/c/games/steam/steamapps/common/Factorio/bin/x64"

Resolves every item the mod needs exactly the way the runtime does (by name)
and prints the value, so a new game version can be checked before launching it.
Exit code 1 if a required item is missing. PDB dumps are cached in
./.symcheck-cache/<pdb size>-<pdb mtime>/.
"""
import os
import pathlib
import re
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEF = ROOT / "src" / "core" / "symbols.def.h"


def parse_def():
    text = DEF.read_text(encoding="utf-8")
    macros = dict(re.findall(r'#define (MP_\w+) "([^"]*)"', text))
    items = []
    for m in re.finditer(r"^(MP_(?:FUNC|DATA|FIELD|SIZE|BASE|VSLOT|ENUM))\((.*?)\)\s*$", text, re.M | re.S):
        kind, body = m.group(1), m.group(2)
        if kind == "MP_FUNC" and "decorated_name" in body:
            continue  # the comment block
        args = []
        for raw in re.findall(r'((?:"[^"]*"\s*|MP_\w+\s*)+|true|false|\w+)', body):
            raw = raw.strip()
            if raw in ("true", "false") or re.fullmatch(r"\w+", raw) and not raw.startswith("MP_"):
                args.append(raw)
            else:
                parts = re.findall(r'"([^"]*)"|(MP_\w+)', raw)
                args.append("".join(p[0] or macros[p[1]] for p in parts))
        items.append((kind[3:], args))
    return items


def run(cmd, out):
    if not out.exists():
        with open(out, "w", encoding="utf-8", errors="replace") as f:
            subprocess.run(cmd, check=True, stdout=f)
    return out


class Pdb:
    def __init__(self, bindir):
        self.exe = bindir / "factorio.exe"
        pdb = bindir / "factorio.pdb"
        st = pdb.stat()
        cache = ROOT / ".symcheck-cache" / f"{st.st_size}-{int(st.st_mtime)}"
        cache.mkdir(parents=True, exist_ok=True)
        publics = run(["llvm-pdbutil", "dump", "--publics", str(pdb)], cache / "publics.txt")
        types = run(["llvm-pdbutil", "dump", "--types", str(pdb)], cache / "types.txt")
        sections = subprocess.run(["llvm-readobj", "--sections", str(self.exe)], check=True,
                                  capture_output=True, text=True).stdout
        self.sections = []  # (index, rva, raw_ptr, raw_size)
        for i, block in enumerate(re.findall(r"Section \{(.*?)\n  \}", sections, re.S), 1):
            g = lambda k: int(re.search(k + r": (0x[0-9A-F]+|\d+)", block).group(1), 0)
            self.sections.append((i, g("VirtualAddress"), g("PointerToRawData"), g("RawDataSize")))
        self.publics = {}
        name = None
        for line in open(publics, encoding="utf-8", errors="replace"):
            m = re.search(r"S_PUB32 \[size = \d+\] `(.*)`", line)
            if m:
                name = m.group(1)
                continue
            m = re.search(r"addr = (\d{4}):(\d+)", line)
            if m and name:
                sec = int(m.group(1))
                if 1 <= sec <= len(self.sections):
                    self.publics[name] = self.sections[sec - 1][1] + int(m.group(2))
                name = None
        self.types_path = types
        self._index_types()

    def _index_types(self):
        self.records = {}   # type id -> list of lines
        self.by_name = {}   # class/enum name -> list of type ids (definitions last)
        cur = None
        rec = re.compile(r"^\s+(0x[0-9A-F]+) \| (LF_\w+) \[size = \d+\](?: `(.*)`)?")
        for line in open(self.types_path, encoding="utf-8", errors="replace"):
            m = rec.match(line)
            if m:
                cur = m.group(1)
                self.records[cur] = [line]
                if m.group(3) and m.group(2) in ("LF_CLASS", "LF_STRUCTURE", "LF_UNION", "LF_ENUM"):
                    self.by_name.setdefault(m.group(3), []).append(cur)
            elif cur:
                self.records[cur].append(line)

    def definition(self, name):
        for tid in reversed(self.by_name.get(name, [])):
            text = "".join(self.records[tid])
            if "forward ref" not in text:
                return text
        return None

    def field_list(self, name):
        text = self.definition(name)
        if not text:
            return None, None
        fl = re.search(r"field list: (0x[0-9A-F]+)", text).group(1)
        return text, "".join(self.records[fl])

    def type_name(self, tid):
        m = re.search(r"`(.*?)`", self.records.get(tid, [""])[0])
        return m.group(1) if m else tid

    def read(self, rva, size):
        for _, va, raw, rsize in self.sections:
            if va <= rva < va + rsize:
                with open(self.exe, "rb") as f:
                    f.seek(raw + rva - va)
                    return f.read(size)
        return None

    # --- lookups, mirroring src/core/symbols.cpp -------------------------------
    def func(self, name):
        rva = self.publics.get(name)
        return None if rva is None else f"rva {rva:#x}"

    def field(self, cls, member):
        _, fl = self.field_list(cls)
        if fl is None:
            return None
        m = re.search(r"LF_MEMBER \[name = `" + re.escape(member) + r"`, Type = [^,]+, offset = (\d+)", fl)
        return None if not m else f"+{int(m.group(1)):#x}"

    def size(self, cls):
        text = self.definition(cls)
        m = text and re.search(r"sizeof (\d+)", text)
        return None if not m else m.group(1)

    def base(self, cls, base):
        _, fl = self.field_list(cls)
        if fl is None:
            return None
        for tid, off in re.findall(r"LF_BCLASS\s+type = (0x[0-9A-F]+), offset = (\d+)", fl):
            if self.type_name(tid) == base:
                return f"+{int(off):#x}"
        return None

    def vslot(self, vftable, impl):
        vt, fn = self.publics.get(vftable), self.publics.get(impl)
        if vt is None or fn is None:
            return None
        base = 0x140000000
        text = self.sections[0]  # .text: slots point into code, RTTI locators do not
        hits = []
        for i in range(512):
            raw = self.read(vt + i * 8, 8)
            if raw is None:
                break
            ptr = struct.unpack("<Q", raw)[0]
            if not text[1] <= ptr - base < text[1] + text[3]:
                break
            if ptr - base == fn:
                hits.append(i * 8)
        return None if len(hits) != 1 else f"vtbl+{hits[0]:#x}"

    def enum(self, etype, name):
        text = self.definition(etype)
        if not text:
            return None
        fl = re.search(r"field list: (0x[0-9A-F]+)", text).group(1)
        m = re.search(r"LF_ENUMERATE \[" + re.escape(name) + r" = (-?\d+)\]", "".join(self.records[fl]))
        return None if not m else m.group(1)


def main():
    bindir = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else \
        pathlib.Path("/mnt/c/games/steam/steamapps/common/Factorio/bin/x64")
    pdb = Pdb(bindir)
    failed = 0
    for kind, args in parse_def():
        ident, required = args[0], args[-1] == "true"
        rest = args[1:-1]
        value = {
            "FUNC": lambda: pdb.func(rest[0]),
            "DATA": lambda: pdb.func(rest[0]),
            "FIELD": lambda: pdb.field(rest[0], rest[1]),
            "SIZE": lambda: pdb.size(rest[0]),
            "BASE": lambda: pdb.base(rest[0], rest[1]),
            "VSLOT": lambda: pdb.vslot(rest[0], rest[1]),
            "ENUM": lambda: pdb.enum(rest[0], rest[1]),
        }[kind]()
        status = "ok " if value is not None else ("MISSING" if required else "absent (optional)")
        if value is None and required:
            failed += 1
        print(f"{status:8} {kind:5} {ident:45} {value or ''}")
    print(f"\n{failed} required item(s) missing" if failed else "\nall required items resolved")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
