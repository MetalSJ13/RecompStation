#!/usr/bin/env bash
# First-time setup on a new computer: installs the build dependencies,
# builds RecompStation with its tests, and offers to start it.

set -Eeuo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "======================================"
echo "       RecompStation Setup"
echo "======================================"
echo
echo "Project folder: $PROJECT_ROOT"
echo

read -r -p "Install the build dependencies (needs sudo)? [Y/n] " answer
if [[ ! "$answer" =~ ^[Nn]$ ]]; then
    bash "$PROJECT_ROOT/scripts/install_dependencies.sh"
fi

echo
read -r -p "Build RecompStation and run its tests now? [Y/n] " answer
if [[ ! "$answer" =~ ^[Nn]$ ]]; then
    bash "$PROJECT_ROOT/scripts/build.sh" --test
    echo
    read -r -p "Start RecompStation? [Y/n] " answer
    if [[ ! "$answer" =~ ^[Nn]$ ]]; then
        exec bash "$PROJECT_ROOT/scripts/run.sh"
    fi
fi

echo
echo "Next time:"
echo "  ./scripts/build.sh --test   build and run the tests"
echo "  ./scripts/run.sh            start RecompStation"
