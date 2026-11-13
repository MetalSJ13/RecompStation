
#!/usr/bin/env bash

set -Eeuo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "======================================"
echo "       RecompStation Setup"
echo "======================================"

echo
echo "Project directory:"
echo "$PROJECT_ROOT"

echo
read -r -p "Install development dependencies? [Y/n] " answer

if [[ ! "$answer" =~ ^[Nn]$ ]]; then
    bash "$PROJECT_ROOT/scripts/install_dependencies.sh"
fi

echo
echo "Creating project structure..."

bash "$PROJECT_ROOT/scripts/create_project.sh"

echo
echo "Checking Git configuration..."

if command -v git >/dev/null 2>&1; then
    if [[ ! -d "$PROJECT_ROOT/.git" ]]; then
        read -r -p "Initialize Git repository? [Y/n] " git_answer

        if [[ ! "$git_answer" =~ ^[Nn]$ ]]; then
            git -C "$PROJECT_ROOT" init
        fi
    else
        echo "Git repository already initialized."
    fi
fi

echo
echo "======================================"
echo " RecompStation setup complete!"
echo "======================================"

echo
echo "Next steps:"
echo "  1. Add application source code."
echo "  2. Build using scripts/build.sh"
echo "  3. Launch using scripts/run.sh"
