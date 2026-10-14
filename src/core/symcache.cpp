#include "core/symcache.h"

#include <cstdio>
#include <fstream>
#include <sstream>

#include "nlohmann/json.hpp"

namespace modpacks {

namespace {

struct CodeViewRsds {
    DWORD signature;  // 'RSDS'
    GUID guid;
    DWORD age;
    char path[1];
};

// The table itself is part of the key: a new mod version may look up other items.
std::string TableFingerprint() {
    std::string all;
    for (const SymDef& d : SymbolTable()) {
        all += d.idName;
        all += '|';
        all += d.a;
        all += '|';
        all += d.b ? d.b : "";
        all += '\n';
    }
    // FNV-1a, enough to notice a changed table.
    unsigned long long h = 1469598103934665603ull;
    for (unsigned char c : all) h = (h ^ c) * 1099511628211ull;
    char buf[20];
    std::snprintf(buf, sizeof(buf), "%016llx", h);
    return buf;
}

}  // namespace

std::string ModuleKey(HMODULE module) {
    auto* base = reinterpret_cast<const BYTE*>(module);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    auto* entries = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(base + dir.VirtualAddress);
    size_t count = dir.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].Type != IMAGE_DEBUG_TYPE_CODEVIEW || !entries[i].AddressOfRawData) continue;
        auto* cv = reinterpret_cast<const CodeViewRsds*>(base + entries[i].AddressOfRawData);
        if (cv->signature != 0x53445352) continue;  // "RSDS"
        const GUID& g = cv->guid;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%08lX%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X-%lu-%08lX-%lu", g.Data1,
                      g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5],
                      g.Data4[6], g.Data4[7], cv->age, nt->FileHeader.TimeDateStamp,
                      nt->OptionalHeader.SizeOfImage);
        return buf;
    }
    return {};
}

bool LoadSymbolCache(const std::filesystem::path& path, const std::string& key, Resolved& out) {
    if (key.empty()) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    nlohmann::json root = nlohmann::json::parse(ss.str(), nullptr, false);
    if (!root.is_object() || root.value("key", "") != key || root.value("mod", "") != MODPACKS_VERSION ||
        root.value("table", "") != TableFingerprint() || !root.contains("values") || !root["values"].is_object()) {
        return false;
    }
    Resolved r;
    const auto& values = root["values"];
    for (const SymDef& d : SymbolTable()) {
        if (!values.contains(d.idName)) return false;
        const auto& v = values[d.idName];
        if (v.is_null()) {
            if (d.required) return false;
            continue;
        }
        if (!v.is_number_unsigned()) return false;
        r.value[static_cast<size_t>(d.id)] = v.get<uint64_t>();
        r.present[static_cast<size_t>(d.id)] = true;
    }
    out = r;
    return true;
}

void SaveSymbolCache(const std::filesystem::path& path, const std::string& key, const Resolved& r) {
    if (key.empty()) return;
    nlohmann::ordered_json root;
    root["key"] = key;
    root["mod"] = MODPACKS_VERSION;
    root["table"] = TableFingerprint();
    nlohmann::ordered_json values = nlohmann::ordered_json::object();
    for (const SymDef& d : SymbolTable()) {
        if (r.has(d.id)) {
            values[d.idName] = r[d.id];
        } else {
            values[d.idName] = nullptr;
        }
    }
    root["values"] = std::move(values);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::filesystem::path tmp = path;
    tmp += L".tmp";
    {
        std::ofstream outFile(tmp, std::ios::binary | std::ios::trunc);
        outFile << root.dump(1);
        if (!outFile) return;
    }
    MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
}

}  // namespace modpacks
