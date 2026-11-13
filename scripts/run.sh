
#!/usr/bin/env bash

set -Eeuo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXECUTABLE="$PROJECT_ROOT/build/RecompStationApp"

if [[ ! -x "$EXECUTABLE" ]]; then
    echo "RecompStation executable not found."
    echo "Building the application first..."

    bash "$PROJECT_ROOT/scripts/build.sh"
fi

echo "Starting RecompStation..."

exec "$EXECUTABLE" "$@"
