
#!/usr/bin/env bash

set -Eeuo pipefail

echo "======================================"
echo " RecompStation - Arch Dependency Setup"
echo "======================================"

# Verify distribution

if [[ ! -f /etc/arch-release ]]; then
    echo "ERROR: This installer targets Arch Linux."
    exit 1
fi

# Verify package manager

if ! command -v pacman >/dev/null 2>&1; then
    echo "ERROR: Pacman was not found."
    exit 1
fi

# Verify sudo

if ! command -v sudo >/dev/null 2>&1; then
    echo "ERROR: sudo is required."
    exit 1
fi

PACKAGES=(
    base-devel
    cmake
    ninja
    git
    pkgconf
    qtcreator
    gdb
    qt6-base
    qt6-declarative
    qt6-tools
    qt6-wayland
    clang
    ccache
    sqlite
)

echo
echo "The following packages will be installed:"
printf ' - %s\n' "${PACKAGES[@]}"

echo
echo "Updating system and installing dependencies..."

sudo pacman -Syu --needed "${PACKAGES[@]}"

echo
echo "Checking development tools..."

for tool in cmake ninja git g++; do
    if command -v "$tool" >/dev/null 2>&1; then
        echo "[OK] $tool"
    else
        echo "[ERROR] $tool not found"
        exit 1
    fi
done

echo
echo "Checking Qt installation..."

if command -v qmake6 >/dev/null 2>&1; then
    qmake6 -query QT_VERSION
else
    echo "Qt installed; qmake6 not found in PATH."
fi

echo
echo "Checking SQLite..."

if command -v sqlite3 >/dev/null 2>&1; then
    sqlite3 --version
else
    echo "SQLite library installed."
fi

echo
echo "======================================"
echo " Dependencies installed successfully!"
echo "======================================"
