// State of one open "Mods" window: the pack store, the two pack bars, the
// "Mod packs" tab and the draft of checkbox changes for the selected pack. Created by the ModsGui constructor hook, destroyed by the
// ManageModsPane destructor hook (design D4).
#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/engine.h"
#include "packs/pack_store.h"

namespace modpacks {

class PackPanel;
class EditorTab;

struct InstalledMod {
    std::string name;   // internal name
    std::string title;  // as shown in the game's list
};

class Window {
public:
    explicit Window(engine::ModsGui* gui);
    ~Window();

    // Inserts the pack bar into the "Installed" tab and adds the "Mod packs" tab.
    void Build();

    engine::ManagePane* pane() const { return pane_; }
    const PackStore& store() const { return store_; }
    std::vector<InstalledMod> InstalledMods() const;  // without `base`, sorted by title

    // Actions from the pack bars and the editor; each saves and refreshes views.
    void Select(std::optional<std::string> name, bool apply);
    NameError Create(const std::string& name);
    NameError Rename(const std::string& name);
    void DeleteSelected();
    void AddMod(const std::string& mod);
    void RemoveMod(const std::string& mod);

    void ShowMessage(const std::string& text);

    bool EditorTabSelected() const;
    // The window switched tabs (hook on ModsGui::updateTabContentsVisibility).
    void OnTabsChanged();
    // The game applied the checkboxes (hook on ManageModsPane::apply).
    void OnConfirmed();
    // The game restyled a row of the "Installed" tab, e.g. after a click on its
    // checkbox (hook on ManageModsPane::updateStyleForModWidgets).
    void OnRowRestyled(engine::ModRow* row);

    // Whether writing the draft would change the selected pack (the "*" mark).
    bool HasDraft() const;

private:
    void Save();
    void ApplySelected();
    void RefreshViews();

    // Checkbox changes since the baseline go into the selected pack (design D1, D2).
    ModStates CurrentStates() const;
    void CommitDraft(const char* reason);
    void ResetBaseline();
    void UpdateDraftMark();

    engine::ModsGui* gui_;
    engine::ManagePane* pane_;
    std::filesystem::path path_;
    PackStore store_;
    std::unique_ptr<PackPanel> managePanel_;
    std::unique_ptr<PackPanel> editorPanel_;
    std::unique_ptr<EditorTab> editor_;
    ModStates baseline_;  // checkboxes when the selected pack was applied or last written
    ModStates seen_;      // checkboxes as last seen by OnRowRestyled
    bool shownDraft_ = false;
    bool applying_ = false;  // ApplySelected is changing the checkboxes
};

}  // namespace modpacks
