@echo off
pushd %~dp0
rem 32-bit console helper - CubeProgrammer_API.dll is 32-bit on this bench,
rem so the 64-bit GUI delegates the whole SWD session to this little one.
rem Built beside the GUI executable; the GUI runs it from its own directory.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
cl /nologo /EHsc /O2 dump_helper.cpp /Fe:5x6_dump_helper.exe
popd
