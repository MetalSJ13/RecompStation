# RecompStation — Project Context

## Project objective

RecompStation is a native desktop game-library frontend for managing and launching native game recompilation projects. It is conceptually similar to EmulationStation, but instead of managing emulators, it manages supported recompilation launchers, their releases, installations, and launch configurations.

Users can organize games by console platform, scan local game directories, associate metadata and artwork, discover compatible recompilation projects, install their launchers, and launch supported games from their library.

## Technology stack

- Language: C++20
- Framework: Qt 6.5 or newer
- UI: QML / Qt Quick
- Build system: CMake (Ninja supported by build scripts)
- Database: SQLite
- Networking: Qt Network
- Process management: QProcess
- Primary development environment: Arch Linux / KDE Plasma

## Initial platform coverage

- Nintendo 64 (N64)
- PlayStation 1 (PS1)
- Nintendo GameCube
- Nintendo Wii

Platform support is not equivalent to universal recompilation support. A game is launchable only when a compatible project is represented in the curated project catalog and the user's game/ROM meets that project's requirements.

## Core capabilities

1. Platform directory creation and game-directory scanning.
2. Local game-library management backed by SQLite.
3. Metadata search, platform mapping, and metadata-to-ROM association.
4. Cover-art retrieval and local caching.
5. Curated recompilation project catalog.
6. GitHub repository and release discovery.
7. Architecture-aware launcher asset selection.
8. Safe launcher download, extraction, installation, discovery, and uninstall.
9. Native launcher process execution.
10. Project-specific launch configuration, launcher updates, and optional release integrity verification (planned).

## Architecture

The application uses a modular C++ backend and QML frontend.

Implemented backend areas:
- `PlatformManager`: supported platform directory setup and platform information.
- `LibraryScanner`: recursive game-file discovery and registration.
- `DatabaseManager`: SQLite persistence for games and metadata associations.
- `MetadataProvider` / `TheGamesDbClient`: asynchronous metadata and platform catalog requests.
- `MetadataPlatformMapping`: mapping from RecompStation platform identifiers to provider platform IDs.
- `MetadataManager`: metadata search state, results, configuration, and QML-facing integration.
- `ArtworkManager`: asynchronous artwork download and local caching.
- `GitHubManager`: asynchronous public repository and latest-release retrieval.
- `ProjectCatalog`: bundled, schema-validated catalog of supported recompilation projects and their platform/asset rules.
- `ArchiveExtractor`: safe extraction of supported archive formats.
- `LauncherDownloadManager`: staged downloads, installation validation, managed launcher discovery, and uninstall.
- `GameLaunchManager`: launch of managed executables as separate processes and process-state/error reporting.

The UI is implemented in QML. The current interface is functional but is now the focus of a dedicated refinement milestone. The intent is to establish shared design tokens, reusable components, clear navigation, and polished workflows without destabilizing the working backend.

## Scanner formats

The scanner currently recognizes these extensions:

- N64: `.z64`, `.n64`, `.v64`
- PS1: `.cue`, `.chd`, `.pbp`, `.iso`, `.bin`, `.img`
- GameCube: `.iso`, `.gcm`, `.rvz`, `.ciso`
- Wii: `.iso`, `.wbfs`, `.rvz`, `.wud`, `.wux`

PS1 `.bin` and `.img` companion files are skipped when a `.cue` file is present in the same directory. Detection is extension-based; the scanner does not yet prove file integrity or ROM revision compatibility.

## Current implementation status

The following workflows are implemented in the current development baseline:

- Recursive scanning of configured platform folders, with duplicate-path protection.
- Asynchronous TheGamesDB title search and runtime platform catalog retrieval/mapping for the four initial platforms.
- Association of provider metadata with an individual local game record without overwriting the scanned title or path.
- Front-cover URL parsing, HTTPS artwork download, local cache storage, and cover display in the library.
- Asynchronous GitHub public repository/latest-release requests and release asset parsing.
- A versioned bundled project catalog with an initial Banjo: Recompiled entry for N64 Banjo-Kazooie.
- Normalized metadata-title/platform matching to catalog entries.
- Linux x86_64 and ARM64 release-asset filtering using catalog filename rules.
- Staged downloads, safe extraction of supported `tar.gz` and `tar.xz` packages, executable validation, managed installation, installed-launcher rediscovery, and confirmation-protected uninstall.
- A Play Game action that launches a validated installed executable as a separate process and reports running/finished/failed states.

The initial Banjo: Recompiled catalog entry records its North American 1.0 ROM requirement. Current matching uses associated metadata title and platform; it does not hash or inspect ROM contents, so a title match must not be presented as proof of ROM compatibility.

Regression tests have been added for the scanner, database associations, metadata provider/mapping, artwork, GitHub parsing, project catalog, archive extraction, launcher installation/management, and process launching. The full suite should be run against the current branch before merge.

## Development direction and roadmap

The immediate priority is UI/UX refinement. The current launcher-download and native-launch implementation remains unmerged while this work proceeds on the dedicated `ui/interface-redesign` branch. Keep `main` stable until the feature set is reviewed and the merge is explicitly approved.

Development order:

1. **UI/UX audit and design foundation:** inspect all current pages and interactions; agree on visual direction, theme tokens, component patterns, navigation, and window behavior.
2. **Application shell and navigation:** establish a consistent main window, navigation model, page layout, and reusable QML patterns.
3. **Game Library:** refine game cards/artwork, filtering, metadata/project status, and launch states.
4. **Metadata and ROM management:** refine search, platform selection, association, scanning, and feedback states.
5. **Launcher Discovery and installation:** refine project/release selection, architecture guidance, download/install progress, errors, and uninstall.
6. **Settings and first-run experience:** add clear configuration and setup workflows and consistent confirmation/notification patterns.
7. **Accessibility and regression:** verify keyboard/focus behavior, contrast, scaling, responsive layouts, full build, tests, and manual workflow checks.
8. **Launcher lifecycle completion:** installed version tracking, update discovery, rollback-safe replacement, and staging recovery.
9. **Project-specific launch configuration:** catalog-driven arguments, ROM path handling, and compatibility validation where reliable.
10. **Optional integrity verification:** verify publisher-provided checksums/signatures against a documented trust mechanism. Missing verification metadata alone must not block installation; failed verification should stop installation by default.
11. **Catalog and platform expansion:** add projects and package formats only after validating their release, architecture, package layout, executable, launch behavior, and ROM requirements.

The detailed checklist, build workflow, development principles, and definition of done are maintained in `DEVELOPMENT.md`.

## Development guidelines

- Keep application/business logic in C++ and presentation/navigation in QML.
- Prefer small, reusable QML components and focused pages; avoid a monolithic main view.
- Keep project-specific behavior in catalog/configuration data where practical, not hardcoded into shared launcher code.
- Preserve existing functionality when introducing UI or backend changes.
- Keep each change focused and the application buildable throughout development.
- Add regression tests when backend behavior changes.
- Do not routinely delete or recreate the local build directory; incremental builds should remain the normal development path.
- Never claim ROM compatibility solely from a metadata title match.
- Do not treat the absence of a publisher checksum/signature as an installation failure.

## Build workflow

From the repository root:

```bash
./scripts/build.sh
./scripts/build.sh --test
```

The script reuses the configured build directory and generator. Release configurations can be selected with `./scripts/build.sh Release` or `./scripts/build.sh RelWithDebInfo`. See `DEVELOPMENT.md` for dependency details and the full development roadmap.
