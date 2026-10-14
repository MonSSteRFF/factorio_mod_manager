#include "ui/pack_panel.h"

#include "core/guard.h"
#include "i18n/strings.h"
#include "ui/window.h"

namespace modpacks {

namespace {

engine::Widget* Button(Str text, Str tip) {
    engine::Widget* b = engine::NewTextButton(std::string(Text(text)), "button");
    if (tip != Str::Count) engine::SetToolTip(b, std::string(Text(tip)));
    return b;
}

}  // namespace

PackPanel::PackPanel(Window& window, bool applyOnSelect) : window_(window), applyOnSelect_(applyOnSelect) {
    root_ = engine::NewHorizontalFlow("horizontal_flow");

    // normal mode
    normal_ = engine::NewHorizontalFlow("horizontal_flow");
    list_ = engine::NewDropDown("dropdown");
    engine::SetToolTip(list_, std::string(Text(Str::TipPackList)));
    engine::Widget* create = Button(Str::ButtonCreate, Str::TipCreate);
    rename_ = Button(Str::ButtonRename, Str::TipRename);
    remove_ = Button(Str::ButtonDelete, Str::TipDelete);
    engine::Add(normal_, list_);
    engine::Add(normal_, create);
    engine::Add(normal_, rename_);
    engine::Add(normal_, remove_);

    // naming mode (create / rename)
    naming_ = engine::NewHorizontalFlow("horizontal_flow");
    nameField_ = engine::NewTextField("textbox");
    engine::Widget* ok = Button(Str::ButtonOk, Str::Count);
    engine::Widget* cancel = Button(Str::ButtonCancel, Str::Count);
    engine::Add(naming_, engine::NewLabel(std::string(Text(Str::PromptName)), "label"));
    engine::Add(naming_, nameField_);
    engine::Add(naming_, ok);
    engine::Add(naming_, cancel);

    // delete confirmation
    confirm_ = engine::NewHorizontalFlow("horizontal_flow");
    question_ = engine::NewLabel("", "label");
    engine::Widget* yes = Button(Str::ButtonYes, Str::Count);
    engine::Widget* no = Button(Str::ButtonNo, Str::Count);
    engine::Add(confirm_, question_);
    engine::Add(confirm_, yes);
    engine::Add(confirm_, no);

    message_ = engine::NewLabel("", "bold_red_label");

    engine::Add(root_, normal_);
    engine::Add(root_, naming_);
    engine::Add(root_, confirm_);
    engine::Add(root_, message_);

    engine::OnItemSelect(list_, GuardedHandler<int>("pack list select", [this](int index) { OnSelect(index); }));
    engine::OnClick(create, GuardedHandler("create", [this] { SetMode(Mode::Create); }));
    engine::OnClick(rename_, GuardedHandler("rename", [this] { SetMode(Mode::Rename); }));
    engine::OnClick(remove_, GuardedHandler("delete", [this] { SetMode(Mode::Confirm); }));
    engine::OnClick(ok, GuardedHandler("name ok", [this] { OnConfirmName(); }));
    engine::OnClick(cancel, GuardedHandler("name cancel", [this] { SetMode(Mode::Normal); }));
    engine::OnClick(yes, GuardedHandler("delete yes", [this] {
        SetMode(Mode::Normal);
        window_.DeleteSelected();
    }));
    engine::OnClick(no, GuardedHandler("delete no", [this] { SetMode(Mode::Normal); }));

    SetMode(Mode::Normal);
    ShowMessage({});
}

void PackPanel::SetMode(Mode mode) {
    mode_ = mode;
    const auto& selected = window_.store().selected();
    if (mode == Mode::Create) {
        engine::SetFieldText(nameField_, {});
    } else if (mode == Mode::Rename) {
        if (!selected) return SetMode(Mode::Normal);
        engine::SetFieldText(nameField_, *selected);
    } else if (mode == Mode::Confirm) {
        if (!selected) return SetMode(Mode::Normal);
        engine::SetText(question_, Format(Str::ConfirmDelete, *selected));
    }
    engine::SetVisible(normal_, mode == Mode::Normal);
    engine::SetVisible(naming_, mode == Mode::Create || mode == Mode::Rename);
    engine::SetVisible(confirm_, mode == Mode::Confirm);
    if (mode != Mode::Normal) ShowMessage({});
}

void PackPanel::OnConfirmName() {
    std::string name = engine::FieldText(nameField_);
    NameError error = mode_ == Mode::Rename ? window_.Rename(name) : window_.Create(name);
    if (error == NameError::Empty) {
        ShowMessage(std::string(Text(Str::ErrorNameEmpty)));
        return;
    }
    if (error == NameError::Duplicate) {
        ShowMessage(std::string(Text(Str::ErrorNameDuplicate)));
        return;
    }
    ShowMessage({});
    SetMode(Mode::Normal);
}

void PackPanel::OnSelect(int index) {
    if (refreshing_) return;
    if (index <= 0 || static_cast<size_t>(index) > shownNames_.size()) {
        window_.Select(std::nullopt, false);
        return;
    }
    window_.Select(shownNames_[static_cast<size_t>(index) - 1], applyOnSelect_);
}

void PackPanel::Refresh() {
    refreshing_ = true;
    const PackStore& store = window_.store();
    std::vector<std::string> names;
    for (const Pack& p : store.packs()) names.push_back(p.name);
    bool empty = names.empty();

    // Items are rebuilt only when the names change, never from inside the
    // list's own selection handler (selection alone does not change names).
    if (names != shownNames_ || empty != shownEmpty_) {
        engine::DropDownClear(list_);
        engine::DropDownAdd(list_, std::string(Text(empty ? Str::PlaceholderNoPacks : Str::PlaceholderNotSelected)));
        for (const std::string& n : names) engine::DropDownAdd(list_, n);
        shownNames_ = names;
        shownEmpty_ = empty;
    }
    int index = 0;
    if (store.selected()) {
        for (size_t i = 0; i < names.size(); ++i) {
            if (names[i] == *store.selected()) index = static_cast<int>(i) + 1;
        }
    }
    if (engine::DropDownSelected(list_) != index) engine::DropDownSelect(list_, index);
    engine::SetEnabled(list_, !empty);
    engine::SetEnabled(rename_, store.selected().has_value());
    engine::SetEnabled(remove_, store.selected().has_value());
    refreshing_ = false;
}

void PackPanel::ShowMessage(const std::string& text) {
    engine::SetText(message_, text);
    engine::SetVisible(message_, !text.empty());
}

}  // namespace modpacks
