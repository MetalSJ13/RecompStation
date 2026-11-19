# RecompStation — Project Context

## Objective

RecompStation is a native desktop environment for browsing and launching the user's own collection of games with supported native recompilation projects. It manages game libraries and recompilation launchers rather than emulators or a storefront.

## Technology

- C++20
- Qt 6.5+
- Qt Quick / QML
- CMake and Ninja
- SQLite
- Qt Network and QProcess
- Primary development environment: Arch Linux / KDE Plasma

## Initial platform coverage and expansion plan

Current library platforms are Nintendo 64, PlayStation 1, Nintendo GameCube, and Nintendo Wii. Startup previously passed the recompilation project's supported-platform list into PlatformManager, which limited library scanning to N64 because the current native project catalog is N64-only. Startup now initializes the independent library platform defaults instead. The fix is committed on `ui/interface-redesign`; local build/runtime verification is still required.

The next platform-expansion work is to make library platform discovery/folder creation independent of the curated recompilation-project catalog, verify GameCube and Wii detection, and add original Xbox support with clearly defined disc-image formats. Xbox 360 is a later investigation: establish the correct supported disc/container format and reliable detection rules before implementing it. Library detection support does not imply that a native recompilation project or launcher exists for a game; those remain separate capabilities.

## Current development state

**Active branch:** `ui/interface-redesign`  
**Merge policy:** Keep this branch separate from `main` until the user explicitly approves a merge. The preceding `feature/launcher-download-staging` branch is retained as a backup.

Implemented capabilities include:
- One configurable library root, catalog-derived platform subfolders, first-run setup, and recursive game scanning
- Persistent startup library scan preference and recursive filesystem monitoring with debounced automatic reconciliation
- Automatic metadata matching after library scans, with persistent lookup outcomes, explicit retry support, and stale failed-lookup reset when an unassociated ROM reappears (latest changes await local build/runtime verification)
- SQLite game-library records
- TheGamesDB asynchronous search and runtime platform mapping, with a persistent provider enable/disable preference
- Metadata association to local ROM records
- Artwork retrieval, local caching, and library thumbnails
- Curated N64 recompilation/native-port catalog covering 16 projects, including Star Fox 64, Donkey Kong 64, Mario Kart 64, the three Mario Party titles, Super Smash Bros., Ghostship, Banjo-Tooie, and Paper Mario
- GitHub release discovery and architecture-aware Linux asset filtering
- Staged launcher downloads, safe archive extraction, installation validation, managed launcher discovery, and uninstall
- Native process launch with running/finished/error state
- Regression tests for scanner, database, metadata, artwork, GitHub, catalog, archive, launcher management, and process launching

The project catalog now includes ten additional N64 projects. Linux release discovery is filtered to verified Linux archive assets for the detected CPU architecture; Windows and Flatpak packages are excluded. ROM revision/hash requirements are documented per project, and Super Mario 64 exposes both the existing n64hub recompile and HarbourMasters Ghostship as separate choices in Game Details.

The library Settings page uses one local root folder. RecompStation currently creates platform subfolders from the platforms represented in the supported project catalog, and the scanner searches those subfolders. First-run setup asks the user to choose the root before scanning; changing it triggers a full library scan. The startup scan preference remains persistent. Planned platform expansion must decouple folder creation from the recompilation-project catalog so GameCube, Wii, and original Xbox folders are reliably created even without compatible projects. Xbox 360 support remains conditional on identifying and validating the correct game format(s).

Current scanning otherwise remains user-invoked; The active UI branch now connects the Scan Library action to a sequential automatic metadata matcher. Its initial policy accepts only a strong normalized-title match within the mapped platform; ambiguous and missing results remain unassociated. Lookup outcomes and provider failures are persisted to prevent repeated requests during automatic scans. Explicit Refresh Metadata retries previously checked games. Accepted metadata triggers the existing artwork cache. Local build and regression-test verification are pending.

Continuous filesystem monitoring now has an initial implementation: LibraryScanner watches the configured platform directories and their existing subdirectories, debounces directory changes, rescans the library, and refreshes watches so newly created nested folders are included. The initial regression tests cover nested-folder additions, file removals, and stopping/restarting monitoring. This implementation is committed on the active branch but still requires local build, test, and runtime validation.

## Product and UI direction

The product experience is defined in `docs/UI_DESIGN.md`.

Primary navigation is limited to:
1. **Game Library** — default landing view; browse all games, filter by platform, optionally search/sort, open game details, and initiate a manual scan/scrape.
2. **Settings** — appearance, display, library, metadata, launcher, and application configuration.

Metadata Search is removed as a standalone destination. Platform selection becomes part of the Game Library. Launcher discovery/installation should be surfaced contextually from game details or supported-project state rather than as a separate primary destination.

The visual direction is a polished, dark, immersive personal collection experience. It must not resemble a storefront or directly copy Steam Big Picture or another launcher. Use restrained blue-violet/electric-blue accents, strong artwork presentation, consistent typography and spacing, smooth purposeful transitions, and visible selection/focus states.

### Settings experience

The Settings navigation has been refined with a wider, more readable category panel and bundled SVG icons for Appearance, Display, Library, Metadata, Controller, and Application.

The Metadata page now exposes a persistent TheGamesDB enable/disable checkbox and clearly labels ScreenScraper, Hasheous, and IGDB as not yet integrated or still under evaluation. Disabling TheGamesDB prevents initial platform-catalog requests, subsequent game-search requests, and new gameplay-screenshot refreshes; provider selection is initialized from the saved preference. Provider priority, fallback, and separate artwork-source selection remain future work.

The Controller category currently provides live connected/disconnected status, controller name/type, and SDL support availability. Per-device profiles and button remapping are still planned and must not be represented as available functionality. This Settings pass is awaiting local build and runtime verification.

### Library presentation

The current Library UI has a curated Grid/List presentation, adjustable grid density, a compact Library/Settings header, and a device-adaptive navigation hint panel with mouse, keyboard, and controller icons. The user has confirmed arrow-key grid navigation, Enter/Return selection, Escape back from game details, and controller hot-plug detection. The latest tested UI still needs broader accessibility and Settings navigation validation. Controller remapping is planned as per-device profiles keyed by hardware GUID; it has not yet been implemented.

The library presentation now offers curated Grid and List modes, three responsive grid-density presets, and a library-specific background treatment: static theme-coordinated (default), optional animated ambience, or a user-selected still image copied into application data. The signature Grid View remains the default; animation respects Reduced Motion and pauses while the window is inactive or minimized.

### Display behavior

Display behavior must remain under the desktop compositor; the initial compatibility target is KDE Plasma on Wayland, with X11/Xlibre and other desktop environments investigated later. The Display settings now detect the current screen and persist preferred window-size presets (including ultrawide and 4:3 options), constrained to the available desktop area. These are application window dimensions only, never monitor-wide mode changes. The saved preset is applied on startup when the window is windowed; the system option leaves the current window size unchanged. Appearance will offer curated static, optional animated, and user-selected custom-image backgrounds, plus scalable font sizes and reduced-motion support.

### Input behavior

Mouse and keyboard remain supported. Controller support will include Xbox and PlayStation defaults with user remapping. RecompStation must relinquish controller input while a launched game session is active and resume only after the actual game process/session exits.

## Metadata automation direction

Metadata is a background library service, not a user-facing manual chore. Manual scanning/scraping and automatic folder monitoring should share one discovery/matching pipeline.

N64 hash groundwork is now present in `N64RomHash`: detect ROM byte order from magic, canonicalize to big-endian, and calculate MD5/CRC32. TheGamesDB `Games/ByGameHash` request integration remains the next step; hash results must be verified against known-good ROMs before they can override conservative title matching.

The intended workflow is:
1. Discover a ROM and platform.
2. Search the provider using title and platform context.
3. Automatically associate only sufficiently confident matches.
4. Keep ambiguous/low-confidence matches unresolved rather than silently associating incorrect metadata.
5. Cache results and artwork, avoid redundant provider calls, and respect API limits.
6. Present exceptional review in the context of the affected game.

## Development workflow

From the repository root:

```bash
./scripts/build.sh
./scripts/build.sh --test
```

The build script reuses the existing build directory and generator. Prefer incremental builds; do not routinely delete or recreate the local build directory.

Keep business logic in C++ and presentation/navigation in QML. Make small, buildable changes, preserve existing backend workflows, and add regression tests for backend changes. The detailed roadmap is maintained in `DEVELOPMENT.md`.
