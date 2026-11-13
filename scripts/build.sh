
#!/usr/bin/env bash

set -Eeuo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

BUILD_TYPE="${1:-Debug}"

echo "======================================"
echo " RecompStation - Build"
echo "======================================"

if [[ ! -s "$PROJECT_ROOT/CMakeLists.txt" ]]; then
    echo "ERROR: CMakeLists.txt is empty or missing."
    echo "Add the project configuration before building."
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: CMake is not installed."
    exit 1
fi

GENERATOR="Unix Makefiles"

if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
fi

echo "Build type: $BUILD_TYPE"
echo "Generator: $GENERATOR"

cmake -S "$PROJECT_ROOT" \
      -B "$BUILD_DIR" \
      -G "$GENERATOR" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

cmake --build "$BUILD_DIR" --parallel

echo
echo "Build completed successfully!"
echo "Executable: $BUILD_DIR/RecompStationApp"
