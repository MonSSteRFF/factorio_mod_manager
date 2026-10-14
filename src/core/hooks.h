// Hooks on the "Mods" window (design D4).
#pragma once

#include <string>

namespace modpacks {

// Creates and enables the hooks. All or nothing: on failure no hook stays enabled.
bool InstallWindowHooks(std::string* error);

}  // namespace modpacks
