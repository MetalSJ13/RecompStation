# RecompStation — Project Context

Last updated: 2026-11-25. The detailed status, open checks and roadmap live in [`DEVELOPMENT.md`](DEVELOPMENT.md); the product and UI direction in [`docs/UI_DESIGN.md`](docs/UI_DESIGN.md); the record of earlier work in [`docs/development-history.md`](docs/development-history.md).

## Objective

RecompStation is a native Linux desktop app for browsing the user's own game collection and playing it through native recompilation and source ports. It manages the game library and the ports' launchers (download, install, play, update, uninstall). It is not an emulator and not a storefront, and it never supplies, hosts or redistributes game files: players use games they own.

## Technology

- C++20, Qt 6.5+ (Qt Quick / QML, Qt Network, Qt SQL / SQLite, Qt SVG, Qt Shader Tools), SDL2 for controllers and interface sounds, libarchive, xxHash
- CMake and Ninja; a shared `recompstation_core` static library linked by the app and the tests; ccache and mold/lld when available
- 25 CTest suites, run in parallel by `./scripts/build.sh --test`, and on GitHub Actions for every release
- Developed on Arch Linux (Garuda) with KDE Plasma on Wayland; release files tested on Debian 13 with XFCE
- License: GPL-3.0

## Releases

- **v0.1.0**, the first public release, published 2026-11-24: `RecompStation-<version>-x86_64.AppImage` and `RecompStation-<version>-linux-x86_64.zip`, both with Qt 6.10 included, plus `SHA256SUMS`, for 64-bit Linux (glibc 2.35 or newer).
- Built by `.github/workflows/release.yml` on Ubuntu 22.04 with `scripts/package.sh` (linuxdeploy and its Qt plugin). Pushing a tag `v<version>` that matches `CMakeLists.txt` drafts a release; the user reviews and publishes it.
- Third-party notices (Qt under LGPLv3, SDL2, libarchive, xxHash, and the bundled libraries) ship in each download.
- Next: Windows `.exe` builds once the Linux files have proven themselves; ARM later.

## Branches and workflow

- `main` holds the released code and later approved work; `ui/interface-redesign` is the development branch.
- Each change: mocked up on the design canvas when it affects the UI, built on a short-lived branch from `ui/interface-redesign`, built and checked by the user (`./scripts/build.sh --test`, `./scripts/run.sh`), recorded in `DEVELOPMENT.md`, then fast-forward merged. `main` is updated only when the user approves. The user deletes old branches on GitHub.
- Business logic in C++, presentation and navigation in QML; regression tests for backend changes; never delete the user's build directory, app data, games or saves.

## What the app does today

**Library.** One library folder with a subfolder per system. Twelve systems, ordered by maker and generation: Game Boy, Game Boy Color, Game Boy Advance, SNES, N64, GameCube, Wii, Wii U, PS1, PS2, Xbox and Xbox 360. Games are found by recursive scanning and filesystem monitoring. Four views (Shelves, Grid, List, Carousel); covers keep their system's box shape. The header has a single system picker: LB/RB step through systems that have games, and the list groups them by maker.

**Adding games.** First-time setup (folder choice, Add games, covers and info) and Settings › Library › Add games, plus drag-and-drop. Systems are identified from file contents, not just extensions: Game Boy headers (GB vs GBC), disc headers (GameCube, Wii, Xbox, Xbox 360), `SYSTEM.CNF` (PS1 vs PS2), CHD metadata (CD vs DVD). Cartridge ROMs are unpacked from `.zip`/`.7z`. Files are copied (or moved) and never overwritten.

**Metadata.** LaunchBox's offline catalog (first by default) and TheGamesDB (user's own key; never bundled). Matching is automatic after scans and only accepts a clear match; region and language tags are removed from file names first, including several regions such as "(Japan, USA)". Covers and screenshots come from either provider.

**Launchers.** A curated catalog (`data/projects.json`) of 42 projects: 30 N64, 4 GameCube, and 2 each for PS1, Xbox, Xbox 360 and SNES. Only official native Linux release assets are used (AppImage, zip, tar.gz), found through the GitHub API and filtered by CPU architecture; unofficial ports are not used, and Flatpak-only projects are an open question. Each project names the game file it needs (`romFile`).

**Launcher lifecycle.** Downloads are checked against the SHA-256 GitHub publishes for each release file. Each install records its version; Game Details offers "Update to <version>" when a newer release is out. Updates unpack beside the install, swap folders, carry the player's files over and roll back on any failure; an interrupted update is finished or undone at startup. Saves a launcher keeps in its install folder are set aside on uninstall and restored on reinstall. When a release has nothing for this system, Game Details says what it offers instead.

**Seamless play.** Catalog `launch` recipes let Play hand the library's game to the launcher:
- `n64recomp` (22 projects): the ROM is checked against the launcher's own XXH3 checksum and copied to `~/.config/<program_id>/<game_id>.z64`.
- `harbourmasters` (Shipwright, Ghostship, PaperBoat): saves are kept outside the install folder via `SHIP_HOME`.
- `romfolder` (Pokémon Snap, Snowboard Kids): the ROM is placed in the launcher's data folder.
- `discargument` (Luigi's Mansion) and `fileargument` (Super Metroid, Mega Man X): the game file itself is passed as an argument.
- `manual`: the game page shows the game file and copies its path.
RecompStation never creates a launcher's settings files; it only changes the window-mode key in a `graphics.json` the launcher wrote itself.

**Settings.** Appearance (four themes, text size, reduced motion, interface sounds), Display, Background (theme gradient, shader waves, own image), Library, Metadata, Controller (layout drawing, button test, remapping) and Application (storage, folders, detailed logging, reset).

**Logs.** `runtime.log` opens with a session header (version and commit, system, desktop, display, settings, library and launcher summary) and records one line per event, with the home folder written as `~`. Each launcher's output goes to `logs/launchers/<project>.log`; when a launcher crashes, its last 40 lines are copied into `runtime.log`.

**Identity.** An original logo (pixel d-pad) in `assets/logo/`, used in the header, first-time setup, the README, the desktop entry (`io.github.MetalSJ13.RecompStation`) and as the window icon. The README opens with the no-game-files notice.

## Current focus and next steps

- Waiting for the user's test: Super Metroid and Mega Man X open their own launcher with the ROM already selected, so settings and mods stay reachable (`fix/snes-launcher-settings`).
- Validating supported games from the release files, and the remaining launchers in the "RecompStation launcher test list" spreadsheet.
- Desktop-environment checks on VMs of mainstream distributions (GNOME, Cinnamon, MATE) after Debian 13 with XFCE.
- Catalog: add Final Fantasy Tactics Advance (GBA), Budokai Tenkaichi 3 (PS2), Goemon's Great Adventure (N64) and Gen1Recomp (needs a field for projects covering several systems). Donkey Kong Country waits for an official Linux release.
- Open decisions: projects that compile the game on the player's machine at first start; Flatpak-only projects.
- In-app updates are built (`feature/app-updates`); they can only be validated once a version newer than 0.1.0 is released.
- Later: Fix match in Game Details, request retry and backoff, multi-display placement, Windows builds.

## Key decisions

- Controller-first, library-first, fullscreen by default; RecompStation keeps its own visual identity (RecompLauncher was only a reference).
- Metadata is matched only when one result clearly fits.
- Never supply game files; never overwrite or delete the user's games; uninstalling a launcher keeps ROMs and saves.
- Never create a launcher's settings files; change only single documented keys in files the launcher wrote.
- Official releases only: no unofficial ports and no forks built by this project (2026-11-25).
- No settings without real behaviour; no "Add to application menu" option for now (2026-11-24).
- Screenshots committed to the repository have their metadata removed.
