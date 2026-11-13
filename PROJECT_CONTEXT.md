
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
- Framework: Qt 6
- UI: QML / Qt Quick
- Build system: CMake
- Database: SQLite
- Networking: Qt Network
- Process management: QProcess
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

## Known Limitations

- Graphics settings are controlled by individual launchers.
- Controller configuration is launcher-dependent.
- Save states are generally unavailable.
- Cheats are launcher-dependent.

## Architecture

The application uses a modular C++ backend and QML frontend.

Planned backend modules:

- GameManager
- PlatformManager
- LibraryScanner
- LauncherManager
- GitHubManager
- ProcessManager
- MetadataManager
- DatabaseManager
- SettingsManager

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

Next milestone:
- Implement LibraryScanner.
- Detect game files.
- Register games in SQLite.
- Display discovered games dynamically in QML.

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
