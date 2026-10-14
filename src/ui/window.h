// State of one open "Mods" window: the pack store, the two pack bars and the
// "Mod packs" tab. Created by the ModsGui constructor hook, destroyed by the
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

private:
    void Save();
    void ApplySelected();
    void RefreshViews();

    engine::ModsGui* gui_;
    engine::ManagePane* pane_;
    std::filesystem::path path_;
    PackStore store_;
    std::unique_ptr<PackPanel> managePanel_;
    std::unique_ptr<PackPanel> editorPanel_;
    std::unique_ptr<EditorTab> editor_;
};

}  // namespace modpacks
