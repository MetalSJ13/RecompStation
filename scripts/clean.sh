
#!/usr/bin/env bash

set -Eeuo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

echo "RecompStation build cleanup"

if [[ -d "$BUILD_DIR" ]]; then
    rm -rf -- "$BUILD_DIR"
    echo "Build directory removed."
else
    echo "No build directory found."
fi

echo "Cleanup complete."
