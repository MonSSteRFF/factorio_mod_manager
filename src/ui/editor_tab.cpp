#include "ui/editor_tab.h"

#include <windows.h>

#include <algorithm>
#include <set>

#include "common/utf.h"
#include "core/guard.h"
#include "core/log.h"
#include "i18n/strings.h"
#include "ui/pack_panel.h"
#include "ui/window.h"

namespace modpacks {

namespace {

std::wstring Lower(const std::string& s) {
    std::wstring w = Utf8ToWide(s);
    if (w.empty()) return w;
    LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, w.data(), static_cast<int>(w.size()), w.data(),
                  static_cast<int>(w.size()), nullptr, nullptr, 0);
    return w;
}

// A cell we keep across table rebuilds: created like the engine's own cells,
// then released so clearing the table does not delete it.
engine::Widget* Cell(engine::Widget* w) { return engine::Disown(w); }

}  // namespace

EditorTab::EditorTab(Window& window, PackPanel& panel) : window_(window) {
    tab_ = engine::NewTab(std::string(Text(Str::TabTitle)));
    content_ = engine::NewFrameWithSubheader("deep_frame_in_shallow_frame");
    engine::Stretch(content_);

    // Subheader like the "Installed" tab: [pack bar] [pusher] [search]
    engine::Widget* subheader = engine::Subheader(content_);
    engine::Add(subheader, panel.root());
    engine::Add(subheader, engine::NewPusher());
    engine::Widget* search = engine::NewSearchBar(
        subheader, GuardedHandler<const std::string&>("pack search", [this](const std::string& text) {
            filter_ = Lower(text);
            Refresh();
        }));
    engine::Add(subheader, search);

    engine::Widget* body = engine::NewHorizontalFlow("horizontal_flow");
    engine::Stretch(body);
    BuildSide(body, left_);
    BuildSide(body, right_);
    hint_ = engine::NewLabel(std::string(Text(Str::HintNoPack)), "label");
    engine::Add(right_.column, hint_);
    engine::Add(content_, body);

    for (engine::ModRow* row : engine::ModRows(window_.pane())) {
        const std::string& name = engine::RowModName(row);
        if (name != kBaseMod) EnsureRow(name, engine::RowTitle(row), true, row);
    }
    Refresh();
}

EditorTab::~EditorTab() {
    // The tables outlive us (they belong to the window); take our cells out
    // and delete them.
    engine::TableClearRows(left_.table);
    engine::TableClearRows(right_.table);
    for (auto& [name, row] : rows_) {
        for (engine::Widget* w : {row.leftName, row.leftVersion, row.addButton, row.rightName, row.rightVersion,
                                  row.removeButton}) {
            if (w) engine::DeleteWidget(w);
        }
    }
}

void EditorTab::BuildSide(engine::Widget* body, Side& side) {
    engine::Widget* column = side.column = engine::NewVerticalFlow("vertical_flow");
    side.header = engine::NewLabel("", "bold_label");
    engine::Widget* scroll = engine::NewVerticalScrollPane("mods_scroll_pane");
    side.table = engine::NewTable("table_with_selection");
    // The stretched name header keeps the table full width even when it has no rows.
    engine::Widget* nameHeader = engine::NewLabel(std::string(Text(Str::ColumnName)), "label");
    engine::StretchHorizontally(nameHeader);
    engine::TableAddHeader(side.table, nameHeader, true);
    engine::TableAddHeader(side.table, engine::NewLabel(std::string(Text(Str::ColumnVersion)), "label"), false);
    engine::TableAddHeader(side.table, engine::NewLabel("", "label"), false);
    // Like the "Installed" tab: the column count after the headers, then alignment.
    // Name on the left (and stretched, see EnsureRow), version and button on the right.
    engine::TableSetColumnCount(side.table, 3);
    engine::TableAlignColumn(side.table, 0, false);
    engine::TableAlignColumn(side.table, 1, true);
    engine::TableAlignColumn(side.table, 2, true);
    engine::TableSetOrdering(side.table, 0, true);
    engine::StretchHorizontally(side.table);
    engine::SquashTable(side.table);
    engine::Add(scroll, side.table);
    engine::Stretch(scroll);
    engine::SquashScrollPane(scroll);
    engine::Add(column, side.header);
    engine::Add(column, scroll);
    engine::Stretch(column);
    engine::EqualWidth(column);
    engine::Add(body, column);

    engine::OnTableOrdering(side.table, GuardedHandler<unsigned, bool>("pack table sort", [this, &side](unsigned, bool ascending) {
        side.ascending = ascending;
        Refresh();
    }));
    engine::OnTableSelection(side.table, GuardedHandler<int>("pack table select", [this, &side](int row) {
        Log("pack table select: row ", row, " of ", static_cast<unsigned long long>(side.shown.size()));
        if (row >= 0 && static_cast<size_t>(row) < side.shown.size()) Select(side.shown[static_cast<size_t>(row)]);
    }));
}

EditorTab::Row& EditorTab::EnsureRow(const std::string& name, const std::string& title, bool installed,
                                     engine::ModRow* modRow) {
    auto it = rows_.find(name);
    if (it != rows_.end()) return it->second;

    Row& row = rows_[name];
    row.name = name;
    row.installed = installed;
    row.modRow = modRow;
    row.title = title.empty() ? name : title;
    if (!installed) row.title += " (" + std::string(Text(Str::NotInstalled)) + ")";
    row.sortKey = Lower(row.title);
    row.search = row.sortKey + L"\n" + Lower(name);
    std::string version = modRow ? engine::RowVersion(modRow) : std::string("-");
    const char* nameStyle = installed ? "label" : "bold_red_label";

    row.leftName = Cell(engine::NewLabel(row.title, nameStyle));
    engine::StretchHorizontally(row.leftName);
    row.leftVersion = Cell(engine::NewLabel(version, "label"));
    row.addButton = Cell(engine::NewTextButton(">", "tool_button"));  // square 28x28
    engine::SetToolTip(row.addButton, std::string(Text(Str::TipAddToPack)));
    engine::OnClick(row.addButton, GuardedHandler("add to pack", [this, name] { window_.AddMod(name); }));

    row.rightName = Cell(engine::NewLabel(row.title, nameStyle));
    engine::StretchHorizontally(row.rightName);
    row.rightVersion = Cell(engine::NewLabel(version, "label"));
    row.removeButton = Cell(engine::NewTextButton("<", "tool_button"));
    engine::SetToolTip(row.removeButton, std::string(Text(Str::TipRemoveFromPack)));
    engine::OnClick(row.removeButton, GuardedHandler("remove from pack", [this, name] { window_.RemoveMod(name); }));
    return row;
}

bool EditorTab::Matches(const Row& row) const {
    return filter_.empty() || row.search.find(filter_) != std::wstring::npos;
}

void EditorTab::Fill(Side& side, const std::vector<Row*>& rows, bool left) {
    std::vector<Row*> sorted = rows;
    std::sort(sorted.begin(), sorted.end(), [&](const Row* a, const Row* b) {
        return side.ascending ? a->sortKey < b->sortKey : b->sortKey < a->sortKey;
    });
    engine::TableClearRows(side.table);
    side.shown.clear();
    for (Row* row : sorted) {
        if (!Matches(*row)) continue;
        engine::Add(side.table, left ? row->leftName : row->rightName);
        engine::Add(side.table, left ? row->leftVersion : row->rightVersion);
        engine::Add(side.table, left ? row->addButton : row->removeButton);
        side.shown.push_back(row);
    }
}

void EditorTab::Refresh() {
    const PackStore& store = window_.store();
    const Pack* pack = store.selected() ? store.Find(*store.selected()) : nullptr;
    std::set<std::string, std::less<>> inPack;
    if (pack) {
        inPack.insert(pack->mods.begin(), pack->mods.end());
        for (const std::string& mod : pack->mods) {
            if (!rows_.contains(mod)) EnsureRow(mod, mod, false, nullptr);
        }
    }

    std::vector<Row*> leftRows;
    std::vector<Row*> rightRows;
    for (auto& [name, row] : rows_) {
        if (inPack.contains(name)) {
            rightRows.push_back(&row);
        } else if (row.installed) {
            leftRows.push_back(&row);
        }
        engine::SetEnabled(row.addButton, pack != nullptr);  // nothing to add to without a pack
    }
    Fill(left_, leftRows, true);
    Fill(right_, rightRows, false);
    engine::SetText(left_.header, Format(Str::ColumnAll, std::to_string(leftRows.size())));
    engine::SetText(right_.header, Format(Str::ColumnPack, std::to_string(rightRows.size())));
    engine::SetVisible(hint_, pack == nullptr);
}

void EditorTab::Select(const Row* row) {
    selected_ = row;
    Log("mod info: ", row ? row->name : std::string("(none)"), row && row->modRow ? "" : " (not installed)",
        engine::InfoPaneAvailable() ? "" : " - info pane unavailable");
    if (engine::InfoPaneAvailable()) engine::ShowModInfo(window_.pane(), row ? row->modRow : nullptr);
}

void EditorTab::OnTabsChanged() {
    if (!engine::InfoPaneAvailable()) return;
    if (window_.EditorTabSelected()) {
        engine::ShowModInfo(window_.pane(), selected_ ? selected_->modRow : nullptr);
    } else {
        engine::RestoreManageInfo(window_.pane());
    }
}

}  // namespace modpacks
