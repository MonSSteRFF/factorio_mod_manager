#include "core/symbols.h"

#include <windows.h>
#include <dbghelp.h>
#include <oleauto.h>

#include <functional>
#include <string>

#include "common/utf.h"

namespace modpacks {

const std::vector<SymDef>& SymbolTable() {
    static const std::vector<SymDef> table = {
#define MP_FUNC(id, name, required) {SymId::id, SymKind::Func, #id, name, nullptr, required},
#define MP_DATA(id, name, required) {SymId::id, SymKind::Data, #id, name, nullptr, required},
#define MP_FIELD(id, cls, member, required) {SymId::id, SymKind::Field, #id, cls, member, required},
#define MP_SIZE(id, cls, required) {SymId::id, SymKind::Size, #id, cls, nullptr, required},
#define MP_BASE(id, cls, base, required) {SymId::id, SymKind::Base, #id, cls, base, required},
#define MP_VSLOT(id, vftable, impl, required) {SymId::id, SymKind::VSlot, #id, vftable, impl, required},
#define MP_ENUM(id, type, name, required) {SymId::id, SymKind::Enum, #id, type, name, required},
#include "core/symbols.def.h"
    };
    return table;
}

ResolveReport ResolveAll(SymbolSource& source, Resolved& out, std::string_view forceMissing) {
    ResolveReport report;
    out = Resolved{};
    for (const SymDef& def : SymbolTable()) {
        std::optional<uint64_t> value;
        switch (def.kind) {
            case SymKind::Func:
            case SymKind::Data:
                value = source.Rva(def.a);
                break;
            case SymKind::Field:
                value = source.FieldOffset(def.a, def.b);
                break;
            case SymKind::Size:
                value = source.TypeSize(def.a);
                break;
            case SymKind::Base:
                value = source.BaseOffset(def.a, def.b);
                break;
            case SymKind::Enum:
                value = source.EnumValue(def.a, def.b);
                break;
            case SymKind::VSlot: {
                auto vftable = source.Rva(def.a);
                auto impl = source.Rva(def.b);
                if (vftable && impl) value = source.VSlot(*vftable, *impl);
                break;
            }
        }
        if (!forceMissing.empty() && forceMissing == def.idName) value.reset();
        if (value) {
            out.value[static_cast<size_t>(def.id)] = *value;
            out.present[static_cast<size_t>(def.id)] = true;
            continue;
        }
        std::string what = std::string(def.idName) + " (" + def.a + (def.b ? std::string(" / ") + def.b : "") + ")";
        (def.required ? report.missingRequired : report.missingOptional).push_back(std::move(what));
    }
    return report;
}

std::string CheckAbi(const Resolved& r) {
    struct Check {
        SymId id;
        size_t ours;
        const char* name;
    };
    const Check checks[] = {
        {SymId::abi_String, sizeof(std::string), "std::string"},
        {SymId::abi_FunctionVoid, sizeof(std::function<void()>), "std::function<void()>"},
        {SymId::abi_FunctionInt, sizeof(std::function<void(int)>), "std::function<void(int)>"},
        {SymId::abi_FunctionString, sizeof(std::function<void(const std::string&)>),
         "std::function<void(const std::string&)>"},
        {SymId::abi_NamedBoolInSubheader, sizeof(bool), "NamedBool"},
    };
    for (const Check& c : checks) {
        if (r.has(c.id) && r[c.id] != c.ours) {
            return std::string(c.name) + ": game " + std::to_string(r[c.id]) + " bytes, mod " +
                   std::to_string(c.ours) + " bytes";
        }
    }
    return {};
}

// --- DbgHelp ------------------------------------------------------------------------

namespace {

// Values from cvconst.h (DIA SDK), which dbghelp.h does not include.
enum : DWORD { kSymTagData = 7, kSymTagUDT = 11, kSymTagEnum = 12, kSymTagBaseClass = 18 };
enum : DWORD { kDataIsMember = 7 };

// DbgHelp identifies a session by an arbitrary handle when it does not invade
// the process; a private value keeps us apart from the game's own crash
// handler, which also uses DbgHelp.
HANDLE const kSession = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x4D504B53));

std::wstring SymName(HANDLE process, uint64_t base, ULONG index) {
    WCHAR* name = nullptr;
    if (!SymGetTypeInfo(process, base, index, TI_GET_SYMNAME, &name) || !name) return {};
    std::wstring out = name;
    LocalFree(name);
    return out;
}

template <typename T>
bool Info(HANDLE process, uint64_t base, ULONG index, IMAGEHLP_SYMBOL_TYPE_INFO what, T& out) {
    return SymGetTypeInfo(process, base, index, what, &out) != FALSE;
}

}  // namespace

DbgHelpSource::~DbgHelpSource() {
    if (process_) {
        SymCleanup(process_);
        // Options are process-wide; give the game's crash handler its own back.
        SymSetOptions(savedOptions_);
    }
}

bool DbgHelpSource::Open(HMODULE module, const std::wstring& exePath, std::string* error) {
    auto fail = [&](const std::string& msg) {
        if (error) *error = msg + " (error " + std::to_string(GetLastError()) + ")";
        return false;
    };
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<const BYTE*>(module) + dos->e_lfanew);
    base_ = reinterpret_cast<uint64_t>(module);
    imageSize_ = nt->OptionalHeader.SizeOfImage;
    const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        uint64_t begin = base_ + section->VirtualAddress;
        uint64_t end = begin + section->Misc.VirtualSize;
        if (!codeBegin_ || begin < codeBegin_) codeBegin_ = begin;
        if (end > codeEnd_) codeEnd_ = end;
    }

    std::wstring dir = exePath.substr(0, exePath.find_last_of(L"\\/"));
    savedOptions_ = SymGetOptions();
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_EXACT_SYMBOLS | SYMOPT_FAIL_CRITICAL_ERRORS |
                  SYMOPT_NO_PROMPTS | SYMOPT_AUTO_PUBLICS);
    if (!SymInitializeW(kSession, dir.c_str(), FALSE)) return fail("SymInitialize failed");
    process_ = kSession;
    if (!SymLoadModuleExW(process_, nullptr, exePath.c_str(), nullptr, base_, static_cast<DWORD>(imageSize_),
                          nullptr, 0)) {
        return fail("SymLoadModuleEx failed");
    }
    IMAGEHLP_MODULEW64 info = {};
    info.SizeOfStruct = sizeof(info);
    if (!SymGetModuleInfoW64(process_, base_, &info)) return fail("SymGetModuleInfo failed");
    if (info.SymType == SymDeferred) {
        // Force the deferred load now so a mismatch is reported here.
        SYMBOL_INFO probe = {};
        probe.SizeOfStruct = sizeof(probe);
        SymFromName(process_, "?global@@3PEAVGlobalContext@@EA", &probe);
        SymGetModuleInfoW64(process_, base_, &info);
    }
    if (info.SymType != SymPdb) return fail("factorio.pdb not found or not usable (SymType " + std::to_string(info.SymType) + ")");
    if (info.PdbUnmatched || info.DbgUnmatched) return fail("factorio.pdb does not match factorio.exe");
    return true;
}

std::optional<uint64_t> DbgHelpSource::Rva(std::string_view decorated) {
    // SymFromName on a decorated name scans all ~200k public symbols each time
    // (~200 ms); enumerate them once instead.
    if (!publicsLoaded_) {
        publicsLoaded_ = true;
        DWORD options = SymGetOptions();
        SymSetOptions(options | SYMOPT_PUBLICS_ONLY);
        auto cb = [](PSYMBOL_INFO sym, ULONG, PVOID user) -> BOOL {
            auto* self = static_cast<DbgHelpSource*>(user);
            if (sym->Address >= self->base_ && sym->Address < self->base_ + self->imageSize_) {
                self->publics_.emplace(std::string(sym->Name, sym->NameLen), sym->Address - self->base_);
            }
            return TRUE;
        };
        SymEnumSymbols(process_, base_, "*", cb, this);
        SymSetOptions(options);
    }
    auto it = publics_.find(std::string(decorated));
    if (it == publics_.end()) return std::nullopt;
    return it->second;
}

std::optional<ULONG> DbgHelpSource::FindType(std::string_view name, bool wantEnum) {
    // SymGetTypeFromName is a hashed lookup; enumerating types by mask would
    // scan the whole 460 MB type stream for every item.
    alignas(SYMBOL_INFOW) char buffer[sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(WCHAR)];
    auto* sym = reinterpret_cast<SYMBOL_INFOW*>(buffer);
    *sym = {};
    sym->SizeOfStruct = sizeof(SYMBOL_INFOW);
    sym->MaxNameLen = MAX_SYM_NAME;
    std::wstring wname = Utf8ToWide(name);
    if (!SymGetTypeFromNameW(process_, base_, wname.c_str(), sym)) return std::nullopt;
    DWORD tag = 0;
    if (!Info(process_, base_, sym->TypeIndex, TI_GET_SYMTAG, tag)) return std::nullopt;
    if (tag != (wantEnum ? kSymTagEnum : kSymTagUDT)) return std::nullopt;
    return sym->TypeIndex;
}

std::vector<ULONG> DbgHelpSource::Children(ULONG type) {
    DWORD count = 0;
    if (!Info(process_, base_, type, TI_GET_CHILDRENCOUNT, count) || count == 0) return {};
    std::vector<char> buf(sizeof(TI_FINDCHILDREN_PARAMS) + count * sizeof(ULONG));
    auto* params = reinterpret_cast<TI_FINDCHILDREN_PARAMS*>(buf.data());
    params->Count = count;
    params->Start = 0;
    if (!SymGetTypeInfo(process_, base_, type, TI_FINDCHILDREN, params)) return {};
    return std::vector<ULONG>(params->ChildId, params->ChildId + count);
}

std::optional<uint64_t> DbgHelpSource::FieldOffset(std::string_view cls, std::string_view member) {
    auto type = FindType(cls, false);
    if (!type) return std::nullopt;
    std::wstring want = Utf8ToWide(member);
    for (ULONG child : Children(*type)) {
        DWORD tag = 0;
        DWORD kind = 0;
        if (!Info(process_, base_, child, TI_GET_SYMTAG, tag) || tag != kSymTagData) continue;
        if (!Info(process_, base_, child, TI_GET_DATAKIND, kind) || kind != kDataIsMember) continue;
        if (SymName(process_, base_, child) != want) continue;
        DWORD offset = 0;
        if (Info(process_, base_, child, TI_GET_OFFSET, offset)) return offset;
    }
    return std::nullopt;
}

std::optional<uint64_t> DbgHelpSource::TypeSize(std::string_view cls) {
    auto type = FindType(cls, false);
    ULONG64 length = 0;
    if (!type || !Info(process_, base_, *type, TI_GET_LENGTH, length)) return std::nullopt;
    return length;
}

std::optional<uint64_t> DbgHelpSource::BaseOffset(std::string_view cls, std::string_view base) {
    auto type = FindType(cls, false);
    if (!type) return std::nullopt;
    std::wstring want = Utf8ToWide(base);
    for (ULONG child : Children(*type)) {
        DWORD tag = 0;
        if (!Info(process_, base_, child, TI_GET_SYMTAG, tag) || tag != kSymTagBaseClass) continue;
        DWORD baseType = 0;
        if (!Info(process_, base_, child, TI_GET_TYPEID, baseType)) continue;
        if (SymName(process_, base_, baseType) != want) continue;
        DWORD offset = 0;
        if (Info(process_, base_, child, TI_GET_OFFSET, offset)) return offset;
    }
    return std::nullopt;
}

std::optional<uint64_t> DbgHelpSource::EnumValue(std::string_view type, std::string_view name) {
    auto e = FindType(type, true);
    if (!e) return std::nullopt;
    std::wstring want = Utf8ToWide(name);
    for (ULONG child : Children(*e)) {
        if (SymName(process_, base_, child) != want) continue;
        VARIANT v;
        VariantInit(&v);
        if (!Info(process_, base_, child, TI_GET_VALUE, v)) continue;
        switch (v.vt) {
            case VT_I1: return static_cast<uint64_t>(v.cVal);
            case VT_UI1: return v.bVal;
            case VT_I2: return static_cast<uint64_t>(v.iVal);
            case VT_UI2: return v.uiVal;
            case VT_I4: case VT_INT: return static_cast<uint64_t>(v.lVal);
            case VT_UI4: case VT_UINT: return v.ulVal;
            case VT_I8: return static_cast<uint64_t>(v.llVal);
            case VT_UI8: return v.ullVal;
            default: return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<uint64_t> DbgHelpSource::VSlot(uint64_t vftableRva, uint64_t implRva) {
    const auto* slots = reinterpret_cast<const uint64_t*>(base_ + vftableRva);
    std::optional<uint64_t> found;
    for (size_t i = 0; i < 1024; ++i) {
        uint64_t ptr = slots[i];
        // Slots point into code; the next vftable's RTTI locator points into
        // .rdata and marks the end of this one.
        if (ptr < codeBegin_ || ptr >= codeEnd_) break;
        if (ptr - base_ == implRva) {
            if (found) return std::nullopt;  // ambiguous (identical code folding)
            found = i * sizeof(uint64_t);
        }
    }
    return found;
}

}  // namespace modpacks
