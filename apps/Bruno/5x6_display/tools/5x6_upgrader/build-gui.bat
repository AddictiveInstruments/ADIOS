@echo off
REM The Qt window. Everything it needs is inside the Qt installation - no PATH
REM setup and no Qt Creator required, so this builds the same way from a
REM terminal, from an IDE, or from a script.

setlocal
pushd "%~dp0"

set VS=C:\Program Files\Microsoft Visual Studio\2022\Community
set QT=C:\Qt\6.11.2\msvc2022_64
set CMAKE=C:\Qt\Tools\CMake_64\bin\cmake.exe
set NINJA=C:\Qt\Tools\Ninja\ninja.exe

if not exist "%QT%\lib\cmake\Qt6" (
  echo Qt 6 MSVC 2022 64-bit not found at "%QT%"
  exit /b 1
)
if not exist "%CMAKE%" ( echo CMake not found at "%CMAKE%" & exit /b 1 )

call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 ( echo Visual Studio 2022 not found at "%VS%" & exit /b 1 )

"%CMAKE%" -B build-gui -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_PREFIX_PATH="%QT%" ^
  -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

"%CMAKE%" --build build-gui
if errorlevel 1 exit /b 1

REM A Qt program needs its DLLs beside it on Windows. windeployqt copies
REM exactly the ones this binary references - not the whole installation.
REM --no-compiler-runtime: by default windeployqt drops a 24 MB vc_redist
REM INSTALLER next to the binary, which helps nobody here. Distribution will
REM need the VC runtime one way or another - as DLLs beside the executable, or
REM as a prerequisite - but that is a packaging decision, not a build one.
"%QT%\bin\windeployqt.exe" --no-translations --no-system-d3d-compiler ^
    --no-compiler-runtime build-gui\tr5x6_upgrader.exe >nul
if errorlevel 1 exit /b 1

echo.
echo built build-gui\tr5x6_upgrader.exe
