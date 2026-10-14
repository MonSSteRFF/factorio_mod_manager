#include "core/engine.h"

#include <map>
#include <stdexcept>

#include "core/code_scan.h"
#include "core/log.h"

namespace modpacks::engine {

namespace {

// Same layout as the game's NamedBool<Tag> (checked: sizeof 1, one bool).
struct NamedBoolArg {
    bool value;
};

// Same layout as agui::TableIndex (checked: sizeof 8).
struct TableIndex {
    bool hasValue;
    unsigned value;
};

// Same layout as ModVersion / Version (checked: sizeof 6).
struct ModVersionArg {
    uint16_t major, minor, sub;
};

const Resolved* g_r = nullptr;
uintptr_t g_base = 0;
uint32_t g_ownedFlag = 0;

uint64_t R(SymId id) { return (*g_r)[id]; }

template <typename Fn>
Fn F(SymId id) {
    return reinterpret_cast<Fn>(g_base + R(id));
}

// Virtual call through the object's own vftable.
template <typename Fn>
Fn V(const void* object, SymId slot) {
    auto* vftable = *reinterpret_cast<const uintptr_t* const*>(object);
    return reinterpret_cast<Fn>(vftable[R(slot) / sizeof(uintptr_t)]);
}

template <typename T>
T& At(const void* object, SymId field) {
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(object) + R(field));
}

void* Allocate(SymId size) {
    using New = void* (*)(size_t);
    return F<New>(SymId::operatorNew)(static_cast<size_t>(R(size)));
}

Widget* Own(Widget* w) {
    At<uint32_t>(w, SymId::Widget_usageBitMask) |= g_ownedFlag;
    return w;
}

void* Target(Widget* w) { return reinterpret_cast<char*>(w) + R(SymId::Widget_GenericTargetable); }

const void* StyleOrDefault(const char* name) {
    const void* s = Style(name);
    if (!s) Log("style not found: ", name);
    return s;
}

}  // namespace

bool Init(const Resolved& resolved, uintptr_t imageBase, std::string* error) {
    g_r = &resolved;
    g_base = imageBase;
    const auto* code = reinterpret_cast<const uint8_t*>(g_base + R(SymId::Widget_clearParentWidget));
    auto mask = FindTestedMask(code, 96, static_cast<uint32_t>(R(SymId::Widget_usageBitMask)));
    if (!mask || *mask == 0) {
        if (error) *error = "widget ownership bit not found in agui::Widget::clearParentWidget";
        return false;
    }
    g_ownedFlag = *mask;
    Log("widget ownership bit: ", Hex(g_ownedFlag));
    return true;
}

const void* Style(const char* name) {
    using StyleMap = std::map<std::string, void*, std::less<>>;
    auto* global = *reinterpret_cast<void**>(g_base + R(SymId::global));
    if (!global) return nullptr;
    void* guiStyle = At<void*>(global, SymId::GlobalContext_style);  // UniquePointer<GuiStyle>
    if (!guiStyle) return nullptr;
    const auto& styles = At<StyleMap>(guiStyle, SymId::GuiStyle_styles);
    auto it = styles.find(std::string_view(name));
    if (it == styles.end() || !it->second) return nullptr;
    using GetAguiStyle = const void* (*)(void*);
    return V<GetAguiStyle>(it->second, SymId::IntegratedStyle_vGetAguiStyle)(it->second);
}

// --- creating widgets -----------------------------------------------------------------

Widget* NewTextButton(const std::string& text, const char* style) {
    using Ctor = Widget* (*)(void*, std::string*, const void*);
    std::string moved = text;  // the constructor takes std::string&&
    void* p = Allocate(SymId::size_TextButton);
    return Own(F<Ctor>(SymId::TextButton_ctor)(p, &moved, StyleOrDefault(style)));
}

Widget* NewDropDown(const char* style) {
    using Ctor = Widget* (*)(void*, const void*);
    void* p = Allocate(SymId::size_DropDown);
    return Own(F<Ctor>(SymId::DropDown_ctor)(p, StyleOrDefault(style)));
}

Widget* NewTextField(const char* style) {
    using Ctor = Widget* (*)(void*, const void*);
    void* p = Allocate(SymId::size_TextField);
    return Own(F<Ctor>(SymId::TextField_ctor)(p, StyleOrDefault(style)));
}

Widget* NewLabel(const std::string& text, const char* style) {
    using Ctor = Widget* (*)(void*, const std::string&, const void*);
    void* p = Allocate(SymId::size_Label);
    return Own(F<Ctor>(SymId::Label_ctor)(p, text, StyleOrDefault(style)));
}

Widget* NewHorizontalFlow(const char* style) {
    using Ctor = Widget* (*)(void*, const void*);
    void* p = Allocate(SymId::size_HorizontalFlow);
    return Own(F<Ctor>(SymId::HorizontalFlow_ctor)(p, StyleOrDefault(style)));
}

Widget* NewVerticalFlow(const char* style) {
    using Ctor = Widget* (*)(void*, const void*);
    void* p = Allocate(SymId::size_VerticalFlow);
    return Own(F<Ctor>(SymId::VerticalFlow_ctor)(p, StyleOrDefault(style)));
}

Widget* NewPusher() {
    using Ctor = Widget* (*)(void*, const void*);
    void* p = Allocate(SymId::size_EmptyWidget);
    Widget* w = Own(F<Ctor>(SymId::EmptyWidget_ctor)(p, StyleOrDefault("empty_widget")));
    StretchHorizontally(w);
    return w;
}

Widget* NewVerticalScrollPane(const char* style) {
    using Ctor = Widget* (*)(void*, const void*);
    void* p = Allocate(SymId::size_VerticalScrollPane);
    return Own(F<Ctor>(SymId::VerticalScrollPane_ctor)(p, StyleOrDefault(style)));
}

Widget* NewTab(const std::string& caption) {
    using Ctor = Widget* (*)(void*, const std::string&, const void*);
    void* p = Allocate(SymId::size_Tab);
    return Own(F<Ctor>(SymId::Tab_ctor)(p, caption, nullptr));
}

Widget* NewFrameWithSubheader(const char* style) {
    // Same arguments as ManageModsPane: (style, false, true).
    using Ctor = Widget* (*)(void*, const void*, bool, bool);
    void* p = Allocate(SymId::size_FrameWithSubheader);
    return Own(F<Ctor>(SymId::FrameWithSubheader_ctor)(p, StyleOrDefault(style), false, true));
}

Widget* NewSearchBar(Widget* frame, std::function<void(const std::string&)> onChange) {
    using Ctor = Widget* (*)(void*, Widget*, std::function<void(const std::string&)>, NamedBoolArg);
    void* p = Allocate(SymId::size_SearchBar);
    return Own(F<Ctor>(SymId::SearchBar_ctor)(p, frame, std::move(onChange), NamedBoolArg{true}));
}

Widget* Subheader(Widget* frameWithSubheader) {
    return reinterpret_cast<char*>(frameWithSubheader) + R(SymId::FrameWithSubheader_subheader);
}

// --- widget operations ----------------------------------------------------------------

void Add(Widget* parent, Widget* child) {
    using Fn = void (*)(Widget*, Widget*);
    V<Fn>(parent, SymId::Widget_vAdd)(parent, child);
}

void Insert(Widget* parent, Widget* child, unsigned index) {
    using Fn = void (*)(Widget*, Widget*, unsigned);
    V<Fn>(parent, SymId::Widget_vInsert)(parent, child, index);
}

void Clear(Widget* parent) {
    using Fn = void (*)(Widget*);
    V<Fn>(parent, SymId::Widget_vClear)(parent);
}

Widget* Parent(Widget* widget) { return At<Widget*>(widget, SymId::Widget_parentWidget); }

void SetVisible(Widget* widget, bool visible) {
    using Fn = void (*)(Widget*, bool);
    F<Fn>(SymId::Widget_setVisible)(widget, visible);
}

void SetEnabled(Widget* widget, bool enabled) {
    using Fn = Widget* (*)(Widget*, bool);
    V<Fn>(widget, SymId::Widget_vSetEnabled)(widget, enabled);
}

void SetText(Widget* widget, const std::string& text) {
    using Fn = void (*)(Widget*, const std::string&);
    V<Fn>(widget, SymId::Widget_vSetText)(widget, text);
}

void SetToolTip(Widget* widget, const std::string& text) {
    using Fn = Widget* (*)(Widget*, const std::string&);
    F<Fn>(SymId::Widget_setToolTip)(widget, text);
}

void StretchHorizontally(Widget* widget) {
    using Fn = Widget* (*)(Widget*);
    F<Fn>(SymId::Widget_stretchHorizontally)(widget);
}

void Stretch(Widget* widget) {
    using Fn = Widget* (*)(Widget*);
    F<Fn>(SymId::Widget_stretch)(widget);
}

void OnClick(Widget* widget, std::function<void()> handler) {
    using Fn = Widget* (*)(Widget*, void*, std::function<void()>);
    F<Fn>(SymId::Widget_onClick)(widget, Target(widget), std::move(handler));
}

void DropDownClear(Widget* dropDown) {
    using Fn = void (*)(Widget*);
    F<Fn>(SymId::DropDown_clearItems)(dropDown);
}

void DropDownAdd(Widget* dropDown, const std::string& item) {
    using Fn = void (*)(Widget*, const std::string&);
    F<Fn>(SymId::DropDown_addItem)(dropDown, item);
}

void DropDownSelect(Widget* dropDown, int index) {
    using Fn = void (*)(Widget*, int);
    F<Fn>(SymId::DropDown_setSelectedIndex)(dropDown, index);
}

int DropDownSelected(Widget* dropDown) {
    using Fn = int (*)(Widget*);
    return F<Fn>(SymId::DropDown_getSelectedIndex)(dropDown);
}

void OnItemSelect(Widget* dropDown, std::function<void(int)> handler) {
    using Fn = void (*)(Widget*, void*, std::function<void(int)>);
    F<Fn>(SymId::DropDown_onItemSelect)(dropDown, Target(dropDown), std::move(handler));
}

std::string FieldText(Widget* textField) {
    using Fn = const std::string& (*)(Widget*);
    return F<Fn>(SymId::TextBox_getText)(textField);
}

void SetFieldText(Widget* textField, const std::string& text) {
    using Fn = void (*)(Widget*, const std::string&);
    F<Fn>(SymId::TextBox_setText)(textField, text);
}

void AddTab(Widget* tabbedPane, Widget* tab, Widget* content) {
    using Fn = void (*)(Widget*, Widget*, Widget*, Widget*);
    F<Fn>(SymId::TabbedPane_addTab)(tabbedPane, tab, content, nullptr);
}

// --- tables ----------------------------------------------------------------------------

Widget* NewTable(const char* style) {
    using Ctor = Widget* (*)(void*, const void*);
    void* p = Allocate(SymId::size_TableWithSelection);
    return Own(F<Ctor>(SymId::TableWithSelection_ctor)(p, StyleOrDefault(style)));
}

void TableAddHeader(Widget* table, Widget* header, bool sortable) {
    // Same flags as the "Name" column of the "Installed" tab: (sortable, false, true).
    using Fn = void (*)(Widget*, Widget*, bool, bool, bool);
    F<Fn>(SymId::TableWithSelection_addHeader)(table, header, sortable, false, true);
}

void TableClearRows(Widget* table) {
    using Fn = void (*)(Widget*);
    F<Fn>(SymId::TableWithSelection_clearApartHeader)(table);
}

void TableSetColumnCount(Widget* table, int columns) {
    using Fn = void (*)(Widget*, int);
    V<Fn>(table, SymId::Table_vSetColumnCount)(table, columns);
}

void TableAlignColumn(Widget* table, int column, bool right) {
    using Fn = Widget* (*)(Widget*, int, uint8_t);
    auto align = static_cast<uint8_t>(R(right ? SymId::AreaAlign_RightMiddle : SymId::AreaAlign_LeftMiddle));
    F<Fn>(SymId::Table_alignColumn)(table, column, align);
}

void TableSetOrdering(Widget* table, unsigned column, bool ascending) {
    using Fn = void (*)(Widget*, unsigned, bool);
    F<Fn>(SymId::TableWithSelection_setColumnOrdering)(table, column, ascending);
}

void OnTableOrdering(Widget* table, std::function<void(unsigned, bool)> handler) {
    using Fn = void (*)(Widget*, void*, std::function<void(TableIndex, bool)>);
    F<Fn>(SymId::TableWithSelection_onOrderingChange)(
        table, Target(table), [handler = std::move(handler)](TableIndex column, bool ascending) {
            handler(column.hasValue ? column.value : 0u, ascending);
        });
}

void OnTableSelection(Widget* table, std::function<void(int)> handler) {
    using Fn = void (*)(Widget*, void*, std::function<void(TableIndex, TableIndex, int)>);
    F<Fn>(SymId::TableWithSelection_onSelectionChange)(
        table, Target(table), [handler = std::move(handler)](TableIndex selected, TableIndex /*previous*/, int) {
            // (new, previous, ?); row indices count the header row as 0.
            handler(selected.hasValue && selected.value > 0 ? static_cast<int>(selected.value) - 1 : -1);
        });
}

Widget* Disown(Widget* widget) {
    At<uint32_t>(widget, SymId::Widget_usageBitMask) &= ~g_ownedFlag;
    return widget;
}

void DeleteWidget(Widget* widget) {
    using Fn = void* (*)(Widget*, unsigned);  // scalar deleting destructor
    V<Fn>(widget, SymId::Widget_vDelete)(widget, 1);
}

namespace {
void Stretchy(Widget* widget, void* style, bool naturalZero) {
    using SetRule = void (*)(void*, uint8_t);
    using SetNatural = void (*)(void*, int, int);
    using Apply = void (*)(Widget*, const void*);
    auto on = static_cast<uint8_t>(R(SymId::StretchRule_On));
    F<SetRule>(SymId::Style_setHorizontallyStretchable)(style, on);
    F<SetRule>(SymId::Style_setHorizontallySquashable)(style, on);
    if (naturalZero) {
        int height = At<int>(style, SymId::Style_naturalHeight);
        V<SetNatural>(style, SymId::Style_vSetNaturalSize)(style, 0, height);
    }
    F<Apply>(SymId::Widget_applySizeRestrictions)(widget, style);
}
}  // namespace

void EqualWidth(Widget* verticalFlow) {
    Stretchy(verticalFlow, reinterpret_cast<char*>(verticalFlow) + R(SymId::VerticalFlow_style), true);
}

void SquashScrollPane(Widget* scrollPane) {
    Stretchy(scrollPane, reinterpret_cast<char*>(scrollPane) + R(SymId::ScrollPane_style), false);
}

void SquashTable(Widget* table) {
    Stretchy(table, reinterpret_cast<char*>(table) + R(SymId::Table_style), false);
}

// --- mods window ------------------------------------------------------------------------

Widget* ModsGuiTabs(ModsGui* gui) { return reinterpret_cast<char*>(gui) + R(SymId::ModsGui_tabs); }

ManagePane* ModsGuiManagePane(ModsGui* gui) {
    return reinterpret_cast<char*>(gui) + R(SymId::ModsGui_manageTabContents);
}

Widget* ManagePaneSearchBar(ManagePane* pane) {
    return reinterpret_cast<char*>(pane) + R(SymId::ManageModsPane_searchBar);
}

std::vector<ModRow*> ModRows(ManagePane* pane) {
    // std::vector<std::unique_ptr<ModWidgets>>: unique_ptr with the default
    // deleter is a plain pointer, so the elements read as ModRow*.
    const auto& rows = At<std::vector<ModRow*>>(pane, SymId::ManageModsPane_modWidgets);
    return rows;
}

namespace {
void* RowState(ModRow* row) { return At<void*>(row, SymId::ModWidgets_stateData); }  // ModAndName&
void* RowCheckBox(ModRow* row) { return reinterpret_cast<char*>(row) + R(SymId::ModWidgets_enabled); }
}  // namespace

const std::string& RowModName(ModRow* row) {
    void* mod = At<void*>(RowState(row), SymId::ModAndName_mod);
    return At<std::string>(mod, SymId::Mod_name);
}

const std::string& RowTitle(ModRow* row) { return At<std::string>(RowState(row), SymId::ModAndName_localisedName); }

std::string RowVersion(ModRow* row) {
    const char* version = reinterpret_cast<const char*>(RowState(row)) + R(SymId::ModAndName_activeVersion);
    auto part = [&](SymId field) { return std::to_string(At<uint16_t>(version, field)); };
    return part(SymId::Version_major) + "." + part(SymId::Version_minor) + "." + part(SymId::Version_sub);
}

bool RowChecked(ModRow* row) {
    return At<int32_t>(RowCheckBox(row), SymId::ToggleButton_checkedState) ==
           static_cast<int32_t>(R(SymId::CheckedState_CHECKED));
}

void RowSetChecked(ModRow* row, bool checked) {
    At<int32_t>(RowCheckBox(row), SymId::ToggleButton_checkedState) =
        static_cast<int32_t>(R(checked ? SymId::CheckedState_CHECKED : SymId::CheckedState_UNCHECKED));
    if (checked) At<bool>(RowState(row), SymId::ModAndName_toBeDeleted) = false;
}

void RefreshRows(ManagePane* pane) {
    using UpdateStyle = void (*)(ManagePane*, ModRow*);
    using Simple = void (*)(ManagePane*);
    for (ModRow* row : ModRows(pane)) {
        // std::optional<ModDependency::Info> computed: mark empty, as toggleMasterEnabled does.
        char* computed = reinterpret_cast<char*>(row) + R(SymId::ModWidgets_computed);
        computed[R(SymId::Optional_DependencyInfo_hasValue)] = 0;
    }
    // Like ManageModsPane::toggleMasterEnabled: restyle the rows and update the
    // "enable all" box. No refreshView: rebuilding the table drops its selection.
    for (ModRow* row : ModRows(pane)) F<UpdateStyle>(SymId::ManageModsPane_updateStyleForModWidgets)(pane, row);
    F<Simple>(SymId::ManageModsPane_updateMasterEnabledStatus)(pane);
}

// --- mod info pane ----------------------------------------------------------------------

bool InfoPaneAvailable() {
    for (SymId id : {SymId::TabbedPane_selectedTab, SymId::TabbedPane_tabs, SymId::TabEntry_tab,
                     SymId::GenericTargeter_target, SymId::ManageModsPane_modInfoPane, SymId::ModInfoPane_showMod,
                     SymId::ModInfoPane_clearInfo, SymId::ManageModsPane_refreshSelectedModInfo,
                     SymId::ModsGui_updateTabContentsVisibility}) {
        if (!g_r->has(id)) return false;
    }
    return !g_r->has(SymId::abi_ModVersion) || R(SymId::abi_ModVersion) == sizeof(ModVersionArg);
}

bool IsTabSelected(Widget* tabbedPane, Widget* tab) {
    // selectedTab is a vector iterator into tabs; TabEntry::tab targets the
    // tab's GenericTargetable base.
    char* entry = At<char*>(tabbedPane, SymId::TabbedPane_selectedTab);
    const auto& tabs = At<std::vector<char>>(tabbedPane, SymId::TabbedPane_tabs);  // only begin/end are read
    char* end = const_cast<char*>(tabs.data()) + tabs.size();
    if (!entry || entry == end) return false;
    char* targeter = entry + R(SymId::TabEntry_tab);
    void* target = At<void*>(targeter, SymId::GenericTargeter_target);
    return target == Target(tab);
}

namespace {
void* InfoPane(ManagePane* pane) { return reinterpret_cast<char*>(pane) + R(SymId::ManageModsPane_modInfoPane); }
}  // namespace

void ShowModInfo(ManagePane* pane, ModRow* row) {
    void* info = InfoPane(pane);
    SetVisible(info, true);
    if (!row) {
        using Clear = void (*)(void*);
        F<Clear>(SymId::ModInfoPane_clearInfo)(info);
        return;
    }
    using Show = void (*)(void*, void*, ModVersionArg);
    const char* version = reinterpret_cast<const char*>(RowState(row)) + R(SymId::ModAndName_activeVersion);
    ModVersionArg v{At<uint16_t>(version, SymId::Version_major), At<uint16_t>(version, SymId::Version_minor),
                    At<uint16_t>(version, SymId::Version_sub)};
    F<Show>(SymId::ModInfoPane_showMod)(info, RowState(row), v);
}

void RestoreManageInfo(ManagePane* pane) {
    using Fn = void (*)(ManagePane*);
    F<Fn>(SymId::ManageModsPane_refreshSelectedModInfo)(pane);
}

// --- locale -----------------------------------------------------------------------------

std::string EngineLocale() {
    if (!g_r->has(SymId::GlobalContext_localeProvider) || !g_r->has(SymId::LocaleProvider_currentLocale) ||
        !g_r->has(SymId::Locale_language)) {
        return {};
    }
    auto* global = *reinterpret_cast<void**>(g_base + R(SymId::global));
    if (!global) return {};
    void* provider = At<void*>(global, SymId::GlobalContext_localeProvider);  // UniquePointer<LocaleProvider>
    if (!provider) return {};
    char* locale = reinterpret_cast<char*>(provider) + R(SymId::LocaleProvider_currentLocale);
    return At<std::string>(locale, SymId::Locale_language);
}

}  // namespace modpacks::engine
