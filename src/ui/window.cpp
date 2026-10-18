#include "ui/window.h"

#include <algorithm>
#include <map>

#include "common/utf.h"
#include "core/log.h"
#include "i18n/strings.h"
#include "packs/pack_file.h"
#include "ui/editor_tab.h"
#include "ui/pack_panel.h"

namespace modpacks {

Window::Window(engine::ModsGui* gui)
    : gui_(gui), pane_(engine::ModsGuiManagePane(gui)), path_(DefaultPacksPath()) {}

Window::~Window() = default;

void Window::Build() {
    LoadResult loaded = LoadPacks(path_, store_);
    Log("mod packs: ", loaded.status == LoadStatus::Ok        ? "loaded "
                       : loaded.status == LoadStatus::Missing ? "no file "
                                                              : "corrupt ",
        path_.wstring(), " (", static_cast<unsigned long long>(store_.packs().size()), " packs)",
        loaded.error.empty() ? "" : ": ", loaded.error);

    // Pack bar on the "Installed" tab, left of the search box.
    managePanel_ = std::make_unique<PackPanel>(*this, true);
    engine::Widget* searchBar = engine::ManagePaneSearchBar(pane_);
    engine::Widget* subheader = engine::Parent(searchBar);
    if (subheader) {
        engine::Insert(subheader, managePanel_->root(), 0);
    } else {
        Log("search bar has no parent; pack bar not inserted");
    }

    // "Mod packs" tab, right of "Updates".
    editorPanel_ = std::make_unique<PackPanel>(*this, false);
    editor_ = std::make_unique<EditorTab>(*this, *editorPanel_);
    engine::AddTab(engine::ModsGuiTabs(gui_), editor_->tab(), editor_->content());

    // The selection follows the enabled mods; the saved one only breaks ties (design D3).
    ResetBaseline();
    std::optional<std::string> saved = store_.selected();
    std::optional<std::string> match =
        MatchPack(store_.packs(), baseline_, saved ? std::optional<std::string_view>(*saved) : std::nullopt);
    if (match != saved) {
        store_.Select(match ? std::optional<std::string_view>(*match) : std::nullopt);
        Log("pack on open: ", match ? *match : std::string("(none)"), ", saved ",
            saved ? *saved : std::string("(none)"));
        Save();
    }

    RefreshViews();
    if (loaded.status == LoadStatus::Corrupt) {
        ShowMessage(Format(Str::ErrorLoad, WideToUtf8(loaded.backup.filename().wstring())));
    }
}

std::vector<InstalledMod> Window::InstalledMods() const {
    std::vector<InstalledMod> mods;
    for (engine::ModRow* row : engine::ModRows(pane_)) {
        const std::string& name = engine::RowModName(row);
        if (name == kBaseMod) continue;
        mods.push_back({name, engine::RowTitle(row)});
    }
    std::sort(mods.begin(), mods.end(),
              [](const InstalledMod& a, const InstalledMod& b) { return NameLess(a.title, b.title); });
    return mods;
}

void Window::Select(std::optional<std::string> name, bool apply) {
    CommitDraft("pack switch");
    store_.Select(name ? std::optional<std::string_view>(*name) : std::nullopt);
    ShowMessage({});
    Save();
    if (apply) ApplySelected();
    RefreshViews();
}

NameError Window::Create(const std::string& name) {
    std::vector<std::string> enabled;
    for (engine::ModRow* row : engine::ModRows(pane_)) {
        if (engine::RowChecked(row)) enabled.push_back(engine::RowModName(row));
    }
    NameError error = store_.Create(name, std::move(enabled));
    if (error != NameError::None) return error;
    Log("pack created: ", name);
    ResetBaseline();  // the new pack is the checkboxes; the old pack keeps its draft unwritten
    Save();
    RefreshViews();
    return error;
}

NameError Window::Rename(const std::string& name) {
    if (!store_.selected()) return NameError::None;
    std::string from = *store_.selected();
    NameError error = store_.Rename(from, name);
    if (error != NameError::None) return error;
    Log("pack renamed: ", from, " -> ", name);
    Save();
    RefreshViews();
    return error;
}

void Window::DeleteSelected() {
    if (!store_.selected()) return;
    std::string name = *store_.selected();
    store_.Delete(name);
    Log("pack deleted: ", name);
    ResetBaseline();  // drops the draft of the deleted pack
    Save();
    RefreshViews();
}

void Window::AddMod(const std::string& mod) {
    if (!store_.selected() || !store_.AddMod(*store_.selected(), mod)) return;
    Save();
    RefreshViews();
}

void Window::RemoveMod(const std::string& mod) {
    if (!store_.selected() || !store_.RemoveMod(*store_.selected(), mod)) return;
    Save();
    RefreshViews();
}

void Window::ShowMessage(const std::string& text) {
    if (managePanel_) managePanel_->ShowMessage(text);
    if (editorPanel_) editorPanel_->ShowMessage(text);
}

bool Window::EditorTabSelected() const {
    return editor_ && engine::IsTabSelected(engine::ModsGuiTabs(gui_), editor_->tab());
}

void Window::OnTabsChanged() {
    CommitDraft("tab switch");
    if (editor_) editor_->OnTabsChanged();
}

void Window::OnConfirmed() { CommitDraft("confirm"); }

void Window::OnRowRestyled(engine::ModRow* row) {
    if (applying_) return;
    const std::string& name = engine::RowModName(row);
    if (name == kBaseMod) return;
    bool checked = engine::RowChecked(row);
    auto it = seen_.find(name);
    if (it != seen_.end() && it->second == checked) return;  // refreshView, sorting, search
    seen_[name] = checked;
    Log("checkbox: ", name, checked ? " on" : " off");
    UpdateDraftMark();
}

bool Window::HasDraft() const {
    const Pack* pack = store_.selected() ? store_.Find(*store_.selected()) : nullptr;
    return pack && DeltaChangesPack(*pack, DraftDelta(baseline_, CurrentStates()));
}

void Window::Save() {
    std::string error;
    if (!SavePacks(path_, store_, &error)) {
        Log("saving mod packs failed: ", error);
        ShowMessage(Format(Str::ErrorSave, error));
    }
}

ModStates Window::CurrentStates() const {
    ModStates states;
    for (engine::ModRow* row : engine::ModRows(pane_)) {
        const std::string& name = engine::RowModName(row);
        if (name != kBaseMod) states[name] = engine::RowChecked(row);
    }
    return states;
}

void Window::CommitDraft(const char* reason) {
    ModStates current = CurrentStates();
    if (store_.selected()) {
        ModDelta delta = DraftDelta(baseline_, current);
        std::string pack = *store_.selected();
        if (store_.ApplyDelta(pack, delta)) {
            std::string changes;
            for (const std::string& m : delta.added) changes += " +" + m;
            for (const std::string& m : delta.removed) changes += " -" + m;
            Log("draft written to ", pack, " on ", reason, ":", changes);
            Save();
            RefreshViews();
        }
    }
    baseline_ = std::move(current);
    UpdateDraftMark();
}

void Window::ResetBaseline() {
    baseline_ = CurrentStates();
    seen_ = baseline_;
    UpdateDraftMark();
}

void Window::UpdateDraftMark() {
    bool draft = HasDraft();
    if (draft == shownDraft_) return;
    shownDraft_ = draft;
    if (managePanel_) managePanel_->ShowDraftMark(draft);
    if (editorPanel_) editorPanel_->ShowDraftMark(draft);
}

void Window::ApplySelected() {
    const Pack* pack = store_.selected() ? store_.Find(*store_.selected()) : nullptr;
    if (!pack) return;
    std::vector<engine::ModRow*> rows = engine::ModRows(pane_);
    std::vector<std::string> installed;
    for (engine::ModRow* row : rows) installed.push_back(engine::RowModName(row));
    ApplyPlan plan = PlanApply(*pack, installed);

    std::map<std::string, bool, std::less<>> enabled(plan.enabled.begin(), plan.enabled.end());
    applying_ = true;  // our own checkbox changes are not the user's
    for (engine::ModRow* row : rows) {
        auto it = enabled.find(engine::RowModName(row));
        if (it != enabled.end()) engine::RowSetChecked(row, it->second);  // `base` is not in the plan
    }
    engine::RefreshRows(pane_);
    applying_ = false;
    ResetBaseline();
    Log("pack applied: ", pack->name, ", missing ", static_cast<unsigned long long>(plan.missing.size()));

    std::string missing;
    for (const std::string& m : plan.missing) missing += (missing.empty() ? "" : ", ") + m;
    ShowMessage(missing.empty() ? std::string() : Format(Str::MissingMods, missing));
}

void Window::RefreshViews() {
    if (managePanel_) managePanel_->Refresh();
    if (editorPanel_) editorPanel_->Refresh();
    if (editor_) editor_->Refresh();
}

}  // namespace modpacks
