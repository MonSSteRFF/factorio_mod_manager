// Every piece of mod code the engine calls runs inside Guarded(): C++ and SEH
// exceptions (the DLL is built with /EHa) are logged instead of crashing the
// game (spec native-loader, "Ошибка внутри модификации").
#pragma once

#include <functional>
#include <utility>

namespace modpacks {

// Runs `fn`; logs and swallows any exception. `name` identifies the handler in
// the log and in MODPACKS_THROW_IN=<name> (test hook that makes it throw).
// Returns false if `fn` did not complete.
bool Guarded(const char* name, const std::function<void()>& fn);

// Wraps a handler so each call goes through Guarded().
template <typename... Args, typename Fn>
std::function<void(Args...)> GuardedHandler(const char* name, Fn fn) {
    return [name, fn = std::move(fn)](Args... args) {
        Guarded(name, [&] { fn(args...); });
    };
}

}  // namespace modpacks
