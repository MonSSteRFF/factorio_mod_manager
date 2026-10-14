// Scenarios of spec native-loader that can run without the game: the
// all-or-nothing symbol table, ABI checks, the cache and the code scan.
#include <filesystem>
#include <map>
#include <set>

#include "core/code_scan.h"
#include "core/symbols.h"
#include "core/symcache.h"
#include "doctest/doctest.h"

using namespace modpacks;

namespace {

// Answers every lookup with a fixed value, except names listed in `missing`.
class FakeSource : public SymbolSource {
public:
    std::set<std::string> missing;
    std::map<std::string, uint64_t> sizes;

    std::optional<uint64_t> Rva(std::string_view n) override { return Get(n, 0x1000); }
    std::optional<uint64_t> FieldOffset(std::string_view c, std::string_view m) override {
        return Get(std::string(c) + "::" + std::string(m), 8);
    }
    std::optional<uint64_t> TypeSize(std::string_view c) override {
        if (auto it = sizes.find(std::string(c)); it != sizes.end()) return it->second;
        return Get(c, 64);
    }
    std::optional<uint64_t> BaseOffset(std::string_view c, std::string_view b) override {
        return Get(std::string(c) + ":" + std::string(b), 8);
    }
    std::optional<uint64_t> EnumValue(std::string_view t, std::string_view n) override {
        return Get(std::string(t) + "::" + std::string(n), 1);
    }
    std::optional<uint64_t> VSlot(uint64_t, uint64_t) override { return 0x100; }

private:
    std::optional<uint64_t> Get(std::string_view key, uint64_t value) {
        if (missing.contains(std::string(key))) return std::nullopt;
        return value;
    }
};

FakeSource GoodSource() {
    FakeSource s;
    s.sizes["std::basic_string<char,std::char_traits<char>,std::allocator<char> >"] = sizeof(std::string);
    s.sizes["NamedBool<InSubheaderTag>"] = 1;
    return s;
}

}  // namespace

TEST_CASE("table: every entry has a unique id in order") {
    const auto& table = SymbolTable();
    REQUIRE(table.size() == static_cast<size_t>(SymId::Count));
    for (size_t i = 0; i < table.size(); ++i) CHECK(static_cast<size_t>(table[i].id) == i);
}

TEST_CASE("all items found: resolution succeeds") {
    FakeSource source = GoodSource();
    Resolved r;
    ResolveReport report = ResolveAll(source, r);
    CHECK(report.ok());
    CHECK(r.has(SymId::ModsGui_ctor));
    CHECK(r[SymId::ModsGui_ctor] == 0x1000);
}

TEST_CASE("a missing required item fails the whole table") {
    FakeSource source = GoodSource();
    source.missing.insert("??0ModsGui@@QEAA@XZ");
    Resolved r;
    ResolveReport report = ResolveAll(source, r);
    CHECK_FALSE(report.ok());
    REQUIRE(report.missingRequired.size() == 1);
    CHECK(report.missingRequired[0].find("ModsGui_ctor") == 0);
}

TEST_CASE("a missing optional item does not") {
    FakeSource source = GoodSource();
    source.missing.insert("GlobalContext::localeProvider");
    Resolved r;
    ResolveReport report = ResolveAll(source, r);
    CHECK(report.ok());
    CHECK(report.missingOptional.size() == 1);
    CHECK_FALSE(r.has(SymId::GlobalContext_localeProvider));
}

TEST_CASE("MODPACKS_FAKE_MISSING_SYMBOL forces an item to be missing") {
    FakeSource source = GoodSource();
    Resolved r;
    ResolveReport report = ResolveAll(source, r, "ManageModsPane_refreshView");
    CHECK_FALSE(report.ok());
    CHECK(report.missingRequired[0].find("ManageModsPane_refreshView") == 0);
}

TEST_CASE("ABI check compares std type sizes") {
    FakeSource source = GoodSource();
    Resolved r;
    REQUIRE(ResolveAll(source, r).ok());
    CHECK(CheckAbi(r) == "");
    r.value[static_cast<size_t>(SymId::abi_String)] = 40;  // e.g. a debug-STL game build
    CHECK(CheckAbi(r).find("std::string") == 0);
}

TEST_CASE("symbol cache round trip and invalidation") {
    FakeSource source = GoodSource();
    source.missing.insert("GlobalContext::localeProvider");  // optional, stored as absent
    Resolved r;
    REQUIRE(ResolveAll(source, r).ok());
    std::filesystem::path file = std::filesystem::temp_directory_path() / "modpacks-test-symcache.json";
    SaveSymbolCache(file, "KEY-1", r);

    Resolved loaded;
    REQUIRE(LoadSymbolCache(file, "KEY-1", loaded));
    CHECK(loaded[SymId::ModsGui_ctor] == r[SymId::ModsGui_ctor]);
    CHECK_FALSE(loaded.has(SymId::GlobalContext_localeProvider));

    // Another exe (game updated): the cache is not used.
    CHECK_FALSE(LoadSymbolCache(file, "KEY-2", loaded));
    CHECK_FALSE(LoadSymbolCache(file, "", loaded));
    std::filesystem::remove(file);
}

TEST_CASE("ownership bit is read from agui::Widget::clearParentWidget (Factorio 2.1.21 bytes)") {
    // First 48 bytes of the function in 2.1.21; contains
    // `test dword ptr [rdi+88h], 200h` (f7 87 88 00 00 00 00 02 00 00).
    const uint8_t code[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x20, 0x0f, 0xb6,
                            0xda, 0x48, 0x8b, 0xf9, 0xe8, 0xfb, 0xfd, 0xff, 0xff, 0x48, 0x8b, 0xcf,
                            0xe8, 0xc3, 0xfe, 0xff, 0xff, 0x84, 0xdb, 0x75, 0x18, 0xf7, 0x87, 0x88,
                            0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x74, 0x0c, 0x48, 0x8b, 0x07};
    CHECK(FindTestedMask(code, sizeof(code), 0x88) == std::optional<uint32_t>(0x200));
    CHECK_FALSE(FindTestedMask(code, sizeof(code), 0x90).has_value());
}

TEST_CASE("ownership bit as a byte test") {
    // test byte ptr [rcx+89h], 2  ->  bit 0x200 of the dword at 0x88
    const uint8_t code[] = {0x90, 0xf6, 0x81, 0x89, 0x00, 0x00, 0x00, 0x02, 0x90, 0x90, 0x90, 0x90};
    CHECK(FindTestedMask(code, sizeof(code), 0x88) == std::optional<uint32_t>(0x200));
}
