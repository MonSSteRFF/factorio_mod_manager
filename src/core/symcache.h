// Cache of resolved symbols (design D3): %APPDATA%\Factorio\modpacks\symcache.json.
#pragma once

#include <windows.h>

#include <filesystem>
#include <string>

#include "core/symbols.h"

namespace modpacks {

// Identity of the loaded exe: PDB GUID+age from its CodeView record,
// TimeDateStamp and SizeOfImage. Empty if the exe has no CodeView record.
std::string ModuleKey(HMODULE module);

// Fills `out` from the cache if it was written for the same key, the same
// mod version and the same symbol table. Returns false otherwise.
bool LoadSymbolCache(const std::filesystem::path& path, const std::string& key, Resolved& out);

// Stores a fully successful resolution.
void SaveSymbolCache(const std::filesystem::path& path, const std::string& key, const Resolved& r);

}  // namespace modpacks
