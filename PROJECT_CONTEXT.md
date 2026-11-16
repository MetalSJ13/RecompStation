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

## Initial platform coverage

- Nintendo 64
- PlayStation 1
- Nintendo GameCube
- Nintendo Wii

Platform directory/scanner support does not imply every game has a compatible recompilation project. Launchability depends on the curated project catalog and project-specific ROM requirements.

## Current development state

**Active branch:** `ui/interface-redesign`  
**Merge policy:** Keep this branch separate from `main` until the user explicitly approves a merge. The preceding `feature/launcher-download-staging` branch is retained as a backup.

Implemented capabilities include:
- Platform directory setup, configurable per-platform game folders, and recursive game scanning
- Persistent startup library scan preference; live filesystem monitoring remains planned
- SQLite game-library records
- TheGamesDB asynchronous search and runtime platform mapping
- Metadata association to local ROM records
- Artwork retrieval, local caching, and library thumbnails
- Curated recompilation project catalog, initially including Banjo: Recompiled
- GitHub release discovery and architecture-aware Linux asset filtering
- Staged launcher downloads, safe archive extraction, installation validation, managed launcher discovery, and uninstall
- Native process launch with running/finished/error state
- Regression tests for scanner, database, metadata, artwork, GitHub, catalog, archive, launcher management, and process launching

The library Settings page supports changing the local folder for each platform and persisting whether a scan runs at startup. Folder changes trigger a full library scan. Current scanning otherwise remains user-invoked; The active UI branch now connects the Scan Library action to a sequential automatic metadata matcher. Its initial policy accepts only a unique exact normalized-title match within the mapped platform; ambiguous and missing results remain unassociated. Lookup attempts are persisted to avoid repeating completed searches, and accepted metadata triggers the existing artwork cache. This initial implementation is awaiting local build and regression-test verification. Continuous filesystem monitoring remains planned.

## Product and UI direction

The product experience is defined in `docs/UI_DESIGN.md`.

Primary navigation is limited to:
1. **Game Library** — default landing view; browse all games, filter by platform, optionally search/sort, open game details, and initiate a manual scan/scrape.
2. **Settings** — appearance, display, library, metadata, launcher, and application configuration.

Metadata Search is removed as a standalone destination. Platform selection becomes part of the Game Library. Launcher discovery/installation should be surfaced contextually from game details or supported-project state rather than as a separate primary destination.

The visual direction is a polished, dark, immersive personal collection experience. It must not resemble a storefront or directly copy Steam Big Picture or another launcher. Use restrained blue-violet/electric-blue accents, strong artwork presentation, consistent typography and spacing, smooth purposeful transitions, and visible selection/focus states.

Fullscreen is the default launch mode. Supported windowed/borderless alternatives and actual system-reported display modes belong in Settings. Appearance should offer a small curated set of coordinated built-in themes and backgrounds before considering extensive customization.

Mouse and keyboard remain supported. The interface should be structured for future full controller navigation; controller support is planned, not yet complete.

## Metadata automation direction

Metadata is a background library service, not a user-facing manual chore. Manual scanning/scraping and automatic folder monitoring should share one discovery/matching pipeline:
- Discover a ROM and platform.
- Search the provider using title and platform context.
- Automatically associate only sufficiently confident matches.
- Keep ambiguous/low-confidence matches unresolved rather than silently associating incorrect metadata.
- Cache results and artwork, avoid redundant provider calls, and respect API limits.
- Present exceptional review in the context of the affected game.

## Development workflow

From the repository root:

```bash
./scripts/build.sh
./scripts/build.sh --test
```

The build script reuses the existing build directory and generator. Prefer incremental builds; do not routinely delete or recreate the local build directory.

Keep business logic in C++ and presentation/navigation in QML. Make small, buildable changes, preserve existing backend workflows, and add regression tests for backend changes. The detailed roadmap is maintained in `DEVELOPMENT.md`.
