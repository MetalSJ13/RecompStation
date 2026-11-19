# RecompStation Development Roadmap

This document is the authoritative development status and implementation order for RecompStation. The immediate focus is a user-experience redesign, followed by the library automation needed to make metadata maintenance hands-off, then the remaining launcher and platform work.

## Project status

**Current implementation focus:** The Settings navigation has been widened and given category-specific SVG icons; Appearance, Display, Library, Metadata, Controller, and Application categories are now represented. Appearance and Application now use the same scrollable content pattern as the other settings pages. The Metadata page exposes the integrated TheGamesDB provider as a persistent checkbox, reports its configuration state, and lists ScreenScraper, Hasheous, and IGDB as future integrations. The provider preference is applied before metadata initialization, so a disabled TheGamesDB provider does not issue its initial platform-catalog request, later game searches, or new gameplay-screenshot refreshes. The Controller page now presents connected/disconnected state, controller name/type, input enablement, the active default profile, and a read-only summary of current navigation bindings. Per-device profiles and button remapping remain planned, not implemented. The Settings controller-focus work has since been built and exercised: category navigation and visible focus styling are in place across the settings categories and their interactive controls.

 The Settings foundation now persists preferred window-size presets and applies them on startup without changing monitor modes; this needs local build and KDE Wayland validation. Automatic metadata matching is connected to the library scan flow. The first pass uses conservative title matching within the mapped platform, records completed lookups and provider failures to avoid repeated requests during automatic scans, and downloads artwork for accepted matches. Explicit Refresh Metadata retries previously checked games. Re-adding a previously unavailable ROM now clears its stale failed lookup attempt when no metadata association exists, so it can be retried automatically. New diagnostic logging records why automatic metadata scans are skipped or how many games they queue. Provider failures remain distinct from definitive no-match and ambiguous results. The latest database change and regression test are committed; local build and regression-test verification are pending. A first N64 ROM hash utility has now been added: it detects .z64/.n64/.v64 byte order from the ROM magic, normalizes bytes to canonical big-endian order, and calculates MD5 and CRC32. Unit tests cover all three byte orders and invalid inputs. The TheGamesDB ByGameHash endpoint contract has been confirmed in a maintained API client (hash, filter[type]=md5|crc, filter[platform], fields, and includes); the public Swagger remains incomplete. API request integration and local test verification are the next steps.

**Current development branch:** `ui/interface-redesign`  
**Base:** The existing launcher download/installation and native-launch implementation, including the locally tested Play Game feature.  
**Merge policy:** This work remains unmerged into `main` until reviewed and explicitly approved.

Keep the application buildable after each focused change. Preserve working backend behavior while the user-facing workflows are redesigned.

### Controller settings presentation update (2026-11-18)

- [x] Redesign the Controller settings page with separate device, controller-options, active-profile, and default-button-layout sections.
- [x] Expose the existing controller-input enable/disable setting in the page.
- [x] Document the actual current navigation bindings without presenting read-only mappings as editable.
- [x] Expose stable SDL controller GUIDs and persist validated logical-action profiles independently per GUID.
- [ ] Apply saved profile mappings in the input router; implement button capture/remapping and per-device reset controls.
- [x] Build and verify Settings category navigation and visible focus styling across Appearance, Display, Library, Metadata, and Controller controls.
- [ ] Build and visually verify the revised Controller page with connected and disconnected controller states.

### Settings foundation update (2026-11-18)

- [x] Normalize Appearance and Application settings content into scrollable panels consistent with Display, Library, and Controller.
- [x] Add a dedicated Metadata category to Settings, ordered after Library and before Controller.
- [x] Add a persistent TheGamesDB provider checkbox and configured/disabled/API-key-required status.
- [x] Gate TheGamesDB platform-catalog and search requests using the saved provider preference.
- [x] Add regression coverage for provider preference persistence and disabled-provider search behavior.
- [ ] Build and run the application locally; visually verify Settings scrolling, category navigation, provider toggle persistence, and metadata refresh behavior.
- [ ] Add provider priority/fallback and independent artwork-source controls as additional providers are integrated.

### Recently completed and user-verified UI work (2026-11-18)

- The Library and Settings primary navigation has been simplified to compact icon-led controls; the user approved the current appearance.
- The Game Library uses a curated artwork grid with configurable density and a restrained Grid/List choice, preserving the collection-first identity. Title search is available in the Library header and combines with the platform filter.
- Library backgrounds support the coordinated static theme treatment, optional animated wave ambience, and custom still images. Custom images have been adjusted to scale more appropriately across the library.
- The animated wave background is a lightweight QML effect. A more detailed pre-rendered 1080p/30fps ambient-video treatment was discussed and deliberately deferred until the final polish phase; it is not a current priority.
- Game-card selection styling, artwork presentation, theme switching, and the adaptive navigation-hint panel have received iterative visual polish.
- The navigation-hint panel now uses device-appropriate icons and labels for mouse, keyboard, and controller input.
- Library keyboard navigation is confirmed working: arrow keys move through the grid, Enter/Return selects a game, and Escape returns from game details to the library.
- Controller hot-plug detection is confirmed working when a controller is connected after the application starts. Controller navigation updates the active input hints when controller input is used.
- The user approved the current Library/Settings button treatment and the icon-based navigation hints.

These confirmations cover the specific interactions the user tested; they do not mean every screen, settings category, display configuration, or controller workflow has completed accessibility validation.

### N64 project catalog expansion (2026-11-18)

- [x] Add Star Fox 64, Donkey Kong 64, Mario Kart 64, Mario Party, Mario Party 2, Mario Party 3, and Super Smash Bros. projects.
- [x] Add HarbourMasters Ghostship, Banjo-Tooie: Recompiled, and PaperBoat (Paper Mario).
- [x] Restrict release matching to published Linux ZIP/TAR.GZ packages and supported CPU architectures; exclude Windows and Flatpak assets.
- [x] Record project-specific ROM requirements, including documented revision and SHA-1 requirements where available.
- [x] Allow multiple projects to match one game and provide a project selector in Game Details (notably Super Mario 64).
- [x] Add catalog regression coverage for new game-title matching, multiple Super Mario 64 projects, and Linux asset filtering.
- [x] Build and run locally; verify Linux archive extraction, expected executable paths, installation, and launch for representative projects (Star Fox 64, Donkey Kong 64, Banjo-Tooie, and PaperBoat).
- [ ] Complete install/launch/uninstall checks for every remaining catalog project.

### Linux launcher execution and archive handling (2026-11-18)

- [x] Support release ZIPs that wrap a single TAR.GZ/TGZ payload; safely extract the nested archive into the requested staging destination.
- [x] Add regression coverage confirming nested archive contents (executable and supporting assets) land in the expected destination.
- [x] Launch shell startup scripts from their installation directory using a relative invocation such as `./run.sh`, preserving scripts' expected relative asset paths.
- [x] Add regression coverage that checks the shell script is invoked as `./run.sh`.
- [x] Correct the PaperBoat catalog executable filename to the archive's exact case-sensitive name, `Paperboat.AppImage`.
- [x] User-confirmed launch success for Star Fox 64, Donkey Kong 64, Banjo-Tooie, and PaperBoat.
- [ ] Continue testing remaining catalog projects and verify uninstall preserves ROM files.

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

### Milestone 1: Product experience and design foundation — complete (design specification and code audit, 2026-11-18)

- [x] Agree on the product direction: immersive personal game collection, not a storefront or clone of another launcher
- [x] Establish the two primary destinations: Game Library and Settings
- [x] Define metadata as an automated library service rather than a standalone search/association workflow
- [x] Define dark visual language, curated themes, and fullscreen-first behavior
- [x] Audit the current primary pages, dialogs, overlays, and interactions against `docs/UI_DESIGN.md`; record aligned areas, legacy cleanup, and deferred validation.
- [x] Bundle reusable SVG icon assets for navigation and common actions; define shared icon sizing (local validation pending)
- [x] Define the application/page hierarchy, Library and Settings navigation, Game Details overlay behavior, and contextual launcher/metadata workflows.
- [x] Define the selection-versus-focus model and keyboard/controller navigation expectations for Library, Settings, nested popups, and Escape handling.
- [x] Define display behavior accurately: Windowed, Maximized, and Fullscreen; preserve prior mode on F11; compositor owns monitor placement/resolution; borderless and explicit monitor selection remain deferred.

**Milestone 1 exit note:** The design direction, hierarchy, interaction model, and current implementation audit are now documented in `docs/UI_DESIGN.md`. This closes the Milestone 1 planning/specification work. It does not mark later implementation/validation items complete: full accessibility/controller-only testing, legacy unreachable QML cleanup, additional display modes, and broader library states remain tracked in their respective milestones.

### Milestone 2: Application shell and navigation

- [x] Refine main navigation and add an upward details-page entrance with a slower artwork dissolve; user visually validated the navigation and details transitions.
- [x] Replace the width-reserving sidebar with an integrated horizontal Library and Settings navigation bar; main content uses the available window area. User approved the updated navigation layout.
- [x] Add a dedicated Back to Library control to immersive game details; Escape also closes the details view.
- [x] Remove Metadata Search and Platforms as top-level navigation destinations; platform selection now lives in the Library. Legacy page components remain in QML pending safe cleanup.
- [x] Keep platform selection/filtering inside the Game Library; combine it with title search.
- [x] Add live game-title search in the Library; search respects the selected platform and shows a no-results state.
- [x] Consolidate Grid/List and view-specific sizing controls in the Library header; persist Grid density and List row size independently.
- [x] Constrain List View to approximately half the content width, preserving a large visible area for custom/animated backgrounds; add reduced-motion-aware selected/hover zoom.
- [x] Rehome launcher discovery and management contextually in Game Details, preserving the existing download, install, launch, and uninstall capabilities.
- [x] Add the bottom-up, staggered Grid card entrance for application startup, List-to-Grid switching, and return from Settings; respect Reduced Motion. User has visually validated the timing and behavior across these entry points.
- [x] Normalize the Settings category content gutters, title/description/divider hierarchy, section spacing, and horizontal-scroll behavior; retain the shared focus treatment and on-demand vertical scrolling.
- [ ] Complete a full navigation/backend regression pass to confirm all existing pages and backend connections remain functional after the transition.

**Milestone 2 status:** The core shell, Library browsing controls, contextual launcher workflow, and card entrance behavior are implemented and user-verified. Remaining work is the full navigation/backend regression pass; obsolete Metadata Search, Platforms, and standalone Launcher Discovery page code has now been removed while retaining the underlying backend workflows. Do not close the milestone until remaining work and the relevant build/test workflow are confirmed.

### Milestone 3: Game Library experience

- [x] Simplify the library header to one platform selector (All Platforms by default); remove the separate collection search and duplicate management panel
- [x] Expand game tiles into a responsive multi-row grid with larger artwork, clearer title/platform typography, and stronger selected-game emphasis (local validation pending)
- [x] Add a theme-aware, selected-game artwork atmosphere layer behind the library content, with contrast overlays and reduced-motion-aware fading (first visual pass; local validation pending)
- [x] Add subtle focus depth to the current game tile, respecting reduced motion (local validation pending)
- [x] Replace the compact game-details popup with a full-window immersive details experience, using the selected cover as a softly blurred, gradually fading background (local validation pending)
- [x] Keep the library backdrop stable and theme-driven instead of changing it with game selection (local validation pending)
- [ ] Add a dedicated game-details experience
- [ ] Present metadata, supported project, installation state, and launch action in the game context
- [ ] Add useful empty, loading, scanning, missing-artwork, unsupported, and error states
- [x] Keep manual library scanning in Settings; remove duplicate scan and metadata-refresh controls from the Library page
- [ ] Verify missing and differently proportioned artwork

### Milestone 4: Automatic library and metadata maintenance

- [ ] Use one shared discovery/matching pipeline for manual and automatic scans
- [x] Add recursive filesystem monitoring for configured platform folders, with a 750 ms debounce and automatic full-library reconciliation (implementation and regression tests committed; local build/runtime validation pending)
- [x] Detect added and removed games, including newly created nested folders; preserve unavailable database records and avoid duplicate registration (local validation pending)
- [x] Automatically identify metadata using conservative title and platform matching (initial implementation; local build/runtime validation pending)
- [ ] Define and test conservative match-confidence rules; keep ambiguous matches unresolved
- [x] Add an isolated N64 ROM hashing utility with byte-order detection and MD5/CRC32 calculation (unit tests added; local build/test pending)
- [ ] Integrate TheGamesDB Games/ByGameHash requests (MD5 then CRC fallback) and preserve title search as the final fallback
- [ ] Verify provider hash conventions against known-good N64 ROMs before enabling automatic hash association
- [x] Persist completed lookup outcomes and provider failures to avoid repeated automatic requests; explicit refresh can retry checked games (local validation pending)
- [ ] Further validate artwork cache behavior, provider rate-limit handling, and retry/backoff policy
- [ ] Surface scan status and exceptional review in the affected game/library context, not a separate Metadata Search page
- [ ] Add regression tests for matching, monitoring, duplicate events, API failures, and cache behavior

### Milestone 5: Settings and first-run experience

- [x] Move persistent preferences to `<dataDirectory>/config/settings.ini`, migrate existing QSettings values, and restore first-run setup when the file is absent
- [x] Establish the first curated static theme foundation: Midnight Violet, Electric Blue, and Ember with semantic color tokens, coordinated surfaces/status colors, theme-aware background/artwork treatments, and live preview before applying (local validation pending)
- [x] Add optional animated theme backgrounds and custom still-image backgrounds with preview/apply/restore behavior (initial lightweight wave ambience and image scaling implemented; further visual refinement remains)
- [x] Add compositor-respecting preferred window-size presets and safe windowed sizing (never force monitor-wide resolution changes; local Wayland validation pending)
- [ ] Enumerate and support selecting actual display devices; current size presets are constrained to the current screen's available logical area
- [x] Add a single configurable library root, catalog-derived platform folders, first-run setup, and startup scan preference
- [x] Add automatic filesystem monitoring for configured folders (initial implementation; local build/runtime validation pending)
- [x] Add a separate gameplay screenshot refresh control for games with saved TheGamesDB associations; cache screenshots independently of cover art (local build/API validation pending)
- [x] Expose TheGamesDB provider status/enablement plus manual metadata and gameplay-screenshot refresh controls in Settings → Metadata; field selection and independent cache controls remain planned
- [ ] Add launcher installation/update preferences as supported
- [ ] Add application preferences and useful first-run guidance
- [ ] Expose only settings backed by real functionality

### Milestone 6: Launcher experience

- [ ] Present supported project discovery and installation contextually from game details
- [ ] Derive and display one accurate Playable/Beta/Alpha status per game from curated project/launcher compatibility data (intentionally deferred until the core library experience is settled)
- [ ] Refine release/asset selection and architecture compatibility explanations
- [ ] Improve download progress, cancellation, extraction, installation, and failure feedback
- [ ] Improve installed-launcher status and confirmation-protected uninstall
- [ ] Make installed release/version information clear when available

### Milestone 7: Input, accessibility, and regression pass

- [x] Add application-level arrow-key navigation for the Game Library Grid View and Enter/Return game selection
- [x] Support Escape to return from immersive game details to the Game Library
- [x] Add adaptive mouse, keyboard, and controller navigation hints with bundled icons
- [x] Detect controllers connected after application startup; switch hints when controller input is used
- [ ] Ensure keyboard navigation and visible focus across all screens (Library keyboard navigation works; Settings category focus/navigation remains deferred)
- [x] Add controller-friendly category navigation and visible focus styling for Settings controls across the tested categories.
- [ ] Complete controller-only navigation across every settings control, dialog, dropdown, and long scrolling page.
- [ ] Persist independent controller profiles keyed by stable SDL hardware GUID (backend foundation complete); apply assignments to the input router and implement interactive remapping/reset
- [ ] Ensure Xbox/PlayStation defaults and launcher/game-session input ownership remain correct
- [ ] Review contrast, accessible labels, tab order, and scaling (global small/medium/large font sizes and reduced-motion preference implemented; validate locally)
- [ ] Add lightweight navigation sounds with volume controls
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
- Qt 6.5 or newer (including Qt SVG)
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
