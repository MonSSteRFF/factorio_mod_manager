// Engine symbols, fields and layout facts the mod depends on.
//
// Everything is looked up by name in factorio.pdb at runtime (src/core/symbols.cpp)
// and verified offline by scripts/check-symbols.py. Nothing here is an address.
// See docs/engine-notes.md for where each item comes from.
//
// X-macros, included with the macros defined by the consumer:
//   MP_FUNC(id, decorated_name, required)        address of a function
//   MP_DATA(id, decorated_name, required)        address of a global variable
//   MP_FIELD(id, "Class", "member", required)    offset of a data member
//   MP_SIZE(id, "Class", required)               sizeof(Class)
//   MP_BASE(id, "Class", "Base", required)       offset of Base inside Class
//   MP_VSLOT(id, vftable_decorated, impl_decorated, required)
//       byte offset of the slot in that vftable holding impl's address
//   MP_ENUM(id, "Enum::Type", "NAME", required)  value of an enumerator
//
// Optional items (required = false) only degrade a feature; a missing required
// item disables the whole mod (spec native-loader).

#ifndef MP_FUNC
#define MP_FUNC(id, name, required)
#endif
#ifndef MP_DATA
#define MP_DATA(id, name, required)
#endif
#ifndef MP_FIELD
#define MP_FIELD(id, cls, member, required)
#endif
#ifndef MP_SIZE
#define MP_SIZE(id, cls, required)
#endif
#ifndef MP_BASE
#define MP_BASE(id, cls, base, required)
#endif
#ifndef MP_VSLOT
#define MP_VSLOT(id, vftable, impl, required)
#endif
#ifndef MP_ENUM
#define MP_ENUM(id, type, name, required)
#endif

#define MP_STD_STRING "V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@"

// --- globals and allocation ----------------------------------------------------
MP_DATA(global, "?global@@3PEAVGlobalContext@@EA", true)
MP_FUNC(operatorNew, "??2@YAPEAX_K@Z", true)

// --- hooks -------------------------------------------------------------------------
MP_FUNC(ModsGui_ctor, "??0ModsGui@@QEAA@XZ", true)
MP_FUNC(ManageModsPane_dtor, "??1ManageModsPane@@UEAA@XZ", true)
MP_FUNC(ModsGui_updateTabContentsVisibility, "?updateTabContentsVisibility@ModsGui@@AEAAXXZ", false)

// --- ManageModsPane ---------------------------------------------------------------
MP_FUNC(ManageModsPane_updateStyleForModWidgets, "?updateStyleForModWidgets@ManageModsPane@@AEBAXAEAVModWidgets@1@@Z", true)
MP_FUNC(ManageModsPane_updateMasterEnabledStatus, "?updateMasterEnabledStatus@ManageModsPane@@AEAAXXZ", true)
MP_FUNC(ManageModsPane_refreshView, "?refreshView@ManageModsPane@@AEAAXXZ", true)

// --- agui::Widget ------------------------------------------------------------------
MP_FUNC(Widget_onClick, "?onClick@Widget@agui@@QEAAAEAV12@PEAVGenericTargetable@2@V?$function@$$A6AXXZ@std@@@Z", true)
MP_FUNC(Widget_setVisible, "?setVisible@Widget@agui@@QEAAX_N@Z", true)
MP_FUNC(Widget_setToolTip, "?setToolTip@Widget@agui@@QEAAAEAV12@AEB" MP_STD_STRING "@Z", true)
MP_FUNC(Widget_getIndexInParent, "?getIndexInParent@Widget@agui@@QEBAHXZ", true)
MP_FUNC(Widget_stretchHorizontally, "?stretchHorizontally@Widget@agui@@QEAAAEAV12@XZ", true)
MP_FUNC(Widget_stretch, "?stretch@Widget@agui@@QEAAAEAV12@XZ", true)
MP_FUNC(Widget_clearParentWidget, "?clearParentWidget@Widget@agui@@UEAAXV?$NamedBool@VKeepWidgetAliveTag@agui@@@@@Z", true)
MP_VSLOT(Widget_vAdd, "??_7Widget@agui@@6B@", "?add@Widget@agui@@UEAAXPEAV12@@Z", true)
MP_VSLOT(Widget_vInsert, "??_7Widget@agui@@6B@", "?insert@Widget@agui@@UEAAXPEAV12@I@Z", true)
MP_VSLOT(Widget_vSetEnabled, "??_7Widget@agui@@6B@", "?setEnabled@Widget@agui@@UEAAAEAV12@_N@Z", true)
MP_VSLOT(Widget_vSetText, "??_7Widget@agui@@6B@", "?setText@Widget@agui@@UEAAXAEB" MP_STD_STRING "@Z", true)
MP_VSLOT(Widget_vClear, "??_7Widget@agui@@6B@", "?clear@Widget@agui@@UEAAXXZ", true)

// --- widget constructors ---------------------------------------------------------
MP_FUNC(TextButton_ctor, "??0TextButton@agui@@QEAA@$$QEA" MP_STD_STRING "PEBVButtonStyle@1@@Z", true)
MP_FUNC(DropDown_ctor, "??0DropDown@agui@@QEAA@PEBVDropDownStyle@1@@Z", true)
MP_FUNC(TextField_ctor, "??0TextField@agui@@QEAA@PEBVTextBoxStyle@1@@Z", true)
MP_FUNC(Label_ctor, "??0Label@agui@@QEAA@AEB" MP_STD_STRING "PEBVLabelStyle@1@@Z", true)
MP_FUNC(HorizontalFlow_ctor, "??0HorizontalFlow@agui@@QEAA@PEBVHorizontalFlowStyle@1@@Z", true)
MP_FUNC(VerticalFlow_ctor, "??0VerticalFlow@agui@@QEAA@PEBVVerticalFlowStyle@1@@Z", true)
MP_FUNC(EmptyWidget_ctor, "??0EmptyWidget@agui@@QEAA@PEBVEmptyWidgetStyle@1@@Z", true)
MP_FUNC(VerticalScrollPane_ctor, "??0VerticalScrollPane@agui@@QEAA@PEBVScrollPaneStyle@1@@Z", true)
MP_FUNC(Tab_ctor, "??0Tab@agui@@QEAA@AEB" MP_STD_STRING "PEBVTabStyle@1@@Z", true)
MP_FUNC(FrameWithSubheader_ctor, "??0FrameWithSubheader@@QEAA@PEBVFrameStyle@agui@@_N1@Z", true)
MP_FUNC(SearchBar_ctor, "??0SearchBar@@QEAA@PEAVFrame@agui@@V?$function@$$A6AXAEB" MP_STD_STRING "@Z@std@@V?$NamedBool@VInSubheaderTag@@@@@Z", true)

// --- concrete widget methods (called on objects whose exact type we know) ---------
MP_FUNC(TabbedPane_addTab, "?addTab@TabbedPane@agui@@QEAAXPEAVTab@2@PEAVWidget@2@0@Z", true)
MP_FUNC(DropDown_onItemSelect, "?onItemSelect@DropDown@agui@@QEAAXPEAVGenericTargetable@2@V?$function@$$A6AXH@Z@std@@@Z", true)
MP_FUNC(DropDown_addItem, "?addItem@DropDown@agui@@UEAAXAEB" MP_STD_STRING "@Z", true)
MP_FUNC(DropDown_clearItems, "?clearItems@DropDown@agui@@UEAAXXZ", true)
MP_FUNC(DropDown_setSelectedIndex, "?setSelectedIndex@DropDown@agui@@UEAAXH@Z", true)
MP_FUNC(DropDown_getSelectedIndex, "?getSelectedIndex@DropDown@agui@@UEBAHXZ", true)
MP_FUNC(TextBox_getText, "?getText@TextBox@agui@@UEBAAEB" MP_STD_STRING "XZ", true)
MP_FUNC(TextBox_setText, "?setText@TextBox@agui@@UEAAXAEB" MP_STD_STRING "@Z", true)

// --- styles ------------------------------------------------------------------------
MP_FIELD(GlobalContext_style, "GlobalContext", "style", true)
MP_FIELD(GuiStyle_styles, "GuiStyle", "styles", true)
MP_VSLOT(IntegratedStyle_vGetAguiStyle, "??_7?$IntegratedStyle@VButtonStyleSpecification@@@@6B@",
         "?getAguiStyle@?$IntegratedStyle@VButtonStyleSpecification@@@@UEAAPEAVStyle@agui@@XZ", true)

// --- locale (optional: falls back to config.ini) ---------------------------------
MP_FIELD(GlobalContext_localeProvider, "GlobalContext", "localeProvider", false)
MP_FIELD(LocaleProvider_currentLocale, "LocaleProvider", "currentLocale", false)
MP_FIELD(Locale_language, "LocaleProvider::Locale", "language", false)

// --- widget tree -------------------------------------------------------------------
MP_FIELD(Widget_parentWidget, "agui::Widget", "parentWidget", true)
MP_FIELD(Widget_usageBitMask, "agui::Widget", "usageBitMask", true)
MP_BASE(Widget_GenericTargetable, "agui::Widget", "agui::GenericTargetable", true)

// --- mods window -------------------------------------------------------------------
MP_FIELD(ModsGui_tabs, "ModsGui", "tabs", true)
MP_FIELD(ModsGui_manageTabContents, "ModsGui", "manageTabContents", true)
MP_FIELD(FrameWithSubheader_subheader, "FrameWithSubheader", "subheader", true)
MP_FIELD(ManageModsPane_modWidgets, "ManageModsPane", "modWidgets", true)
MP_FIELD(ManageModsPane_searchBar, "ManageModsPane", "searchBar", true)
MP_FIELD(ModWidgets_stateData, "ManageModsPane::ModWidgets", "stateData", true)
MP_FIELD(ModWidgets_enabled, "ManageModsPane::ModWidgets", "enabled", true)
MP_FIELD(ModWidgets_computed, "ManageModsPane::ModWidgets", "computed", true)
MP_FIELD(Optional_DependencyInfo_hasValue, "std::_Optional_destruct_base<ModDependency::Info,1>", "_Has_value", true)
MP_FIELD(ModAndName_mod, "ModAndName", "mod", true)
MP_FIELD(ModAndName_toBeDeleted, "ModAndName", "toBeDeleted", true)
MP_FIELD(ModAndName_localisedName, "ModAndName", "localisedName", true)
MP_FIELD(Mod_name, "Mod", "name", true)
MP_FIELD(ToggleButton_checkedState, "agui::ToggleButton", "checkedState", true)
MP_ENUM(CheckedState_UNCHECKED, "agui::ToggleButton::CheckedState", "UNCHECKED", true)
MP_ENUM(CheckedState_CHECKED, "agui::ToggleButton::CheckedState", "CHECKED", true)

// --- "Mod packs" tab: tables like the "Installed" tab ------------------------------
MP_FUNC(TableWithSelection_ctor, "??0TableWithSelection@agui@@QEAA@PEBVTableStyle@1@@Z", true)
MP_FUNC(TableWithSelection_addHeader, "?addHeader@TableWithSelection@agui@@UEAAXAEAVWidget@2@_N11@Z", true)
MP_FUNC(TableWithSelection_clearApartHeader, "?clearApartHeader@TableWithSelection@agui@@UEAAXXZ", true)
MP_FUNC(TableWithSelection_setColumnOrdering, "?setColumnOrdering@TableWithSelection@agui@@QEAAXI_N@Z", true)
MP_FUNC(TableWithSelection_onOrderingChange,
        "?onTableOrderingColumnChange@TableWithSelection@agui@@QEAAXPEAVGenericTargetable@2@V?$function@$$A6AXUTableIndex@agui@@_N@Z@std@@@Z",
        true)
MP_FUNC(TableWithSelection_onSelectionChange,
        "?onTableSelectionChange@TableWithSelection@agui@@QEAAXPEAVGenericTargetable@2@V?$function@$$A6AXUTableIndex@agui@@0H@Z@std@@@Z",
        true)
MP_SIZE(abi_TableIndex, "agui::TableIndex", true)
MP_VSLOT(Table_vSetColumnCount, "??_7TableWithSelection@agui@@6B@", "?setColumnCount@Table@agui@@UEAAXH@Z", true)
MP_FUNC(Table_alignColumn, "?alignColumn@Table@agui@@QEAAAEAV12@HW4AreaAlign@2@@Z", true)
MP_ENUM(AreaAlign_LeftMiddle, "agui::AreaAlign", "LeftMiddle", true)
MP_ENUM(AreaAlign_RightMiddle, "agui::AreaAlign", "RightMiddle", true)
MP_VSLOT(Widget_vDelete, "??_7Widget@agui@@6B@", "??_GWidget@agui@@UEAAPEAXI@Z", true)

// --- equal column widths -------------------------------------------------------------
MP_FIELD(VerticalFlow_style, "agui::VerticalFlow", "style", true)
MP_FIELD(ScrollPane_style, "agui::ScrollPane", "style", true)
MP_FIELD(Table_style, "agui::Table", "style", true)
MP_FUNC(Style_setHorizontallyStretchable, "?setHorizontallyStretchable@Style@agui@@QEAAXW4StretchRule@2@@Z", true)
MP_FUNC(Style_setHorizontallySquashable, "?setHorizontallySquashable@Style@agui@@QEAAXW4StretchRule@2@@Z", true)
MP_VSLOT(Style_vSetNaturalSize, "??_7Style@agui@@6B@", "?setNaturalSize@Style@agui@@UEAAXHH@Z", true)
MP_FIELD(Style_naturalHeight, "agui::Style", "naturalHeight", true)
MP_FUNC(Widget_applySizeRestrictions, "?applySizeRestrictions@Widget@agui@@QEAAXAEBVStyle@2@@Z", true)
MP_ENUM(StretchRule_On, "agui::StretchRule", "On", true)

// --- mod info pane (optional: without it the tab shows no mod details) -------------
MP_FIELD(TabbedPane_selectedTab, "agui::TabbedPane", "selectedTab", false)
MP_FIELD(TabbedPane_tabs, "agui::TabbedPane", "tabs", false)
MP_FIELD(TabEntry_tab, "agui::TabbedPane::TabEntry", "tab", false)
MP_FIELD(GenericTargeter_target, "agui::GenericTargeterBase", "target", false)
MP_FIELD(ManageModsPane_modInfoPane, "ManageModsPane", "modInfoPane", false)
MP_FUNC(ModInfoPane_showMod, "?showMod@ModInfoPane@@QEAAXAEBVModAndName@@VModVersion@@@Z", false)
MP_FUNC(ModInfoPane_clearInfo, "?clearInfo@ModInfoPane@@QEAAXXZ", false)
MP_FUNC(ManageModsPane_refreshSelectedModInfo, "?refreshSelectedModInfo@ManageModsPane@@AEAAXXZ", false)
MP_SIZE(abi_ModVersion, "ModVersion", false)

// --- versions ------------------------------------------------------------------------
MP_FIELD(ModAndName_activeVersion, "ModAndName", "activeVersion", true)
MP_FIELD(Version_major, "Version", "majorVersion", true)
MP_FIELD(Version_minor, "Version", "minorVersion", true)
MP_FIELD(Version_sub, "Version", "subVersion", true)

// --- sizes for operator new ------------------------------------------------------
MP_SIZE(size_TextButton, "agui::TextButton", true)
MP_SIZE(size_DropDown, "agui::DropDown", true)
MP_SIZE(size_TextField, "agui::TextField", true)
MP_SIZE(size_Label, "agui::Label", true)
MP_SIZE(size_HorizontalFlow, "agui::HorizontalFlow", true)
MP_SIZE(size_VerticalFlow, "agui::VerticalFlow", true)
MP_SIZE(size_EmptyWidget, "agui::EmptyWidget", true)
MP_SIZE(size_VerticalScrollPane, "agui::VerticalScrollPane", true)
MP_SIZE(size_Tab, "agui::Tab", true)
MP_SIZE(size_FrameWithSubheader, "FrameWithSubheader", true)
MP_SIZE(size_SearchBar, "SearchBar", true)
MP_SIZE(size_TableWithSelection, "agui::TableWithSelection", true)

// --- ABI checks: compared with our own sizeof at startup ------------------------
MP_SIZE(abi_String, "std::basic_string<char,std::char_traits<char>,std::allocator<char> >", true)
MP_SIZE(abi_FunctionVoid, "std::function<void __cdecl(void)>", true)
MP_SIZE(abi_FunctionInt, "std::function<void __cdecl(int)>", true)
MP_SIZE(abi_FunctionString,
        "std::function<void __cdecl(std::basic_string<char,std::char_traits<char>,std::allocator<char> > const &)>", true)
MP_SIZE(abi_NamedBoolInSubheader, "NamedBool<InSubheaderTag>", true)

#undef MP_STD_STRING
#undef MP_FUNC
#undef MP_DATA
#undef MP_FIELD
#undef MP_SIZE
#undef MP_BASE
#undef MP_VSLOT
#undef MP_ENUM
