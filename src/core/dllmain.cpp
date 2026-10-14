// modpacks.dll bootstrap (design D1-D3).
//
// DllMain runs under the loader lock, so it only checks that this is
// factorio.exe and hooks the exe's entry point. The entry hook starts a
// background thread for the real start-up (symbol resolution, hooks on the
// Mods window) and lets the game start immediately: resolving symbols from the
// PDB takes a few seconds the first time, and the Mods window cannot be opened
// that early anyway. MinHook suspends the other threads while it patches code,
// so enabling the hooks from that thread is safe. On any problem the game runs
// untouched.

#include <windows.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>

#include "MinHook.h"
#include "common/utf.h"
#include "core/engine.h"
#include "core/hooks.h"
#include "core/log.h"
#include "core/runtime.h"
#include "core/symbols.h"
#include "core/symcache.h"
#include "nlohmann/json.hpp"

namespace modpacks {

namespace {

HMODULE g_self = nullptr;
uintptr_t g_imageBase = 0;
Resolved g_symbols;

using EntryPoint = DWORD(WINAPI*)(void*);
EntryPoint g_origEntry = nullptr;

std::wstring ModulePath(HMODULE module) {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        DWORD n = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (n == 0) return {};
        if (n < path.size()) {
            path.resize(n);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

std::wstring Dir(const std::wstring& path) { return path.substr(0, path.find_last_of(L"\\/")); }
std::wstring FileName(const std::wstring& path) { return path.substr(path.find_last_of(L"\\/") + 1); }

std::string EnvVar(const char* name) {
    char buf[256] = {};
    DWORD n = GetEnvironmentVariableA(name, buf, sizeof(buf));
    return n > 0 && n < sizeof(buf) ? std::string(buf, n) : std::string();
}

// Version from data/base/info.json next to bin/x64. Read from a file rather
// than the exe's version resource so this DLL does not depend on version.dll,
// which other tools replace in the game folder.
std::string GameVersion(const std::wstring& exePath) {
    std::filesystem::path info = std::filesystem::path(exePath).parent_path().parent_path().parent_path() /
                                 L"data" / L"base" / L"info.json";
    std::ifstream in(info, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    nlohmann::json root = nlohmann::json::parse(ss.str(), nullptr, false);
    if (root.is_object() && root.contains("version") && root["version"].is_string()) return root["version"];
    return "unknown";
}

bool ResolveSymbols(HMODULE exe, const std::wstring& exePath) {
    std::string forceMissing = EnvVar("MODPACKS_FAKE_MISSING_SYMBOL");
    std::string key = ModuleKey(exe);
    std::filesystem::path cache = DataDir() / L"symcache.json";

    if (forceMissing.empty() && LoadSymbolCache(cache, key, g_symbols)) {
        Log("symbols: from cache (", key, ")");
        return true;
    }

    auto start = std::chrono::steady_clock::now();
    DbgHelpSource source;
    std::string error;
    if (!source.Open(exe, exePath, &error)) {
        Log("symbols: ", error);
        return false;
    }
    ResolveReport report = ResolveAll(source, g_symbols, forceMissing);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    Log("symbols: resolved from factorio.pdb in ", static_cast<long long>(ms), " ms");
    for (const std::string& m : report.missingOptional) Log("symbols: optional item not found: ", m);
    for (const std::string& m : report.missingRequired) Log("symbols: REQUIRED item not found: ", m);
    if (!report.ok()) return false;
    if (forceMissing.empty()) SaveSymbolCache(cache, key, g_symbols);
    return true;
}

void Start() {
    std::wstring exePath = ModulePath(nullptr);
    HMODULE exe = GetModuleHandleW(nullptr);
    g_imageBase = reinterpret_cast<uintptr_t>(exe);
    LogOpen(DataDir() / L"modpacks.log");
    Log("modpacks ", MODPACKS_VERSION, ", Factorio ", GameVersion(exePath), ", ", exePath);

    if (!ResolveSymbols(exe, exePath)) {
        Log("mod NOT active: the game's symbols do not match what the mod needs; the game runs unmodified");
        return;
    }
    if (std::string abi = CheckAbi(g_symbols); !abi.empty()) {
        Log("mod NOT active: ABI mismatch, ", abi);
        return;
    }
    std::string error;
    if (!engine::Init(g_symbols, g_imageBase, &error)) {
        Log("mod NOT active: ", error);
        return;
    }
    if (!InstallWindowHooks(&error)) {
        Log("mod NOT active: ", error);
        return;
    }
    Log("mod active");
}

void LogStartException(DWORD code) { Log("mod NOT active: exception ", Hex(code), " during start-up"); }

// No C++ objects in this frame: __try cannot be mixed with unwinding.
void StartSafely() {
    DWORD code = 0;
    __try {
        Start();
    } __except (code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        LogStartException(code);
    }
}

DWORD WINAPI StartThread(void*) {
    StartSafely();
    return 0;
}

DWORD WINAPI EntryDetour(void* arg) {
    if (HANDLE thread = CreateThread(nullptr, 0, &StartThread, nullptr, 0, nullptr)) CloseHandle(thread);
    return g_origEntry(arg);
}

// Only factorio.exe from the folder this DLL was installed into.
bool IsTargetProcess() {
    std::wstring exe = ModulePath(nullptr);
    std::wstring self = ModulePath(g_self);
    return !exe.empty() && !self.empty() && CompareStringOrdinal(FileName(exe).c_str(), -1, L"factorio.exe", -1, TRUE) == CSTR_EQUAL &&
           CompareStringOrdinal(Dir(exe).c_str(), -1, Dir(self).c_str(), -1, TRUE) == CSTR_EQUAL;
}

void HookEntryPoint() {
    auto* base = reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    void* entry = const_cast<BYTE*>(base) + nt->OptionalHeader.AddressOfEntryPoint;
    if (MH_Initialize() != MH_OK) return;
    if (MH_CreateHook(entry, reinterpret_cast<void*>(&EntryDetour), reinterpret_cast<void**>(&g_origEntry)) != MH_OK)
        return;
    if (MH_EnableHook(entry) != MH_OK) MH_RemoveHook(entry);
}

}  // namespace

uintptr_t ImageBase() { return g_imageBase; }
const Resolved& Symbols() { return g_symbols; }

}  // namespace modpacks

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        modpacks::g_self = instance;
        if (!modpacks::IsTargetProcess()) return TRUE;  // stays loaded, does nothing
        // Engine objects keep our std::function handlers; never unload.
        HMODULE pinned = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           reinterpret_cast<LPCWSTR>(&DllMain), &pinned);
        modpacks::HookEntryPoint();
    }
    return TRUE;
}
