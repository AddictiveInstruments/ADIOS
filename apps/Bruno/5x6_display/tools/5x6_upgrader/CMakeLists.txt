cmake_minimum_required(VERSION 3.21)

# macOS: WHAT this binary is allowed to run on. Both must be set BEFORE
# project(), which is when CMake freezes them.
#
# Without them CMake silently takes the build machine's own values, and that is
# not a default - it is an accident. Measured on 2026-08-27: the executable came
# out arm64-only with a floor of macOS 15.0, so no Intel Mac could open it and
# no Mac below 15 either, while the Qt libraries macdeployqt had put INSIDE the
# same bundle were universal and went down to 13.0. The bundle was narrower than
# its own contents.
#
# 13.0 is the lowest floor reachable without rebuilding Qt itself - its binaries
# stop there. ⚠️ And a lowered floor is a PERMISSION, not a proof: it tells the
# linker to accept an older system, it does not check that nothing calls a newer
# API. Ours is plain CoreMIDI, all of it far older than 13.0, but that is an
# argument and not a test - nobody here has a Mac below 15 to try it on.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Intel and Apple Silicon")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "oldest macOS supported")
endif()

project(tr5x6_upgrader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# The protocol encoder and the Intel-HEX loader live in tools/common at the
# repository root - shared verbatim with ADIOS Studio and the console prototype,
# they know no operating system and no toolkit. cli/upgrade.cpp is this tool's.
set(COMMON ../../../../../tools/common)
set(CORE
    ${COMMON}/sysex.cpp
    ${COMMON}/hexfile.cpp
    cli/upgrade.cpp)

# The transport is the only platform-specific file. winmm on Windows is what
# RtMidi wraps there; a macOS build swaps this one file for an RtMidi-backed
# implementation of the same three classes and nothing above it changes.
if(WIN32)
    set(TRANSPORT platform/midi_win.cpp)
    set(TRANSPORT_LIBS winmm)
elseif(APPLE)
    set(TRANSPORT platform/midi_mac.cpp)
    set(TRANSPORT_LIBS "-framework CoreMIDI" "-framework CoreFoundation")
else()
    message(FATAL_ERROR
        "No transport for this platform yet - add platform/midi_<os>.cpp "
        "implementing midiInPorts/midiOutPorts/MidiIn/MidiOut from "
        "platform/midi_win.h, then list it here.")
endif()

qt_add_executable(tr5x6_upgrader MACOSX_BUNDLE WIN32
    ${CORE} ${TRANSPORT}
    gui/main.cpp gui/MainWindow.cpp gui/MainWindow.h)

if(APPLE)
    set_target_properties(tr5x6_upgrader PROPERTIES
        # The placeholder identifier was still com.yourcompany.tr5x6_upgrader.
        # It is what LaunchServices files the application under, so it wants to
        # be ours before anything is handed to a tester.
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.addictiveinstruments.tr5x6upgrader"
        MACOSX_BUNDLE_BUNDLE_NAME    "5x6 Firmware Update"
        # Measured empty on 2026-08-27, and that is worse than it sounds: a
        # bundle that declares no system requirement is not refused politely on
        # an older Mac - it is launched, and it dies in the dynamic loader with
        # a message no musician can act on. Declaring the floor turns a crash
        # into a sentence.
        MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION "${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

qt_add_resources(tr5x6_upgrader "style" PREFIX "/" FILES gui/style.qss)

target_compile_definitions(tr5x6_upgrader PRIVATE _CRT_SECURE_NO_WARNINGS)
target_include_directories(tr5x6_upgrader PRIVATE ${COMMON})
target_link_libraries(tr5x6_upgrader PRIVATE Qt6::Widgets ${TRANSPORT_LIBS})

# The firmware images travel BESIDE the executable rather than inside it: the
# build must be able to refresh them without relinking, and a tester who is
# asked which version they ran can read the file names. The application picks
# WHICH migration image by the machine type the ping returned.
#
# Two ways to find them, and the second is what makes this folder portable:
#   - inside the repository, they come from the firmware projects that built
#     them, so a rebuild is picked up with no copying;
#   - on their own (this folder handed to another machine), an `images/`
#     directory beside this file is used instead.
# The second is how the macOS build works today, the repository being on the
# PC only.
set(IMAGE_NAMES
    5x6_migration/5x6_migration_505.hex
    5x6_migration/5x6_migration_626.hex
    5x6_display/5x6_display_rev1_app_only.hex)

set(IMAGES "")
foreach(rel ${IMAGE_NAMES})
    get_filename_component(base ${rel} NAME)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../apps/Bruno/${rel})
        list(APPEND IMAGES ${CMAKE_CURRENT_SOURCE_DIR}/../../apps/Bruno/${rel})
    elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/images/${base})
        list(APPEND IMAGES ${CMAKE_CURRENT_SOURCE_DIR}/images/${base})
    else()
        message(FATAL_ERROR
            "missing firmware image ${base} - build it in the repository, or "
            "put it in ${CMAKE_CURRENT_SOURCE_DIR}/images/")
    endif()
endforeach()

# WHERE they land is not the same on the two systems, and it is not cosmetic.
# In a .app, Contents/MacOS/ is reserved for CODE: codesign treats anything it
# finds there as a subcomponent to sign, chokes on an Intel-HEX file and gives
# up ("code object is not signed at all ... In subcomponent: ...hex"), leaving
# the bundle unsealed. Data belongs in Contents/Resources/.
if(APPLE)
    set(IMAGE_DEST $<TARGET_BUNDLE_CONTENT_DIR:tr5x6_upgrader>/Resources/images)
else()
    set(IMAGE_DEST $<TARGET_FILE_DIR:tr5x6_upgrader>/images)
endif()

add_custom_command(TARGET tr5x6_upgrader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${IMAGE_DEST}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${IMAGES} ${IMAGE_DEST}
    COMMENT "copying the firmware images into the application")
