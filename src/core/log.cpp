#include "core/log.h"

#include <windows.h>
#include <shlobj.h>

#include <cstdio>
#include <mutex>
#include <string>

#include "common/utf.h"

namespace modpacks {

namespace {

std::mutex g_mutex;
HANDLE g_file = INVALID_HANDLE_VALUE;
ULONGLONG g_start = 0;

}  // namespace

std::filesystem::path DataDir() {
    PWSTR appdata = nullptr;
    std::filesystem::path path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
        path = std::filesystem::path(appdata) / L"Factorio" / L"modpacks";
    }
    CoTaskMemFree(appdata);
    return path;
}

void LogOpen(const std::filesystem::path& path) {
    std::lock_guard lock(g_mutex);
    if (g_file != INVALID_HANDLE_VALUE) return;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    g_file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
    g_start = GetTickCount64();
}

void LogLine(std::string_view line) {
    std::lock_guard lock(g_mutex);
    char prefix[32];
    ULONGLONG ms = GetTickCount64() - g_start;
    std::snprintf(prefix, sizeof(prefix), "%6llu.%03llu ", ms / 1000, ms % 1000);
    std::string text = prefix;
    text.append(line);
    text.append("\r\n");
    if (g_file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(g_file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    }
    OutputDebugStringA(text.c_str());
}

void AppendPart(std::string& line, std::string_view s) { line.append(s); }
void AppendPart(std::string& line, const char* s) { line.append(s ? s : "(null)"); }
void AppendPart(std::string& line, const std::string& s) { line.append(s); }
void AppendPart(std::string& line, const std::wstring& s) { line.append(WideToUtf8(s)); }
void AppendPart(std::string& line, long long v) { line.append(std::to_string(v)); }
void AppendPart(std::string& line, unsigned long long v) { line.append(std::to_string(v)); }

std::string Hex(unsigned long long v) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "0x%llx", v);
    return buf;
}

}  // namespace modpacks
