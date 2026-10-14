#include "i18n/strings.h"

#include <array>

namespace modpacks {

namespace {

struct Entry {
    std::string_view en;
    std::string_view ru;
};

constexpr std::array<Entry, static_cast<size_t>(Str::Count)> kStrings = {{
    /* TabTitle */ {"Mod packs", "Пакеты модов"},
    /* PlaceholderNoPacks */ {"No mod packs", "Нет пакетов"},
    /* PlaceholderNotSelected */ {"No pack selected", "Пакет не выбран"},
    /* ButtonCreate */ {"Create", "Создать"},
    /* ButtonRename */ {"Rename", "Переименовать"},
    /* ButtonDelete */ {"Delete", "Удалить"},
    /* TipCreate */ {"Save the currently enabled mods as a new pack",
                     "Сохранить включённые сейчас моды в новый пакет"},
    /* TipRename */ {"Rename the selected pack", "Переименовать выбранный пакет"},
    /* TipDelete */ {"Delete the selected pack", "Удалить выбранный пакет"},
    /* TipPackList */ {"Selected mod pack", "Выбранный пакет модов"},
    /* PromptName */ {"Pack name:", "Название пакета:"},
    /* ButtonOk */ {"OK", "OK"},
    /* ButtonCancel */ {"Cancel", "Отмена"},
    /* ConfirmDelete */ {"Delete pack \"{}\"?", "Удалить пакет «{}»?"},
    /* ButtonYes */ {"Yes", "Да"},
    /* ButtonNo */ {"No", "Нет"},
    /* ErrorNameEmpty */ {"The name cannot be empty", "Название не может быть пустым"},
    /* ErrorNameDuplicate */ {"A pack with this name already exists", "Пакет с таким названием уже есть"},
    /* ErrorSave */ {"Could not save mod packs: {}", "Не удалось сохранить пакеты: {}"},
    /* ErrorLoad */ {"mod-packs.json could not be read, a copy was saved: {}",
                     "Не удалось прочитать mod-packs.json, копия сохранена: {}"},
    /* MissingMods */ {"Not installed: {}", "Не установлены: {}"},
    /* ColumnName */ {"Name", "Название"},
    /* ColumnVersion */ {"Version", "Версия"},
    /* ColumnAll */ {"All mods ({})", "Все моды ({})"},
    /* ColumnPack */ {"In pack ({})", "В пакете ({})"},
    /* HintNoPack */ {"Create or select a pack", "Создайте или выберите пакет"},
    /* NotInstalled */ {"not installed", "не установлен"},
    /* TipAddToPack */ {"Add to the pack", "Добавить в пакет"},
    /* TipRemoveFromPack */ {"Remove from the pack", "Убрать из пакета"},
}};

Language g_language = Language::En;

}  // namespace

Language CurrentLanguage() { return g_language; }
void SetLanguage(Language language) { g_language = language; }

std::string_view Text(Str id, Language language) {
    const Entry& e = kStrings[static_cast<size_t>(id)];
    if (language == Language::Ru && !e.ru.empty()) return e.ru;
    return e.en;
}

std::string Format(Str id, std::string_view arg) {
    std::string text(Text(id));
    size_t pos = text.find("{}");
    if (pos != std::string::npos) text.replace(pos, 2, arg);
    return text;
}

}  // namespace modpacks
