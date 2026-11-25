# RecompStation Development Roadmap

This document is the current development status and implementation order for RecompStation. It was reviewed and reorganised on 2026-11-23 and updated on 2026-11-24 (branch cleanup and merge into `main`): finished work is summarised, open checks are collected in one list, and items that no longer apply were removed. The detailed step-by-step record of earlier work (where logic lives, why decisions were made) is preserved in [`docs/development-history.md`](docs/development-history.md).

## Status

- **Branches:** `main` was brought up to date with `ui/interface-redesign` on 2026-11-24 (launcher lifecycle, packaging, the simpler Recompilation pane and license notices) with the user's approval, and tagged `v0.1.0`, the first release. `ui/interface-redesign` remains the development branch: feature work happens on short-lived branches from it, is fast-forward merged after the user validates it, and reaches `main` when the user approves.
- **Releases:** pushing a tag `v<version>` matching `CMakeLists.txt` drafts a GitHub release with the AppImage, the zip and `SHA256SUMS`; the user reviews and publishes it. Before the next release, raise the version in `CMakeLists.txt` (for example 0.1.1 or 0.2.0).
- **Build:** C++20 / Qt 6.5+ / QML, 22 CTest suites (`./scripts/build.sh --test`), all passing on 2026-11-24.
- **Last validated:** launch recipes for Luigi's Mansion and Pokémon Snap, saves kept across reinstall, the background ROM check, accessibility (contrast, names, Grid inset), button-mapping capture states, the Open sound in every view, interface sounds, TheGamesDB request savings, the redesigned folder browser, Settings navigation and interface scale, Library sizing in all views, the Game Details ⋯ menu, the second controller taking over, zipped ROMs unpacked from system folders, Game Details without game info, placeholder covers; twelve library systems, the system picker, the simplified first-time setup, Update catalog, TheGamesDB key test, Game Details states; Bomberman 64 and Bomberman Hero (2026-11-24). Earlier: README, license, logo; seamless play (Banjo-Kazooie, Shipwright, Ghostship); the Settings redesign; the LaunchBox progress fix (2026-11-23).

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

- [x] Settings: LB/RB and Q/E switch sections; ◀ value ▶ rows with D-pad and keyboard; interface scale change and restart (2026-11-24).
- [x] Metadata: Update catalog refreshes the catalog and its size; a TheGamesDB key added in the app tests successfully and shows the remaining request quota (2026-11-24).
- [ ] Metadata: with automatic matching off, a scan does not match; with artwork downloads off, matching does not download covers.
- [x] Metadata request savings (validated 2026-11-24): after one start with the key, the next start shows **Ready** without a request (the count on your TheGamesDB API key page does not go up); **Test** shows **Connected** and the requests left; screenshots for several games arrive after one lookup; the allowance line shows the pause note when 50 or fewer requests are left.
- [x] Interface sounds (validated 2026-11-24): Move when focus moves (library, Settings, dialogs, Game Details); Open a game, Back once (B, Esc and the close button), the swoosh for LB/RB or Q/E in the library and Settings, the switch sound for picking a system in the picker (opening it is silent), ⋯ and system menus, switches, Play, a failed launch; the volume row plays a sample per step; off means silent; no sounds while a game runs.
- [x] Game Details plays Open from every view, including Grid and List (validated 2026-11-24).
- [x] Accessibility (validated 2026-11-24): muted text is easier to read on panels in every theme; Midnight Violet's accent is a little lighter; with a screen reader (Orca), moving in each Library view reads the game and system.
- [x] Capture states (validated 2026-11-24): Add with ✕/A shows "Let go of ✕ first", then the listening panel and countdown; Replace strikes out the old inputs; adding an input another action uses shows the warning; waiting 15 s shows "Nothing was pressed"; Cancel works and focus returns to the row's Add; other rows dim while one listens.
- [x] Luigi's Mansion opens straight to the game from Play with `--disc`, builds its shaders on the first start and starts fullscreen (validated 2026-11-24).
- [x] Pokémon Snap boots straight to the game from Play (validated 2026-11-24).
- [ ] Not yet tried: Conker and Snowboard Kids open straight to the game from Play; Snowboard Kids' saves sit under `~/.local/share/SnowboardKidsRecompiled/userdata`.
- [x] Saves on reinstall (validated 2026-11-24 with Conker: a saves/ folder made in the install came back after Uninstall and Install).
- [x] Opening a game page for the first time doesn't stall (validated 2026-11-24).
- [ ] LaunchBox card: with a catalog from before a system was supported, it names the systems missing; Update catalog clears the note. (With a current catalog no note shows.)
- [ ] Startup cleanup: a leftover file in `downloads/staging/<project>/` from an interrupted download is gone after a restart, and the log says how much was removed.
- [x] Controller: a second connected controller takes over navigation when it is used (fixed on `fix/validation-round-1`; only the first controller used to be read), confirmed 2026-11-24.
- [ ] Controller: a remapped Select shows in the hint bar; default Xbox and PlayStation mappings.
- [x] First-time setup: the simplified first step (folder choice only), confirmed 2026-11-24.
- [ ] First-time setup: from scratch; adding from Downloads and another folder; a zipped N64 ROM; a `.cue`/`.bin` game; an unsure `.iso` with the system picker; Move; drag-and-drop onto the Library; Settings › Library › Add games.
- [x] Library: size stepping, persistence and spacing in all four views (2026-11-24).
- [x] System picker: shows only systems with games; LB/RB and Q/E step through them; the controller's Select button opens the list; removing a system's last game updates it (2026-11-24).
- [x] New systems: each appears in the library, with its folder (confirmed 2026-11-24).
- [x] Zipped cartridge ROMs (N64, GB, GBC, GBA, SNES) copied straight into a system folder are unpacked by the scanner and the archive removed (on `fix/validation-round-1`), confirmed 2026-11-24.
- [ ] New systems: adding a GB, GBC (dual-mode and colour-only), GBA and SNES ROM, loose and zipped; a PS2 `.iso` and `.chd` are not taken for PS1; a Wii U `.wua`; covers have the right shape after Update catalog.
- [x] Game Details states: downloading/installing pill, release and launch errors, a game with several projects (Open Nectar, Super Mario 64), a game without metadata, Uninstall under ⋯ (2026-11-24). Follow-ups: the ⋯ menu and the no-metadata page (roadmap 6a).
- [ ] Launchers: install, launch and uninstall for each catalog project, tracked in the "RecompStation launcher test list" spreadsheet (one row per project with a Result column; 32 of 42 not tested on 2026-11-24); download cancellation and failure feedback; uninstall keeps ROM files. Confirmed so far: Banjo-Kazooie, Banjo-Tooie, Star Fox 64, Donkey Kong 64, PaperBoat, Ship of Harkinian, Ghostship, Shadow the Hedgehog (Xbox), Bomberman 64, Bomberman Hero.
- [ ] Controller-only and keyboard-only pass over dialogs and the in-app folder browser (focus entry, actions, cancel, focus return). Found 2026-11-24: the folder browser needed a double click to open a folder, Home / Up were unclear, and the controller drove the page behind it.
- [x] Folder browser redesigned on `ui/folder-browser` from the approved mockup: one click (or A) opens a folder; a clickable path (Home › Documents › …) replaces Home and Up; Back returns to the previous folder (B, Backspace or Alt+←; B closes the browser from its first folder); Places lists Home, Downloads, Documents, the library folder and mounted drives; Use “folder” always picks the folder shown (Y on a controller); New folder opens the folder it makes; hidden folders are left out; the hint bar shows the browser's own buttons. Validated 2026-11-24.
- [ ] Full regression pass across pages and backend workflows before the first public release.

## Roadmap

Ordered by priority. Each item ships on its own branch with tests, a build, and user validation.

### 1. Seamless launching — follow-ups
The core is done (see What is done). Remaining:
- [x] Launch recipes from each project's source (Luigi's Mansion and Pokémon Snap validated 2026-11-24; Conker and Snowboard Kids not yet tried): Conker (`n64recomp`, `ConkerRecompiled`, `conker.n64.us.1.0.z64`, XXH3 `23FBBA2DBCF2FD8E`); Pokémon Snap (`romfolder`: `SNAP_DATA_DIR` → `~/.local/share/Snap64Recomp`, ROM `pokemonsnap.z64`, XXH3 `73CBBC5C7DE9425C`, saves and mods copied over from the install folder); Snowboard Kids (`romfolder`: ROM in `~/.local/share/SnowboardKidsRecompiled/rom/` passed as the first argument, so saves land in `userdata/` beside it, XXH3 `F384619787B78D4B`); Luigi's Mansion (`discargument`: `--disc <your file>`; ISO, GCM, RVZ, WIA, GCZ, CISO, WBFS, NFS, TGC). New recipe kinds are validated strictly in the catalog: arguments are flags or placeholders only, environment values only the prepared folder, no `PATH`/`LD_*`/`XDG_*`/`QT_*`/`SDL_*`.
- [ ] Still manual, and why (2026-11-24): Diddy Kong Racing (own ROM library, "Play → Add a ROM…"); Glover (data folder not traceable in the published source); Open Nectar (installs game data into a folder you choose); Super Mario Strikers and Dusklight (pick the disc in their own launcher; no disc argument found); MediEvil (disc files with fixed names in a `disc` folder inside its install: possible later, needs .cue track names rewritten); Bloody Roar 2, Ninja Gaiden Black, Lost Odyssey, The Simpsons Game (first-run importer that remembers the file); Shadow (`--install` reruns its installer); Super Metroid and Mega Man X (shared SNES runtime, ROM argument not confirmed).
- [x] Saves in a launcher's install folder survive uninstall and update (validated 2026-11-24): each install records the package's own files (`.recompstation-files`); uninstall moves everything else (saves, `portable.txt`, settings the launcher created) to `launcher-data/<project>` in RecompStation's data folder, and the next install puts them back unless the new package ships a file of the same name. Installs from before the list keep `saves`, `portable`, `portable.txt`, `userdata`, `mods`, `mod_config` and `mods.json`. Snap's saves now live outside its install anyway.
- [x] The ROM check runs off the UI thread (validated 2026-11-24): the Play line says "Checking your copy…" while a new game file is read in the background, then updates; results are cached by path, size and modification time.
- [x] Recipes come from the source, not release notes: for an N64Recomp launcher, the program id (`set_program_id` / `project_name`), the game id (`GameEntry.game_id`, stored as `<game_id>.z64`) and `rom_hash` (XXH3 of the big-endian ROM); for others, the command-line parser and how the data folder is chosen (environment variables, portable mode). Shallow, blob-filtered clones keep this quick: `git clone --depth 1 --filter=blob:limit=200k --no-checkout`, then `git grep` on `HEAD`.

### 2. Catalog, platforms and file formats
Source list: playported.com (about 1,200 entries; it names repositories but not owners, so each owner is found separately). A project is added only when its GitHub releases carry a native Linux build (AppImage, tar.gz or zip) that the installer can use; Flatpak-only projects are skipped until RecompStation can run them (see the Flatpak item below).
- [x] Super Metroid and Mega Man X Recompiled get the ROM from RecompStation (`--rom <file>`, new `fileargument` recipe for cartridge ROMs) instead of asking for it in their launcher (Super Metroid validated 2026-11-25: plays seamlessly; Mega Man X not yet tested).
- [x] File names with several regions, such as "Super Metroid (Japan, USA) (En,Ja)", now match covers and game information; the region cleanup only handled one region (branch `fix/super-metroid`, needs validation).
- [x] Batch 1 (merged; Shadow the Hedgehog confirmed, the rest pending testing): Xbox 360 as a library platform (XDVDFS detection, metadata mappings); PS1: MediEvil, Bloody Roar II; Xbox: Shadow the Hedgehog, Ninja Gaiden Black; Xbox 360: Lost Odyssey, The Simpsons Game. Projects that publish only pre-releases are supported (`includePrereleases`, and an automatic fallback when a project has no full release).
- [ ] Next batches from playported.com: more PS1 (Castlevania: SotN, Dino Crisis 1/2, Ape Escape), Xbox (Black, Jet Set Radio Future, SoulCalibur II), Xbox 360 (Spider-Man: Edge of Time, Ninja Gaiden II).
- [ ] Skipped for now: Blue Dragon (re:Blue's setup copies the program to a folder of the user's choice, outside the managed install), Gran Turismo 2 (Windows only), SSX 3 (Linux build from source only).
- [ ] Waiting for an official Linux release: Donkey Kong Country (elliotttate/DKC1Recomp, Windows and macOS only as of v0.0.17; a Linux build is proposed upstream in pull request #8). An unofficial Linux port (victor-oliveira1/DKC1Recomp-Linux) exists but is several versions behind, so it is not used: the catalog takes official releases only (decided 2026-11-25). When Linux releases appear, the recipe is ready: it takes the ROM as its first argument (`fileargument` with `{gamePath}`), Donkey Kong Country (USA, 1.0).
- [x] LaunchBox: a catalog downloaded before a system was supported has none of its games. Settings › Metadata now names those systems on the LaunchBox card and points to Update catalog (`missingSystems`, from the systems that have games in the catalog; branch `feature/quick-tasks`, needs validation).
- [x] Six more systems (merged; in the UI, confirmed): Game Boy, Game Boy Color, Game Boy Advance, SNES, PlayStation 2 and Wii U, each with library folder, file types, detection when adding games, TheGamesDB and LaunchBox mappings and a cover shape (GB/GBC/GBA 0.92, SNES 7:5, PS2 and Wii U 5:7). Detection: the Game Boy header's CGB flag tells GB from GBC (also inside .zip/.7z); `SYSTEM.CNF` (`BOOT2`) tells PS2 discs from PS1, in `.iso`, `.bin` and `.cue`; a CHD's metadata tells DVD (PS2) from CD (PS1); `.wua`/`.wud`/`.wux` are Wii U (no longer listed under Wii).
- [x] Batch 2 (merged; Bomberman 64 and Bomberman Hero confirmed, the rest pending testing): SNES: Super Metroid (precompiled zip, `Launch.sh`) and Mega Man X (AppImage); N64: Bomberman 64, Bomberman Hero (pre-releases), Harvest Moon 64, Snowboard Kids 2, Space Station Silicon Valley and Quest 64 with `n64recomp` launch recipes read from each project's source, plus Snowboard Kids and Glover-R (own launchers, manual). Where a release file name couldn't be confirmed, the pattern matches any Linux x86-64 zip of that project and excludes ARM64, debug, Flatpak, AppImage, Windows and macOS builds.
- [ ] Skipped in batch 2: Aladdin (AgrabahPort) and Gundam Wing: Endless Duel (their Linux releases are setup packs that compile the game from the ROM on first run with an ~800 MB toolchain); Dr. Mario 64 (Linux build from source only); Pilotwings 64, Rocket: Robot on Wheels, Mortal Kombat Trilogy (repositories not found yet).
- [x] Research for GB/GBC/GBA, PS2 and Wii U (2026-11-24). Ready to add (ready-to-run Linux builds that ask for the game themselves): Final Fantasy Tactics Advance (GBA, `FFTARecomp-*-linux-x64.zip`), Dragon Ball Z: Budokai Tenkaichi 3 (PS2, `BT3-Recomp-linux-x86_64.tar.gz`), Goemon's Great Adventure (N64, pre-releases, `n64recomp`: `GGARecompiled`, `gga.us`, `A62841FD58D33B4E`), and Gen1Recomp (Pokémon on GB, GBC and GBA in one AppImage; needs a catalog field for extra systems). Windows only or build from source on Linux: Link's Awakening DX, mstan's GBA ports (Minish Cap, Emerald, FireRed/LeafGreen, Mega Man Zero), Tekken Tag Tournament 2.
- [ ] Add the four ready projects above, with a catalog field that lets one project cover several systems (Gen1Recomp).
- [ ] Decide on projects that compile the game on the player's machine at first start (a downloaded or bundled compiler): The Wind Waker HD (the only Wii U option), God of War (PS2, experimental), Boktai (GBA, also needs a BIOS dump), Aladdin and Gundam Wing (SNES). Either list them with a "builds on first start" note or keep skipping them.
- [ ] Many projects publish a `recomplauncher.json` (RecompLauncher's manifest: start program, arguments such as `--rom {game}`, config and save folders). Reading it could give launch recipes for projects that take the game file as an argument (Aladdin, Super Metroid).
- [x] Archive extraction safety reviewed (2026-11-24): absolute, `..`, backslash and drive-letter paths, symlinks, hardlinks and special files, duplicate paths, declared sizes, a 16 GB total and 100,000-entry cap, setuid/sticky and group/other-write bits dropped, and one level of ZIP-around-tar.gz unwrapping (two packages refused). The code needed no change; regression tests now cover each case except the size and entry caps, which are too large to build in a test.
- [ ] Projects whose only Linux release is a Flatpak, such as Unleashed Recompiled (Sonic Unleashed, Xbox 360). Players shouldn't have to install Flatpak themselves for one or two games. To work out: what a `.flatpak` bundle needs to run (the Flatpak runtime it names, OSTree contents), whether its program can be unpacked and run against the system's libraries, or whether RecompStation should use Flatpak where it's installed (`flatpak install --user` of the bundle) and explain the one-time setup where it isn't. Which projects are Flatpak-only today.

### 3. Launcher lifecycle and integrity
- [x] Track installed launcher versions and show them in Game Details: each install records its release tag, asset, checksum result and install time in `.recompstation-release`; Game Details shows it in one "Version" row with any newer release ("v1.0.1 · v1.0.2 available"); installs from before this show "Unknown" (validated 2026-11-24).
- [x] Simpler Recompilation pane in Game Details: Status, Version and a short "Requires" line naming the game file (each catalog project has a `romFile` such as "Banjo-Tooie (USA) ROM"; the long notes stay in the catalog). The package file name, install path and checksum wording were removed; the install folder is in the ⋯ menu (validated 2026-11-24).
- [x] (Validated 2026-11-24.) Update flow with rollback-safe replacement: when the latest release differs from the installed one, Game Details offers "Update to <tag>" (not while the game runs). The new version is unpacked beside the old, the old steps aside into a hidden `.<id>.previous-*` folder, the new one moves in and is checked, then the player's files (saves, settings, portable mode) move across; if a package file has the same name, the new one wins and the player's copy goes to `launcher-data/<id>`. Any failure puts the old version back. At startup, an update cut short is finished or undone (branch `feature/launcher-lifecycle`, needs validation).
- [x] Leftover downloads (partial files, extraction folders, archives never installed) are cleared from the staging area at startup; links are removed as links and unknown entries left alone (branch `feature/quick-tasks`, needs validation).
- [x] Clearer release and architecture explanations when no compatible asset exists: Game Details says what the release offers instead (Windows/macOS/Android only, Linux for ARM only, Linux only as a Flatpak, no downloads, or files RecompStation doesn't recognise) (branch `feature/launcher-lifecycle`, needs validation).
- [x] Verify published SHA-256 checksums: GitHub publishes a `digest` for each release asset; downloads are hashed as they arrive and a mismatch is refused and not kept. Releases without a digest install as "unverified", shown in Game Details. Signatures (GPG/minisign) are not checked: no catalog project publishes them today (branch `feature/launcher-lifecycle`, needs validation).
- [x] Tests for update failure, recovery and version state: update keeping saves, broken and incomplete packages leaving the old version intact, refusing folders RecompStation didn't make, recovery in both interrupted states, digest parsing, hashing and the no-package explanations.

### 4. Metadata follow-ups
- [x] Use fewer TheGamesDB requests: the platform list is reused for 30 days, screenshots are looked up 20 games per request and games without any are not asked again for 30 days, automatic lookups pause at 50 or fewer requests left, and a 403 pauses them for 6 hours (validated 2026-11-24).
- [x] Key policy: each user supplies their own TheGamesDB key; the app never ships one (TheGamesDB support, 2026-11-24).
- [x] Tests for the platform list cache, request allowance and screenshot checks (2026-11-24).
- [ ] Fix match: a Game Details action to search either provider and pick the right game.
- [ ] Bounded retry and backoff for HTTP 429/5xx on metadata and artwork requests.
- [ ] Regression tests for match-confidence rules and monitoring duplicate events.

### 5. Build and test performance
- [x] Shared `recompstation_core` static library: the app and every test link it, instead of each test recompiling the sources it uses (was 98 compilations of 29 files, now 29).
- [x] ccache when installed; mold, then lld, as the linker when the compiler accepts it (`-DRECOMPSTATION_FAST_LINKER=OFF` to opt out). `mold` added to the Arch dependency script.
- [x] Tests run in parallel (`ctest -j`, `JOBS` to override); `build.sh` prints configure, build and test times.
- [x] Fixed waits removed from the slowest tests without changing what they check: the library watcher's debounce is 50 ms in tests (750 ms in the app), and the launch tests' scripts sleep 0.3 s instead of 1 s.
- [x] Measured (2026-11-24, `scripts/slowest_steps.sh`): an edit to Main.qml (9,100 lines) rebuilds in 3 s wall clock: qmlcachegen 1.0 s, compiling its generated C++ 2.0 s, linking 0.1 s. Not worth splitting Main.qml for build speed; revisit if an edit ever takes over about 5 s. A build with nothing changed takes 0 s, and the 25 test suites take 1 s.

### 5b. Runtime log — done
`runtime.log` is written for whoever reads a bug report: a session header (version and commit, system, desktop and session type, display, settings, library counts, installed launchers, data and log paths) and footer, one line per event (`2026-11-23 15:14:02.239 INFO  launch    …`), the home folder written as `~`, and the renderer (graphics API and OpenGL driver) once the window is up. Per-frame and per-request chatter (geometry, screenshot rotation, every metadata query) is debug-only; debug is off unless Settings › Application › Detailed logging is on or `RECOMPSTATION_DEBUG=1` (`QT_LOGGING_RULES` still overrides). Launcher output is captured to `logs/launchers/<project>.log` and, when a launcher crashes or exits with an error, its last 40 lines are copied into `runtime.log`.

### 6a. Validation follow-ups (2026-11-24)
- [x] Game Details ⋯ opens a drop-down menu under the button: Show install folder, Open launcher log (the folder of logs before a first start), and Uninstall… set apart in red, still asking first. D-pad moves inside it, A picks, B or Esc closes it and focus returns to ⋯. Built from the approved mockup; validated 2026-11-24.
- [x] Game Details without game info: the hero layout with a "No game info" chip, the file name under the title, Find game info (the main action when there is no launcher) and Metadata settings as styled buttons, an About card explaining what happened and how to fix it, matching Recompilation card text, three empty screenshot frames, and an accent glow behind the page when there is neither a screenshot nor a cover. Built from the approved mockup; validated 2026-11-24.
- [x] Placeholder covers drawn at each system's box shape in every view, replacing the generic portrait picture: a plain dark tile with the logo faintly in the middle. The system band and title from the mockup were dropped after review (2026-11-24): the title and system are already shown around the cover. Shelves cards have no caption, so their placeholders show the title (`placeholderTitle`). Validated 2026-11-24.
- [x] Validated 2026-11-24: Game Details without game info (layout, chips, Find game info and Metadata settings, About and Recompilation cards, screenshot frames) and placeholder covers in Shelves (with the title), Grid, List, Carousel and Game Details.
- [x] The ⋯ menu: Show install folder, Open launcher log, Uninstall… and its confirmation; validated 2026-11-24.

### 6. Input, accessibility and regression
- [x] Contrast and accessible names (validated 2026-11-24): textMuted lifted per theme and Midnight Violet's accent lifted to meet WCAG AA 4.5:1 where they are used; the Library views tell screen readers the selected game and system; the header Library and Settings buttons, Game Details' back button and the × on mapping chips are named; the screenshot viewer hint follows Text size. Tab order: every control is reachable (audit 2026-11-24).
- [x] Scaling review at Large text and Spacious (2026-11-24): only Grid clipped; Shelves and Carousel are fine.
  - [x] Grid: the selected card in the first column was clipped on the left (its 3.5% grow and focus ring spilled past the grid). Both grids now leave a 14 px left inset, matching the gap on the right (validated 2026-11-24).
- [x] Button-mapping capture states (validated 2026-11-24; no sounds while mapping, by choice): the row shows let go first, listening with a 15-second countdown, replacing (old inputs struck out), added (with a warning when the input is already used by another action) and nothing pressed; other rows dim; Settings-style buttons (`SettingsButton.qml`). Fixes Replace showing "Listening…".
- [ ] Multi-display placement.
- [x] Interface sounds (validated 2026-11-24): the "Glass, refined" set, synthesized by `tools/sounds/generate.py` and played through SDL2 audio with a small mixer (Qt Multimedia 6.12 reported playback but stayed silent on PipeWire, 2026-11-24). Move on every directional input that moves something, Open a game, Back, a swoosh for cycling with the shoulder buttons, the switch sound for picking a system, menus, switches, Play and launch errors. Settings › Appearance › Sounds: on/off and volume (default on, 60%). Silent while a game is running.

### 7. Desktop environment compatibility
Developed and tested on KDE Plasma (Wayland). Before a public release, check the release files (AppImage and zip) on a Debian-based system first, then on VMs of mainstream distributions with XFCE, Cinnamon and MATE (all X11-based), and GNOME:
- [ ] The AppImage and the portable zip start, show the right icon and name in the taskbar, and restart correctly after an interface scale change.
- [ ] Fullscreen on launch and F11 toggling, maximized and windowed sizes, window placement.
- [ ] The shader wave background under each compositor or none (XFCE and MATE may run without compositing), and the software-renderer fallback.
- [ ] High-DPI and fractional scaling, including the interface scale's Auto mode under X11.
- [ ] Controller hot-plug and input focus while a launcher runs; drag-and-drop from each file manager (Thunar, Nemo, Caja, Nautilus); opening folders with `xdg-open`.

### 7b. Packaging — Linux x86_64 (started 2026-11-24)
Two release files, both with Qt included, built by GitHub Actions on Ubuntu 22.04 (glibc 2.35, so they run on Debian 12, Ubuntu 22.04, Fedora 36 and newer) with Qt 6.10 from the Qt installer (6.9 or newer is needed for the window icon on Wayland). x86_64 only; ARM later.
- [x] `scripts/package.sh`: Release build (tests with `--test`), installed into an AppDir, Qt and libraries bundled with linuxdeploy and its Qt plugin (X11 and Wayland, SQLite driver only), then packed twice: `RecompStation-<version>-x86_64.AppImage` and `RecompStation-<version>-linux-x86_64.zip` (the same files with a `RecompStation` start script, a README and the desktop entry), plus `SHA256SUMS`. Libraries are found through RPATH and `qt.conf`, so no `LD_LIBRARY_PATH` reaches the games RecompStation starts; the AppImage's own variables (`APPDIR`, `APPIMAGE`, …) are removed from launchers' environment too (branch `feature/packaging`, needs a first CI run).
- [x] `.github/workflows/release.yml`: run by hand from the Actions tab for test files (workflow artifact), or by pushing a tag `v<version>` matching `CMakeLists.txt`, which drafts a GitHub release with both files to review and publish by hand.
- [x] Desktop entry and icons installed by CMake (`io.github.MetalSJ13.RecompStation`); the app sets the same desktop file name so Wayland shows its icon; the version shown comes from `CMakeLists.txt`; restarting from inside an AppImage restarts the AppImage file.
- [x] First CI run passed (2026-11-24): build with GCC 11 and Qt 6.8.3, all 25 test suites, AppImage 59 MB, zip 65 MB, Wayland and X11 plugins bundled. Qt from the installer needed its unused SQL drivers removed together with their CMake entries.
- [x] Both files tested by the user (2026-11-24): start, versions, update check and install.
- [x] Debian 13 VM with XFCE: the AppImage and the portable zip both run well (validated 2026-11-24).
- [x] Taskbar icon on KDE Wayland (fixed, validated 2026-11-24) showed a generic "W": Qt 6.8 can't send window icons on Wayland and the desktop entry isn't installed when running the AppImage or zip. The release build now uses Qt 6.10, which sends the icon (KDE Plasma 6.3+). GNOME doesn't support that protocol and needs the desktop entry installed; an "Add to application menu" option is not planned for now (decided 2026-11-24).
- [ ] Test both files on the Debian-based device: start, library, install and play a game, a launcher update, interface scale restart, controller and sounds.
- [x] Third-party license notices in both files: `LICENSE`, `THIRD_PARTY_NOTICES.md` (Qt under LGPLv3 with its source link, SDL2, libarchive, xxHash, plus the Qt version and the list of bundled libraries, written at packaging time) and `licenses/LGPL-3.0.txt`, at the top of the zip and AppImage and in `usr/share/licenses/RecompStation`; system libraries' copyright files are under `usr/share/doc` (branch `feature/license-notices`, needs validation).
- [ ] Validate that supported games install and play with their launchers from the release files (not only from a development build).
- [ ] AppImage FUSE: check whether the AppImage starts on Ubuntu 24.04 without `libfuse2t64`; if not, document it (already in the release notes) or switch to the static AppImage runtime.
- [ ] AppStream metadata (`.metainfo.xml`) for software centres and AppImage catalogues.
- [ ] Application updates (section 9) once releases exist.

### 8. Windows support — decided, after Linux packaging
Decided 2026-11-24: Windows becomes a supported system, with `.exe` builds once the AppImage and portable zip work well. Work to plan when it starts:
- Builds: a GitHub Actions `windows-latest` runner with MSVC and Qt from aqtinstall, packaged with `windeployqt`. No Windows PC is needed to build; a Windows VM is useful for testing. Cross-compiling from Linux (MinGW/MXE) is possible but harder to keep working with Qt 6 and SDL2.
- Code: the catalog needs Windows asset matching (`.zip`/`.exe`) alongside Linux; Linux-specific parts need Windows versions: paths (XDG folders → `%LOCALAPPDATA%`), launching (AppImage, `./run.sh`), file permissions (the owner-only key file), `xdg-open`, and the build scripts.
- Release: a portable zip first, an installer later; unsigned builds trigger SmartScreen warnings until code signing is set up.
- Testing cost: every feature needs checking on both systems.

### 9. Application updates — deferred
The first release, v0.1.0, was drafted on 2026-11-24. When it does: a manual "Check for updates" and an optional daily check that compares the installed version with the latest release and links to the release page; never download or install automatically. The Settings switch and the first-run question were removed until this exists.

### 10. Documentation and licensing — done
GPL-3.0 `LICENSE`; `README.md` with the no-game-files notice at the top, features, screenshots (metadata stripped, see `docs/screenshots/README.md`), building from source and file locations; the original logo (pixel d-pad) in `assets/logo/`, used in the header, first-time setup, the README and as the window icon. Follow-up when packaging: a `.desktop` file and hicolor icons so Wayland taskbars show the icon.

## Key decisions

- Controller-first, library-first, and fullscreen by default; RecompStation keeps its own visual identity (RecompLauncher, GPL-3, was used only as a reference).
- Artwork comes from LaunchBox and TheGamesDB box art; no other artwork provider is planned.
- TheGamesDB keys are supplied by each user; the app never ships one (TheGamesDB counts allowances per key and advised against bundling, 2026-11-24).
- Metadata is matched only when one result clearly fits; ambiguous games stay unmatched rather than getting the wrong cover.
- RecompStation never supplies game files and never overwrites or deletes the user's games; uninstalling a launcher keeps ROMs and saves. One exception, at the user's request: a .zip/.7z in a cartridge system's folder whose only file is a ROM is removed after that ROM has been unpacked next to it and its size checked; archives with other files are kept.
- RecompStation never creates a launcher's settings files; it changes single documented keys in files the launcher wrote itself.
- Launchers come from native Linux release assets of curated projects. Flatpak-only projects are an open question (Catalog, platforms and file formats), not ruled out: players shouldn't need to install Flatpak themselves.
- No settings without real behaviour: placeholder controls are removed until their feature exists.
- Releases: an AppImage and a portable zip for 64-bit Linux (x86_64), Qt included, built by GitHub Actions; ARM later. Windows `.exe` builds follow once the Linux files work well (2026-11-24).
- Launcher updates replace the installed version only after the new one is unpacked and checked, keep the player's files, and roll back on failure; downloads are checked against the SHA-256 the project publishes on GitHub (2026-11-24).

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
