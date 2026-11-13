#!/usr/bin/env bash

set -Eeuo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
BUILD_TYPE="Debug"
RUN_TESTS="false"

usage() {
    cat <<'EOF'
Usage: ./scripts/build.sh [Debug|Release|RelWithDebInfo] [--test]

Options:
  Debug             Build with debug symbols (default).
  Release           Build an optimized release build.
  RelWithDebInfo    Build optimized with debug symbols.
  --test            Run the CTest regression suite after building.
  -h, --help        Show this help.

Environment:
  BUILD_DIR         Override the build directory.
  BUILD_TESTING     Set to ON or OFF (default: ON).
EOF
}

for arg in "$@"; do
    case "$arg" in
        Debug|Release|RelWithDebInfo)
            BUILD_TYPE="$arg"
            ;;
        --test)
            RUN_TESTS="true"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: Unknown argument: $arg"
            usage
            exit 2
            ;;
    esac
done

if [[ ! -s "$PROJECT_ROOT/CMakeLists.txt" ]]; then
    echo "ERROR: CMakeLists.txt is empty or missing."
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: CMake is not installed."
    exit 1
fi

BUILD_TESTING="${BUILD_TESTING:-ON}"
if [[ "$RUN_TESTS" == "true" ]]; then
    BUILD_TESTING="ON"
fi

# Reuse the generator already configured in this build directory.
# This avoids CMake generator mismatch errors when switching build tools.
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    GENERATOR="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "$BUILD_DIR/CMakeCache.txt" | head -n 1)"
else
    GENERATOR="Unix Makefiles"
    if command -v ninja >/dev/null 2>&1; then
        GENERATOR="Ninja"
    fi
fi

echo "======================================"
echo " RecompStation - Build"
echo "======================================"
echo "Build type:    $BUILD_TYPE"
echo "Build dir:     $BUILD_DIR"
echo "Generator:     $GENERATOR"
echo "Build tests:   $BUILD_TESTING"
echo

cmake -S "$PROJECT_ROOT" \
      -B "$BUILD_DIR" \
      -G "$GENERATOR" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DBUILD_TESTING="$BUILD_TESTING"

cmake --build "$BUILD_DIR" --parallel

if [[ "$RUN_TESTS" == "true" ]]; then
    echo
    echo "Running scanner regression tests..."
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

echo
echo "Build completed successfully!"
echo "Executable: $BUILD_DIR/RecompStationApp"
