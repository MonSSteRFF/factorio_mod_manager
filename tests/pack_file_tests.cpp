// Scenarios of spec mod-pack-storage: storage, reliable writes, corrupt file.
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "doctest/doctest.h"
#include "packs/pack_file.h"

using namespace modpacks;
namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        wchar_t base[MAX_PATH];
        GetTempPathW(MAX_PATH, base);
        path = fs::path(base) / (L"modpacks-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                 std::to_wstring(GetTickCount64()));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::string Read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void Write(const fs::path& p, const std::string& text) {
    std::ofstream out(p, std::ios::binary);
    out << text;
}

int CountBackups(const fs::path& dir) {
    int n = 0;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (e.path().filename().wstring().find(L".broken-") != std::wstring::npos) ++n;
    }
    return n;
}

}  // namespace

TEST_CASE("missing file: empty store, nothing created") {
    TempDir dir;
    fs::path file = dir.path / "mod-packs.json";
    PackStore store;
    store.Create("leftover", {});
    LoadResult r = LoadPacks(file, store);
    CHECK(r.status == LoadStatus::Missing);
    CHECK(store.packs().empty());
    CHECK_FALSE(fs::exists(file));
}

TEST_CASE("save and load round trip keeps packs and selection") {
    TempDir dir;
    fs::path file = dir.path / "sub" / "mod-packs.json";
    PackStore store;
    store.Create("Пакет", {"flib", "Krastorio2"});
    store.Create("Vanilla", {"space-age"});
    store.Select("Пакет");
    REQUIRE(SavePacks(file, store));

    PackStore loaded;
    LoadResult r = LoadPacks(file, loaded);
    REQUIRE(r.status == LoadStatus::Ok);
    REQUIRE(loaded.packs().size() == 2);
    CHECK(loaded.Find("Пакет")->mods == std::vector<std::string>{"Krastorio2", "flib"});
    CHECK(loaded.selected() == std::optional<std::string>("Пакет"));
    CHECK_FALSE(fs::exists(file.string() + ".tmp"));
}

TEST_CASE("corrupt file: backed up, not overwritten, store empty") {
    TempDir dir;
    fs::path file = dir.path / "mod-packs.json";
    const std::string garbage = "{ this is not json";
    Write(file, garbage);

    PackStore store;
    LoadResult r = LoadPacks(file, store);
    CHECK(r.status == LoadStatus::Corrupt);
    CHECK_FALSE(r.error.empty());
    CHECK(store.packs().empty());
    REQUIRE_FALSE(r.backup.empty());
    CHECK(Read(r.backup) == garbage);
    CHECK(Read(file) == garbage);
    CHECK(CountBackups(dir.path) == 1);
}

TEST_CASE("corrupt structure is rejected") {
    PackStore store;
    std::string error;
    CHECK_FALSE(ParsePacks(R"({"format": 2, "packs": []})", store, &error));
    CHECK_FALSE(ParsePacks(R"({"format": 1})", store, &error));
    CHECK_FALSE(ParsePacks(R"({"format": 1, "packs": [{"name": 5, "mods": []}]})", store, &error));
    CHECK_FALSE(ParsePacks(R"({"format": 1, "packs": [{"name": "a", "mods": [1]}]})", store, &error));
    CHECK(ParsePacks(R"({"format": 1, "selected": null, "packs": []})", store, &error));
}

TEST_CASE("interrupted write leaves the previous file intact") {
    TempDir dir;
    fs::path file = dir.path / "mod-packs.json";
    PackStore v1;
    v1.Create("Old", {"flib"});
    REQUIRE(SavePacks(file, v1));
    std::string before = Read(file);

    // A crash after writing the temporary file but before the swap leaves a
    // stray .tmp next to the intact original.
    Write(file.string() + ".tmp", "{ half written");
    PackStore loaded;
    REQUIRE(LoadPacks(file, loaded).status == LoadStatus::Ok);
    CHECK(loaded.Find("Old"));
    CHECK(Read(file) == before);

    // The next save replaces both.
    PackStore v2;
    v2.Create("New", {});
    REQUIRE(SavePacks(file, v2));
    PackStore after;
    REQUIRE(LoadPacks(file, after).status == LoadStatus::Ok);
    CHECK(after.Find("New"));
    CHECK_FALSE(fs::exists(file.string() + ".tmp"));
}

TEST_CASE("documented example in docs/mod-packs-format.md parses") {
    const char* example = R"({
  "format": 1,
  "selected": "Krastorio run",
  "packs": [
    { "name": "Krastorio run", "mods": ["Krastorio2", "Krastorio2Assets", "flib", "space-age"] },
    { "name": "Vanilla Space Age", "mods": ["elevated-rails", "quality", "space-age"] }
  ]
})";
    PackStore store;
    std::string error;
    REQUIRE(ParsePacks(example, store, &error));
    CHECK(store.packs().size() == 2);
    CHECK(store.selected() == std::optional<std::string>("Krastorio run"));
}
