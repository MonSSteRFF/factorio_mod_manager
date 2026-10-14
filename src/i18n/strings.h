// UI strings in Russian and English (spec mod-pack-localization, design D9).
#pragma once

#include <string>
#include <string_view>

namespace modpacks {

enum class Language { En, Ru };

enum class Str {
    TabTitle,
    PlaceholderNoPacks,
    PlaceholderNotSelected,
    ButtonCreate,
    ButtonRename,
    ButtonDelete,
    TipCreate,
    TipRename,
    TipDelete,
    TipPackList,
    PromptName,
    ButtonOk,
    ButtonCancel,
    ConfirmDelete,  // {} = pack name
    ButtonYes,
    ButtonNo,
    ErrorNameEmpty,
    ErrorNameDuplicate,
    ErrorSave,      // {} = reason
    ErrorLoad,      // {} = backup file
    MissingMods,    // {} = comma-separated mod names
    ColumnName,
    ColumnVersion,
    ColumnAll,      // {} = count
    ColumnPack,     // {} = count
    HintNoPack,
    NotInstalled,
    TipAddToPack,
    TipRemoveFromPack,
    Count,
};

// The current UI language; English until SetLanguage is called.
Language CurrentLanguage();
void SetLanguage(Language language);

// Text for `id` in `language`, falling back to English if the Russian entry is empty.
std::string_view Text(Str id, Language language);
inline std::string_view Text(Str id) { return Text(id, CurrentLanguage()); }

// Text with the single "{}" placeholder replaced by `arg`.
std::string Format(Str id, std::string_view arg);

}  // namespace modpacks
