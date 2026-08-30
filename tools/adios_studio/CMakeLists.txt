cmake_minimum_required(VERSION 3.21)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Intel and Apple Silicon")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "oldest macOS supported")
endif()

project(adios_studio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# The protocol encoder and the Intel-HEX loader live in tools/common at the
# repository root, shared WITHOUT A COPY by every tool that speaks the ADIOS
# bootloader protocol - they are OS-free and describe the protocol itself. The
# MIDI TRANSPORT is NOT shared: a monitor needs every message, so ADIOS Studio
# carries its own richer transport under midi/.
set(COMMON ../common)

if(WIN32)
    set(MIDI midi/midi_win.cpp)
elseif(APPLE)
    set(MIDI midi/midi_mac.cpp)
else()
    message(FATAL_ERROR "No MIDI transport for this platform yet - add midi/midi_<os>.cpp")
endif()

qt_add_executable(adios_studio MACOSX_BUNDLE WIN32
    app/main.cpp
    app/MainWindow.cpp app/MainWindow.h app/MainWindow.ui
    app/Uploader.cpp   app/Uploader.h
    app/decode.cpp     app/decode.h
    ${MIDI} midi/midi.h
    ${COMMON}/sysex.cpp
    ${COMMON}/hexfile.cpp)

# uic writes ui_MainWindow.h into the build tree; let the sources #include it.
# tools/common is on the include path so the shared headers resolve by name.
target_include_directories(adios_studio PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${COMMON})

qt_add_resources(adios_studio "style" PREFIX "/" FILES app/style.qss)

if(APPLE)
    set_target_properties(adios_studio PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.addictiveinstruments.adiosstudio"
        MACOSX_BUNDLE_BUNDLE_NAME    "ADIOS Studio"
        MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION "${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

target_compile_definitions(adios_studio PRIVATE _CRT_SECURE_NO_WARNINGS)
if(WIN32)
    target_link_libraries(adios_studio PRIVATE Qt6::Widgets winmm)
elseif(APPLE)
    target_link_libraries(adios_studio PRIVATE Qt6::Widgets "-framework CoreMIDI" "-framework CoreFoundation")
endif()
