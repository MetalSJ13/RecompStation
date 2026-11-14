# RecompStation - Project Context

## Project Objective

RecompStation is a native game library frontend for managing
and launching recompilation projects.

It is conceptually similar to EmulationStation, but instead
of managing emulators, it manages native game recompilation
launchers downloaded from GitHub.

Users can organize their games by console platform, install
supported recompilation launchers, and launch games directly
from their library.

## Technology Stack

- Language: C++20
- Framework: Qt 6.5 or newer
- UI: QML / Qt Quick
- Build system: CMake
- Database: SQLite
- Networking: Qt Network
- Process management: QProcess (planned)
- Primary development OS: Arch Linux
- Desktop environment: KDE Plasma

## Initial Platform Support

- Nintendo 64 (N64)
- PlayStation 1 (PS1)
- Nintendo GameCube
- Nintendo Wii

## Core Features

1. Automatic platform directory creation.
2. Game directory scanning.
3. Game library management.
4. Game artwork and metadata.
5. GitHub API integration.
6. Automatic recompilation launcher downloads.
7. Launcher installation and updates.
8. Native executable launching.
9. Custom game and launcher directories.

## Architecture

The application uses a modular C++ backend and QML frontend.

Backend modules:
- GameManager (planned)
- PlatformManager
- LibraryScanner
- LauncherManager (planned)
- GitHubManager (planned)
- ProcessManager (planned)
- MetadataProvider (implemented)
- TheGamesDbClient (implements asynchronous game search and platform catalog retrieval)
- MetadataPlatformMapping (parses the provider catalog and maps supported internal platforms)
- MetadataManager (exposes search state, results, and mapped platform options to QML)
- DatabaseManager
- SettingsManager (planned)

The scanner currently supports these file extensions:
- N64: .z64, .n64, .v64
- PS1: .cue, .chd, .pbp, .iso, .bin, .img
- GameCube: .iso, .gcm, .rvz, .ciso
- Wii: .iso, .wbfs, .rvz, .wud, .wux

PS1 .bin and .img companion files are skipped when a .cue file is present in the same directory. Format detection is extension-based; file integrity and game metadata are not yet validated. The TheGamesDB client supports asynchronous title searches and platform catalog retrieval. MetadataManager maps the four supported internal platforms to provider IDs at runtime and exposes a platform selector to QML. The metadata search page displays provider results and supports filtering by the selected platform. Metadata search results can now be associated with an individual local ROM record through a separate game_metadata table. The local scanner title and ROM path remain unchanged; the provider ID and descriptive metadata are stored independently. Artwork downloading and caching remain future work.

## Current Development Status

Milestone 1: Application foundation.

Completed:
- Qt application initialization.
- C++20 and CMake configuration.
- QML main interface.
- Automatic platform directory creation.
- SQLite initialization.
- Initial platform registration.
- Successful application launch on Arch Linux.

Milestone 2: Initial game library scanning.

Implemented in code:
- Recursive scanning of platform game folders.
- Extension-based game-file discovery.
- SQLite registration with unique game-path protection.
- Basic game listing in QML and platform filtering.

Regression tests cover supported extensions, recursive discovery, duplicate scans, PS1 cue/bin/img companion handling, scan-completion signaling, and invalid platform handling. Metadata provider tests cover missing API-key handling, empty searches, game and platform catalog JSON parsing, all four supported platform mappings, and malformed responses. Both suites require local build and execution on Arch Linux.

Milestone 3A: TheGamesDB metadata provider and platform mapping.

Implemented:
- Asynchronous TheGamesDB game search using a locally supplied API key.
- Runtime retrieval of the TheGamesDB platform catalog.
- Mapping of N64, PS1, GameCube, and Wii to provider platform IDs.
- Platform-filtered metadata search in the QML interface.
- Regression tests for provider response parsing and platform mapping.

Live platform-filtered search has been confirmed on Arch Linux. The full regression test suite should still be run against the final branch state before the milestone is considered fully validated.

Milestone 3B: Associate provider metadata with local ROM records.

Implemented on the current feature branch:
- Separate SQLite game_metadata table with a one-to-one foreign-key relationship to games.
- Metadata association keyed by the local game's database ID.
- QML selector for choosing a scanned local ROM.
- Association action on each metadata search result.
- Safe replacement of an existing metadata match without changing the local game title or path.
- Database regression tests for successful association, replacement, and unknown local game handling.

This milestone awaits a local build, regression test run, and interactive validation before merge.

## Development Guidelines

- Prefer modular C++ architecture.
- Keep UI logic in QML.
- Keep application logic in C++.
- Use SQLite for persistent library data.
- Use Qt APIs wherever practical.
- Maintain Linux compatibility.
- Avoid hardcoding launcher-specific behavior.
- Preserve existing functionality when introducing new features.

## Important Instructions

Before modifying existing code:

1. Inspect the current project structure.
2. Review existing implementations.
3. Identify dependencies and relationships.
4. Avoid replacing working code unnecessarily.
5. Provide complete files when significant modifications
   are required.
6. Explain build and testing instructions.
