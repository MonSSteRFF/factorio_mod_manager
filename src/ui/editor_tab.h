// The "Mod packs" tab (spec mod-pack-editor-tab, design D8): a table of all
// installed mods on the left, the selected pack on the right, both looking like
// the "Installed" tab's table (name, version, sorting arrow), equal widths, one
// search for both and the mod details on the right of the window.
//
// Like ManageModsPane::refreshView, the row cells are created once per mod and
// re-added to the tables after every change; the tables never delete them, so a
// cell whose click handler is running stays alive. They are deleted when the
// window closes.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/engine.h"

namespace modpacks {

class Window;
class PackPanel;

class EditorTab {
public:
    EditorTab(Window& window, PackPanel& panel);
    ~EditorTab();

    engine::Widget* tab() const { return tab_; }
    engine::Widget* content() const { return content_; }

    // Re-reads the selected pack and rebuilds both tables, counters and hints.
    void Refresh();
    // Called when the window switches tabs (mod info pane belongs to the window).
    void OnTabsChanged();

private:
    struct Row {
        std::string name;
        std::string title;     // shown text
        std::wstring sortKey;  // lowercased title
        std::wstring search;   // lowercased title and internal name
        bool installed = true;
        engine::ModRow* modRow = nullptr;  // the "Installed" tab's row, for details
        // Cells, one set per table.
        engine::Widget* leftName = nullptr;
        engine::Widget* leftVersion = nullptr;
        engine::Widget* addButton = nullptr;
        engine::Widget* rightName = nullptr;
        engine::Widget* rightVersion = nullptr;
        engine::Widget* removeButton = nullptr;
    };

    struct Side {
        engine::Widget* column = nullptr;
        engine::Widget* header = nullptr;  // "All mods (N)"
        engine::Widget* table = nullptr;
        bool ascending = true;
        std::vector<Row*> shown;  // rows in table order, for selection
    };

    Row& EnsureRow(const std::string& name, const std::string& title, bool installed, engine::ModRow* modRow);
    bool Matches(const Row& row) const;
    void BuildSide(engine::Widget* body, Side& side);
    void Fill(Side& side, const std::vector<Row*>& rows, bool left);
    void Select(const Row* row);

    Window& window_;
    engine::Widget* tab_ = nullptr;
    engine::Widget* content_ = nullptr;
    engine::Widget* hint_ = nullptr;
    Side left_;
    Side right_;
    std::map<std::string, Row> rows_;  // by internal name
    std::wstring filter_;              // lowercased search text
    const Row* selected_ = nullptr;    // shown in the mod info pane
};

}  // namespace modpacks
