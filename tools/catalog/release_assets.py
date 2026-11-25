#!/usr/bin/env python3
"""List the downloads in each catalog project's latest official release.

Used when checking the catalog: which projects publish Windows downloads,
whether a project renamed its files, and so on. Reads data/projects.json and
asks GitHub's public API for each project's latest release (the latest
pre-release for projects marked includePrereleases).

    python3 tools/catalog/release_assets.py            # every project
    python3 tools/catalog/release_assets.py banjo      # ids containing "banjo"

GitHub allows 60 requests an hour without signing in, enough for one run.
Set GITHUB_TOKEN to a personal access token for more.

Output, one block per project:
    project-id  owner/repo  tag
      W  Name-Windows-x64.zip      (W: looks like Windows, L: Linux, ·: other)
"""

import json
import os
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WINDOWS = re.compile(r"(win(dows)?(32|64)?[^a-z]|\.exe$|\.msi$|msvc)", re.I)
MAC = re.compile(r"(mac|darwin|osx|\.dmg$|apple)", re.I)
LINUX = re.compile(r"(linux|\.appimage$)", re.I)


def kind(name: str) -> str:
    if MAC.search(name):
        return "·"
    if WINDOWS.search(name + " "):
        return "W"
    if LINUX.search(name):
        return "L"
    return "·"


def releases(repo: str):
    request = urllib.request.Request(
        f"https://api.github.com/repos/{repo}/releases?per_page=10",
        headers={"Accept": "application/vnd.github+json", "User-Agent": "RecompStation-catalog"},
    )
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        request.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def main() -> int:
    wanted = sys.argv[1] if len(sys.argv) > 1 else ""
    catalog = json.loads((ROOT / "data" / "projects.json").read_text())
    for project in catalog["projects"]:
        if wanted and wanted not in project["id"]:
            continue
        repo = f'{project["repositoryOwner"]}/{project["repositoryName"]}'
        try:
            found = releases(repo)
        except urllib.error.HTTPError as error:
            print(f'{project["id"]}  {repo}  ERROR {error.code} {error.reason}')
            if error.code == 403:
                print("  GitHub's limit was reached; set GITHUB_TOKEN or try again in an hour.")
                return 1
            continue
        except OSError as error:
            print(f'{project["id"]}  {repo}  ERROR {error}')
            continue
        if not project.get("includePrereleases"):
            found = [r for r in found if not r.get("prerelease")] or found
        if not found:
            print(f'{project["id"]}  {repo}  no releases')
            continue
        latest = found[0]
        tag = latest.get("tag_name", "?") + (" (pre-release)" if latest.get("prerelease") else "")
        print(f'{project["id"]}  {repo}  {tag}')
        for asset in latest.get("assets", []):
            print(f'  {kind(asset["name"])}  {asset["name"]}')
    return 0


if __name__ == "__main__":
    sys.exit(main())
