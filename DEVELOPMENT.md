# RecompStation Development Roadmap

This document is the current development status and implementation order for RecompStation. It was reviewed and reorganised on 2026-11-23 and updated on 2026-11-24 (branch cleanup and merge into `main`): finished work is summarised, open checks are collected in one list, and items that no longer apply were removed. The detailed step-by-step record of earlier work (where logic lives, why decisions were made) is preserved in [`docs/development-history.md`](docs/development-history.md).

## Status

- **Branches:** `main` holds the interface redesign and everything since; it was brought up to date with `ui/interface-redesign` on 2026-11-24 with the user's approval, and the old feature branches were removed. `ui/interface-redesign` remains the development branch: feature work happens on short-lived branches from it, is fast-forward merged after the user validates it, and reaches `main` when the user approves.
- **Build:** C++20 / Qt 6.5+ / QML, 22 CTest suites (`./scripts/build.sh --test`), all passing on 2026-11-24.
- **Last validated:** twelve library systems in the UI, the system picker and the simplified first-time setup; Bomberman 64 and Bomberman Hero launch; the build speedups and the runtime log (2026-11-24). Earlier: README, license, logo; seamless play (Banjo-Kazooie, Shipwright, Ghostship); the Settings redesign; the LaunchBox progress fix (2026-11-23).

## What is done

**Application shell and navigation**
- Two destinations, Library and Settings, with a console-style header; fullscreen on launch; F11 and Windowed/Maximized/Fullscreen modes; Game Details as a full-window overlay.
- Full controller, keyboard and mouse navigation, with an adaptive hint bar whose button symbols follow the connected controller's layout and its mapping profile.
- Interface scale (Auto from the screen's logical height, or 75–200%) applied at startup through `QT_SCALE_FACTOR`; changing it restarts the app on the same Settings page.

**Library**
- Twelve library systems, ordered by maker and generation: Game Boy, Game Boy Color, Game Boy Advance, SNES, N64, GameCube, Wii, Wii U, PS1, PS2, Xbox, Xbox 360; each has its folder, file types, detection when adding games, metadata mappings and box shape.
- Four views: Shelves (with a spotlight panel), Grid (sectioned by platform in All Platforms), List with a preview pane, and a full-screen hero Carousel. Covers use their platform's box shape (`GameArtwork.qml`). A system picker in the header (one chip; LB/RB and Q/E step through systems with games, the list groups them by maker), expanding search, per-view size presets, all persisted.
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
- Full-window setup: library folder (just the folder choice; suggests `~/Games/RecompStation` or `~/RecompStation`), Add games, covers and info (LaunchBox progress with stages, Cancel and Retry), ready.
- `GameImporter`: finds games in folders or dropped files, identifies the system (extension plus disc headers), keeps `.cue` tracks together, unpacks cartridge ROMs (N64, GB, GBC, GBA, SNES) from `.zip`/`.7z`, copies (default) or moves them into the platform folders, never overwrites. Available from setup, Settings › Library and drag-and-drop.

**Launchers**
- Curated catalog of 42 projects (30 N64, 4 GameCube, 2 each for PS1, Xbox, Xbox 360 and SNES) queried through the GitHub API; Linux x86_64/arm64 AppImage, ZIP and TAR.GZ assets (including a ZIP wrapping a TAR.GZ); projects that publish only pre-releases (`includePrereleases`, with an automatic fallback when there is no full release); staged download, extraction, install, launch, uninstall, rediscovery. Game input ownership while a launcher runs. The launcher file inside an archive is matched by name without case (it must still be the only match).

**Seamless play** (`LaunchPreparer`, `RomInspector`, the `launch` recipe in `data/projects.json`)
- Play hands the library's game file to the launcher, so it opens straight to the game. Three catalog-driven methods, no per-project code:
  - `n64recomp` (22 projects): the ROM is checked against the launcher's own XXH3-64 checksum (read from each project's source, `.rom_hash`) and copied, padded and converted to big-endian, to `~/.config/<program_id>/<game_id>.z64` — where N64ModernRuntime keeps it after its first start. A `portable.txt` in the install folder means the install folder instead.
  - `harbourmasters` (Shipwright, Ghostship, PaperBoat): `SHIP_HOME` is set to `~/.local/share/<name>`, so saves live outside the managed install folder (Ghostship and PaperBoat previously saved into it, because libultraship falls back to the working directory). Existing saves and settings are copied there once; the ROM is placed there for the port's own extraction prompt.
  - `manual` (the GameCube ports, Conker, Diddy Kong Racing, Pokémon Snap, Snowboard Kids, Glover-R, and the PS1, Xbox, Xbox 360 and SNES projects): the game page offers Show file and Copy path.
- Never replaces a file a launcher already has; library files are only read; the status line under Play explains wrong versions (region and revision from the ROM header) and wrong games.
- Display: launchers must write their own `graphics.json` (a file missing options crashes RecompFrontend launchers: `ds_option` is parsed as a number). RecompStation only changes `wm_option` in an existing file — "Start games in fullscreen" once per launcher (remembered in `launch/fullscreenApplied/<id>`), or the per-game Window choice every start. Widescreen ("Expand") is already the launchers' default.
- Settings › Library › Playing (prepare games, start in fullscreen); Game Details status line and Launch section (game file, window, saves folder); a short starting screen listing what was done.
- xxHash 0.8.5 header in `third_party/xxhash` (BSD 2-Clause).

## Open validation checklist

Runtime checks for implemented features that have not been fully confirmed yet. Tick them as they are exercised.

- [ ] Settings: LB/RB and Q/E switch sections; ◀ value ▶ rows with D-pad and keyboard; interface scale change and restart (a manual value, then back to Auto).
- [x] Metadata: Update catalog refreshes the catalog and its size; a TheGamesDB key added in the app tests successfully and shows the remaining request quota (2026-11-24).
- [ ] Metadata: with automatic matching off, a scan does not match; with artwork downloads off, matching does not download covers.
- [x] Controller: a second connected controller takes over navigation when it is used (fixed on `fix/validation-round-1`; only the first controller used to be read), confirmed 2026-11-24.
- [ ] Controller: a remapped Select shows in the hint bar; default Xbox and PlayStation mappings.
- [ ] Application: Remove for a launcher whose game is gone; Reset settings.
- [x] First-time setup: the simplified first step (folder choice only), confirmed 2026-11-24.
- [ ] First-time setup: from scratch; adding from Downloads and another folder; a zipped N64 ROM; a `.cue`/`.bin` game; an unsure `.iso` with the system picker; Move; drag-and-drop onto the Library; Settings › Library › Add games.
- [ ] Library: List and Carousel size stepping and persistence; Shelves spacing polish; the empty-library screen after a reset.
- [x] System picker: shows only systems with games; LB/RB and Q/E step through them; the controller's Select button opens the list; removing a system's last game updates it (2026-11-24).
- [x] New systems: each appears in the library, with its folder (confirmed 2026-11-24).
- [x] Zipped cartridge ROMs (N64, GB, GBC, GBA, SNES) copied straight into a system folder are unpacked by the scanner and the archive removed (on `fix/validation-round-1`), confirmed 2026-11-24.
- [ ] New systems: adding a GB, GBC (dual-mode and colour-only), GBA and SNES ROM, loose and zipped; a PS2 `.iso` and `.chd` are not taken for PS1; a Wii U `.wua`; covers have the right shape after Update catalog.
- [x] Game Details states: downloading/installing pill, release and launch errors, a game with several projects (Open Nectar, Super Mario 64), a game without metadata, Uninstall under ⋯ (2026-11-24). Follow-ups: the ⋯ menu and the no-metadata page (roadmap 6a).
- [ ] Launchers: install, launch and uninstall for the catalog projects not yet exercised (confirmed so far: Banjo-Kazooie, Banjo-Tooie, Star Fox 64, Donkey Kong 64, PaperBoat, Shadow the Hedgehog (Xbox), Bomberman 64, Bomberman Hero); download cancellation and failure feedback; uninstall keeps ROM files.
- [ ] Controller-only and keyboard-only pass over dialogs and the in-app folder browser (focus entry, actions, cancel, focus return).
- [ ] Full regression pass across pages and backend workflows before the first public release.

## Roadmap

Ordered by priority. Each item ships on its own branch with tests, a build, and user validation.

### 1. Seamless launching — follow-ups
The core is done (see What is done). Remaining:
- [ ] Projects still on `manual`: research and add recipes where a launcher accepts its file in a predictable way — for example Luigi's Mansion (`--us-disc` / `--pal-disc` by disc ID), Open Nectar (`--data-dir`), Super Mario Strikers, Dusklight; Pokémon Snap stores its ROM and saves next to the executable, so it also needs save protection.
- [ ] Protect saves of launchers that keep them in their install folder (portable mode, Pokémon Snap) on uninstall and update.
- [ ] Read the ROM check off the UI thread (the first open of a game page reads up to 64 MB once; results are cached by path, size and modification time).
- [ ] When adding a catalog project, record its launch recipe from the project's source (program id, game id, ROM checksum) rather than from release notes.

### 2. Catalog, platforms and file formats
Source list: playported.com (about 1,200 entries; it names repositories but not owners, so each owner is found separately). A project is added only when its GitHub releases carry a native Linux build (AppImage, tar.gz or zip) that the installer can use; Flatpak-only projects are skipped (for example Unleashed Recompiled, whose Linux release is a Flatpak).
- [x] Batch 1 (merged; Shadow the Hedgehog confirmed, the rest pending testing): Xbox 360 as a library platform (XDVDFS detection, metadata mappings); PS1: MediEvil, Bloody Roar II; Xbox: Shadow the Hedgehog, Ninja Gaiden Black; Xbox 360: Lost Odyssey, The Simpsons Game. Projects that publish only pre-releases are supported (`includePrereleases`, and an automatic fallback when a project has no full release).
- [ ] Next batches from playported.com: more PS1 (Castlevania: SotN, Dino Crisis 1/2, Ape Escape), Xbox (Black, Jet Set Radio Future, SoulCalibur II), Xbox 360 (Spider-Man: Edge of Time, Ninja Gaiden II).
- [ ] Skipped for now: Blue Dragon (re:Blue's setup copies the program to a folder of the user's choice, outside the managed install), Gran Turismo 2 (Windows only), SSX 3 (Linux build from source only).
- [ ] The launchers of batch 1 pick the game file themselves; research launch recipes (command-line arguments or data folders) so Play can hand them the file.
- [ ] LaunchBox: catalogs downloaded before a system was added (Xbox 360, and now GB, GBC, GBA, SNES, PS2, Wii U) don't contain its games; Update catalog fixes that. Detect a missing platform and offer the update.
- [x] Six more systems (merged; in the UI, confirmed): Game Boy, Game Boy Color, Game Boy Advance, SNES, PlayStation 2 and Wii U, each with library folder, file types, detection when adding games, TheGamesDB and LaunchBox mappings and a cover shape (GB/GBC/GBA 0.92, SNES 7:5, PS2 and Wii U 5:7). Detection: the Game Boy header's CGB flag tells GB from GBC (also inside .zip/.7z); `SYSTEM.CNF` (`BOOT2`) tells PS2 discs from PS1, in `.iso`, `.bin` and `.cue`; a CHD's metadata tells DVD (PS2) from CD (PS1); `.wua`/`.wud`/`.wux` are Wii U (no longer listed under Wii).
- [x] Batch 2 (merged; Bomberman 64 and Bomberman Hero confirmed, the rest pending testing): SNES: Super Metroid (precompiled zip, `Launch.sh`) and Mega Man X (AppImage); N64: Bomberman 64, Bomberman Hero (pre-releases), Harvest Moon 64, Snowboard Kids 2, Space Station Silicon Valley and Quest 64 with `n64recomp` launch recipes read from each project's source, plus Snowboard Kids and Glover-R (own launchers, manual). Where a release file name couldn't be confirmed, the pattern matches any Linux x86-64 zip of that project and excludes ARM64, debug, Flatpak, AppImage, Windows and macOS builds.
- [ ] Skipped in batch 2: Aladdin (AgrabahPort) and Gundam Wing: Endless Duel (their Linux releases are setup packs that compile the game from the ROM on first run with an ~800 MB toolchain); Dr. Mario 64 (Linux build from source only); Pilotwings 64, Rocket: Robot on Wheels, Mortal Kombat Trilogy (repositories not found yet).
- [x] Research for GB/GBC/GBA, PS2 and Wii U (2026-11-24). Ready to add (ready-to-run Linux builds that ask for the game themselves): Final Fantasy Tactics Advance (GBA, `FFTARecomp-*-linux-x64.zip`), Dragon Ball Z: Budokai Tenkaichi 3 (PS2, `BT3-Recomp-linux-x86_64.tar.gz`), Goemon's Great Adventure (N64, pre-releases, `n64recomp`: `GGARecompiled`, `gga.us`, `A62841FD58D33B4E`), and Gen1Recomp (Pokémon on GB, GBC and GBA in one AppImage; needs a catalog field for extra systems). Windows only or build from source on Linux: Link's Awakening DX, mstan's GBA ports (Minish Cap, Emerald, FireRed/LeafGreen, Mega Man Zero), Tekken Tag Tournament 2.
- [ ] Add the four ready projects above, with a catalog field that lets one project cover several systems (Gen1Recomp).
- [ ] Decide on projects that compile the game on the player's machine at first start (a downloaded or bundled compiler): The Wind Waker HD (the only Wii U option), God of War (PS2, experimental), Boktai (GBA, also needs a BIOS dump), Aladdin and Gundam Wing (SNES). Either list them with a "builds on first start" note or keep skipping them.
- [ ] Many projects publish a `recomplauncher.json` (RecompLauncher's manifest: start program, arguments such as `--rom {game}`, config and save folders). Reading it could give launch recipes for projects that take the game file as an argument (Aladdin, Super Metroid).
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
- [x] Shared `recompstation_core` static library: the app and every test link it, instead of each test recompiling the sources it uses (was 98 compilations of 29 files, now 29).
- [x] ccache when installed; mold, then lld, as the linker when the compiler accepts it (`-DRECOMPSTATION_FAST_LINKER=OFF` to opt out). `mold` added to the Arch dependency script.
- [x] Tests run in parallel (`ctest -j`, `JOBS` to override); `build.sh` prints configure, build and test times.
- [x] Fixed waits removed from the slowest tests without changing what they check: the library watcher's debounce is 50 ms in tests (750 ms in the app), and the launch tests' scripts sleep 0.3 s instead of 1 s.
- [ ] Measure again with the printed times; if Main.qml's QML compilation (qmlcachegen, 8,000 lines) dominates, split it into components (Library, Settings pages) so changes recompile less.

### 5b. Runtime log — done
`runtime.log` is written for whoever reads a bug report: a session header (version and commit, system, desktop and session type, display, settings, library counts, installed launchers, data and log paths) and footer, one line per event (`2026-11-23 15:14:02.239 INFO  launch    …`), the home folder written as `~`, and the renderer (graphics API and OpenGL driver) once the window is up. Per-frame and per-request chatter (geometry, screenshot rotation, every metadata query) is debug-only; debug is off unless Settings › Application › Detailed logging is on or `RECOMPSTATION_DEBUG=1` (`QT_LOGGING_RULES` still overrides). Launcher output is captured to `logs/launchers/<project>.log` and, when a launcher crashes or exits with an error, its last 40 lines are copied into `runtime.log`.

### 6a. Validation follow-ups (2026-11-24)
- [x] Game Details ⋯ opens a drop-down menu under the button: Show install folder, Open launcher log (the folder of logs before a first start), and Uninstall… set apart in red, still asking first. D-pad moves inside it, A picks, B or Esc closes it and focus returns to ⋯. Built on `fix/validation-round-1` from the approved mockup; to validate.
- [x] Game Details without game info: the hero layout with a "No game info" chip, the file name under the title, Find game info (the main action when there is no launcher) and Metadata settings as styled buttons, an About card explaining what happened and how to fix it, matching Recompilation card text, three empty screenshot frames, and an accent glow behind the page when there is neither a screenshot nor a cover. Built from the approved mockup; to validate.
- [x] Placeholder covers drawn at each system's box shape in every view, replacing the generic portrait picture: a plain dark tile with the logo faintly in the middle. The system band and title from the mockup were dropped after review (2026-11-24): the title and system are already shown around the cover. Shelves cards have no caption, so their placeholders show the title (`placeholderTitle`). To validate.
- [ ] Validate the three items above: a game without info (focus starts on Find game info; Metadata settings opens Settings › Metadata), placeholders in Shelves, Grid, List, Carousel, Game Details and the starting screen for several systems, and the ⋯ menu with a controller, keyboard and mouse.
- [ ] A launcher whose game was removed stays installed by design (uninstalling never happens behind the user's back); Settings › Application › Launchers whose game is gone lists it with Remove. Consider pointing to that from the Library once a game disappears.

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

### 10. Documentation and licensing — done
GPL-3.0 `LICENSE`; `README.md` with the no-game-files notice at the top, features, screenshots (metadata stripped, see `docs/screenshots/README.md`), building from source and file locations; the original logo (pixel d-pad) in `assets/logo/`, used in the header, first-time setup, the README and as the window icon. Follow-up when packaging: a `.desktop` file and hicolor icons so Wayland taskbars show the icon.

## Key decisions

- Controller-first, library-first, and fullscreen by default; RecompStation keeps its own visual identity (RecompLauncher, GPL-3, was used only as a reference).
- Artwork comes from LaunchBox and TheGamesDB box art; no other artwork provider is planned.
- Metadata is matched only when one result clearly fits; ambiguous games stay unmatched rather than getting the wrong cover.
- RecompStation never supplies game files and never overwrites or deletes the user's games; uninstalling a launcher keeps ROMs and saves. One exception, at the user's request: a .zip/.7z in a cartridge system's folder whose only file is a ROM is removed after that ROM has been unpacked next to it and its size checked; archives with other files are kept.
- RecompStation never creates a launcher's settings files; it changes single documented keys in files the launcher wrote itself.
- Launchers come only from native Linux release assets of curated projects; Flatpak is out of scope.
- No settings without real behaviour: placeholder controls are removed until their feature exists.

## Build and development workflow

Dependencies:
- Qt 6.5 or newer, including Qt SVG and Qt Shader Tools (`qt6-shadertools` on Arch Linux)
- A C++20 compiler and CMake; Ninja is recommended
- Optional, for faster builds: ccache and mold (or lld), picked up automatically
- libarchive (`libarchive` on Arch Linux)
- SDL2 for controller support (optional at build time)

From the repository root:

```bash
./scripts/install_dependencies.sh   # Arch-based systems
./scripts/build.sh                  # Debug build
./scripts/build.sh --test           # build and run the CTest suites in parallel
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
