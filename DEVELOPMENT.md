# RecompStation Development Roadmap

This document is the authoritative development status and implementation order for RecompStation. Milestones are ordered by dependency and user experience: stabilize the working baseline, establish a coherent interface, then complete and expand the workflows behind it.

## Project status

**Current development branch:** `ui/interface-redesign`  
**Base:** The existing launcher download/installation and native-launch implementation, including the latest locally tested Play Game feature.  
**Merge policy:** This work remains unmerged into `main` while the UI and remaining workflows are developed and validated. Do not merge without an explicit project decision.

The current branch is a functional development baseline. UI work should preserve existing behavior and keep the application buildable at each step.

## Completed foundation

- [x] C++20, CMake, Qt 6.5+, and QML application foundation
- [x] SQLite database initialization and platform directory creation
- [x] Game scanning and library registration for N64, PS1, GameCube, and Wii
- [x] TheGamesDB asynchronous search, platform catalog retrieval, and runtime platform-ID mapping
- [x] Metadata association with local ROM records without overwriting local titles or paths
- [x] Cover-art URL parsing, asynchronous download, local caching, and library thumbnails
- [x] Asynchronous GitHub repository and latest-release discovery
- [x] Curated, schema-validated recompilation project catalog with an initial Banjo: Recompiled entry
- [x] Architecture-aware Linux release asset filtering
- [x] Staged launcher downloads, safe archive extraction, installation validation, managed uninstall, and installed-launcher rediscovery
- [x] Native launcher process startup and process-state/error reporting
- [x] Regression tests for the implemented scanner, metadata, artwork, GitHub, catalog, archive, launcher-management, and process-launch components

## Development principles

1. Keep `main` stable; develop and validate unfinished work on a dedicated branch.
2. Keep the application buildable and runnable after each focused change.
3. Preserve working behavior while improving presentation and architecture.
4. Keep application/business logic in C++ and presentation/navigation in QML.
5. Prefer small reusable QML components and focused pages over a monolithic main view.
6. Avoid project-specific assumptions in shared launcher code; put project differences in catalog/configuration data where practical.
7. Run the existing build and regression suite after meaningful changes, and add tests when backend behavior changes.
8. Do not remove or invalidate a user's existing build directory or application data as part of routine UI work.

## Ordered development roadmap

### Milestone 1: UI/UX audit and design foundation — current

Goal: establish a consistent visual language and a clear application structure before adding more features.

- [ ] Review every existing page, dialog, navigation path, and major interaction
- [ ] Record usability issues, inconsistent patterns, missing states, and redundant UI
- [ ] Agree on the visual direction, including dark/light behavior and accent colors
- [ ] Define reusable theme tokens for colors, typography, spacing, radii, borders, and elevation
- [ ] Define shared controls and patterns for buttons, cards, inputs, dialogs, notifications, loading, empty, and error states
- [ ] Agree on responsive window behavior and sensible minimum dimensions
- [ ] Document the page hierarchy and navigation model before large-scale QML changes

### Milestone 2: Application shell and navigation

- [ ] Refine the main window, header, navigation/sidebar, content area, and page transitions
- [ ] Establish consistent page titles, actions, spacing, and scroll behavior
- [ ] Create reusable QML components only where they remove real duplication
- [ ] Keep existing pages and backend connections functional throughout the shell refactor

### Milestone 3: Game Library experience

- [ ] Redesign the library layout, game tiles/cards, cover-art presentation, and information hierarchy
- [ ] Improve platform filtering, search, sorting, and library empty/loading states
- [ ] Make metadata association and supported-project status clear without cluttering the library
- [ ] Refine Play Game, running state, launch errors, and unavailable/incompatible states
- [ ] Verify artwork fallback behavior and layouts with missing or differently sized cover images

### Milestone 4: Metadata and ROM management experience

- [ ] Polish metadata search, platform selection, result presentation, and association workflow
- [ ] Make selected-ROM context and existing associations clear
- [ ] Improve scan progress, scan completion, duplicate handling, and error feedback
- [ ] Provide clear explanations when metadata or artwork is unavailable

### Milestone 5: Launcher Discovery and installation experience

- [ ] Polish supported-project discovery and release/asset selection
- [ ] Improve architecture compatibility explanations and unsupported-platform states
- [ ] Refine download progress, cancellation, extraction, installation, and failure feedback
- [ ] Improve installed-launcher status and confirmation-protected uninstall
- [ ] Make it clear which release asset and version are being installed

### Milestone 6: Settings, first-run setup, and application feedback

- [ ] Design settings for library paths, platform directories, and application preferences
- [ ] Improve first-run guidance and configuration validation
- [ ] Standardize confirmations, notifications, errors, and recovery guidance
- [ ] Ensure destructive actions are clear and protected

### Milestone 7: UI accessibility, consistency, and regression pass

- [ ] Review keyboard navigation, focus visibility, accessible labels, and contrast
- [ ] Check layout behavior at different window sizes and display scaling settings
- [ ] Verify all pages use shared visual tokens and components consistently
- [ ] Run a full build and regression suite; manually exercise all existing workflows
- [ ] Fix regressions before considering the UI milestone complete

### Milestone 8: Complete launcher lifecycle

- [ ] Track installed launcher versions and display the installed version
- [ ] Add update discovery and download flow
- [ ] Implement rollback-safe replacement so a failed update preserves the last working install
- [ ] Recover from or clean up abandoned staging/extraction directories safely
- [ ] Add tests for update failure, recovery, and version state

### Milestone 9: Project-specific launch configuration and compatibility

- [ ] Support project-specific command-line arguments and launch configuration
- [ ] Pass the correct ROM/game path when required by a project
- [ ] Define catalog-driven launch behavior rather than hardcoding individual projects
- [ ] Validate ROM revision/compatibility requirements where reliable data is available
- [ ] Clearly distinguish title/platform matches from verified ROM compatibility

### Milestone 10: Optional release integrity verification

- [ ] Detect published SHA-256 checksum files or manifests and verify downloaded assets
- [ ] Detect detached signatures (for example, `.sig`) where projects publish them
- [ ] Verify signatures only against a trusted, documented project-specific key or trust mechanism
- [ ] Record and display whether an asset was verified, had no published verification metadata, or failed verification
- [ ] Do not block installation merely because a publisher provides no checksum or signature
- [ ] Treat present-but-invalid verification data as an integrity failure and stop installation by default

### Milestone 11: Expand supported project and platform coverage

- [ ] Add curated recompilation projects only after validating their release assets, package layout, executable path, and launch requirements
- [ ] Expand platform mappings and catalog coverage as supported projects become available
- [ ] Add package-format support only when a real catalog entry requires it
- [ ] Validate architecture and operating-system compatibility for each catalog entry
- [ ] Document project-specific ROM requirements and limitations accurately

## Build and development workflow

Dependencies:
- Qt 6.5 or newer
- C++20 compiler and CMake
- Ninja (recommended by the build script)
- libarchive (Arch Linux package: `libarchive`)

Build from the repository root:

```bash
./scripts/build.sh
```

Build and run the regression tests:

```bash
./scripts/build.sh --test
```

Choose a configuration:

```bash
./scripts/build.sh Release
./scripts/build.sh RelWithDebInfo
```

The build script reuses the generator and build directory already configured. During UI work, prefer incremental builds and retain the existing build directory so normal iteration remains quick. Use a clean build only when diagnosing a build-system or stale-artifact problem, not as a routine step.

## Definition of done

A milestone is complete when its checklist is satisfied, the application builds, relevant automated tests pass, existing workflows have been manually checked where appropriate, and the documentation reflects the actual state. A feature being present in code does not by itself mean it is validated or ready to merge.
