
#!/usr/bin/env bash

set -Eeuo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Creating RecompStation project structure..."

DIRECTORIES=(
    "scripts"

    "src/core"
    "src/database"
    "src/launcher"
    "src/metadata"
    "src/settings"

    "qml/components"
    "qml/pages"
    "qml/themes"

    "resources/icons"
    "resources/images"

    "data/launchers"

    "docs"
)

for directory in "${DIRECTORIES[@]}"; do
    mkdir -p "$PROJECT_ROOT/$directory"
    echo "[OK] $directory"
done

FILES=(
    "CMakeLists.txt"
    "README.md"
    ".gitignore"
)

for file in "${FILES[@]}"; do
    if [[ ! -e "$PROJECT_ROOT/$file" ]]; then
        touch "$PROJECT_ROOT/$file"
        echo "[CREATED] $file"
    else
        echo "[EXISTS] $file"
    fi
done

echo
echo "Project structure created successfully!"
