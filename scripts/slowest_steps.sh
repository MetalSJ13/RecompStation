#!/usr/bin/env bash
# Rebuilds and lists the slowest steps of that build, from Ninja's log.
#
#   touch qml/Main.qml && ./scripts/slowest_steps.sh
#   ./scripts/slowest_steps.sh 20          # show 20 steps instead of 12
#
# Change one file (or touch it), then run this to see what the change costs.
# Needs a build directory already configured by ./scripts/build.sh (Ninja).
set -euo pipefail

COUNT="${1:-12}"
BUILD_DIR="${BUILD_DIR:-build}"
LOG="${BUILD_DIR}/.ninja_log"

if [[ ! -f "${LOG}" ]]; then
    echo "No ${LOG} yet: run ./scripts/build.sh once first." >&2
    exit 1
fi

BEFORE=$(wc -l < "${LOG}")
SECONDS=0
cmake --build "${BUILD_DIR}"
WALL=${SECONDS}

python3 - "${LOG}" "${BEFORE}" "${COUNT}" "${WALL}" <<'PY'
import sys
path, before, count, wall = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
steps = []
with open(path) as log:
    for number, line in enumerate(log, start=1):
        # Ninja appends one line per finished step; keep this build's lines.
        if number <= before or line.startswith("#"):
            continue
        parts = line.rstrip("\n").split("\t")
        if len(parts) >= 4:
            steps.append(((int(parts[1]) - int(parts[0])) / 1000, parts[3]))
print()
if not steps:
    print("Nothing was rebuilt.")
    sys.exit(0)
work = sum(seconds for seconds, _ in steps)
print(f"This build: {len(steps)} steps, {wall} s wall clock, {work:.1f} s of work in total")
print()
for seconds, output in sorted(steps, reverse=True)[:count]:
    print(f"{seconds:7.2f} s  {output}")
PY
