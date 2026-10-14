// Process-wide state set up once at startup (src/core/dllmain.cpp).
#pragma once

#include <cstdint>

#include "core/symbols.h"

namespace modpacks {

uintptr_t ImageBase();         // factorio.exe base address
const Resolved& Symbols();     // resolved symbol table

}  // namespace modpacks
