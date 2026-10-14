#include "core/guard.h"

#include <windows.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>

#include "core/log.h"

namespace modpacks {

namespace {

std::string ThrowIn() {
    char buf[128] = {};
    DWORD n = GetEnvironmentVariableA("MODPACKS_THROW_IN", buf, sizeof(buf));
    return n > 0 && n < sizeof(buf) ? std::string(buf, n) : std::string();
}

}  // namespace

bool Guarded(const char* name, const std::function<void()>& fn) {
    static const std::string throwIn = ThrowIn();
    try {
        if (!throwIn.empty() && throwIn == name) throw std::runtime_error("MODPACKS_THROW_IN test exception");
        fn();
        return true;
    } catch (const std::exception& e) {
        Log("error in ", name, ": ", e.what());
    } catch (...) {
        // With /EHa this also catches access violations and other SEH exceptions.
        Log("error in ", name, ": unknown or structured exception");
    }
    return false;
}

}  // namespace modpacks
