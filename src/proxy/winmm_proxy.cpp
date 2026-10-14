// winmm.dll proxy for factorio.exe.
//
// Every export of the system winmm.dll is re-exported with the same name and
// ordinal (winmm.def). Each export is a one-instruction stub (winmm_stubs.asm)
// that jumps through modpacks_winmm_table, which is filled here from the real
// %SystemRoot%\System32\winmm.dll. After that the proxy loads modpacks.dll from
// its own folder; a missing or failing modpacks.dll never affects the game.

#include <windows.h>

#include <cstddef>

namespace {

struct Export {
    WORD ordinal;
    const char* name;
};

constexpr Export kExports[] = {
#include "winmm_exports.inc"
};

constexpr size_t kExportCount = sizeof(kExports) / sizeof(kExports[0]);

}  // namespace

extern "C" void* modpacks_winmm_table[kExportCount] = {};

namespace {

HMODULE g_system_winmm = nullptr;

// Calls to an export the system library does not have end up here instead of
// jumping to address 0.
DWORD WINAPI MissingExport() {
    SetLastError(ERROR_PROC_NOT_FOUND);
    return 0;
}

bool LoadSystemWinmm() {
    wchar_t path[MAX_PATH];
    UINT len = GetSystemDirectoryW(path, MAX_PATH);
    if (len == 0 || len + 11 >= MAX_PATH) return false;
    lstrcatW(path, L"\\winmm.dll");
    g_system_winmm = LoadLibraryW(path);
    if (!g_system_winmm) return false;

    for (size_t i = 0; i < kExportCount; ++i) {
        FARPROC proc = kExports[i].name
                           ? GetProcAddress(g_system_winmm, kExports[i].name)
                           : GetProcAddress(g_system_winmm, MAKEINTRESOURCEA(kExports[i].ordinal));
        modpacks_winmm_table[i] = proc ? reinterpret_cast<void*>(proc)
                                       : reinterpret_cast<void*>(&MissingExport);
    }
    return true;
}

void LoadModpacks(HMODULE self) {
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(self, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;
    wchar_t* slash = path + len;
    while (slash > path && slash[-1] != L'\\') --slash;
    *slash = L'\0';
    if (lstrlenW(path) + 13 >= MAX_PATH) return;
    lstrcatW(path, L"modpacks.dll");
    // Failure is silent on purpose: without modpacks.dll this is a plain proxy.
    LoadLibraryW(path);
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        if (!LoadSystemWinmm()) return FALSE;
        LoadModpacks(instance);
    }
    return TRUE;
}
