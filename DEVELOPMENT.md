# RecompStation Development Roadmap

This document is the authoritative development status and implementation order for RecompStation. The immediate focus is a user-experience redesign, followed by the library automation needed to make metadata maintenance hands-off, then the remaining launcher and platform work.

## Project status

**Current development branch:** `ui/interface-redesign`  
**Base:** The existing launcher download/installation and native-launch implementation, including the locally tested Play Game feature.  
**Merge policy:** This work remains unmerged into `main` until reviewed and explicitly approved.

Keep the application buildable after each focused change. Preserve working backend behavior while the user-facing workflows are redesigned.

## Completed foundation

- [x] C++20, CMake, Qt 6.5+, and QML application foundation
- [x] SQLite database initialization and platform directory creation
- [x] Game scanning and library registration for N64, PS1, GameCube, and Wii
- [x] TheGamesDB asynchronous search, platform catalog retrieval, and runtime platform-ID mapping
- [x] Metadata association with local ROM records without overwriting local titles or paths
- [x] Cover-art retrieval, local caching, and library thumbnails
- [x] Asynchronous GitHub repository and latest-release discovery
- [x] Curated recompilation project catalog with an initial Banjo: Recompiled entry
- [x] Architecture-aware Linux release asset filtering
- [x] Staged launcher downloads, safe archive extraction, installation validation, managed uninstall, and installed-launcher rediscovery
- [x] Native launcher process startup and process-state/error reporting
- [x] Regression tests for implemented scanner, metadata, artwork, GitHub, catalog, archive, launcher-management, and process-launch components

## Development principles

1. Keep `main` stable; develop unfinished work on the dedicated UI branch.
2. Keep the application buildable and runnable after each focused change.
3. Preserve existing behavior while improving presentation and architecture.
4. Keep application/business logic in C++ and presentation/navigation in QML.
5. Prefer small reusable QML components and focused pages over a monolithic main view.
6. Do not expose backend workflows as user-facing chores when they can be automated safely.
7. Automatically associate metadata only when the match is sufficiently reliable; do not silently attach uncertain results.
8. Run builds and regression tests after meaningful changes. Add tests when backend behavior changes.
9. Do not invalidate or delete the user's existing build directory or application data during routine UI work.

## Ordered development roadmap

### Milestone 1: Product experience and design foundation — current

- [x] Agree on the product direction: immersive personal game collection, not a storefront or clone of another launcher
- [x] Establish the two primary destinations: Game Library and Settings
- [x] Define metadata as an automated library service rather than a standalone search/association workflow
- [x] Define dark visual language, curated themes, and fullscreen-first behavior
- [ ] Audit all existing pages, dialogs, and interactions against `docs/UI_DESIGN.md`
- [ ] Define reusable theme tokens and shared component patterns
- [ ] Define page hierarchy, selection/focus behavior, and display-mode behavior

### Milestone 2: Application shell and navigation

- [ ] Refine the main window, navigation, content area, and page transitions
- [ ] Remove Metadata Search and Platforms as top-level navigation destinations
- [ ] Move platform selection/filtering into the Game Library
- [ ] Rehome launcher discovery and management contextually without removing existing capabilities
- [ ] Establish shared page headers, actions, spacing, scroll behavior, and focus styling
- [ ] Keep existing pages and backend connections functional during the transition

### Milestone 3: Game Library experience

- [ ] Create a cover-art-forward library with platform filtering, optional search, and sorting
- [ ] Add a dedicated game-details experience
- [ ] Present metadata, supported project, installation state, and launch action in the game context
- [ ] Add useful empty, loading, scanning, missing-artwork, unsupported, and error states
- [ ] Add a visible manual scan/scrape action
- [ ] Verify missing and differently proportioned artwork

### Milestone 4: Automatic library and metadata maintenance

- [ ] Use one shared discovery/matching pipeline for manual and automatic scans
- [ ] Add filesystem monitoring for configured game folders, with debouncing and safe handling of bursts of changes
- [ ] Detect additions, removals, and relevant changes without repeatedly registering duplicates
- [ ] Automatically identify metadata using title and platform context
- [ ] Define and test conservative match-confidence rules; keep ambiguous matches unresolved
- [ ] Cache metadata and artwork, avoid redundant API requests, and respect provider limits
- [ ] Surface scan status and exceptional review in the affected game/library context, not a separate Metadata Search page
- [ ] Add regression tests for matching, monitoring, duplicate events, API failures, and cache behavior

### Milestone 5: Settings and first-run experience

- [ ] Add curated appearance presets and coordinated background/accent/surface palettes
- [ ] Add fullscreen default plus supported windowed and borderless modes
- [ ] Enumerate actual display devices and supported resolutions before offering choices
- [ ] Add library folder and automatic-monitoring configuration
- [ ] Add metadata provider status and cache/refresh controls
- [ ] Add launcher installation/update preferences as supported
- [ ] Add application preferences and useful first-run guidance
- [ ] Expose only settings backed by real functionality

### Milestone 6: Launcher experience

- [ ] Present supported project discovery and installation contextually from game details
- [ ] Refine release/asset selection and architecture compatibility explanations
- [ ] Improve download progress, cancellation, extraction, installation, and failure feedback
- [ ] Improve installed-launcher status and confirmation-protected uninstall
- [ ] Make installed release/version information clear when available

### Milestone 7: Input, accessibility, and regression pass

- [ ] Ensure keyboard navigation and visible focus across all screens
- [ ] Establish controller-friendly navigation and add gamepad support
- [ ] Review contrast, accessible labels, tab order, and scaling
- [ ] Check fullscreen, windowed, and multi-display behavior
- [ ] Run the full build and regression suite; manually exercise existing workflows
- [ ] Fix regressions before considering the UI milestone complete

### Milestone 8: Complete launcher lifecycle

- [ ] Track installed launcher versions
- [ ] Add update discovery and download flow
- [ ] Implement rollback-safe replacement
- [ ] Recover from or clean up abandoned staging/extraction directories safely
- [ ] Add tests for update failure, recovery, and version state

### Milestone 9: Project-specific launch configuration and compatibility

- [ ] Support project-specific command-line arguments and launch configuration
- [ ] Pass the correct ROM/game path when required
- [ ] Define catalog-driven launch behavior rather than hardcoding individual projects
- [ ] Validate ROM revision/compatibility where reliable data is available
- [ ] Never present a title/platform match as proof of ROM compatibility

### Milestone 10: Optional release integrity verification

- [ ] Detect published SHA-256 checksum files or manifests and verify downloaded assets
- [ ] Detect detached signatures (for example, `.sig`) where projects publish them
- [ ] Verify signatures only against a trusted, documented project-specific key or trust mechanism
- [ ] Record whether an asset was verified, had no published verification metadata, or failed verification
- [ ] Do not block installation merely because verification metadata is absent
- [ ] Treat present-but-invalid verification data as an integrity failure and stop installation by default

### Milestone 11: Expand supported project and platform coverage

- [ ] Add curated projects only after validating release assets, package layout, executable path, and launch requirements
- [ ] Expand platform mappings and catalog coverage as supported projects become available
- [ ] Add package formats only when a real catalog entry requires them
- [ ] Validate architecture and operating-system compatibility per catalog entry
- [ ] Document project-specific ROM requirements and limitations accurately

## Build and development workflow

Dependencies:
- Qt 6.5 or newer
- C++20 compiler and CMake
- Ninja (recommended by the build script)
- libarchive (Arch Linux package: `libarchive`)

From the repository root:

```bash
./scripts/build.sh
./scripts/build.sh --test
```

Choose a configuration with `./scripts/build.sh Release` or `./scripts/build.sh RelWithDebInfo`.

The build script reuses the configured generator and build directory. Prefer incremental builds and retain the existing build directory. Use a clean build only to diagnose build-system or stale-artifact problems.

## Definition of done

A milestone is complete when its checklist is satisfied, the application builds, relevant automated tests pass, existing workflows have been manually checked where appropriate, and documentation reflects the actual implementation. Do not describe planned monitoring, controller support, display controls, or other future behavior as already implemented.
