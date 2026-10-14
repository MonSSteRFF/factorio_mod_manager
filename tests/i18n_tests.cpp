// Scenarios of spec mod-pack-localization.
#include <filesystem>
#include <fstream>

#include "doctest/doctest.h"
#include "i18n/language.h"
#include "i18n/strings.h"

using namespace modpacks;

TEST_CASE("language codes") {
    CHECK(LanguageFromCode("ru") == Language::Ru);
    CHECK(LanguageFromCode("RU-ru") == Language::Ru);
    CHECK(LanguageFromCode("en") == Language::En);
    CHECK(LanguageFromCode("de") == Language::En);
    CHECK(LanguageFromCode("") == Language::En);
}

TEST_CASE("the running game's language wins") {
    CHECK(ResolveLanguage("ru", "en", "en") == Language::Ru);
    CHECK(ResolveLanguage("en", "ru", "ru") == Language::En);  // game in English on Russian Windows
    CHECK(ResolveLanguage("de", "", "ru") == Language::En);
}

TEST_CASE("config.ini when the game cannot be asked") {
    CHECK(ResolveLanguage("", "ru", "en") == Language::Ru);
    CHECK(ResolveLanguage("", "de", "ru") == Language::En);
    CHECK(ResolveLanguage("", "auto", "ru") == Language::Ru);  // auto follows Windows
    CHECK(ResolveLanguage("", "auto", "en") == Language::En);
    CHECK(ResolveLanguage("", "", "ru") == Language::Ru);      // no setting at all
}

TEST_CASE("reading locale from config.ini") {
    std::filesystem::path p = std::filesystem::temp_directory_path() / "modpacks-test-config.ini";
    {
        std::ofstream out(p, std::ios::binary);
        out << "; version=13\r\n[path]\r\nread-data=x\r\nlocale=wrong-section\r\n\r\n[general]\r\nlocale=ru\r\n\r\n[other]\r\n";
    }
    CHECK(ReadConfigLocale(p) == std::optional<std::string>("ru"));
    std::filesystem::remove(p);
    CHECK_FALSE(ReadConfigLocale(p).has_value());
}

TEST_CASE("every string exists in English; Russian falls back") {
    for (int i = 0; i < static_cast<int>(Str::Count); ++i) {
        auto id = static_cast<Str>(i);
        CHECK_FALSE(Text(id, Language::En).empty());
        CHECK_FALSE(Text(id, Language::Ru).empty());
    }
    CHECK(Text(Str::ButtonCreate, Language::Ru) == "Создать");
    CHECK(Text(Str::ButtonCreate, Language::En) == "Create");
}

TEST_CASE("format inserts the argument") {
    SetLanguage(Language::Ru);
    CHECK(Format(Str::ConfirmDelete, "Krastorio") == "Удалить пакет «Krastorio»?");
    SetLanguage(Language::En);
    CHECK(Format(Str::ColumnAll, "12") == "All mods (12)");
}
