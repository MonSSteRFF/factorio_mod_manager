@echo off
rem Removes the "Mod packs" modification from a Factorio client.
rem Usage: uninstall.bat ["path\to\Factorio"]
rem Deletes only the files listed in GAME\bin\x64\modpacks.install, then the list itself.
rem The user's packs, mods\mod-packs.json in the Factorio data folder, are kept.
setlocal EnableExtensions EnableDelayedExpansion
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
  set "T_TITLE=Удаление модификации «Пакеты модов» из Factorio"
  set "T_NOTFOUND=Factorio не найдена. Укажите путь к игре аргументом, например:"
  set "T_GAME=Папка игры:"
  set "T_NOTHING=Модификация не установлена, удалять нечего."
  set "T_RUNNING=Factorio сейчас запущена. Сначала необходимо закрыть клиент игры."
  set "T_PRESS=Закройте игру и нажмите любую клавишу, чтобы продолжить (или закройте это окно для отмены)."
  set "T_DELFAIL=Не удалось удалить файл. Проверьте права на запись в папку игры:"
  set "T_DONE=Модификация удалена, игра в исходном состоянии."
  set "T_KEPT=Ваши пакеты сохранены, они вернутся при повторной установке:"
) else (
  set "T_TITLE=Removing the Mod packs modification from Factorio"
  set "T_NOTFOUND=Factorio was not found. Pass the game folder as an argument, for example:"
  set "T_GAME=Game folder:"
  set "T_NOTHING=The modification is not installed, nothing to remove."
  set "T_RUNNING=Factorio is running. The game client must be closed first."
  set "T_PRESS=Close the game and press any key to continue (or close this window to cancel)."
  set "T_DELFAIL=Could not delete a file. Check that the game folder is writable:"
  set "T_DONE=Modification removed, the game is back to its original state."
  set "T_KEPT=Your mod packs are kept and will be back after reinstalling:"
)

echo !T_TITLE!
echo.

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
set "MARK=!BIN!\modpacks.install"
echo !T_GAME! !GAME!
if not exist "!MARK!" (
  echo.
  echo !T_NOTHING!
  call :finish
  exit /b 0
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

rem ---------------------------------------------------------------- delete our files
for /f "usebackq tokens=1,* delims==" %%a in ("!MARK!") do (
  if /i "%%a"=="file" if exist "!BIN!\%%b" (
    del /f /q "!BIN!\%%b" 2>nul
    if exist "!BIN!\%%b" (
      echo.
      echo !T_DELFAIL! !BIN!\%%b
      goto :fail
    )
  )
)
del /f /q "!MARK!" 2>nul
if exist "!MARK!" (
  echo.
  echo !T_DELFAIL! !MARK!
  goto :fail
)

echo.
echo !T_DONE!
echo !T_KEPT!
echo   %APPDATA%\Factorio\mods\mod-packs.json
call :finish
exit /b 0

:notfound
echo !T_NOTFOUND!
echo   uninstall.bat "D:\Games\Factorio"
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
