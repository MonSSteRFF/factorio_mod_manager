#include "i18n/language.h"

#include <windows.h>
#include <shlobj.h>

#include <fstream>

#include "common/utf.h"

namespace modpacks {

namespace {

std::string Lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::string_view Trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

}  // namespace

Language LanguageFromCode(std::string_view code) {
    std::string c = Lower(Trim(code));
    if (c == "ru" || c.starts_with("ru-") || c.starts_with("ru_")) return Language::Ru;
    return Language::En;
}

Language ResolveLanguage(std::string_view engineLocale, std::string_view configLocale,
                         std::string_view windowsLanguage) {
    if (!Trim(engineLocale).empty()) return LanguageFromCode(engineLocale);
    std::string config = Lower(Trim(configLocale));
    if (!config.empty() && config != "auto") return LanguageFromCode(config);
    return LanguageFromCode(windowsLanguage);
}

std::optional<std::string> ReadConfigLocale(const std::filesystem::path& configIni) {
    std::ifstream in(configIni, std::ios::binary);
    if (!in) return std::nullopt;
    std::string line;
    bool inGeneral = false;
    while (std::getline(in, line)) {
        std::string_view l = Trim(line);
        if (l.starts_with("\xEF\xBB\xBF")) l.remove_prefix(3);
        if (l.starts_with("[")) {
            inGeneral = l == "[general]";
            continue;
        }
        if (!inGeneral || l.starts_with(";") || l.starts_with("#")) continue;
        if (l.starts_with("locale=")) return std::string(Trim(l.substr(7)));
    }
    return std::nullopt;
}

std::filesystem::path DefaultConfigIniPath() {
    PWSTR appdata = nullptr;
    std::filesystem::path path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
        path = std::filesystem::path(appdata) / L"Factorio" / L"config" / L"config.ini";
    }
    CoTaskMemFree(appdata);
    return path;
}

std::string WindowsUiLanguage() {
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
    LCID lcid = MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT);
    if (LCIDToLocaleName(lcid, name, LOCALE_NAME_MAX_LENGTH, 0) == 0) return {};
    std::string code = WideToUtf8(name);
    size_t dash = code.find('-');
    return dash == std::string::npos ? code : code.substr(0, dash);
}

}  // namespace modpacks
