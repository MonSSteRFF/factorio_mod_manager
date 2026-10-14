# Mod packs for Factorio

[Русский](README.md) | **English**

A native modification of the Factorio client for Windows. It adds **mod packs** to the **Mods** window: named sets of enabled mods that you can switch between in a couple of clicks.

This is **not a regular mod** from the mod portal. Factorio's Lua mods do not run in the main menu and cannot change the Mods window. The modification is therefore a DLL that the game loads at start-up. `factorio.exe` itself is not changed.

Tested on Factorio 2.1.21 (Steam, Windows 11).

![The Installed tab with the QOL pack selected](docs/screenshots/installed-tab-qol.png)

## What it does

### The "Installed" tab

A pack bar appears to the left of the search box: a drop-down list and the Create, Rename and Delete buttons.

![The "Base space age" pack selected: only the DLCs are enabled](docs/screenshots/installed-tab-base-space-age.png)

- The **drop-down list** shows the selected pack. While there are no packs it says "No mod packs". Choosing a pack sets the mod checkboxes from it: the pack's mods are enabled, all others disabled, `base` is left alone. The changes are applied with the game's own **Confirm** button and the game restarts as usual. If you close the window without confirming, the game offers to discard the changes and nothing changes.
- **Create** saves all mods that are currently checked into a new pack. The name is typed right in the bar, then OK or Cancel.
- **Rename** changes the name of the selected pack. The field starts with the current name.
- **Delete** removes the selected pack after asking "Delete pack "…"?" with Yes and No. The mod checkboxes are not changed.

Empty names and duplicates (case-insensitive: `Test` and `test` are the same name) are rejected with a red message in the bar. If a pack contains a mod that is not installed, the other mods are enabled and the modification lists the missing ones.

### The "Mod packs" tab

A new tab to the right of "Updates" for putting a pack together.

![The Mod packs tab: the "Base space age" pack with mod details on the right](docs/screenshots/mod-packs-tab-base-space-age.png)

- Two tables in the same style as the Installed tab, each taking half of the width. On the left, all installed mods except `base` and those already in the pack. On the right, the mods of the selected pack. The number of mods is shown above each table.
- Each row has the name, the version and a square button. `>` adds the mod to the pack, `<` removes it. Changes are saved immediately.
- The arrow in the "Name" header sorts the table alphabetically one way or the other; each table has its own direction.
- The search (magnifier) filters **both** tables at once.
- Clicking a mod shows the same details on the right as on the Installed tab.
- Mods of the pack that are not installed are shown in red and marked "not installed".
- The selected pack is shared with the Installed tab, but selecting a pack here does **not** change the checkboxes: the pack is only opened for editing.

![The "QOL" pack on the Mod packs tab](docs/screenshots/mod-packs-tab-qol.png)

### Language

The modification's texts are in Russian when the game is in Russian, and in English otherwise. The same goes for the install scripts.

## Installing and removing

Requirements: Windows 10/11 x64 and Factorio for Windows with `bin\x64\factorio.pdb` in its folder. The Steam version has this file.

The ready build is in `dist/`:

```
dist/
  install.bat
  uninstall.bat
  files/winmm.dll
  files/modpacks.dll
  files/VERSION
```

### Installing: `install.bat`

Double-click it. If the game is not installed through Steam or the script cannot find it, pass the game folder as an argument:

```bat
install.bat "D:\Games\Factorio"
```

What the script does:
1. Finds the game: from the argument, otherwise in the Steam libraries (`libraryfolders.vdf`).
2. Checks that `bin\x64\factorio.exe` and `bin\x64\factorio.pdb` exist.
3. If Factorio is running, asks you to **close the game client** and waits for a key press. It keeps asking while the game is open. Close the script window to cancel.
4. If `bin\x64` already contains a `winmm.dll` that **is not ours**, it stops and overwrites nothing.
5. Copies `winmm.dll` and `modpacks.dll` to `bin\x64` and writes the marker file `modpacks.install` next to them with the list of its files and the version.

Running `install.bat` again updates the modification to the version next to the script. No administrator rights are needed if you can write to the game folder.

### Removing: `uninstall.bat`

1. Finds the game the same way as the installer.
2. If the game is running, asks you to close it.
3. Deletes **only** the files listed in `modpacks.install`, then the marker itself. Other files, such as other DLLs in `bin\x64`, are not touched.

After that the game is back to its original state. Your packs (`mod-packs.json`) are kept and come back if you install the modification again.

### Where the data is

| What | Where |
|---|---|
| Packs | `%APPDATA%\Factorio\mods\mod-packs.json` (format in [docs/mod-packs-format.md](docs/mod-packs-format.md), in Russian) |
| Modification log | `%APPDATA%\Factorio\modpacks\modpacks.log` |
| Symbol cache | `%APPDATA%\Factorio\modpacks\symcache.json` |

The log and the cache can be deleted at any time without side effects.

## How it works

```
 Steam starts factorio.exe
      |
      v
 Windows loads winmm.dll from the game folder (our proxy)
      |  every winmm call is forwarded to the system winmm.dll
      |  and modpacks.dll is loaded
      v
 modpacks.dll, on a background thread (the game starts without delay):
      1. looks up the functions and fields of the Mods window in factorio.pdb
         (~3 s the first time, then from the cache until the next game update)
      2. if everything is found, hooks the creation of the Mods window;
         if anything is missing, hooks NOTHING and the game runs as usual
      v
 the Mods window is built by the game, and the modification adds
 the pack bar and the Mod packs tab using the engine's own widgets
```

**Game updates.** No addresses are hard-coded; they come from the `factorio.pdb` of the installed version. Regular updates therefore work without rebuilding. If Wube reworks the Mods window and something the modification needs is gone, it simply stays off and the log says why. Engine details and the post-update checklist are in [docs/engine-notes.md](docs/engine-notes.md) (in Russian).

**Limitations.**
- Windows x64 only, and only builds that ship `factorio.pdb`.
- Works only in the main menu's Mods window.
- Mod settings and mod versions are not stored in packs.
- This is an unofficial modification of a closed-source client and is not supported by Wube. Antivirus software may be wary of the proxy DLL; the source code is open.

## Building from source

Requirements:
- **Visual Studio 2022** (Community is fine) with the "Desktop development with C++" workload: MSVC x64 and the Windows SDK.
- **CMake 3.20+** on Windows: the "C++ CMake tools for Windows" component in the Visual Studio Installer, or `winget install Kitware.CMake`.
- For the helper scripts (usually not needed): WSL with `python3`, `llvm-readobj` and `llvm-pdbutil`.

Third-party libraries are already in `third_party/` (see [third_party/README.md](third_party/README.md)): MinHook, nlohmann/json, doctest.

### From Windows

```bat
build.cmd
```

### From WSL

```bash
./build.sh
```

Both build Release x64, run the unit tests and put the result into `dist/` together with `install.bat` and `uninstall.bat`.

### Checking a game version

Before starting a new Factorio version you can check that the modification finds everything it needs:

```bat
build\Release\modpacks_symtool.exe "C:\...\Factorio\bin\x64\factorio.exe"
```

or from WSL: `scripts/check-symbols.sh "/mnt/c/.../Factorio/bin/x64"`.

### Source layout

```
src/proxy/   winmm.dll proxy (export stubs generated by scripts/gen-winmm-proxy.py)
src/core/    start-up, symbol resolution from the PDB, cache, hooks, log, engine wrappers
src/packs/   pack logic and storage (no engine dependencies)
src/i18n/    RU/EN strings and language selection
src/ui/      pack bar, Mod packs tab
scripts/     install.bat, uninstall.bat, helper scripts (see scripts/README.md)
tests/       unit tests (doctest) and modpacks_symtool
docs/        engine notes, mod-packs.json format, screenshots
openspec/    specs and the change plan (OpenSpec)
```

### Regenerating the proxy

Only needed if the set of exports of the system `winmm.dll` changes:

```bash
python3 -I scripts/gen-winmm-proxy.py /mnt/c/Windows/System32/winmm.dll src/proxy
```
