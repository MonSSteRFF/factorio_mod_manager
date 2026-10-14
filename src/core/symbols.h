// Resolution of the engine items listed in symbols.def.h (design D2).
#pragma once

#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace modpacks {

enum class SymKind { Func, Data, Field, Size, Base, VSlot, Enum };

enum class SymId : uint16_t {
#define MP_FUNC(id, name, required) id,
#define MP_DATA(id, name, required) id,
#define MP_FIELD(id, cls, member, required) id,
#define MP_SIZE(id, cls, required) id,
#define MP_BASE(id, cls, base, required) id,
#define MP_VSLOT(id, vftable, impl, required) id,
#define MP_ENUM(id, type, name, required) id,
#include "core/symbols.def.h"
    Count,
};

struct SymDef {
    SymId id;
    SymKind kind;
    const char* idName;
    const char* a;  // decorated name / class / vftable / enum type
    const char* b;  // member / base / impl / enumerator (nullptr if unused)
    bool required;
};

const std::vector<SymDef>& SymbolTable();

// Where values come from: DbgHelp over factorio.pdb in the game, a fake in tests.
// Func/Data/VSlot-vftable/impl return RVAs; Field/Base offsets; Size bytes;
// Enum values; VSlot the byte offset of the slot.
class SymbolSource {
public:
    virtual ~SymbolSource() = default;
    virtual std::optional<uint64_t> Rva(std::string_view decorated) = 0;
    virtual std::optional<uint64_t> FieldOffset(std::string_view cls, std::string_view member) = 0;
    virtual std::optional<uint64_t> TypeSize(std::string_view cls) = 0;
    virtual std::optional<uint64_t> BaseOffset(std::string_view cls, std::string_view base) = 0;
    virtual std::optional<uint64_t> EnumValue(std::string_view type, std::string_view name) = 0;
    // Byte offset of the single slot in the vftable at `vftableRva` holding `implRva`.
    virtual std::optional<uint64_t> VSlot(uint64_t vftableRva, uint64_t implRva) = 0;
};

struct Resolved {
    uint64_t value[static_cast<size_t>(SymId::Count)] = {};
    bool present[static_cast<size_t>(SymId::Count)] = {};

    uint64_t operator[](SymId id) const { return value[static_cast<size_t>(id)]; }
    bool has(SymId id) const { return present[static_cast<size_t>(id)]; }
};

struct ResolveReport {
    std::vector<std::string> missingRequired;  // "id (what)" for the log
    std::vector<std::string> missingOptional;
    bool ok() const { return missingRequired.empty(); }
};

// Resolves the whole table. `forceMissing` (from MODPACKS_FAKE_MISSING_SYMBOL)
// is treated as not found, to test the safe-disable path.
ResolveReport ResolveAll(SymbolSource& source, Resolved& out, std::string_view forceMissing = {});

// Compares the sizes of std types shared with the engine with our own sizeof.
// Returns a description of the first mismatch or empty string.
std::string CheckAbi(const Resolved& r);

// DbgHelp-backed source over the loaded factorio.exe and its PDB.
class DbgHelpSource : public SymbolSource {
public:
    DbgHelpSource() = default;
    ~DbgHelpSource() override;
    DbgHelpSource(const DbgHelpSource&) = delete;
    DbgHelpSource& operator=(const DbgHelpSource&) = delete;

    // Loads symbols for `module` from the folder of `exePath`. Fails if the
    // PDB is missing or does not match the exe (GUID/age).
    bool Open(HMODULE module, const std::wstring& exePath, std::string* error);

    std::optional<uint64_t> Rva(std::string_view decorated) override;
    std::optional<uint64_t> FieldOffset(std::string_view cls, std::string_view member) override;
    std::optional<uint64_t> TypeSize(std::string_view cls) override;
    std::optional<uint64_t> BaseOffset(std::string_view cls, std::string_view base) override;
    std::optional<uint64_t> EnumValue(std::string_view type, std::string_view name) override;
    std::optional<uint64_t> VSlot(uint64_t vftableRva, uint64_t implRva) override;

private:
    std::optional<ULONG> FindType(std::string_view name, bool wantEnum);
    std::vector<ULONG> Children(ULONG type);

    HANDLE process_ = nullptr;
    uint64_t base_ = 0;
    uint64_t imageSize_ = 0;
    uint64_t codeBegin_ = 0;  // executable sections, to tell where a vftable ends
    uint64_t codeEnd_ = 0;
    DWORD savedOptions_ = 0;
    bool publicsLoaded_ = false;
    std::unordered_map<std::string, uint64_t> publics_;  // decorated name -> RVA
};

}  // namespace modpacks
