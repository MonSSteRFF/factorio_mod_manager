@echo off
rem Builds Release x64, runs the unit tests and lays out dist\ (see README.md).
rem Needs Visual Studio 2022 (C++ desktop workload) and CMake 3.20+.
setlocal
set "ROOT=%~dp0"
set "BUILD=%ROOT%build"
set "DIST=%ROOT%dist"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :novswhere
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto :novs
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

where cmake >nul 2>nul
if errorlevel 1 if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake not found. Install "C++ CMake tools for Windows" in Visual Studio Installer
  echo or run: winget install Kitware.CMake
  exit /b 1
)

cmake -S "%ROOT%." -B "%BUILD%" -G "Visual Studio 17 2022" -A x64 || exit /b 1
cmake --build "%BUILD%" --config Release --parallel || exit /b 1

echo.
echo Running unit tests...
"%BUILD%\Release\modpacks_tests.exe" || exit /b 1

if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%\files" || exit /b 1
copy /y "%BUILD%\Release\winmm.dll" "%DIST%\files\" >nul || exit /b 1
copy /y "%BUILD%\Release\modpacks.dll" "%DIST%\files\" >nul || exit /b 1
copy /y "%BUILD%\VERSION" "%DIST%\files\" >nul || exit /b 1
copy /y "%ROOT%scripts\install.bat" "%DIST%\" >nul || exit /b 1
copy /y "%ROOT%scripts\uninstall.bat" "%DIST%\" >nul || exit /b 1
echo.
echo Done: %DIST%
exit /b 0

rem Messages with paths stay outside parenthesized blocks: "(x86)" would close them.
:novswhere
echo Visual Studio Installer not found: %VSWHERE%
exit /b 1

:novs
echo Visual Studio with the C++ x64 tools was not found.
exit /b 1
