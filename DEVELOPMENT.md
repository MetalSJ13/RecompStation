# RecompStation Development Roadmap

This document is the current development status and implementation order for RecompStation. It was reviewed and reorganised on 2026-11-23: finished work is summarised, open checks are collected in one list, and items that no longer apply were removed. The detailed step-by-step record of earlier work (where logic lives, why decisions were made) is preserved in [`docs/development-history.md`](docs/development-history.md).

## Status

- **Development branch:** `ui/interface-redesign`. Feature work happens on short-lived branches from it and is fast-forward merged after the user validates it. `main` stays untouched until the redesign is reviewed and explicitly approved for merge.
- **Build:** C++20 / Qt 6.5+ / QML, 21 CTest suites (`./scripts/build.sh --test`).
- **Last validated:** first-time setup and adding games, the Settings redesign (all seven sections) and the LaunchBox progress fix (2026-11-23).

## What is done

**Application shell and navigation**
- Two destinations, Library and Settings, with a console-style header; fullscreen on launch; F11 and Windowed/Maximized/Fullscreen modes; Game Details as a full-window overlay.
- Full controller, keyboard and mouse navigation, with an adaptive hint bar whose button symbols follow the connected controller's layout and its mapping profile.
- Interface scale (Auto from the screen's logical height, or 75–200%) applied at startup through `QT_SCALE_FACTOR`; changing it restarts the app on the same Settings page.

**Library**
- Four views: Shelves (with a spotlight panel), Grid (sectioned by platform in All Platforms), List with a preview pane, and a full-screen hero Carousel. Covers use their platform's box shape (`GameArtwork.qml`). Platform pills (LB/RB, Q/E), expanding search, per-view size presets, all persisted.
- Backgrounds: theme gradient, live GPU shader waves (color, motion, quality; paused during games and under Reduced motion), or the user's own image (Fill/Fit, Dim).
- Filesystem monitoring and automatic reconciliation of added and removed games.

**Game Details**
- Hero layout (screenshot backdrop, cover at its true shape, chips, title), one primary action that follows the launcher lifecycle (Install → Downloading → Play), Check for updates, Uninstall under ⋯, About, selectable screenshots with a full-screen viewer, and a Recompilation card. LB/RB switch games.

**Settings** (category rail with live summaries, ◀ value ▶ rows, toggles, preview pane)
- Appearance (four themes: Electric Blue, Midnight Violet, Aurora, Carbon; Ember retired), Display, Background, Library (including Add games), Metadata, Controller (layout drawing, live button test, symbols, remapping), Application (about, storage use, data and log folders, launchers whose game is gone, reset settings).

**Metadata**
- LaunchBox (offline catalog, first by default, indexed for supported platforms only) and TheGamesDB (key entered in the app and stored in an owner-only file; the environment variable still wins).
- Automatic matching after every scan, including games added while the app runs; TheGamesDB failures fall back to LaunchBox and are retried an hour later; enabling LaunchBox or updating its catalog re-matches unmatched games. Covers and screenshots from either provider.

**First-time setup and adding games**
- Full-window setup: library folder (suggests `~/Games/RecompStation` or `~/RecompStation`), Add games, covers and info (LaunchBox progress with stages, Cancel and Retry), ready.
- `GameImporter`: finds games in folders or dropped files, identifies the system (extension plus disc headers), keeps `.cue` tracks together, unpacks N64 ROMs from `.zip`/`.7z`, copies (default) or moves them into the platform folders, never overwrites. Available from setup, Settings › Library and drag-and-drop.

**Launchers**
- Curated catalog of 26 projects (22 N64, 4 GameCube) queried through the GitHub API; Linux x86_64/arm64 AppImage, ZIP and TAR.GZ assets (including a ZIP wrapping a TAR.GZ); staged download, extraction, install, launch, uninstall, rediscovery. Game input ownership while a launcher runs.

## Open validation checklist

Runtime checks for implemented features that have not been fully confirmed yet. Tick them as they are exercised.

- [ ] Settings: LB/RB and Q/E switch sections; ◀ value ▶ rows with D-pad and keyboard; interface scale change and restart (a manual value, then back to Auto).
- [ ] Metadata: press Update catalog and compare the catalog size with the previous 498 MB; add a TheGamesDB key in the app and Test it; with automatic matching off, a scan does not match; with artwork downloads off, matching does not download covers.
- [ ] Controller: a second controller type changes the drawing and every symbol; a remapped Select shows in the hint bar; default Xbox and PlayStation mappings.
- [ ] Application: Remove for a launcher whose game is gone; Reset settings.
- [ ] First-time setup: from scratch; adding from Downloads and another folder; a zipped N64 ROM; a `.cue`/`.bin` game; an unsure `.iso` with the system picker; Move; drag-and-drop onto the Library; Settings › Library › Add games.
- [ ] Library: List and Carousel size stepping and persistence; narrow-window behaviour (pills clip rather than wrap); Shelves spacing polish; the empty-library screen after a reset.
- [ ] Game Details states: downloading/installing pill, release and launch errors, a game with several projects (Open Nectar), a game without metadata.
- [ ] Launchers: install, launch and uninstall for the catalog projects not yet exercised (confirmed so far: Banjo-Kazooie, Banjo-Tooie, Star Fox 64, Donkey Kong 64, PaperBoat); download cancellation and failure feedback; uninstall keeps ROM files.
- [ ] Controller-only and keyboard-only pass over dialogs and the in-app folder browser (focus entry, actions, cancel, focus return).
- [ ] Full regression pass across pages and backend workflows before merging into `main`.

## Roadmap

Ordered by priority. Each item ships on its own branch with tests, a build, and user validation.

### 1. Seamless launching — next
Today the user has to locate the ROM inside each launcher's own file picker. Goal: pressing Play starts the game with the right file and no picker.
- [ ] Research, per catalog project, how it finds its game data (first-run file dialog, a copied ROM in its data folder, command-line argument, extracted game files) and where it stores display settings.
- [ ] Add catalog-driven launch behaviour (no per-project code paths): for example a ROM argument template, or the destination where the launcher expects its verified ROM copy.
- [ ] Verify the ROM before providing it (N64: MD5/CRC against the catalog's known hashes, byte-order normalisation); never present a title match as proof of compatibility.
- [ ] Optional first-launch display defaults (fullscreen, widescreen) where a project's settings format is documented, written only when the launcher has no settings yet, so the user's own changes are never overwritten. Off by default unless agreed.

### 2. Catalog, platforms and file formats
- [ ] Add the recompilation projects listed on playported.com that publish native Linux builds; support several projects per game in Game Details.
- [ ] Add platforms supported by recompilation projects (for example Xbox 360, PS2, and others), each with researched file formats, system detection rules for the importer and scanner, TheGamesDB/LaunchBox platform mappings, and box-art shape.
- [ ] Keep library platform support independent of whether a recompilation project exists for it.
- [ ] Review archive extraction safety (path traversal, absolute paths and symlinks, special files, nested archive depth, size and file-count limits) and add regression tests for each case.
- [ ] Flatpak packages stay out of scope.

### 3. Launcher lifecycle and integrity
- [ ] Track installed launcher versions and show them in Game Details.
- [ ] Update flow with rollback-safe replacement; clean up abandoned staging directories at startup.
- [ ] Clearer release and architecture explanations when no compatible asset exists.
- [ ] Verify published SHA-256 checksums or signatures when a project provides them; treat invalid data as a failure, missing data as unverified.
- [ ] Tests for update failure, recovery and version state.

### 4. Metadata follow-ups
- [ ] Fix match: a Game Details action to search either provider and pick the right game.
- [ ] TheGamesDB `Games/ByGameHash` (MD5, then CRC) for N64, after verifying hash conventions against known-good ROMs.
- [ ] Bounded retry and backoff for HTTP 429/5xx on metadata and artwork requests; pause on 403.
- [ ] Regression tests for match-confidence rules, monitoring duplicate events and cache behaviour.
- [ ] Decide whether a shared TheGamesDB key could ever be shipped (licensing, rate limits, attribution) or whether user keys remain the only option.

### 5. Build and test performance
- [ ] Measure where build time goes (Main.qml compilation, moc/rcc, linking) and adopt the safe wins: compiler cache, a faster linker when available, splitting the largest QML file, parallel test runs.
- [ ] Shorten the regression suite's wall time without reducing coverage (parallel CTest, removing fixed waits from the slowest tests).

### 6. Input, accessibility and regression
- [ ] Contrast, accessible names, tab order and scaling review across all pages.
- [ ] Theme polish of the button-mapping capture states.
- [ ] Multi-display placement.
- [ ] Optional navigation sounds with a volume control.

### 7. Desktop environment compatibility
Developed and tested on KDE Plasma (Wayland). Before a public release, check the app on XFCE, Cinnamon and MATE (all X11-based), and GNOME:
- [ ] Fullscreen on launch and F11 toggling, maximized and windowed sizes, window placement.
- [ ] The shader wave background under each compositor or none (XFCE and MATE may run without compositing), and the software-renderer fallback.
- [ ] High-DPI and fractional scaling, including the interface scale's Auto mode under X11.
- [ ] Controller hot-plug and input focus while a launcher runs; drag-and-drop from each file manager (Thunar, Nemo, Caja, Nautilus); opening folders with `xdg-open`.
- [ ] Record any environment-specific workarounds here.

### 8. Windows support — open question
Not decided. Points to discuss before any work starts:
- Most recompilation projects publish Windows builds, so the catalog would need Windows asset matching (`.zip`/`.exe`) alongside Linux.
- Qt, QML and SDL2 are portable; Linux-specific parts are paths (XDG folders), launching (AppImage, `./run.sh`), file permissions (the owner-only key file), `xdg-open`, and the build scripts.
- Packaging (installer or portable ZIP), code signing and SmartScreen warnings, and a CI build matrix.
- Testing cost: every feature would need checking on both systems.

### 9. Application updates — deferred
RecompStation does not publish GitHub releases yet. When it does: a manual "Check for updates" and an optional daily check that compares the installed version with the latest release and links to the release page; never download or install automatically. The Settings switch and the first-run question were removed until this exists.

### 10. Documentation and licensing
- [ ] GPL-3.0 `LICENSE` file in the repository.
- [ ] End-user `README.md`: a clear statement that RecompStation does not supply, host or distribute games, summary, contents, features, screenshots of the views, download and install.

## Key decisions

- Controller-first, library-first, and fullscreen by default; RecompStation keeps its own visual identity (RecompLauncher, GPL-3, was used only as a reference).
- Artwork comes from LaunchBox and TheGamesDB box art; no other artwork provider is planned.
- Metadata is matched only when one result clearly fits; ambiguous games stay unmatched rather than getting the wrong cover.
- RecompStation never supplies game files and never overwrites or deletes the user's games; uninstalling a launcher keeps ROMs and saves.
- Launchers come only from native Linux release assets of curated projects; Flatpak is out of scope.
- No settings without real behaviour: placeholder controls are removed until their feature exists.

## Build and development workflow

Dependencies:
- Qt 6.5 or newer, including Qt SVG and Qt Shader Tools (`qt6-shadertools` on Arch Linux)
- A C++20 compiler and CMake; Ninja is recommended
- libarchive (`libarchive` on Arch Linux)
- SDL2 for controller support (optional at build time)

From the repository root:

```bash
./scripts/install_dependencies.sh   # Arch-based systems
./scripts/build.sh                  # Debug build
./scripts/build.sh --test           # build and run the CTest suites
./scripts/run.sh                    # start the app
```

Choose a configuration with `./scripts/build.sh Release` or `./scripts/build.sh RelWithDebInfo`. The build script reuses the configured generator and build directory; keep the build directory and prefer incremental builds. Every new `.qml` or `.js` file must be listed in `QML_FILES` in `CMakeLists.txt` (the QML tests read from the source tree and will not catch a missing entry).

The CMake `WrapVulkanHeaders` warning is harmless.

## Development principles

1. Keep `main` stable; develop on the redesign branch and short feature branches.
2. Keep the application buildable and runnable after each focused change.
3. Application logic in C++, presentation and navigation in QML; small reusable components over one large file.
4. Automate backend chores (scanning, matching, artwork) rather than exposing them as tasks.
5. Add tests whenever backend behaviour changes; run the full suite before merging.
6. Never delete the user's build directory, application data, games or saves during routine work.

## Definition of done

A roadmap item is done when its checklist is satisfied, the application builds, all CTest suites pass, the user has validated it at runtime, and this document and `docs/UI_DESIGN.md` describe what was actually built.
