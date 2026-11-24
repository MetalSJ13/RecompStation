# RecompStation — Project Context

Last updated: 2026-11-24. The detailed status, open checks and roadmap live in [`DEVELOPMENT.md`](DEVELOPMENT.md); the product and UI direction in [`docs/UI_DESIGN.md`](docs/UI_DESIGN.md); the record of earlier work in [`docs/development-history.md`](docs/development-history.md).

## Objective

RecompStation is a native Linux desktop app for browsing the user's own game collection and playing it through native recompilation and source ports. It manages the game library and the ports' launchers (download, install, play, update, uninstall). It is not an emulator and not a storefront, and it never supplies, hosts or redistributes game files: players use games they own.

## Technology

- C++20, Qt 6.5+ (Qt Quick / QML, Qt Network, Qt SQL / SQLite, Qt SVG, Qt Shader Tools), SDL2 for controllers, libarchive, xxHash
- CMake and Ninja; a shared `recompstation_core` static library linked by the app and the tests; ccache and mold/lld when available
- 22 CTest suites, run in parallel by `./scripts/build.sh --test`
- Developed on Arch Linux (Garuda) with KDE Plasma on Wayland
- License: GPL-3.0

## Branches and workflow

- `main` is up to date with all work through 2026-11-24 (merged with the user's approval). Old feature branches have been removed; their work is in `main`.
- `ui/interface-redesign` is the development branch. Each change is first mocked up on the design canvas when it affects the UI, then built on a short-lived branch from `ui/interface-redesign`, built and checked by the user (`./scripts/build.sh --test`, `./scripts/run.sh`), recorded in `DEVELOPMENT.md`, and fast-forward merged. `main` is updated only when the user approves.
- Business logic in C++, presentation and navigation in QML; regression tests for backend changes; never delete the user's build directory, app data, games or saves.

## What the app does today

**Library.** One library folder with a subfolder per system. Twelve systems, ordered by maker and generation: Game Boy, Game Boy Color, Game Boy Advance, SNES, N64, GameCube, Wii, Wii U, PS1, PS2, Xbox and Xbox 360. Games are found by recursive scanning and filesystem monitoring. Four views (Shelves, Grid, List, Carousel); covers keep their system's box shape. The header has a single system picker: LB/RB step through systems that have games, and the list groups them by maker.

**Adding games.** First-time setup (folder choice, Add games, covers and info) and Settings › Library › Add games, plus drag-and-drop. Systems are identified from file contents, not just extensions: Game Boy headers (GB vs GBC), disc headers (GameCube, Wii, Xbox, Xbox 360), `SYSTEM.CNF` (PS1 vs PS2), CHD metadata (CD vs DVD). Cartridge ROMs are unpacked from `.zip`/`.7z`. Files are copied (or moved) and never overwritten.

**Metadata.** LaunchBox's offline catalog (first by default) and TheGamesDB (user's own key). Matching is automatic after scans and only accepts a clear match; covers and screenshots come from either provider.

**Launchers.** A curated catalog (`data/projects.json`) of 42 projects: 30 N64, 4 GameCube, and 2 each for PS1, Xbox, Xbox 360 and SNES. Only native Linux release assets are used (AppImage, zip, tar.gz; Flatpak is out of scope), found through the GitHub API and filtered by CPU architecture.

**Seamless play.** Catalog `launch` recipes let Play hand the library's game to the launcher:
- `n64recomp` (22 projects): the ROM is checked against the launcher's own XXH3 checksum and copied to `~/.config/<program_id>/<game_id>.z64`.
- `harbourmasters` (Shipwright, Ghostship, PaperBoat): saves are kept outside the install folder via `SHIP_HOME`.
- `manual`: the game page shows the game file and copies its path.
RecompStation never creates a launcher's settings files; it only changes the window-mode key in a `graphics.json` the launcher wrote itself.

**Settings.** Appearance (four themes), Display, Background (theme gradient, shader waves, own image), Library, Metadata, Controller (layout drawing, button test, remapping) and Application (storage, folders, detailed logging, reset).

**Logs.** `runtime.log` opens with a session header (version and commit, system, desktop, display, settings, library and launcher summary) and records one line per event, with the home folder written as `~`. Each launcher's output goes to `logs/launchers/<project>.log`; when a launcher crashes, its last 40 lines are copied into `runtime.log`.

**Identity.** An original logo (pixel d-pad) in `assets/logo/`, used in the header, first-time setup, the README and as the window icon. The README opens with the no-game-files notice.

## Current focus and next steps

- The user is testing the new catalog entries: batch 1 (PS1, Xbox, Xbox 360; Shadow the Hedgehog confirmed) and batch 2 (SNES and N64; Bomberman 64 and Bomberman Hero confirmed).
- Ready to add: Final Fantasy Tactics Advance (GBA), Budokai Tenkaichi 3 (PS2), Goemon's Great Adventure (N64) and Gen1Recomp (Pokémon on GB/GBC/GBA, which needs a catalog field for projects covering several systems).
- Open decision: whether to list projects that compile the game on the player's machine at first start (The Wind Waker HD for Wii U, God of War, Boktai, Aladdin, Gundam Wing).
- Later: launch recipes for `manual` projects, save protection for launchers that keep saves in their install folder, launcher version tracking and updates, archive-extraction safety review, desktop-environment checks, splitting `Main.qml`, and Flatpak-based launchers at some point.

## Key decisions

- Controller-first, library-first, fullscreen by default; RecompStation keeps its own visual identity (RecompLauncher was only a reference).
- Metadata is matched only when one result clearly fits.
- Never supply game files; never overwrite or delete the user's games; uninstalling a launcher keeps ROMs and saves.
- Never create a launcher's settings files; change only single documented keys in files the launcher wrote.
- No settings without real behaviour.
- Screenshots committed to the repository have their metadata removed.
