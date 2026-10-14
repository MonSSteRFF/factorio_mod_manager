@echo off
rem Installs the "Mod packs" modification into a Factorio client (Windows x64).
rem Usage: install.bat ["path\to\Factorio"]
rem Copies files\winmm.dll and files\modpacks.dll into GAME\bin\x64 and
rem records them in GAME\bin\x64\modpacks.install. factorio.exe is not changed.
setlocal EnableExtensions EnableDelayedExpansion
set "SRC=%~dp0files"
set "ARG=%~1"

rem ---------------------------------------------------------------- language
rem Same as the game: locale= from config.ini, "auto" or no setting means the
rem Windows display language (read from the registry, see :uilang).
set "LOC="
set "CFG=%APPDATA%\Factorio\config\config.ini"
if exist "%CFG%" for /f "usebackq tokens=1,* delims==" %%a in ("%CFG%") do if /i "%%a"=="locale" set "LOC=%%b"
if /i "!LOC!"=="auto" set "LOC="
if not defined LOC call :uilang
set "RU=0"
if /i "!LOC:~0,2!"=="ru" set "RU=1"
chcp 65001 >nul

if "%RU%"=="1" (
  set "T_TITLE=Установка модификации «Пакеты модов» для Factorio"
  set "T_NOFILES=Не найдены файлы модификации рядом со скриптом:"
  set "T_NOTFOUND=Factorio не найдена. Укажите путь к игре аргументом, например:"
  set "T_GAME=Папка игры:"
  set "T_NOEXE=В папке игры нет bin\x64\factorio.exe:"
  set "T_NOPDB=В папке игры нет bin\x64\factorio.pdb. Эта сборка игры не поддерживается: модификации нужен файл символов, который есть в Steam-версии."
  set "T_RUNNING=Factorio сейчас запущена. Сначала необходимо закрыть клиент игры."
  set "T_PRESS=Закройте игру и нажмите любую клавишу, чтобы продолжить (или закройте это окно для отмены)."
  set "T_CONFLICT=В bin\x64 уже есть чужой файл, установка остановлена, ничего не изменено:"
  set "T_COPYFAIL=Не удалось скопировать файл. Проверьте права на запись в папку игры:"
  set "T_DONE=Модификация установлена, версия"
  set "T_UPDATED=Модификация обновлена до версии"
  set "T_RUN=Запустите игру и откройте «Моды»."
) else (
  set "T_TITLE=Installing the Mod packs modification for Factorio"
  set "T_NOFILES=Modification files not found next to this script:"
  set "T_NOTFOUND=Factorio was not found. Pass the game folder as an argument, for example:"
  set "T_GAME=Game folder:"
  set "T_NOEXE=The game folder has no bin\x64\factorio.exe:"
  set "T_NOPDB=The game folder has no bin\x64\factorio.pdb. This game build is not supported: the modification needs the symbol file that the Steam version ships."
  set "T_RUNNING=Factorio is running. The game client must be closed first."
  set "T_PRESS=Close the game and press any key to continue (or close this window to cancel)."
  set "T_CONFLICT=bin\x64 already contains a file that does not belong to this modification; installation stopped, nothing changed:"
  set "T_COPYFAIL=Could not copy a file. Check that the game folder is writable:"
  set "T_DONE=Modification installed, version"
  set "T_UPDATED=Modification updated to version"
  set "T_RUN=Start the game and open Mods."
)

echo !T_TITLE!
echo.

if not exist "%SRC%\winmm.dll" goto :nofiles
if not exist "%SRC%\modpacks.dll" goto :nofiles
set "VERSION=unknown"
if exist "%SRC%\VERSION" set /p VERSION=<"%SRC%\VERSION"

rem ---------------------------------------------------------------- game folder
rem Without an argument: SteamPath from the registry, then every library listed
rem in libraryfolders.vdf. The PowerShell source is in scripts/README.md.
set "GAME="
if defined ARG (
  set "GAME=%~f1"
) else (
  for /f "usebackq delims=" %%g in (`powershell -NoProfile -EncodedCommand "JABzACAAPQAgACgARwBlAHQALQBJAHQAZQBtAFAAcgBvAHAAZQByAHQAeQAgACcASABLAEMAVQA6AFwAUwBvAGYAdAB3AGEAcgBlAFwAVgBhAGwAdgBlAFwAUwB0AGUAYQBtACcAIAAtAEUAcgByAG8AcgBBAGMAdABpAG8AbgAgAFMAaQBsAGUAbgB0AGwAeQBDAG8AbgB0AGkAbgB1AGUAKQAuAFMAdABlAGEAbQBQAGEAdABoAAoAaQBmACAAKAAtAG4AbwB0ACAAJABzACkAIAB7ACAAZQB4AGkAdAAgADEAIAB9AAoAJABsAGkAYgBzACAAPQAgAEAAKAAkAHMAKQAKACQAdgBkAGYAIAA9ACAASgBvAGkAbgAtAFAAYQB0AGgAIAAkAHMAIAAnAHMAdABlAGEAbQBhAHAAcABzAFwAbABpAGIAcgBhAHIAeQBmAG8AbABkAGUAcgBzAC4AdgBkAGYAJwAKAGkAZgAgACgAVABlAHMAdAAtAFAAYQB0AGgAIAAkAHYAZABmACkAIAB7AAoAIAAgAGYAbwByAGUAYQBjAGgAIAAoACQAbQAgAGkAbgAgAFsAcgBlAGcAZQB4AF0AOgA6AE0AYQB0AGMAaABlAHMAKAAoAEcAZQB0AC0AQwBvAG4AdABlAG4AdAAgAC0AUgBhAHcAIAAkAHYAZABmACkALAAgACcAIgBwAGEAdABoACIAXABzACsAIgAoAFsAXgAiAF0AKwApACIAJwApACkAIAB7ACAAJABsAGkAYgBzACAAKwA9ACAAJABtAC4ARwByAG8AdQBwAHMAWwAxAF0ALgBWAGEAbAB1AGUALgBSAGUAcABsAGEAYwBlACgAJwBcAFwAJwAsACAAJwBcACcAKQAgAH0ACgB9AAoAZgBvAHIAZQBhAGMAaAAgACgAJABsACAAaQBuACAAJABsAGkAYgBzACkAIAB7AAoAIAAgACQAcAAgAD0AIABKAG8AaQBuAC0AUABhAHQAaAAgACQAbAAgACcAcwB0AGUAYQBtAGEAcABwAHMAXABjAG8AbQBtAG8AbgBcAEYAYQBjAHQAbwByAGkAbwAnAAoAIAAgAGkAZgAgACgAVABlAHMAdAAtAFAAYQB0AGgAIAAoAEoAbwBpAG4ALQBQAGEAdABoACAAJABwACAAJwBiAGkAbgBcAHgANgA0AFwAZgBhAGMAdABvAHIAaQBvAC4AZQB4AGUAJwApACkAIAB7ACAAKABSAGUAcwBvAGwAdgBlAC0AUABhAHQAaAAgACQAcAApAC4AUABhAHQAaAA7ACAAZQB4AGkAdAAgADAAIAB9AAoAfQAKAGUAeABpAHQAIAAxAA==" 2^>nul`) do if exist "%%g\bin\x64\factorio.exe" set "GAME=%%g"
)
if not defined GAME goto :notfound
set "BIN=!GAME!\bin\x64"
echo !T_GAME! !GAME!
if not exist "!BIN!\factorio.exe" (
  echo.
  echo !T_NOEXE! !GAME!
  goto :fail
)
if not exist "!BIN!\factorio.pdb" (
  echo.
  echo !T_NOPDB!
  goto :fail
)

rem ---------------------------------------------------------------- game must be closed
:waitgame
tasklist /FI "IMAGENAME eq factorio.exe" /NH 2>nul | find /I "factorio.exe" >nul
if errorlevel 1 goto :closed
echo.
echo !T_RUNNING!
echo !T_PRESS!
pause >nul
goto :waitgame
:closed

rem ---------------------------------------------------------------- foreign files
rem Our files are listed in modpacks.install; anything else with our names is not ours.
set "MARK=!BIN!\modpacks.install"
for %%f in (winmm.dll modpacks.dll) do (
  if exist "!BIN!\%%f" (
    set "OURS=0"
    if exist "!MARK!" findstr /x /i /c:"file=%%f" "!MARK!" >nul && set "OURS=1"
    if "!OURS!"=="0" (
      echo.
      echo !T_CONFLICT!
      echo   !BIN!\%%f
      goto :fail
    )
  )
)

rem ---------------------------------------------------------------- copy
set "UPDATE=0"
if exist "!MARK!" set "UPDATE=1"
for %%f in (winmm.dll modpacks.dll) do (
  copy /y "%SRC%\%%f" "!BIN!\%%f" >nul
  if errorlevel 1 (
    echo.
    echo !T_COPYFAIL! !BIN!\%%f
    goto :fail
  )
)
(
  echo version=!VERSION!
  echo file=winmm.dll
  echo file=modpacks.dll
) > "!MARK!"
if errorlevel 1 (
  echo.
  echo !T_COPYFAIL! !MARK!
  goto :fail
)

echo.
if "!UPDATE!"=="1" (echo !T_UPDATED! !VERSION!.) else (echo !T_DONE! !VERSION!.)
echo !T_RUN!
call :finish
exit /b 0

:nofiles
echo !T_NOFILES! %SRC%
goto :fail

:notfound
echo !T_NOTFOUND!
echo   install.bat "D:\Games\Factorio"
goto :fail

:fail
call :finish
exit /b 1

:finish
rem Keep the window open when started by double click.
if not defined MODPACKS_NOPAUSE (
  echo.
  pause
)
exit /b 0

:uilang
rem Windows display language: an explicit user override, else the first entry of
rem the user's language list, else the machine default. Values look like ru-RU.
for %%k in ("HKCU\Control Panel\Desktop|PreferredUILanguages" "HKCU\Control Panel\International\User Profile|Languages" "HKCU\Control Panel\Desktop\MuiCached|MachinePreferredUILanguages") do (
  if not defined LOC for /f "tokens=1,2 delims=|" %%a in (%%k) do (
    for /f "tokens=3" %%v in ('reg query "%%a" /v %%b 2^>nul ^| find "REG_"') do set "LOC=%%v"
  )
)
exit /b 0
