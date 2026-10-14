// mod-packs.json persistence (spec mod-pack-storage, design D7).
#pragma once

#include <filesystem>
#include <string>

#include "packs/pack_store.h"

namespace modpacks {

enum class LoadStatus {
    Missing,  // no file yet: empty store, nothing written
    Ok,
    Corrupt,  // unreadable: backed up, store left empty
};

struct LoadResult {
    LoadStatus status = LoadStatus::Missing;
    std::filesystem::path backup;  // set when status == Corrupt and the copy succeeded
    std::string error;             // human-readable reason for Corrupt
};

// %APPDATA%\Factorio\mods\mod-packs.json
std::filesystem::path DefaultPacksPath();

// Reads `path` into `store`. A corrupt file is copied to
// "<name>.broken-YYYYMMDD-HHMMSS" next to it and is not modified.
LoadResult LoadPacks(const std::filesystem::path& path, PackStore& store);

// Writes atomically: a temporary file in the same folder, flushed, then
// swapped in, so a crash leaves either the old or the new file intact.
bool SavePacks(const std::filesystem::path& path, const PackStore& store, std::string* error = nullptr);

// JSON text of the store (format 1), exposed for tests and docs.
std::string SerializePacks(const PackStore& store);
// Parses JSON text; returns false and fills `error` on malformed data.
bool ParsePacks(std::string_view json, PackStore& store, std::string* error);

}  // namespace modpacks
