#!/usr/bin/env bash
# Removes the packages ./scripts/install_dependencies.sh added, for when you
# are finished building RecompStation. Arch (pacman) and Debian/Ubuntu (apt).
#
# What is removed, and what is always kept:
#  - Only packages recorded by install_dependencies.sh: the ones that weren't
#    on the system before it ran, and what they pulled in. Anything you had
#    already is never touched.
#  - A package is kept if anything else installed still needs it (your
#    desktop, other programs, anything you installed later), or, on Debian,
#    if it is marked Essential or has Required/Important priority.
#  - The package manager then works out the removal without changing
#    anything; if it would remove a single package outside the list, nothing
#    is removed.
#  - Nothing happens until you confirm. Use --dry-run to only see the plan.
#
# RecompStation's own settings, library and launchers (in
# ~/.local/share/RecompStation) and your game files are not touched.

set -Eeuo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/recompstation-dev"
RECORD="$STATE_DIR/installed-packages"
DRY_RUN=false

for arg in "$@"; do
    case "$arg" in
        --dry-run|-n) DRY_RUN=true ;;
        -h|--help)
            sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'
            echo
            echo "Usage: ./scripts/uninstall_dependencies.sh [--dry-run]"
            exit 0 ;;
        *) echo "ERROR: unknown argument: $arg"; exit 2 ;;
    esac
done

export LC_ALL=C

if command -v pacman >/dev/null 2>&1; then
    FAMILY=arch
elif command -v apt-get >/dev/null 2>&1; then
    FAMILY=debian
else
    echo "ERROR: this script supports Arch (pacman) and Debian/Ubuntu (apt)."
    exit 1
fi

echo "======================================"
echo " RecompStation - Remove dependencies"
echo "======================================"
echo

if [[ ! -s "$RECORD" ]]; then
    echo "No record of packages added for RecompStation ($RECORD)."
    echo
    echo "The record is written by ./scripts/install_dependencies.sh. Without it there is"
    echo "no safe way to tell what was added for RecompStation and what you had before,"
    echo "so nothing is removed. If you installed the dependencies by hand, remove them"
    echo "the same way."
    exit 0
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# ---- Which recorded packages are still installed --------------------------
if [[ "$FAMILY" == arch ]]; then
    pacman -Qq | sort -u > "$WORK/installed"
else
    dpkg-query -W -f='${Package}\t${db:Status-Abbrev}\n' | awk '$2 ~ /^ii/ {print $1}' | sort -u > "$WORK/installed"
fi
sort -u "$RECORD" | comm -12 - "$WORK/installed" > "$WORK/candidates"
if [[ ! -s "$WORK/candidates" ]]; then
    echo "None of the recorded packages are installed any more. Nothing to do."
    rm -f "$RECORD"
    exit 0
fi
echo "Recorded and still installed: $(wc -l < "$WORK/candidates") package(s)."

# ---- What depends on each candidate ----------------------------------------
# One line per "candidate needed-by" pair, for installed packages only.
: > "$WORK/needed-by"
: > "$WORK/protected"
echo "Checking what else on the system needs them..."
while read -r package; do
    if [[ "$FAMILY" == arch ]]; then
        pacman -Qi "$package" | awk -v p="$package" '
            /^Required By/ { on = 1; sub(/^Required By *: */, "") }
            on && /^[A-Z][A-Za-z ]+:/ && !/^Required By/ { on = 0 }
            on { for (i = 1; i <= NF; i++) if ($i != "None") print p, $i }
        ' >> "$WORK/needed-by"
    else
        apt-cache rdepends --installed --no-suggests --no-recommends --no-conflicts \
                --no-breaks --no-replaces --no-enhances "$package" 2>/dev/null \
            | awk -v p="$package" 'NR > 2 { gsub(/[ |]/, ""); if ($0 != "" && $0 != p) print p, $0 }' \
            >> "$WORK/needed-by"
        # Essential, Required and Important packages are part of the base system.
        info="$(dpkg-query -W -f='${Essential} ${Priority}' "$package" 2>/dev/null || true)"
        if [[ "$info" == yes* || "$info" == *required || "$info" == *important ]]; then
            echo "$package" >> "$WORK/protected"
        fi
    fi
done < "$WORK/candidates"
sort -u -o "$WORK/needed-by" "$WORK/needed-by"

# ---- Keep anything still needed, repeating until nothing changes -----------
# Keeping one package keeps everything it needs too.
cp "$WORK/candidates" "$WORK/remove"
sort -u "$WORK/protected" > "$WORK/keep"
while :; do
    comm -23 "$WORK/candidates" "$WORK/keep" > "$WORK/remove"
    # A candidate is still needed when something not being removed needs it.
    awk 'NR == FNR { removing[$1] = 1; next } !($2 in removing) { print $1 }' \
        "$WORK/remove" "$WORK/needed-by" | sort -u > "$WORK/still-needed"
    new_keep="$(sort -u "$WORK/keep" "$WORK/still-needed")"
    if [[ "$new_keep" == "$(cat "$WORK/keep")" ]]; then
        break
    fi
    printf '%s\n' "$new_keep" | sed '/^$/d' > "$WORK/keep"
done
sed -i '/^$/d' "$WORK/keep"

if [[ -s "$WORK/keep" ]]; then
    echo
    echo "Kept, because your system or other programs still need them:"
    sed 's/^/  /' "$WORK/keep"
fi
if [[ ! -s "$WORK/remove" ]]; then
    echo
    echo "Everything recorded is still needed by something else. Nothing to remove."
    exit 0
fi

mapfile -t REMOVE < "$WORK/remove"

# ---- Let the package manager check the plan without changing anything -----
echo
echo "Checking the removal with the package manager (nothing is changed yet)..."
if [[ "$FAMILY" == arch ]]; then
    if ! pacman -R --print --print-format '%n' "${REMOVE[@]}" > "$WORK/plan" 2> "$WORK/plan-error"; then
        echo "ERROR: pacman refused the removal, so nothing was removed:"
        sed 's/^/  /' "$WORK/plan-error"
        exit 1
    fi
else
    apt-get -s remove "${REMOVE[@]}" > "$WORK/simulation" 2>&1 || {
        echo "ERROR: apt refused the removal, so nothing was removed:"
        sed 's/^/  /' "$WORK/simulation"
        exit 1
    }
    awk '$1 == "Remv" { print $2 }' "$WORK/simulation" | sed 's/:.*//' | sort -u > "$WORK/plan"
fi
sort -u -o "$WORK/plan" "$WORK/plan"
comm -23 "$WORK/plan" "$WORK/remove" > "$WORK/unexpected"
if [[ -s "$WORK/unexpected" ]]; then
    echo "ERROR: the package manager would also remove packages that weren't added for"
    echo "RecompStation. To keep your system safe, nothing was removed:"
    sed 's/^/  /' "$WORK/unexpected"
    exit 1
fi

echo
echo "These $(wc -l < "$WORK/remove") package(s) were added for RecompStation and nothing else needs them:"
column -c "${COLUMNS:-100}" "$WORK/remove" 2>/dev/null | sed 's/^/  /' || sed 's/^/  /' "$WORK/remove"

if $DRY_RUN; then
    echo
    echo "Dry run: nothing was removed."
    exit 0
fi

echo
read -r -p "Remove them? [y/N] " answer
if [[ ! "$answer" =~ ^[Yy]$ ]]; then
    echo "Nothing was removed."
    exit 0
fi

if [[ "$FAMILY" == arch ]]; then
    sudo pacman -R "${REMOVE[@]}"
else
    sudo apt-get remove "${REMOVE[@]}"
fi

# Forget what is gone; keep the record of anything kept.
cp "$WORK/keep" "$RECORD"
[[ -s "$RECORD" ]] || rm -f "$RECORD"
echo
echo "Done."

# ---- Build folders (optional) ----------------------------------------------
FOLDERS=()
for folder in build build-release build-tools dist; do
    [[ -d "$PROJECT_ROOT/$folder" ]] && FOLDERS+=("$PROJECT_ROOT/$folder")
done
if (( ${#FOLDERS[@]} > 0 )); then
    echo
    echo "Build folders in the project:"
    printf '  %s\n' "${FOLDERS[@]}"
    read -r -p "Delete them as well? [y/N] " answer
    if [[ "$answer" =~ ^[Yy]$ ]]; then
        rm -rf -- "${FOLDERS[@]}"
        echo "Build folders deleted."
    fi
fi

echo
echo "RecompStation's own data (~/.local/share/RecompStation) and your games were not touched."
