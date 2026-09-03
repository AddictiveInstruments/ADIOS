@echo off
REM ADIOS Studio - Qt window. Everything it needs is inside the Qt install:
REM no PATH setup, no Qt Creator, builds the same from a terminal or a script.

setlocal
pushd "%~dp0"

set VS=C:\Program Files\Microsoft Visual Studio\2022\Community
set QT=C:\Qt\6.11.2\msvc2022_64
set CMAKE=C:\Qt\Tools\CMake_64\bin\cmake.exe
set NINJA=C:\Qt\Tools\Ninja\ninja.exe

if not exist "%QT%\lib\cmake\Qt6" ( echo Qt 6 MSVC 2022 64-bit not found at "%QT%" & exit /b 1 )
if not exist "%CMAKE%" ( echo CMake not found at "%CMAKE%" & exit /b 1 )

call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 ( echo Visual Studio 2022 not found at "%VS%" & exit /b 1 )

"%CMAKE%" -B build-gui -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_PREFIX_PATH="%QT%" ^
  -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

REM Windows locks a running exe against being OVERWRITTEN, but not against being
REM RENAMED: move the previous binary aside so the link succeeds even while ADIOS
REM Studio is open - the running instance keeps the file it was started from.
REM But the .old.exe NAME is not always free: an older instance can still be holding
REM it, the rename then fails, and the link dies on a locked adios_studio.exe. So:
REM sweep whatever is free, and fall back to a numbered name for the rest.
del /q build-gui\adios_studio.old.exe >nul 2>&1
del /q build-gui\adios_studio.prev*.exe >nul 2>&1
if exist build-gui\adios_studio.exe ren build-gui\adios_studio.exe adios_studio.old.exe >nul 2>&1
if exist build-gui\adios_studio.exe ren build-gui\adios_studio.exe adios_studio.prev%RANDOM%.exe >nul 2>&1
if exist build-gui\adios_studio.exe echo WARNING: could not move the previous binary aside - close every open ADIOS Studio

"%CMAKE%" --build build-gui
if errorlevel 1 exit /b 1

"%QT%\bin\windeployqt.exe" --no-translations --no-system-d3d-compiler ^
    --no-compiler-runtime build-gui\adios_studio.exe >nul
if errorlevel 1 exit /b 1

echo.
echo built build-gui\adios_studio.exe
