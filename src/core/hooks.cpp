#include "core/hooks.h"

#include <windows.h>

#include <map>
#include <memory>
#include <vector>

#include "MinHook.h"
#include "core/engine.h"
#include "core/guard.h"
#include "core/log.h"
#include "core/runtime.h"
#include "i18n/language.h"
#include "ui/window.h"

namespace modpacks {

namespace {

using ModsGuiCtor = void* (*)(void* self);
using PaneDtor = void (*)(void* self);
using UpdateTabs = void (*)(void* self);

ModsGuiCtor g_origModsGuiCtor = nullptr;
PaneDtor g_origPaneDtor = nullptr;
UpdateTabs g_origUpdateTabs = nullptr;

// One Window per open "Mods" window, keyed by its ManageModsPane.
std::map<void*, std::unique_ptr<Window>> g_windows;

void UpdateLanguage() {
    std::string config = ReadConfigLocale(DefaultConfigIniPath()).value_or("");
    std::string engineLocale = engine::EngineLocale();
    Language lang = ResolveLanguage(engineLocale, config, WindowsUiLanguage());
    SetLanguage(lang);
    Log("language: ", lang == Language::Ru ? "ru" : "en", " (game '", engineLocale, "', config '", config, "')");
}

void* HookModsGuiCtor(void* self) {
    void* result = g_origModsGuiCtor(self);
    Guarded("ModsGui constructor", [&] {
        UpdateLanguage();
        void* pane = engine::ModsGuiManagePane(self);
        // Register before building: handlers created during Build point at
        // this Window, so it must live until the pane is destroyed even if
        // Build fails half-way.
        auto& window = g_windows[pane];
        window = std::make_unique<Window>(self);
        window->Build();
        Log("mods window extended");
    });
    return result;
}

void HookPaneDtor(void* self) {
    Guarded("ManageModsPane destructor", [&] { g_windows.erase(self); });
    g_origPaneDtor(self);
}

// After the game shows the info pane of its own tab, show ours for the "Mod packs" tab.
void HookUpdateTabs(void* self) {
    g_origUpdateTabs(self);
    Guarded("tab switch", [&] {
        auto it = g_windows.find(engine::ModsGuiManagePane(self));
        if (it != g_windows.end()) it->second->OnTabsChanged();
    });
}

struct HookSpec {
    SymId id;
    const char* name;
    void* detour;
    void** original;
};

}  // namespace

bool InstallWindowHooks(std::string* error) {
    std::vector<HookSpec> hooks = {
        {SymId::ModsGui_ctor, "ModsGui::ModsGui", reinterpret_cast<void*>(&HookModsGuiCtor),
         reinterpret_cast<void**>(&g_origModsGuiCtor)},
        {SymId::ManageModsPane_dtor, "ManageModsPane::~ManageModsPane", reinterpret_cast<void*>(&HookPaneDtor),
         reinterpret_cast<void**>(&g_origPaneDtor)},
    };
    // Optional: without it the "Mod packs" tab simply shows no mod details.
    if (engine::InfoPaneAvailable()) {
        hooks.push_back({SymId::ModsGui_updateTabContentsVisibility, "ModsGui::updateTabContentsVisibility",
                         reinterpret_cast<void*>(&HookUpdateTabs), reinterpret_cast<void**>(&g_origUpdateTabs)});
    } else {
        Log("mod info pane symbols not found; the Mod packs tab will not show mod details");
    }
    std::vector<void*> created;
    auto rollback = [&](const std::string& what, MH_STATUS st) {
        for (void* t : created) MH_RemoveHook(t);  // also disables it
        if (error) *error = what + ": " + MH_StatusToString(st);
        return false;
    };
    for (const HookSpec& h : hooks) {
        void* target = reinterpret_cast<void*>(ImageBase() + Symbols()[h.id]);
        MH_STATUS st = MH_CreateHook(target, h.detour, h.original);
        if (st != MH_OK) return rollback(std::string("MH_CreateHook ") + h.name, st);
        created.push_back(target);
    }
    for (void* t : created) {
        MH_STATUS st = MH_EnableHook(t);
        if (st != MH_OK) return rollback("MH_EnableHook", st);
    }
    for (const HookSpec& h : hooks) Log("hook installed: ", h.name, " at ", Hex(Symbols()[h.id]));
    return true;
}

}  // namespace modpacks
