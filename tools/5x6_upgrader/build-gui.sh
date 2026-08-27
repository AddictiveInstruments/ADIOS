#!/bin/sh
# The Qt window, macOS. Same CMakeLists as Windows - only the transport file
# and the frameworks differ, and CMake picks those itself.
#
#   ./build-gui.sh                    finds Qt under ~/Qt
#   QT_DIR=/path/to/6.x/macos ./build-gui.sh
set -e
cd "$(dirname "$0")"

# Qt's own CMake is fine, but a system one is just as good - unlike Windows,
# macOS usually has cmake on the PATH already (brew, Xcode tools).
if [ -z "$QT_DIR" ]; then
    QT_DIR=$(ls -d "$HOME"/Qt/6.*/macos 2>/dev/null | sort -V | tail -1)
fi
if [ -z "$QT_DIR" ] || [ ! -d "$QT_DIR/lib/cmake/Qt6" ]; then
    echo "Qt 6 for macOS not found."
    echo "Set QT_DIR, e.g. QT_DIR=\$HOME/Qt/6.11.2/macos $0"
    exit 1
fi

CMAKE=$(command -v cmake || echo "$HOME/Qt/Tools/CMake/CMake.app/Contents/bin/cmake")
if [ ! -x "$CMAKE" ] && ! command -v cmake >/dev/null; then
    echo "cmake not found - install it, or install Qt's CMake component"
    exit 1
fi

"$CMAKE" -B build-gui -DCMAKE_PREFIX_PATH="$QT_DIR" -DCMAKE_BUILD_TYPE=Release
"$CMAKE" --build build-gui

# A .app needs Qt's frameworks copied inside it before it can be handed to
# anyone. Unsigned, it still trips Gatekeeper on another machine - that is a
# signing question, not a build one.
#
# NOT swallowed with `|| true` any more: a failure here used to be hidden while
# the script still announced success. It cost nothing that day - macdeployqt had
# already copied everything and only the final signing step failed - but a
# deployment step that can fail silently is a deployment step you cannot trust.
"$QT_DIR/bin/macdeployqt" build-gui/tr5x6_upgrader.app

echo
echo "built build-gui/tr5x6_upgrader.app"
