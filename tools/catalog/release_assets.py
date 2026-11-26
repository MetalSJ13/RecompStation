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
    python3 tools/catalog/release_assets.py --repos-file repos.txt --csv results.csv
                                                       # many candidates: one owner/name per line,
                                                       # results saved for the Candidates sheet

GitHub allows 60 requests an hour without signing in, enough for the catalog.
Set GITHUB_TOKEN to a personal access token for more; if it isn't set and the
GitHub CLI is signed in, its token (gh auth token) is used. With --csv, a run
stopped by the limit can be started again: repositories already in the file
are skipped.

Output, one block per project:
    project-id  owner/repo  tag
      W  Name-Windows-x64.zip      (W: looks like Windows, L: Linux, ·: other)
"""

import csv
import json
import os
import re
import subprocess
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


_TOKEN = None


def token() -> str:
    global _TOKEN
    if _TOKEN is None:
        _TOKEN = os.environ.get("GITHUB_TOKEN", "")
        if not _TOKEN:
            try:
                _TOKEN = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True,
                                        timeout=10).stdout.strip()
            except (OSError, subprocess.SubprocessError):
                _TOKEN = ""
    return _TOKEN


def releases(repo: str):
    request = urllib.request.Request(
        f"https://api.github.com/repos/{repo}/releases?per_page=10",
        headers={"Accept": "application/vnd.github+json", "User-Agent": "RecompStation-catalog"},
    )
    if token():
        request.add_header("Authorization", f"Bearer {token()}")
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


CSV_COLUMNS = ["repository", "tag", "published", "linux", "windows", "other",
               "newer_prerelease", "error"]


def summarize(repo: str) -> dict:
    """One CSV row: the latest regular release's downloads, sorted by system."""
    row = dict.fromkeys(CSV_COLUMNS, "")
    row["repository"] = repo
    try:
        found = releases(repo)
    except urllib.error.HTTPError as error:
        row["error"] = f"{error.code} {error.reason}"
        if error.code == 403:
            raise RuntimeError("GitHub's limit was reached; set GITHUB_TOKEN or run again in an hour.")
        return row
    except OSError as error:
        row["error"] = str(error)
        return row
    regular = [r for r in found if not r.get("prerelease") and not r.get("draft")]
    if not found:
        row["error"] = "no releases"
        return row
    if not regular:
        row["error"] = "pre-releases only"
    latest = regular[0] if regular else found[0]
    newest = found[0]
    if regular and newest is not latest and newest.get("prerelease"):
        row["newer_prerelease"] = newest.get("tag_name", "")
    row["tag"] = latest.get("tag_name", "") + ("" if regular else " (pre-release)")
    row["published"] = latest.get("published_at", "")[:10]
    names = {"L": [], "W": [], "·": []}
    for asset in latest.get("assets", []):
        names[kind(asset["name"])].append(asset["name"])
    row["linux"], row["windows"], row["other"] = (" | ".join(names[k]) for k in ("L", "W", "·"))
    return row


def run_csv(repos: list, path: str) -> int:
    done = set()
    if os.path.exists(path):
        with open(path, newline="", encoding="utf-8") as file:
            done = {r["repository"].lower() for r in csv.DictReader(file) if not r.get("error", "").startswith("403")}
    write_header = not os.path.exists(path) or os.path.getsize(path) == 0
    with open(path, "a", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=CSV_COLUMNS)
        if write_header:
            writer.writeheader()
        todo = [r for r in repos if r.lower() not in done]
        for number, repo in enumerate(todo, 1):
            try:
                row = summarize(repo)
            except RuntimeError as error:
                print(error)
                print(f"Saved {number - 1} of {len(todo)}; run the same command again to go on.")
                return 1
            writer.writerow(row)
            file.flush()
            print(f"[{number}/{len(todo)}] {repo}: {row['tag'] or row['error']}"
                  f"  L:{len(row['linux'].split(' | ')) if row['linux'] else 0}"
                  f"  W:{len(row['windows'].split(' | ')) if row['windows'] else 0}")
    print(f"Saved to {path}")
    return 0


def read_repos(path: str) -> list:
    repos = []
    with open(path, encoding="utf-8") as file:
        for line in file:
            line = line.strip().replace("https://github.com/", "").strip("/")
            if re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9-]*/[A-Za-z0-9._-]+", line) and line not in repos:
                repos.append(line)
    return repos


def main() -> int:
    arguments = sys.argv[1:]
    if arguments[:1] == ["--repos-file"]:
        if len(arguments) < 2:
            print("--repos-file needs a file with one owner/name per line.")
            return 2
        repos = read_repos(arguments[1])
        if arguments[2:3] == ["--csv"] and len(arguments) > 3:
            return run_csv(repos, arguments[3])
        for repo in repos:
            if not show("candidate", repo, prereleases=False):
                return 1
        return 0
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
