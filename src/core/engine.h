// Thin, typed access to the engine objects the mod touches (design D4-D6).
// Every address and offset comes from the resolved symbol table; see
// docs/engine-notes.md for what each call corresponds to in the game.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/symbols.h"

namespace modpacks::engine {

// Opaque engine objects.
using Widget = void;
using ModsGui = void;
using ManagePane = void;
using ModRow = void;  // ManageModsPane::ModWidgets

// Binds the wrappers to the loaded exe. Fails (and explains why) if a layout
// fact cannot be confirmed, e.g. the widget ownership bit.
bool Init(const Resolved& resolved, uintptr_t imageBase, std::string* error);

// --- styles -------------------------------------------------------------------------
// agui style of the prototype style `name` (e.g. "button"), or nullptr.
const void* Style(const char* name);

// --- creating widgets ---------------------------------------------------------------
// Widgets are allocated with the engine's operator new and marked as owned by
// their parent, so the engine deletes them with the widget tree.
Widget* NewTextButton(const std::string& text, const char* style);
Widget* NewDropDown(const char* style);
Widget* NewTextField(const char* style);
Widget* NewLabel(const std::string& text, const char* style);
Widget* NewHorizontalFlow(const char* style);
Widget* NewVerticalFlow(const char* style);
Widget* NewPusher();  // empty widget that takes the remaining width
Widget* NewVerticalScrollPane(const char* style);
Widget* NewTab(const std::string& caption);
Widget* NewFrameWithSubheader(const char* style);
// Search bar living in `frame`'s subheader; `onChange` receives the text.
Widget* NewSearchBar(Widget* frame, std::function<void(const std::string&)> onChange);
// The subheader frame of a FrameWithSubheader (ours or ManageModsPane).
Widget* Subheader(Widget* frameWithSubheader);

// --- widget operations --------------------------------------------------------------
void Add(Widget* parent, Widget* child);
void Insert(Widget* parent, Widget* child, unsigned index);
void Clear(Widget* parent);  // removes and deletes owned children
Widget* Parent(Widget* widget);
void SetVisible(Widget* widget, bool visible);
void SetEnabled(Widget* widget, bool enabled);
void SetText(Widget* widget, const std::string& text);  // labels and buttons
void SetToolTip(Widget* widget, const std::string& text);
void StretchHorizontally(Widget* widget);
void Stretch(Widget* widget);
// Handlers live as long as `widget` does.
void OnClick(Widget* widget, std::function<void()> handler);

void DropDownClear(Widget* dropDown);
void DropDownAdd(Widget* dropDown, const std::string& item);
void DropDownSelect(Widget* dropDown, int index);
int DropDownSelected(Widget* dropDown);
void OnItemSelect(Widget* dropDown, std::function<void(int)> handler);

std::string FieldText(Widget* textField);
void SetFieldText(Widget* textField, const std::string& text);

void AddTab(Widget* tabbedPane, Widget* tab, Widget* content);

// --- tables (agui::TableWithSelection, like the "Installed" tab) --------------------
Widget* NewTable(const char* style);
// Header cell; `sortable` adds the sorting arrow. The header label is owned by the table.
void TableAddHeader(Widget* table, Widget* header, bool sortable);
// Removes all rows but keeps the header; row cells we own are not deleted.
void TableClearRows(Widget* table);
void TableSetColumnCount(Widget* table, int columns);
void TableAlignColumn(Widget* table, int column, bool right);
void TableSetOrdering(Widget* table, unsigned column, bool ascending);
// `handler(column, ascending)` when the user clicks a sorting arrow.
void OnTableOrdering(Widget* table, std::function<void(unsigned, bool)> handler);
// `handler(row)` with the 0-based row (header excluded), or -1 when nothing is selected.
void OnTableSelection(Widget* table, std::function<void(int)> handler);

// Widgets that we keep and re-add ourselves (table cells) are created owned and
// then released with Disown(); DeleteWidget() destroys such a widget.
Widget* Disown(Widget* widget);
void DeleteWidget(Widget* widget);

// Makes a VerticalFlow take an equal share of the width with its stretchable
// siblings, independent of its content (natural width 0, stretch and squash).
void EqualWidth(Widget* verticalFlow);
// Lets a scroll pane or table shrink below its content width.
void SquashScrollPane(Widget* scrollPane);
void SquashTable(Widget* table);

// --- mods window --------------------------------------------------------------------
Widget* ModsGuiTabs(ModsGui* gui);
ManagePane* ModsGuiManagePane(ModsGui* gui);
Widget* ManagePaneSearchBar(ManagePane* pane);
std::vector<ModRow*> ModRows(ManagePane* pane);
const std::string& RowModName(ModRow* row);
const std::string& RowTitle(ModRow* row);  // localised title as shown in the list
std::string RowVersion(ModRow* row);        // "1.2.3"
bool RowChecked(ModRow* row);
void RowSetChecked(ModRow* row, bool checked);
// After changing checkboxes: drop cached dependency checks, restyle rows and
// refresh the "enable all" box, like the game's own "enable all" toggle does.
void RefreshRows(ManagePane* pane);

// --- mod info pane (on the right of the window) ---------------------------------------
// False when the optional symbols for it were not found.
bool InfoPaneAvailable();
bool IsTabSelected(Widget* tabbedPane, Widget* tab);
// Shows the "Installed" tab's info pane with `row`'s mod, or cleared if null.
void ShowModInfo(ManagePane* pane, ModRow* row);
// Back to what the "Installed" tab itself has selected.
void RestoreManageInfo(ManagePane* pane);

// --- locale -------------------------------------------------------------------------
// Language code of the running game ("ru", "en", ...), empty if unknown.
std::string EngineLocale();

}  // namespace modpacks::engine
