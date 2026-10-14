#include "packs/pack_file.h"

#include <windows.h>
#include <shlobj.h>

#include <fstream>
#include <sstream>

#include "nlohmann/json.hpp"

namespace modpacks {

namespace {

constexpr int kFormat = 1;

std::string Timestamp() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char buf[32];
    wsprintfA(buf, "%04u%02u%02u-%02u%02u%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return buf;
}

bool ReadFile(const std::filesystem::path& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return !in.bad();
}

}  // namespace

std::filesystem::path DefaultPacksPath() {
    PWSTR appdata = nullptr;
    std::filesystem::path path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
        path = std::filesystem::path(appdata) / L"Factorio" / L"mods" / L"mod-packs.json";
    }
    CoTaskMemFree(appdata);
    return path;
}

std::string SerializePacks(const PackStore& store) {
    nlohmann::ordered_json root;
    root["format"] = kFormat;
    root["selected"] = store.selected() ? nlohmann::ordered_json(*store.selected()) : nullptr;
    nlohmann::ordered_json packs = nlohmann::ordered_json::array();
    for (const Pack& p : store.packs()) {
        packs.push_back({{"name", p.name}, {"mods", p.mods}});
    }
    root["packs"] = std::move(packs);
    return root.dump(2) + "\n";
}

bool ParsePacks(std::string_view json, PackStore& store, std::string* error) {
    auto fail = [&](std::string msg) {
        if (error) *error = std::move(msg);
        return false;
    };
    nlohmann::json root = nlohmann::json::parse(json, nullptr, false);
    if (root.is_discarded()) return fail("invalid JSON");
    if (!root.is_object()) return fail("root is not an object");
    if (!root.contains("format") || !root["format"].is_number_integer()) return fail("missing format");
    if (root["format"].get<int>() != kFormat) return fail("unsupported format " + root["format"].dump());
    if (!root.contains("packs") || !root["packs"].is_array()) return fail("missing packs array");

    std::vector<Pack> packs;
    for (const auto& item : root["packs"]) {
        if (!item.is_object() || !item.contains("name") || !item["name"].is_string() ||
            !item.contains("mods") || !item["mods"].is_array()) {
            return fail("malformed pack entry");
        }
        Pack pack{item["name"].get<std::string>(), {}};
        for (const auto& mod : item["mods"]) {
            if (!mod.is_string()) return fail("mod name is not a string in pack " + pack.name);
            pack.mods.push_back(mod.get<std::string>());
        }
        packs.push_back(std::move(pack));
    }
    std::optional<std::string> selected;
    if (root.contains("selected") && root["selected"].is_string()) selected = root["selected"].get<std::string>();
    store.Assign(std::move(packs), std::move(selected));
    return true;
}

LoadResult LoadPacks(const std::filesystem::path& path, PackStore& store) {
    LoadResult result;
    store.Assign({}, std::nullopt);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return result;

    std::string text;
    std::string error;
    if (!ReadFile(path, text)) {
        error = "cannot read file";
    } else {
        PackStore parsed;
        if (ParsePacks(text, parsed, &error)) {
            store = std::move(parsed);
            result.status = LoadStatus::Ok;
            return result;
        }
    }

    result.status = LoadStatus::Corrupt;
    result.error = error;
    std::filesystem::path backup = path;
    backup += L".broken-" + std::filesystem::path(Timestamp()).wstring();
    if (std::filesystem::copy_file(path, backup, std::filesystem::copy_options::skip_existing, ec)) {
        result.backup = backup;
    }
    return result;
}

bool SavePacks(const std::filesystem::path& path, const PackStore& store, std::string* error) {
    auto fail = [&](const char* what) {
        if (error) *error = what + std::string(" (") + std::to_string(GetLastError()) + ")";
        return false;
    };
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::filesystem::path tmp = path;
    tmp += L".tmp";
    std::string text = SerializePacks(store);
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return fail("cannot create temporary file");
    DWORD written = 0;
    BOOL ok = WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
              written == text.size() && FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok) {
        DeleteFileW(tmp.c_str());
        return fail("cannot write temporary file");
    }
    // MoveFileEx with REPLACE_EXISTING is atomic on the same volume (NTFS rename).
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return fail("cannot replace file");
    }
    return true;
}

}  // namespace modpacks
