#!/usr/bin/env bash
# Installs what RecompStation needs to build from source.
#
# Supported: Arch Linux and derivatives (pacman), Debian 13 and newer,
# Ubuntu 25.04 and newer, and derivatives (apt). RecompStation needs
# Qt 6.5 or newer: Debian 12 and Ubuntu 24.04 ship Qt 6.4, which is too old
# (use a prebuilt download there).

set -Eeuo pipefail

MINIMUM_QT="6.5"

if ! command -v sudo >/dev/null 2>&1; then
    echo "ERROR: sudo is required."
    exit 1
fi

version_at_least() {
    # version_at_least 6.8.2 6.5 -> true
    [[ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n 1)" == "$2" ]]
}

install_arch() {
    PACKAGES=(
        base-devel cmake ninja git pkgconf
        qt6-base qt6-declarative qt6-svg qt6-shadertools qt6-wayland
        libarchive sdl2
        ccache mold
    )
    echo "Packages to install with pacman:"
    printf '  %s\n' "${PACKAGES[@]}"
    echo
    sudo pacman -S --needed "${PACKAGES[@]}"
}

install_debian() {
    sudo apt-get update

    # Check the Qt version before installing anything.
    local candidate
    candidate="$(apt-cache policy qt6-base-dev 2>/dev/null | sed -n 's/^ *Candidate: //p')"
    candidate="${candidate#*:}"          # drop an epoch
    candidate="${candidate%%[+~-]*}"     # 6.8.2+dfsg-7 -> 6.8.2
    if [[ -z "$candidate" || "$candidate" == "(none)" ]]; then
        echo "ERROR: Qt 6 (qt6-base-dev) isn't available from your package sources."
        exit 1
    fi
    if ! version_at_least "$candidate" "$MINIMUM_QT"; then
        echo "ERROR: your system offers Qt $candidate; RecompStation needs Qt $MINIMUM_QT or newer."
        echo "Debian 13, Ubuntu 25.04 and newer have it. On this system, use the prebuilt"
        echo "AppImage or zip from the releases page instead."
        exit 1
    fi
    echo "Qt $candidate is available."
    echo

    PACKAGES=(
        # Compiler and build tools
        build-essential cmake ninja-build git pkgconf
        # Qt: libraries and development files
        qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-shadertools-dev
        # Qt: drawing on X11 and Wayland, the SQLite driver, image formats
        qt6-qpa-plugins qt6-wayland libqt6sql6-sqlite qt6-image-formats-plugins
        # QML modules the interface uses
        qml6-module-qtqml qml6-module-qtqml-workerscript qml6-module-qtcore
        qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-templates
        qml6-module-qtquick-controls qml6-module-qtquick-layouts
        qml6-module-qtquick-dialogs qml6-module-qtquick-effects qml6-module-qtquick-shapes
        # QML module for the tests (./scripts/build.sh --test)
        qml6-module-qttest
        # Archives, controllers and sounds
        libarchive-dev libsdl2-dev
        # OpenGL headers
        libgl-dev
        # Faster rebuilds and linking (optional, used automatically)
        ccache mold
    )
    echo "Packages to install with apt:"
    printf '  %s\n' "${PACKAGES[@]}"
    echo
    sudo apt-get install "${PACKAGES[@]}"
}

echo "======================================"
echo " RecompStation - Install dependencies"
echo "======================================"
echo

if command -v pacman >/dev/null 2>&1; then
    install_arch
elif command -v apt-get >/dev/null 2>&1; then
    install_debian
else
    echo "ERROR: this script supports Arch (pacman) and Debian/Ubuntu (apt)."
    echo "On other distributions, install the requirements listed in README.md."
    exit 1
fi

echo
echo "Checking the tools..."
for tool in cmake ninja git g++; do
    if command -v "$tool" >/dev/null 2>&1; then
        echo "[OK] $tool"
    else
        echo "[MISSING] $tool"
        exit 1
    fi
done

QMAKE="$(command -v qmake6 || command -v qmake || true)"
if [[ -z "$QMAKE" && -x /usr/lib/qt6/bin/qmake ]]; then
    QMAKE=/usr/lib/qt6/bin/qmake
fi
if [[ -n "$QMAKE" ]]; then
    echo "[OK] Qt $("$QMAKE" -query QT_VERSION)"
fi

echo
echo "Dependencies installed. Build with: ./scripts/build.sh --test"
