#!/usr/bin/env bash
# Builds the two Linux release files from a fresh Release build:
#   dist/RecompStation-<version>-x86_64.AppImage   one self-contained file
#   dist/RecompStation-<version>-linux-x86_64.zip  portable folder
# plus dist/SHA256SUMS. Both carry the same files: the program, Qt's
# libraries, plugins and QML modules, and the other libraries it links to,
# found through RPATH and qt.conf (no LD_LIBRARY_PATH that would leak into
# the games RecompStation starts).
#
# The release workflow (.github/workflows/release.yml) runs this on Ubuntu
# 22.04 with Qt from the Qt installer, so the files run on that and newer
# systems. Run locally, the result only runs on systems as new as yours.

set -Eeuo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build-release}"
DIST_DIR="${DIST_DIR:-$PROJECT_ROOT/dist}"
TOOLS_DIR="${TOOLS_DIR:-$PROJECT_ROOT/build-tools}"
APP_ID="io.github.MetalSJ13.RecompStation"
RUN_TESTS="false"

usage() {
    cat <<'USAGE'
Usage: ./scripts/package.sh [--test]

  --test     Build and run the regression tests before packaging.

Environment:
  QMAKE      qmake of the Qt to bundle (default: qmake6 or qmake on PATH).
  BUILD_DIR  Release build folder (default: ./build-release).
  DIST_DIR   Where the release files go (default: ./dist).
  TOOLS_DIR  Where linuxdeploy is downloaded to (default: ./build-tools).
  JOBS       Parallel jobs (default: number of CPUs).
USAGE
}

for arg in "$@"; do
    case "$arg" in
        --test) RUN_TESTS="true" ;;
        -h|--help) usage; exit 0 ;;
        *) echo "ERROR: Unknown argument: $arg"; usage; exit 2 ;;
    esac
done

if [[ "$(uname -m)" != "x86_64" ]]; then
    echo "ERROR: Release files are built for x86_64 only (this machine is $(uname -m))."
    exit 1
fi

QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake || true)}"
if [[ -z "$QMAKE" || ! -x "$QMAKE" ]]; then
    echo "ERROR: qmake not found. Set QMAKE to the qmake of the Qt to bundle."
    exit 1
fi
QT_PREFIX="$("$QMAKE" -query QT_INSTALL_PREFIX)"
QT_PLUGINS="$("$QMAKE" -query QT_INSTALL_PLUGINS)"
QT_VERSION="$("$QMAKE" -query QT_VERSION)"
echo "Bundling Qt $QT_VERSION from $QT_PREFIX"

VERSION="$(sed -n 's/^    VERSION \([0-9][0-9.]*\)$/\1/p' "$PROJECT_ROOT/CMakeLists.txt" | head -n 1)"
if [[ -z "$VERSION" ]]; then
    echo "ERROR: Could not read the version from CMakeLists.txt."
    exit 1
fi
JOBS="${JOBS:-$(nproc)}"
GENERATOR=()
command -v ninja >/dev/null 2>&1 && GENERATOR=(-G Ninja)

# 1. Release build (tests optional), installed into an AppDir.
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" "${GENERATOR[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_TESTING="$([[ "$RUN_TESTS" == "true" ]] && echo ON || echo OFF)"
cmake --build "$BUILD_DIR" --parallel "$JOBS"
if [[ "$RUN_TESTS" == "true" ]]; then
    QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
        ctest --test-dir "$BUILD_DIR" --output-on-failure --parallel "$JOBS"
fi

APPDIR="$BUILD_DIR/AppDir"
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR"

# 2. linuxdeploy and its Qt plugin (downloaded once).
mkdir -p "$TOOLS_DIR"
fetch() {
    local name="$1" url="$2"
    if [[ ! -x "$TOOLS_DIR/$name" ]]; then
        echo "Downloading $name"
        curl -fsSL --retry 3 -o "$TOOLS_DIR/$name.part" "$url"
        chmod +x "$TOOLS_DIR/$name.part"
        mv "$TOOLS_DIR/$name.part" "$TOOLS_DIR/$name"
    fi
}
fetch linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fetch linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
export PATH="$TOOLS_DIR:$PATH"
# Run the tools without FUSE (containers and CI often lack it).
export APPIMAGE_EXTRACT_AND_RUN=1
LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"

# 3. Bundle Qt and the libraries the program uses.
export QMAKE
export QML_SOURCES_PATHS="$PROJECT_ROOT/qml"
export LD_LIBRARY_PATH="$("$QMAKE" -query QT_INSTALL_LIBS)${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# Wayland: Qt picks it on Wayland sessions when the plugin is present, and
# falls back to X11 (XWayland) otherwise.
WAYLAND_PLATFORMS=()
for plugin in libqwayland-egl.so libqwayland-generic.so; do
    [[ -f "$QT_PLUGINS/platforms/$plugin" ]] && WAYLAND_PLATFORMS+=("$plugin")
done
if (( ${#WAYLAND_PLATFORMS[@]} )); then
    export EXTRA_PLATFORM_PLUGINS="$(IFS=';'; echo "${WAYLAND_PLATFORMS[*]}")"
else
    echo "WARNING: Qt Wayland platform plugins not found; the build will use X11 (XWayland) on Wayland."
fi
"$LINUXDEPLOY" --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/RecompStationApp" \
    --custom-apprun "$PROJECT_ROOT/packaging/AppRun" \
    --plugin qt

# The Wayland helper plugins (window decorations, shell and graphics
# integration) when the Qt plugin left them out.
if (( ${#WAYLAND_PLATFORMS[@]} )); then
    for folder in wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration; do
        if [[ -d "$QT_PLUGINS/$folder" && ! -d "$APPDIR/usr/plugins/$folder" ]]; then
            mkdir -p "$APPDIR/usr/plugins/$folder"
            cp -a "$QT_PLUGINS/$folder/." "$APPDIR/usr/plugins/$folder/"
            for library in "$APPDIR/usr/plugins/$folder"/*.so; do
                "$LINUXDEPLOY" --appdir "$APPDIR" --deploy-deps-only "$library"
            done
        fi
    done
fi

# Only the SQLite database driver is used; others need client libraries.
find "$APPDIR/usr/plugins/sqldrivers" -name 'libqsql*.so' ! -name 'libqsqlite.so' -delete 2>/dev/null || true
if [[ ! -f "$APPDIR/usr/plugins/sqldrivers/libqsqlite.so" ]]; then
    echo "ERROR: Qt's SQLite driver was not bundled."
    exit 1
fi

# License notices: RecompStation's license, the notices for the bundled
# libraries (with the Qt version and the list of bundled libraries, written
# now) and the LGPL text. System libraries' copyright files were added by
# linuxdeploy under usr/share/doc.
NOTICES="$BUILD_DIR/notices"
rm -rf "$NOTICES"
mkdir -p "$NOTICES/licenses"
cp "$PROJECT_ROOT/LICENSE" "$NOTICES/LICENSE"
cp "$PROJECT_ROOT/packaging/licenses/"*.txt "$NOTICES/licenses/"
{
    cat "$PROJECT_ROOT/packaging/THIRD_PARTY_NOTICES.md"
    echo
    echo "## Bundled in this build"
    echo
    echo "Qt $QT_VERSION. Libraries in \`usr/lib\`:"
    echo
    (cd "$APPDIR/usr/lib" && find . -maxdepth 1 -name '*.so*' ! -type l -printf '%f\n' | sort | sed 's/^/- /')
} > "$NOTICES/THIRD_PARTY_NOTICES.md"
echo "Copyright files from system packages: $(find "$APPDIR/usr/share/doc" -name copyright 2>/dev/null | wc -l)"
# Inside the program folder too, so both packages carry them in usr/.
mkdir -p "$APPDIR/usr/share/licenses/RecompStation"
cp -a "$NOTICES/." "$APPDIR/usr/share/licenses/RecompStation/"

mkdir -p "$DIST_DIR"
APPIMAGE_FILE="RecompStation-$VERSION-x86_64.AppImage"
ZIP_NAME="RecompStation-$VERSION-linux-x86_64"
rm -f "$DIST_DIR/$APPIMAGE_FILE" "$DIST_DIR/$ZIP_NAME.zip" "$DIST_DIR/SHA256SUMS"

# 4. Portable zip: the same files, with a start script.
PORTABLE="$BUILD_DIR/$ZIP_NAME"
rm -rf "$PORTABLE"
mkdir -p "$PORTABLE"
cp -a "$APPDIR/usr" "$PORTABLE/usr"
mkdir -p "$PORTABLE/share/applications"
cp "$PROJECT_ROOT/packaging/$APP_ID.desktop" "$PORTABLE/share/applications/"
cp "$PROJECT_ROOT/packaging/RecompStation" "$PORTABLE/RecompStation"
cp "$PROJECT_ROOT/packaging/README-portable.txt" "$PORTABLE/README.txt"
cp -a "$NOTICES/." "$PORTABLE/"
(cd "$BUILD_DIR" && zip -qry "$DIST_DIR/$ZIP_NAME.zip" "$ZIP_NAME")

# 5. AppImage.
cp -a "$NOTICES/." "$APPDIR/"
(cd "$BUILD_DIR" && rm -f ./*.AppImage && \
    LDAI_OUTPUT="$APPIMAGE_FILE" OUTPUT="$APPIMAGE_FILE" "$LINUXDEPLOY" --appdir "$APPDIR" --output appimage)
mv "$BUILD_DIR/$APPIMAGE_FILE" "$DIST_DIR/$APPIMAGE_FILE"

(cd "$DIST_DIR" && sha256sum "$APPIMAGE_FILE" "$ZIP_NAME.zip" > SHA256SUMS)
echo
echo "Release files:"
ls -lh "$DIST_DIR"
