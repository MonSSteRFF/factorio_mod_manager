// Choosing the UI language like the game does (design D9).
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "i18n/strings.h"

namespace modpacks {

// "ru", "ru-RU" -> Ru; anything else -> En.
Language LanguageFromCode(std::string_view code);

// engineLocale: what the running game reports (empty if unknown).
// configLocale: `locale=` from config.ini (empty if absent); "auto" means
//               "follow Windows".
// windowsLanguage: two-letter Windows UI language, e.g. "ru".
Language ResolveLanguage(std::string_view engineLocale, std::string_view configLocale,
                         std::string_view windowsLanguage);

// `locale=` value from %APPDATA%\Factorio\config\config.ini, if any.
std::optional<std::string> ReadConfigLocale(const std::filesystem::path& configIni);
std::filesystem::path DefaultConfigIniPath();

// Two-letter ISO code of the Windows UI language ("ru", "en", ...).
std::string WindowsUiLanguage();

}  // namespace modpacks
