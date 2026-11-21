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

Current library platforms are Nintendo 64, PlayStation 1, Nintendo GameCube, Nintendo Wii, and original Xbox. Original Xbox scanning recognizes .iso and .xiso candidates in its dedicated folder; .iso files must be Xbox XISO-format images. xemu documents that ordinary full-disc/redump ISOs are not directly compatible until converted/extracted. Startup previously passed the recompilation project's supported-platform list into PlatformManager, which limited library scanning to N64 because the current native project catalog is N64-only. Startup now initializes the independent library platform defaults instead. The fix is committed on `ui/interface-redesign`; local build/runtime verification is still required.

Library platform discovery/folder creation is independent of the curated recompilation-project catalog. GameCube and Wii detection has been user-verified. Original Xbox has been added with a dedicated folder, recursive .iso/.xiso candidate scanning, and metadata platform mapping; local build/runtime verification remains pending. Xbox 360 is a later investigation: establish the correct supported disc/container format and reliable detection rules before implementing it. Library detection support does not imply that a native recompilation project or launcher exists for a game; those remain separate capabilities.

## Current development state

**Active branch:** `ui/interface-redesign`  
**Merge policy:** Keep this branch separate from `main` until the user explicitly approves a merge. The preceding `feature/launcher-download-staging` branch is retained as a backup.

Implemented capabilities include:
- One configurable library root, catalog-derived platform subfolders, first-run setup, and recursive game scanning
- Persistent startup library scan preference and recursive filesystem monitoring with debounced automatic reconciliation
- Automatic metadata matching after library scans, controlled by a persistent Settings preference enabled by default; automatic artwork/screenshot retrieval has a separate persistent preference, also enabled by default. Manual **Refresh All Metadata** explicitly overrides both preferences (latest changes await local build/runtime verification)
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

The library Settings page uses one local root folder. RecompStation creates platform subfolders for supported library platforms, and the scanner searches those subfolders. First-run setup asks the user to choose the root before scanning; changing it triggers a full library scan. The user recently confirmed that correcting an incorrectly selected root allowed the scan to discover existing games and automatically start metadata matching. The startup scan preference remains persistent. Platform folder creation is decoupled from the recompilation-project catalog, so GameCube, Wii, and original Xbox folders exist independently of compatible native projects. Xbox 360 support remains conditional on identifying and validating the correct game format(s).

A full library scan runs after the library root is selected or changed, and the user confirmed that the corrected root discovers existing games and starts automatic metadata matching. Matching uses a sequential pipeline and accepts only a strong normalized-title match within the mapped platform; ambiguous and missing results remain unassociated. Lookup outcomes and provider failures are persisted to prevent repeated requests during automatic scans. The persistent Settings → Metadata preferences independently control automatic matching and automatic artwork/screenshot downloads (both default enabled); **Refresh All Metadata** explicitly overrides them. Recent preference changes still require local build and regression-test verification.

Continuous filesystem monitoring now has an initial implementation: LibraryScanner watches the configured platform directories and their existing subdirectories, debounces directory changes, rescans the library, and refreshes watches so newly created nested folders are included. The initial regression tests cover nested-folder additions, file removals, and stopping/restarting monitoring. This implementation is committed on the active branch but still requires local build, test, and runtime validation.

## LaunchBox metadata fallback

The optional LaunchBox local catalog is integrated as a secondary source in the shared `LibraryMetadataMatcher` queue. Newly discovered games are queued after library scans, and the Settings manual Refresh Metadata action uses the same matching path. TheGamesDB remains primary. LaunchBox is attempted after any successful TheGamesDB lookup that does not produce a confident match, including empty, unrelated, and ambiguous result sets; provider/network/authentication errors do not trigger fallback. Lookup first matches the full normalized ROM filename against platform-specific `Files.xml` associations, preserving region, language, and revision tokens while ignoring the known extension and harmless punctuation/case differences. If no association exists, title matching normalizes known region/language tags, prefers exact revision matches, and accepts revision-insensitive matches only when unique. Competing revision variants remain ambiguous. The fallback inserts metadata only into an empty association and does not supply an artwork URL. A confident TheGamesDB result can later upgrade a LaunchBox association. Catalog updates do not trigger an automatic library metadata retry; after download and extraction finish, Settings reminds users to run the single Universal Library Refresh action. That action retries eligible metadata lookups and then downloads missing TheGamesDB screenshots. Existing screenshot files are preserved and skipped. The separate LaunchBox artwork pass still runs after catalog updates and metadata scrapes, and remains limited to locally matched LaunchBox games. LaunchBox artwork is handled by a separate library-scoped downloader: it uses only saved LaunchBox IDs for matched local games, never iterating the full catalog for image downloads or inferring associations from titles. It selects artwork only for available local games already associated with a LaunchBox DatabaseID; catalog-only games and games without a confident LaunchBox association are never downloaded. It selects one preferred front cover and up to three gameplay screenshots, preserves an existing cover, caches images under the local game's artwork directory, and links cached files back into the library database. Downloads respect the persistent LaunchBox enablement preference and run one request at a time in batches of five with a 300 ms pause between batches. Settings displays separate artwork progress and final downloaded/skipped/failed counts. Refresh runs after catalog updates and metadata scrapes; requests arriving during an active batch are deferred. Implementation is committed on `ui/interface-redesign` and requires local build/runtime validation.

## Product and UI direction

The product experience is defined in `docs/UI_DESIGN.md`.

Primary navigation is limited to:
1. **Game Library** — default landing view; browse all games, filter by platform, optionally search/sort, open game details, and initiate a manual scan/scrape.
2. **Settings** — appearance, display, library, metadata, launcher, and application configuration.

Metadata Search is removed as a standalone destination. Platform selection becomes part of the Game Library. Launcher discovery/installation should be surfaced contextually from game details or supported-project state rather than as a separate primary destination.

The visual direction is a polished, dark, immersive personal collection experience. It must not resemble a storefront or directly copy Steam Big Picture or another launcher. Use restrained blue-violet/electric-blue accents, strong artwork presentation, consistent typography and spacing, smooth purposeful transitions, and visible selection/focus states. The user prefers Midnight Violet and Electric Blue; Ember is currently less successful visually and should be reconsidered in a future theme-design pass rather than changed during the current pause.

### Settings experience

The first-run experience asks whether the user wants automatic application-update checks before showing library-folder setup. Yes/No, the close button, and Escape all persist a completed choice; dismissal is saved as disabled. The preference can be changed later in Settings → Application. This is preference UI and persistence only: automatic release checking is not connected and no network requests are made.

The Settings navigation has been refined with a wider, more readable category panel and bundled SVG icons for Appearance, Display, Library, Metadata, Controller, and Application.

The Metadata page now exposes a persistent TheGamesDB enable/disable checkbox and clearly labels ScreenScraper, Hasheous, and IGDB as not yet integrated or still under evaluation. Disabling TheGamesDB prevents initial platform-catalog requests, subsequent game-search requests, and new gameplay-screenshot refreshes; provider selection is initialized from the saved preference. TheGamesDB remains primary, with optional LaunchBox fallback implemented in the shared matcher; local validation is pending. Separate artwork-source selection remains future work.

The Controller category currently provides live connected/disconnected status, controller name/type, and SDL support availability. Per-device profiles and button remapping are still planned and must not be represented as available functionality. This Settings pass is awaiting local build and runtime verification.

### Library presentation

The current Library UI has a curated Grid/List presentation, adjustable grid density, a compact Library/Settings header, and a device-adaptive navigation hint panel with mouse, keyboard, and controller icons. The user has confirmed arrow-key grid navigation, Enter/Return selection, Escape back from game details, and controller hot-plug detection. The latest tested UI still needs broader accessibility and Settings navigation validation. Controller remapping is planned as per-device profiles keyed by hardware GUID; it has not yet been implemented.

The library presentation now offers curated Grid and List modes, three responsive grid-density presets, and a library-specific background treatment: static theme-coordinated (default), optional animated ambience, or a user-selected still image copied into application data. The signature Grid View remains the default; animation respects Reduced Motion and pauses while the window is inactive or minimized.

### Display behavior

Display behavior must remain under the desktop compositor; the initial compatibility target is KDE Plasma on Wayland, with X11/Xlibre and other desktop environments investigated later. The Display settings now detect the current screen and persist preferred window-size presets (including ultrawide and 4:3 options), constrained to the available desktop area. These are application window dimensions only, never monitor-wide mode changes. The saved preset is applied on startup when the window is windowed; the system option leaves the current window size unchanged. Appearance will offer curated static, optional animated, and user-selected custom-image backgrounds, plus scalable font sizes and reduced-motion support.

### Input behavior

The Library offers Grid, List, and Carousel/Carousel presentations over the same filtered collection. Carousel View presents one horizontal row of moderately sized cards with edge fading and horizontal navigation. Mouse and keyboard remain supported. Controller support will include Xbox and PlayStation defaults with user remapping. RecompStation must relinquish controller input while a launched game session is active and resume only after the actual game process/session exits.

## Metadata automation direction

Metadata is a background library service, not a user-facing manual chore. Manual scanning/scraping and automatic folder monitoring should share one discovery/matching pipeline.

N64 hash groundwork is now present in `N64RomHash`: detect ROM byte order from magic, canonicalize to big-endian, and calculate MD5/CRC32. TheGamesDB `Games/ByGameHash` request integration remains the next step; hash results must be verified against known-good ROMs before they can override conservative title matching.

Automatic metadata matching after library scans and automatic cover-art downloads are independently controlled by persistent Settings → Metadata preferences, both enabled by default. Disabling automatic matching skips matching after scans; disabling automatic artwork downloads suppresses cover downloads during automatic matching. The manual **Refresh All Metadata** action explicitly retries matching and artwork retrieval regardless of these preferences.

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
