// Mod packs: model, naming rules and operations (spec mod-pack-storage).
// Engine-independent; the UI layer translates widget events into these calls.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace modpacks {

// `base` cannot be disabled, so it never belongs to a pack.
inline constexpr std::string_view kBaseMod = "base";

struct Pack {
    std::string name;
    std::vector<std::string> mods;  // internal mod names, sorted, unique, without `base`
};

enum class NameError {
    None,
    Empty,      // nothing left after trimming
    Duplicate,  // another pack has the same name ignoring case
};

// Checkbox state of the installed mods: internal name -> enabled.
using ModStates = std::map<std::string, bool, std::less<>>;

// Mods switched on and off between two checkbox snapshots (spec mod-pack-manage-tab,
// design D1). Both lists are sorted and never contain `base`.
struct ModDelta {
    std::vector<std::string> added;
    std::vector<std::string> removed;
    bool empty() const { return added.empty() && removed.empty(); }
};

// Result of applying a pack to the installed mods.
struct ApplyPlan {
    // For every installed mod except `base`: whether it ends up enabled.
    std::vector<std::pair<std::string, bool>> enabled;
    // Mods of the pack that are not installed.
    std::vector<std::string> missing;
};

// Trims ASCII and Unicode whitespace at both ends of a UTF-8 name.
std::string TrimName(std::string_view name);

// Case-insensitive equality of UTF-8 names (full Unicode simple case folding,
// so "Пакет" == "пакет").
bool NamesEqual(std::string_view a, std::string_view b);

// Case-insensitive ordering used for the pack list.
bool NameLess(std::string_view a, std::string_view b);

class PackStore {
public:
    // Packs ordered by name, case-insensitively.
    const std::vector<Pack>& packs() const { return packs_; }
    const std::optional<std::string>& selected() const { return selected_; }

    const Pack* Find(std::string_view name) const;

    // Validates a (trimmed) name; `renaming` is the pack being renamed, which
    // may keep its own name with a different case.
    NameError ValidateName(std::string_view name, std::string_view renaming = {}) const;

    // Creates a pack from the given enabled mods and selects it.
    NameError Create(std::string_view name, std::vector<std::string> enabledMods);
    NameError Rename(std::string_view from, std::string_view to);
    // Removes the pack; clears the selection if it was selected.
    bool Delete(std::string_view name);
    // Selects an existing pack or clears the selection (nullopt / unknown name).
    void Select(std::optional<std::string_view> name);

    bool AddMod(std::string_view pack, std::string_view mod);
    bool RemoveMod(std::string_view pack, std::string_view mod);
    // Adds `delta.added` and removes `delta.removed`; other mods of the pack,
    // installed or not, stay. Returns whether the pack changed.
    bool ApplyDelta(std::string_view pack, const ModDelta& delta);

    // Replaces all data (used by the file loader). Normalizes and drops invalid
    // or duplicate entries.
    void Assign(std::vector<Pack> packs, std::optional<std::string> selected);

    // Bumped on every change so views can tell they are stale.
    unsigned revision() const { return revision_; }

private:
    Pack* FindMutable(std::string_view name);
    void Sort();
    void Touch() { ++revision_; }

    std::vector<Pack> packs_;
    std::optional<std::string> selected_;
    unsigned revision_ = 0;
};

// Normalizes a mod list: drops `base` and empty names, sorts, removes duplicates.
std::vector<std::string> NormalizeMods(std::vector<std::string> mods);

// Computes which installed mods are enabled by `pack`. `installed` may contain
// `base`, which is skipped.
ApplyPlan PlanApply(const Pack& pack, const std::vector<std::string>& installed);

// Mods whose checkbox differs between `baseline` and `current`. Mods present in
// only one snapshot are ignored, so a mod switched on and back off is no change.
ModDelta DraftDelta(const ModStates& baseline, const ModStates& current);

// Whether ApplyDelta(pack, delta) would change the pack.
bool DeltaChangesPack(const Pack& pack, const ModDelta& delta);

// The pack whose installed mods are exactly the enabled mods of `current`
// (`base` ignored, mods of the pack that are not installed ignored). With several
// matches `preferred` wins if it is one of them, otherwise the first in `packs`.
std::optional<std::string> MatchPack(const std::vector<Pack>& packs, const ModStates& current,
                                     std::optional<std::string_view> preferred);

}  // namespace modpacks
