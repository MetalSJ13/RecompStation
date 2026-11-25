#!/usr/bin/env python3
"""List the downloads in each catalog project's latest official release.

Used when checking the catalog: which projects publish Windows downloads,
whether a project renamed its files, and so on. Reads data/projects.json and
asks GitHub's public API for each project's latest release (the latest
pre-release for projects marked includePrereleases).

    python3 tools/catalog/release_assets.py            # every project
    python3 tools/catalog/release_assets.py banjo      # ids containing "banjo"
    python3 tools/catalog/release_assets.py --repo owner/name [owner/name ...]
                                                       # repositories not in the catalog yet

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
    # Linux first: "Twin-Linux.tar.gz" contains "win-".
    if LINUX.search(name) and not name.lower().endswith((".exe", ".msi")):
        return "L"
    if WINDOWS.search(name + " ") or WINDOWS.search(name):
        return "W"
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


def show(label: str, repo: str, prereleases: bool) -> bool:
    try:
        found = releases(repo)
    except urllib.error.HTTPError as error:
        print(f"{label}  {repo}  ERROR {error.code} {error.reason}")
        if error.code == 403:
            print("  GitHub's limit was reached; set GITHUB_TOKEN or try again in an hour.")
            return False
        return True
    except OSError as error:
        print(f"{label}  {repo}  ERROR {error}")
        return True
    if not prereleases:
        found = [r for r in found if not r.get("prerelease")] or found
    if not found:
        print(f"{label}  {repo}  no releases")
        return True
    latest = found[0]
    tag = latest.get("tag_name", "?") + (" (pre-release)" if latest.get("prerelease") else "")
    print(f"{label}  {repo}  {tag}  {latest.get('published_at', '')[:10]}")
    for asset in latest.get("assets", []):
        print(f"  {kind(asset['name'])}  {asset['name']}")
    return True


def main() -> int:
    arguments = sys.argv[1:]
    if arguments[:1] == ["--repo"]:
        for repo in arguments[1:]:
            if not show("candidate", repo, prereleases=True):
                return 1
        return 0
    wanted = arguments[0] if arguments else ""
    catalog = json.loads((ROOT / "data" / "projects.json").read_text())
    for project in catalog["projects"]:
        if wanted and wanted not in project["id"]:
            continue
        repo = f'{project["repositoryOwner"]}/{project["repositoryName"]}'
        if not show(project["id"], repo, bool(project.get("includePrereleases"))):
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
