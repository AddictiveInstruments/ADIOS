@echo off
REM Console prototype - MSVC only, no CMake, no Qt, no external dependency.
REM The eventual portable build replaces platform\midi_win.cpp with an RtMidi
REM one and puts a Qt window on top; core\ is untouched by either.

setlocal
REM always build from THIS directory, wherever it is invoked from
pushd "%~dp0"

set VS=C:\Program Files\Microsoft Visual Studio\2022\Community
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
  echo Visual Studio 2022 not found at "%VS%"
  exit /b 1
)

if not exist build mkdir build
cl /nologo /EHsc /W3 /O2 /std:c++17 /D_CRT_SECURE_NO_WARNINGS ^
   core\sysex.cpp core\hexfile.cpp platform\midi_win.cpp ^
   cli\upgrade.cpp cli\main.cpp ^
   /Fe:build\5x6_upgrader.exe /Fo:build\ ^
   winmm.lib
if errorlevel 1 exit /b 1

echo.
echo built build\5x6_upgrader.exe
