#!/usr/bin/env python3
"""Credit each game's launchers in COMPATIBILITY.md.

Every game line ("- [x] Banjo-Kazooie") gets the catalog's launchers for that
game after it, each linked to the project's repository, so the people who made
them are credited and easy to find:

    - [x] Banjo-Kazooie: [Banjo: Recompiled](https://github.com/BanjoRecomp/BanjoRecomp)

Only what follows the title is rewritten, so running it again after the
catalog changes keeps the lines current. Which games are ticked stays as it is:
that comes from testing, not from the catalog.

    python3 tools/catalog/compatibility_credits.py            # rewrite the file
    python3 tools/catalog/compatibility_credits.py --check    # exit 1 if it is out of date
"""

import argparse
import json
import re
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LINE = re.compile(r"^(- \[[ xX]\] )(.+?)(?:: \[.*)?$")


def normalized(title: str) -> str:
    text = unicodedata.normalize("NFKD", title).encode("ascii", "ignore").decode().lower()
    text = re.sub(r"\bversion\b", "", text)
    return re.sub(r"[^a-z0-9]+", "", text)


def credits_by_title(catalog: dict) -> dict:
    games: dict[str, list[tuple[str, str]]] = {}
    for project in catalog["projects"]:
        url = f"https://github.com/{project['repositoryOwner']}/{project['repositoryName']}"
        entry = (project["name"], url)
        for title in project["gameTitles"]:
            listed = games.setdefault(normalized(title), [])
            if entry not in listed:
                listed.append(entry)
    return games


def rewritten(text: str, games: dict) -> str:
    out = []
    for line in text.splitlines():
        match = LINE.match(line)
        if match:
            box, title = match.group(1), match.group(2).strip()
            launchers = games.get(normalized(title), [])
            line = box + title
            if launchers:
                line += ": " + " · ".join(f"[{name}]({url})" for name, url in launchers)
        out.append(line)
    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="report whether the file is current, change nothing")
    args = parser.parse_args()
    catalog = json.loads((ROOT / "data" / "projects.json").read_text(encoding="utf-8"))
    target = ROOT / "COMPATIBILITY.md"
    current = target.read_text(encoding="utf-8")
    updated = rewritten(current, credits_by_title(catalog))
    missing = [m.group(2) for m in map(LINE.match, updated.splitlines()) if m and ": [" not in m.string]
    if args.check:
        if updated != current:
            print("COMPATIBILITY.md is out of date: run tools/catalog/compatibility_credits.py")
            return 1
        return 0
    target.write_text(updated, encoding="utf-8")
    for title in missing:
        print(f"no launcher in the catalog for: {title}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
