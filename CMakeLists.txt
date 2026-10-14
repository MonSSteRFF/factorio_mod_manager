cmake_minimum_required(VERSION 3.20)
project(modpacks VERSION 0.1.0 LANGUAGES C CXX ASM_MASM)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "modpacks builds only with MSVC for x64 (Factorio is a 64-bit MSVC binary)")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static release CRT in every configuration: nothing extra to install next to
# factorio.exe, and std::string/std::vector/std::function keep the release
# layout the game uses (a debug CRT adds iterator-debugging members).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
# CMake puts /EHsc into CMAKE_CXX_FLAGS; exception handling is set per target
# instead (modpacks.dll needs /EHa).
string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
add_compile_options(
  "$<$<COMPILE_LANGUAGE:C,CXX>:/W4;/utf-8;/Zi>"
  "$<$<COMPILE_LANGUAGE:CXX>:/permissive->"
  "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<STREQUAL:$<TARGET_PROPERTY:NAME>,modpacks>>>:/EHsc>")
add_link_options(/DEBUG)
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
                        _ITERATOR_DEBUG_LEVEL=0
                        MODPACKS_VERSION="${PROJECT_VERSION}")

# Version for the installer (build.cmd copies it to dist\files\VERSION).
file(WRITE "${CMAKE_BINARY_DIR}/VERSION" "${PROJECT_VERSION}")

# --- third party -----------------------------------------------------------
add_library(minhook STATIC
  third_party/minhook/src/buffer.c
  third_party/minhook/src/hook.c
  third_party/minhook/src/trampoline.c
  third_party/minhook/src/hde/hde64.c)
target_include_directories(minhook PUBLIC third_party/minhook/include)
target_compile_options(minhook PRIVATE /W0)

add_library(thirdparty_headers INTERFACE)
target_include_directories(thirdparty_headers SYSTEM INTERFACE third_party)

# --- engine-independent logic (unit tested) ----------------------------------
add_library(modpacks_logic STATIC
  src/packs/pack_store.cpp
  src/packs/pack_file.cpp
  src/i18n/strings.cpp
  src/i18n/language.cpp)
target_include_directories(modpacks_logic PUBLIC src)
target_link_libraries(modpacks_logic PUBLIC thirdparty_headers)

# --- winmm.dll proxy ---------------------------------------------------------
add_library(winmm SHARED
  src/proxy/winmm_proxy.cpp
  src/proxy/winmm_stubs.asm
  src/proxy/winmm.def)
set_target_properties(winmm PROPERTIES OUTPUT_NAME winmm)

# --- modpacks.dll ------------------------------------------------------------
add_library(modpacks SHARED
  src/core/dllmain.cpp
  src/core/log.cpp
  src/core/guard.cpp
  src/core/symbols.cpp
  src/core/symcache.cpp
  src/core/hooks.cpp
  src/core/engine.cpp
  src/ui/window.cpp
  src/ui/pack_panel.cpp
  src/ui/editor_tab.cpp)
# /EHa: catch(...) in Guarded() also stops access violations from crashing the game.
target_compile_options(modpacks PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/EHa>")
target_link_libraries(modpacks PRIVATE modpacks_logic minhook dbghelp)

# --- tests -------------------------------------------------------------------
add_executable(modpacks_tests
  tests/main.cpp
  tests/pack_store_tests.cpp
  tests/pack_file_tests.cpp
  tests/i18n_tests.cpp
  tests/symbols_tests.cpp
  src/core/symbols.cpp
  src/core/symcache.cpp
  src/core/log.cpp)
target_link_libraries(modpacks_tests PRIVATE modpacks_logic dbghelp)
target_compile_definitions(modpacks_tests PRIVATE MODPACKS_TESTING)

# --- developer tool: symbol resolution outside the game ----------------------
add_executable(modpacks_symtool tests/symtool.cpp src/core/symbols.cpp)
target_link_libraries(modpacks_symtool PRIVATE modpacks_logic dbghelp)
