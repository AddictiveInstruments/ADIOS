cmake_minimum_required(VERSION 3.21)

# Same floor as the upgrader, and for the same reason: without these two set
# BEFORE project(), CMake takes the build machine's own architecture and OS
# version - not a default, an accident. 13.0 is the lowest Qt's own binaries
# reach.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Intel and Apple Silicon")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "oldest macOS supported")
endif()

project(tr5x6_screen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# The MIDI transport is the upgrader's, used HERE WITHOUT A COPY - it was
# written OS-free precisely so a second tool could take it as it is. It only
# carries MIRROR_HALT now: two SysEx bytes of ceremony around each capture.
set(SHARED ../5x6_upgrader)

if(WIN32)
    set(TRANSPORT ${SHARED}/platform/midi_win.cpp)
    set(TRANSPORT_LIBS winmm)
elseif(APPLE)
    set(TRANSPORT ${SHARED}/platform/midi_mac.cpp)
    set(TRANSPORT_LIBS "-framework CoreMIDI" "-framework CoreFoundation")
else()
    message(FATAL_ERROR
        "No transport for this platform yet - add platform/midi_<os>.cpp to "
        "${SHARED}, implementing the classes in platform/midi_win.h.")
endif()

qt_add_executable(tr5x6_screen MACOSX_BUNDLE WIN32
    gui/main.cpp gui/MainWindow.cpp gui/MainWindow.h
    ${TRANSPORT})

qt_add_resources(tr5x6_screen "style" PREFIX "/" FILES gui/style.qss)

if(APPLE)
    set_target_properties(tr5x6_screen PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.addictiveinstruments.tr5x6screen"
        MACOSX_BUNDLE_BUNDLE_NAME    "5x6 Screen"
        MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION "${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

target_compile_definitions(tr5x6_screen PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(tr5x6_screen PRIVATE Qt6::Widgets ${TRANSPORT_LIBS})
