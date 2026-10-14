// Text log of the mod: %APPDATA%\Factorio\modpacks\modpacks.log, rewritten on
// every game start (spec native-loader, "Лог модификации").
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace modpacks {

// %APPDATA%\Factorio\modpacks
std::filesystem::path DataDir();

// Opens (truncates) the log. Safe to call more than once; later calls are ignored.
void LogOpen(const std::filesystem::path& path);
void LogLine(std::string_view line);

void AppendPart(std::string& line, std::string_view s);
void AppendPart(std::string& line, const char* s);
void AppendPart(std::string& line, const std::string& s);
void AppendPart(std::string& line, const std::wstring& s);
void AppendPart(std::string& line, long long v);
void AppendPart(std::string& line, unsigned long long v);
inline void AppendPart(std::string& line, int v) { AppendPart(line, static_cast<long long>(v)); }
inline void AppendPart(std::string& line, unsigned v) { AppendPart(line, static_cast<unsigned long long>(v)); }
inline void AppendPart(std::string& line, long v) { AppendPart(line, static_cast<long long>(v)); }
inline void AppendPart(std::string& line, unsigned long v) { AppendPart(line, static_cast<unsigned long long>(v)); }
inline void AppendPart(std::string& line, bool v) { AppendPart(line, v ? "true" : "false"); }

// Concatenates parts into one log line.
template <typename... Parts>
void Log(const Parts&... parts) {
    std::string line;
    (AppendPart(line, parts), ...);
    LogLine(line);
}

// "0x1a2b"
std::string Hex(unsigned long long v);

}  // namespace modpacks
