// The pack bar shown on the "Installed" tab and on the "Mod packs" tab
// (specs mod-pack-manage-tab, mod-pack-editor-tab; design D5, D8):
//
//   normal:   [ pack list v ] [Create] [Rename] [Delete]  message
//   naming:   Pack name: [__________] [OK] [Cancel]       message
//   confirm:  Delete pack "X"?  [Yes] [No]
//
// Name input and delete confirmation happen inline, so no modal window is needed.
#pragma once

#include <string>
#include <vector>

#include "core/engine.h"

namespace modpacks {

class Window;

class PackPanel {
public:
    // `applyOnSelect`: choosing a pack also applies it (the "Installed" tab).
    PackPanel(Window& window, bool applyOnSelect);

    engine::Widget* root() const { return root_; }

    // Re-reads names and selection from the store.
    void Refresh();
    void ShowMessage(const std::string& text);  // empty hides it

private:
    enum class Mode { Normal, Create, Rename, Confirm };

    void SetMode(Mode mode);
    void OnSelect(int index);
    void OnConfirmName();

    Window& window_;
    bool applyOnSelect_;
    Mode mode_ = Mode::Normal;
    bool refreshing_ = false;
    std::vector<std::string> shownNames_;  // dropdown items after the placeholder
    bool shownEmpty_ = false;

    engine::Widget* root_ = nullptr;
    engine::Widget* normal_ = nullptr;
    engine::Widget* list_ = nullptr;
    engine::Widget* rename_ = nullptr;
    engine::Widget* remove_ = nullptr;
    engine::Widget* naming_ = nullptr;
    engine::Widget* nameField_ = nullptr;
    engine::Widget* confirm_ = nullptr;
    engine::Widget* question_ = nullptr;
    engine::Widget* message_ = nullptr;
};

}  // namespace modpacks
