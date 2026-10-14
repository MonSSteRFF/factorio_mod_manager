// Developer tool: runs the runtime symbol resolution (DbgHelp over factorio.pdb)
// outside the game and prints every item with the time it took.
//
//   modpacks_symtool.exe "C:\...\Factorio\bin\x64\factorio.exe"
//
// The exe is mapped as an image without running it, so the same code path as
// in the game can be checked quickly after a game update or a code change.
#include <windows.h>

#include <chrono>
#include <cstdio>
#include <optional>
#include <string>

#include "core/symbols.h"

using namespace modpacks;

namespace {

// Wraps a source and reports lookups slower than 50 ms.
class Timed : public SymbolSource {
public:
    explicit Timed(SymbolSource& s) : s_(s) {}
    long long last = 0;
    template <typename F> auto T(F f) {
        auto a = std::chrono::steady_clock::now();
        auto v = f();
        last = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - a).count();
        if (last > 50) std::printf("  slow %lld ms\n", last);
        return v;
    }
    std::optional<uint64_t> Rva(std::string_view n) override { return T([&] { return s_.Rva(n); }); }
    std::optional<uint64_t> FieldOffset(std::string_view c, std::string_view m) override { return T([&] { return s_.FieldOffset(c, m); }); }
    std::optional<uint64_t> TypeSize(std::string_view c) override { return T([&] { return s_.TypeSize(c); }); }
    std::optional<uint64_t> BaseOffset(std::string_view c, std::string_view b) override { return T([&] { return s_.BaseOffset(c, b); }); }
    std::optional<uint64_t> EnumValue(std::string_view t, std::string_view n) override { return T([&] { return s_.EnumValue(t, n); }); }
    std::optional<uint64_t> VSlot(uint64_t v, uint64_t i) override { return T([&] { return s_.VSlot(v, i); }); }
private:
    SymbolSource& s_;
};

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: modpacks_symtool <path to factorio.exe>\n");
        return 2;
    }
    std::wstring exe = argv[1];
    HMODULE module = LoadLibraryExW(exe.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!module) {
        std::fprintf(stderr, "cannot map %ls (error %lu)\n", exe.c_str(), GetLastError());
        return 1;
    }
    auto start = std::chrono::steady_clock::now();
    DbgHelpSource source;
    std::string error;
    if (!source.Open(module, exe, &error)) {
        std::fprintf(stderr, "open failed: %s\n", error.c_str());
        return 1;
    }
    auto opened = std::chrono::steady_clock::now();
    Timed timed(source);
    if (argc > 2) {  // verbose: print each lookup as it happens
        for (const SymDef& d : SymbolTable()) {
            std::printf("%s\n", d.idName);
            Resolved one;
            (void)one;
            switch (d.kind) {
                case SymKind::Func: case SymKind::Data: timed.Rva(d.a); break;
                case SymKind::Field: timed.FieldOffset(d.a, d.b); break;
                case SymKind::Size: timed.TypeSize(d.a); break;
                case SymKind::Base: timed.BaseOffset(d.a, d.b); break;
                case SymKind::Enum: timed.EnumValue(d.a, d.b); break;
                case SymKind::VSlot: timed.Rva(d.a); timed.Rva(d.b); break;
            }
        }
    }
    Resolved r;
    ResolveReport report = ResolveAll(source, r);
    auto done = std::chrono::steady_clock::now();
    for (const SymDef& d : SymbolTable()) {
        if (r.has(d.id)) {
            std::printf("ok       %-45s %#llx\n", d.idName, static_cast<unsigned long long>(r[d.id]));
        } else {
            std::printf("%-8s %-45s\n", d.required ? "MISSING" : "absent", d.idName);
        }
    }
    auto ms = [](auto a, auto b) { return static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count()); };
    std::printf("\nopen %lld ms, resolve %lld ms; %zu required missing; abi: %s\n", ms(start, opened), ms(opened, done),
                report.missingRequired.size(), CheckAbi(r).empty() ? "ok" : CheckAbi(r).c_str());
    return report.ok() ? 0 : 1;
}
