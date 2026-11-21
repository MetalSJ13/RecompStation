# RecompStation Development Roadmap

This document is the authoritative development status and implementation order for RecompStation. The immediate focus is a user-experience redesign, followed by the library automation needed to make metadata maintenance hands-off, then the remaining launcher and platform work.

## Project status

### Current focus
- **Active milestone:** Milestones 5 and 6 — Settings/first-run experience and launcher experience. Milestone 2 remains open with its full manual navigation/backend regression pass intentionally deferred. The last user-confirmed baseline before the current artwork change: all 18/18 CTest suites passed on 2026-11-20; the new artwork work still requires a fresh local build/test run.
- **Current UI work:** The Carousel presentation is the first-run default, with persistent user-selected view preferences and independent sizing. The current discussion is focused on Settings/first-run behavior, display-resolution adaptation, and launcher workflow completeness.
- **Next technical focus:** Continue automatic library/metadata maintenance, including validation of N64 ROM hash lookup and provider fallback behavior.

### Implemented foundations
- **Settings:** Appearance, Display, Library, Metadata, Controller, and Application categories are present. Appearance and Application use the shared scrollable content pattern. TheGamesDB enablement is persistent and its configured/disabled/API-key-required state is displayed.
- **Metadata:** Provider preference is applied before metadata initialization. Automatic matching uses conservative title/platform matching and records lookup outcomes to avoid repeated requests. Settings now has one Universal Library Refresh action that retries eligible metadata lookups, then retrieves missing TheGamesDB screenshots; the separate LaunchBox artwork manager fills missing covers and screenshots for LaunchBox-associated library games. Existing screenshot files are skipped and preserved. A successful LaunchBox catalog update still performs its library-scoped artwork pass and prompts the user to run the universal refresh for metadata matching and missing screenshots.

- **LaunchBox artwork:** Implemented a separate downloader scoped to the local game library. It uses saved LaunchBox associations only, fetches one preferred front cover and up to three gameplay screenshots, preserves existing covers, caches files locally, and limits traffic to one request at a time with a 300 ms pause after each five requests. Settings shows artwork progress and final downloaded/skipped/failed counts. Runtime download completion has been confirmed (47 images for 13 games, followed by 36 images for 14 games, with no reported failures); a fresh full build/regression run after the latest unified-refresh changes remains pending.
- **Controller:** Connected-device state, input enablement, active default profile, and read-only navigation bindings are displayed. Stable SDL GUIDs and per-device profile persistence have backend foundations; applying mappings and interactive remapping remain unfinished.
- **Library:** Filesystem monitoring, automatic metadata scan integration, and diagnostic logging are implemented. Re-added ROMs can retry after stale failed lookup state is cleared. A platform-expansion bug was identified: application startup had initialized the library platform list from the recompilation project catalog, which currently exposed only N64. Startup now uses the independent library platform defaults (N64, PS1, GameCube, Wii); the fix is committed but awaits local build/runtime verification.
- **N64 identification:** A ROM hash utility detects `.z64`, `.n64`, and `.v64` byte order, normalizes data, and calculates MD5 and CRC32. Unit tests cover all supported byte orders and invalid input.

### Pending validation and follow-up
- Original Xbox library support has been added to the independent platform list, with a dedicated folder, recursive .iso/.xiso scanning, TheGamesDB platform-name mapping, and regression coverage. A follow-up test run exposed that the SQLite platform seed also needed Xbox; that registration and a database regression test are now committed. The .iso files must be XISO-format images; ordinary/redump full-disc ISO dumps are not necessarily usable by xemu. Implementation is committed but awaits local build/runtime verification. Investigate Xbox 360 only after its supported disc/container format and detection rules are established.
- Local build/runtime validation remains outstanding for recent Settings, metadata, and database changes unless explicitly confirmed in the relevant milestone notes.
- The TheGamesDB ByGameHash request contract has been investigated, but application integration and validation against known-good ROMs remain outstanding.
- The environment-variable API key is a development-time mechanism only. Production authentication/distribution must be investigated and designed separately; see Milestone 4.

### Branch and merge policy
- **Current development branch:** `ui/interface-redesign`
- **Base:** Existing launcher download/installation and native-launch implementation, including the locally tested Play Game feature.
- **Merge policy:** Keep this work unmerged into `main` until reviewed and explicitly approved.

Keep the application buildable after each focused change. Preserve working backend behavior while the user-facing workflows are redesigned.

### Library folder browser
- [x] Replace the desktop-native folder picker with an in-app directory browser for first-run setup and Settings.
- [x] Support folder navigation, Home/Up navigation, right-click context actions, and creating directories without leaving RecompStation.
- [x] Restrict folder creation to a single child name in the current writable directory; selecting the library root delegates platform-subfolder creation and persistence to PlatformManager.
- [x] Add regression coverage for directory listing, safe folder creation, duplicate names, and path traversal rejection.
- [x] User-confirmed local build/run and UI interaction: the browser opens and can create folders.

## Recent implementation notes (2026-11-21)

### First-run experience and library setup
- [x] Add the optional automatic application-update preference prompt before library-folder setup. Yes, No, closing the prompt, and Escape all complete the preference; dismissal is saved as disabled, and the choice can later be changed in Settings → Application. This is preference UI only: release checks are not connected and no network requests are made.
- [x] Polish the first-run library-folder setup overlay to match the active theme and the rest of the application.
- [x] Scope the library-folder setup overlay to the Library page and prevent it from covering the controller pause menu.
- [x] Make the library-folder setup overlay non-modal so mouse users can still use the toolbar and navigate to Settings while setup remains incomplete.
- [ ] Test the complete first-run sequence after resetting library data, including update preference, folder selection, initial scan, and the resulting empty/non-empty Library states. This validation is now in progress.
- [ ] Revisit the empty-library presentation only after observing the complete reset/first-run flow. A trial that hid the empty state until a library folder was configured was reverted at the user's request; current behavior is intentionally unchanged.

### Library folder browser
- [x] Replace the native folder picker with the in-app browser shared by first-run setup and Settings.
- [x] Add directory navigation, Home/Up controls, right-click folder actions, and in-app folder creation.
- [x] Add safe folder-creation validation and regression tests.
- [x] User confirmed the browser builds, runs, and creates folders successfully.

## Recent implementation notes (2026-11-18)

The following notes record focused implementation and user-validation updates. The milestone checklists below remain the authoritative task/status tracker.

### Controller settings
- [x] Redesign the Controller settings page with separate device, controller-options, active-profile, and default-button-layout sections.
- [x] Expose the existing controller-input enable/disable setting.
- [x] Document current navigation bindings as read-only rather than presenting them as editable.
- [x] Expose stable SDL controller GUIDs and persist validated logical-action profiles independently per GUID.
- [ ] Apply saved profile mappings in the input router; implement button capture/remapping and per-device reset controls.
- [x] Build and verify Settings category navigation and visible focus styling across the tested categories and controls.
- [ ] Build and visually verify the revised Controller page with connected and disconnected controller states.

### Settings foundation
- [x] Normalize Appearance and Application settings content into scrollable panels consistent with the other Settings categories.
- [x] Add a dedicated Metadata category, ordered after Library and before Controller.
- [x] Add a persistent TheGamesDB provider checkbox and configured/disabled/API-key-required status.
- [x] Gate TheGamesDB platform-catalog and search requests using the saved provider preference.
- [x] Add regression coverage for provider preference persistence and disabled-provider search behavior.
- [ ] Build and run locally; verify Settings scrolling, category navigation, provider-toggle persistence, and metadata refresh behavior.
- [ ] Add provider priority/fallback and independent artwork-source controls as additional providers are integrated.

### Library and navigation polish
- [x] Simplify primary Library/Settings navigation to compact icon-led controls; user approved the appearance.
- [x] Add configurable Grid/List browsing, title search, platform filtering, and independent view sizing.
- [ ] Validate the new Console carousel locally: horizontal scrolling, selected-card centering/focus, mouse/keyboard/controller selection, details opening, persistence, and Grid/List regression.
- [x] Add coordinated static/animated/custom library background treatments.
- [x] Polish game-card selection, artwork presentation, theme switching, and adaptive navigation hints.
- [x] Validate fullscreen startup and the coordinated startup Grid reveal on the user's Wayland desktop; no initial windowed flash or duplicate/late card animation remains.
- [x] Confirm Library keyboard navigation: arrow keys move through the grid, Enter/Return selects a game, and Escape returns from game details.
- [x] Confirm controller hot-plug detection and adaptive input hints when controller input is used.
- [ ] Complete accessibility and controller-only validation across all screens and controls.

### N64 project catalog
- [x] Add Star Fox 64, Donkey Kong 64, Mario Kart 64, Mario Party, Mario Party 2, Mario Party 3, and Super Smash Bros. projects.
- [x] Add HarbourMasters Ghostship, Banjo-Tooie: Recompiled, and PaperBoat (Paper Mario).
- [x] Restrict release matching to published Linux ZIP/TAR.GZ packages and supported CPU architectures; exclude Windows and Flatpak assets.
- [x] Record project-specific ROM requirements, including documented revision and SHA-1 requirements where available.
- [x] Allow multiple projects to match one game and provide a project selector in Game Details (notably Super Mario 64).
- [x] Add catalog regression coverage for title matching, multiple Super Mario 64 projects, and Linux asset filtering.
- [x] User-confirmed build/run and launch checks for representative projects: Star Fox 64, Donkey Kong 64, Banjo-Tooie, and PaperBoat.
- [ ] Complete install/launch/uninstall checks for every remaining catalog project.

### Linux launcher execution and archive handling
- [x] Support release ZIPs that wrap a single TAR.GZ/TGZ payload; safely extract the nested archive into the requested staging destination.
- [x] Add regression coverage confirming nested archive contents land in the expected destination.
- [x] Launch shell startup scripts from their installation directory using a relative invocation such as `./run.sh`.
- [x] Add regression coverage confirming the shell script is invoked as `./run.sh`.
- [x] Correct the PaperBoat executable filename to the archive's exact case-sensitive name, `Paperboat.AppImage`.
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
- [x] Define dark visual language, curated themes, and fullscreen-first behavior- [x] Audit the current primary pages, dialogs, overlays, and interactions against `docs/UI_DESIGN.md`; record aligned areas, legacy cleanup, and deferred validation.
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
- [x] Request fullscreen as the initial native window state on every application launch, including first-run and existing-library installations; gate the startup Grid card reveal until fullscreen is confirmed and initialize delegates in their hidden reveal state to prevent a visible pre-animation flash. User confirmed the startup behavior on Wayland, including the absence of the windowed flash and duplicate/late card reveal.
- [ ] Complete a full navigation/backend regression pass to confirm all existing pages and backend connections remain functional after the transition.

**Milestone 2 status:** The core shell, Library browsing controls, contextual launcher workflow, card entrance behavior, and fullscreen-on-launch behavior are implemented and user-validated. The latest local build completed successfully and all 14 automated tests passed. Recent fixes for Application-page controller/keyboard navigation, Settings scrolling and Escape handling, and adaptive pause-menu hints were also confirmed by the user. The comprehensive manual navigation/backend regression pass remains intentionally open and deferred while work proceeds on Milestone 3; do not mark Milestone 2 complete until that pass is explicitly resumed and finished.

### Milestone 3: Game Library experience

**Progress note (2026-11-21):** The dedicated immersive Game Details experience and contextual metadata/project/installation/launch information are implemented and user-validated. The user confirmed the missing-artwork placeholder appears while artwork loads and is replaced by the retrieved image, and that searches with no matches show a no-results overlay. The platform-filtered empty state was also inspected; retain one generic empty-library experience rather than introducing platform-specific messages. Initial truly-empty-library behavior and scan/error recovery still need verification.

- [x] Simplify the library header to one platform selector (All Platforms by default); remove the separate collection search and duplicate management panel
- [x] Expand game tiles into a responsive multi-row grid with larger artwork, clearer title/platform typography, and stronger selected-game emphasis (local validation pending)
- [x] Add a theme-aware, selected-game artwork atmosphere layer behind the library content, with contrast overlays and reduced-motion-aware fading (first visual pass; local validation pending)
- [x] Add subtle focus depth to the current game tile, respecting reduced motion (local validation pending)
- [x] Replace the compact game-details popup with a full-window immersive details experience, using the selected cover as a softly blurred, gradually fading background (local validation pending)
- [x] Keep the library backdrop stable and theme-driven instead of changing it with game selection (local validation pending)
- [x] Add a dedicated game-details experience; implemented as the immersive full-window Game Details view and user-validated with representative games.
- [x] Present metadata, supported project, installation state, and launch action in the game context; metadata, project selection, installed location/ROM requirements, Play Game, and Uninstall are present and user-validated.
- [ ] Verify the single universal empty-library screen when no games have been discovered; keep its guidance general (no platform-specific empty screens) and do not imply manual scanning is required when automatic scanning is enabled.
- [x] Confirm the no-search-results overlay and missing-artwork placeholder/loading-to-artwork behavior.
- [ ] Verify loading/scanning feedback, unsupported-file handling, and scan/metadata error recovery.
- [x] Keep manual library scanning in Settings; remove duplicate scan and metadata-refresh controls from the Library page
- [ ] Verify missing and differently proportioned artwork beyond the already-confirmed placeholder and artwork-loading behavior.

### Milestone 4: Automatic library and metadata maintenance


#### LaunchBox optional local-catalog fallback — planned

**Agreed design decisions**
- TheGamesDB remains the primary metadata provider. LaunchBox is an optional secondary source and must be disabled by default for new installations.
- Download the LaunchBox `Metadata.zip` archive only after the user explicitly enables LaunchBox. Show download/extraction or indexing progress, support cancellation, and retain a usable existing catalog if an update fails.
- Keep a local searchable index rather than reparsing the extracted XML for every game lookup. The supplied archive is approximately 106 MB compressed and 550 MB extracted, so avoid unnecessary repeated extraction and processing.
- Use `Platforms.xml` only as reference data. Map RecompStation platforms to explicit canonical LaunchBox platform names; do not blindly accept every LaunchBox alias (for example, the Nintendo 64DD alias collision).
- Use `Files.xml` platform-specific filename associations as the preferred route: first match the ROM's full filename (including region, language, and revision tags, excluding only its known file extension and harmless punctuation/case differences), then use its `GameName` to locate the corresponding `Metadata.xml` record. Only if no exact filename association exists, fall back to conservative platform-and-title matching.
- Treat platform as a mandatory matching constraint. Prefer exact revision-preserving title matches, then allow a unique revision-insensitive canonical-title match when the provider omits ROM revision details. Normalize known region and language tags for title lookup, but preserve them in the filename-association key. Reject multiple plausible candidates rather than guessing. Treat `ReleaseType` as a confidence/safeguard signal, not a blanket exclusion; supported libraries may include ports, homebrew, and ROM hacks.
- Try LaunchBox after any successful TheGamesDB search that does not produce a confident match, including empty, unrelated, and ambiguous result sets. Do not use LaunchBox to mask network errors, rate limits, authentication failures, or other provider errors.
- Run the same fallback pipeline for newly discovered games after library scans and for explicit manual metadata refreshes. Both paths use `LibraryMetadataMatcher`; do not duplicate provider logic in the scanner or Settings UI.
- Updating the LaunchBox catalog does not automatically retry unresolved games. After download and extraction complete, prompt the user to run the single **Refresh All Metadata** action; that action retries eligible metadata lookups and then retrieves missing TheGamesDB screenshots. The catalog's separate library-scoped LaunchBox artwork pass remains independent.
- During a catalog update, skip fallback lookups rather than opening a catalog while its file is being replaced. Existing LaunchBox associations remain intact if TheGamesDB has no result; a confident TheGamesDB match may upgrade a LaunchBox association to the primary provider.
- Never replace an existing metadata association with LaunchBox data. Add a database-level guard in addition to matcher-level routing. Missing LaunchBox fields must not erase populated values.
- Keep artwork storage and download logic separate from metadata association. The initial implementation downloads LaunchBox artwork only for local games with a saved LaunchBox `DatabaseID`; a future independent artwork-provider association can support games whose primary metadata came from TheGamesDB without changing metadata provenance.
- Parse `<GameImage>` records from the existing `Metadata.xml` and index image metadata locally by LaunchBox `DatabaseID`, image type, and region. Store the image `FileName` and construct downloads only from the verified image host `https://images.launchbox-app.com/{FileName}`; do not use the Games Database page URL, which produced redirect-loop behavior in diagnostics.
- Initially select one `Box - Front` cover and up to three `Screenshot - Gameplay` images. Prefer North America, then World, Europe, Japan, and unspecified regions; retain all indexed variants so selection can be adjusted without re-downloading the catalog.
- Download actual image files only when a LaunchBox-associated game in the user's available library needs them. Reuse cached files, validate HTTPS/final host, HTTP status, supported image MIME/signature, and a 20 MB response limit before database association. Never replace an existing cover automatically. Keep screenshots separate in the existing screenshot model.
- Use conservative image-request scheduling: one in-flight HTTP request at a time, process up to five requests, then wait 300 ms before the next batch. This is a RecompStation safety default, not a claim about LaunchBox's published limits (no authoritative numeric image-host limit has been established).
- Future hardening: handle HTTP 429 using `Retry-After` or bounded backoff, pause on HTTP 403, and retry transient network/5xx failures a limited number of times. The initial downloader reports failures and caches successful downloads but does not yet implement automatic retries.
- Do not integrate `Mame.xml` in this initial console-library implementation. It is an arcade/MAME machine catalog with distinct short-name and clone relationships; revisit it only if arcade library support is planned.
- Provide a manual catalog update action and consider an optional periodic update check. Do not silently download large archives without the user's enablement and update preferences.

**LaunchBox artwork integration — implementation status**

- [x] Reuse the locally indexed GameImage records and image lookup API keyed by LaunchBox database ID, type, and region.
- [x] Select one Box - Front cover (North America preferred) and up to three Screenshot - Gameplay images; do not fetch unrelated image categories or regional variants.
- [x] Download images only for available local games with an explicit LaunchBox metadata association. Do not download artwork for catalog-only games or infer an image association from title alone.
- [x] Use the verified images.launchbox-app.com CDN host, validate HTTPS/final host, HTTP success, supported image format, and a 20 MB response limit.
- [x] Cache covers and screenshots under the local game's artwork directory, reuse existing files, preserve existing cover artwork, and register cached files with the database.
- [x] Schedule one request at a time; after every five image requests, pause for 300 ms. This is intentionally more conservative than five concurrent requests.
- [x] Trigger refresh after successful catalog updates and metadata scrapes; defer a refresh request if a download batch is already active.
- [x] Add focused tests for trusted CDN URL construction, regional cover selection, gameplay screenshot selection, and cover-skip behavior.
- [x] Replace the separate manual metadata and screenshot actions with one **Refresh All Metadata** workflow: retry metadata matching first, then retrieve missing TheGamesDB screenshots. Preserve existing screenshot files and skip games that already have screenshots. **User-confirmed working in the application.**
- [x] Keep **Update Catalog** as a separate action; after the catalog finishes downloading and extracting, prompt the user to run **Refresh All Metadata**. The catalog's existing local-library-only LaunchBox artwork pass remains intact. **Runtime logs confirmed the catalog artwork pass and its completion reporting.**
- [ ] Build and run the complete regression suite after the latest unified-refresh changes; verify the coordinated sequence, skip-existing behavior, catalog completion reminder, and user-visible results on representative games.

**Implementation order**
- [x] Strengthen `DatabaseManager::associateMetadata()` so an existing provider association cannot be overwritten by a fallback insert; add regression tests for existing metadata and missing-field preservation. **Verified:** user confirmed `./scripts/build.sh --test` completed successfully with all tests passing.
- [x] Introduce and test a LaunchBox XML parser for `Platforms.xml`, `Files.xml`, and `Metadata.xml`; validate malformed, incomplete, duplicate, and unexpected records. **Verified:** user confirmed the new build completed and all tests passed.
- [x] Add explicit RecompStation-to-LaunchBox platform mappings and conservative filename/title matching, including region/revision handling and ambiguity rejection. **Verified:** user confirmed `./scripts/build.sh --test` completed successfully with all tests passing.
- [x] Build a local SQLite catalog/index and query API; avoid loading or reparsing the full XML archive for individual game lookups. **Verified:** user confirmed the local build and all 17 regression tests passed, including `LaunchBoxCatalogDatabaseTests`.
  - A separate SQLite catalog stores platform references/aliases, filename associations, and game metadata with lookup indexes. XML is streamed record-by-record into a transaction; a failed parse or insert rolls back and preserves the previous catalog.
- [x] Add archive download, progress reporting, cancellation, extraction, validation, and rollback-safe catalog replacement/update handling. Reuse existing archive infrastructure where appropriate. **Verified:** user confirmed the local build completed and all regression tests passed after background extraction/indexing and cancellation support were added.
  - Download remains asynchronous; archive extraction and SQLite indexing run through `QtConcurrent` with a worker-owned database connection. Cancellation is shared through extraction and each streamed XML record, and indexing cancellation rolls back its transaction. Extraction reports processed megabytes; indexing reports platform/file/game/finalization stages. The worker validates the staged database before the existing rollback-safe activation path. Regression tests cover pre-cancelled extraction cleanup and cancelled indexing preserving the previously indexed catalog.
- [x] Add persistent LaunchBox enablement, defaulting to disabled, plus catalog status and a user-controlled update workflow in Settings → Metadata. Download/index only when enabled. **Verified:** user confirmed the build and regression suite passed, found the new Settings controls at runtime, downloaded `Metadata.zip`, and successfully extracted/indexed the catalog through the application.
  - The preference defaults to disabled and persists across restarts; the last-successful-update timestamp is persisted. Settings exposes catalog availability, manual Update Catalog and Cancel controls, progress/status feedback, and clarifies that enabling the provider does not automatically download the archive. Actual TheGamesDB-to-LaunchBox fallback matching remains a separate subsequent task.
- [x] Integrate LaunchBox as a second-chance provider whenever TheGamesDB completes successfully without a confident match; preserve TheGamesDB priority and never fallback on provider errors. **Runtime matching confirmed:** Metroid Prime 2 was successfully associated through LaunchBox after TheGamesDB returned ambiguous results. Full regression/build verification and edge-case coverage remain pending.
  - Both automatic library-scan scraping and manual metadata refresh use the same matcher queue. Empty, unrelated, and ambiguous successful TheGamesDB results can enter LaunchBox fallback; network/authentication/provider failures cannot.
  - LaunchBox first checks a full normalized ROM filename against platform-specific `Files.xml` associations, retaining region/language/revision tokens. If no association exists, it queries platform-specific title candidates, normalizes region/language tags, prefers exact revision matches, and accepts a revision-insensitive match only when unique.
  - Revision variants are retrieved together so competing revisions remain ambiguous rather than silently selecting the canonical record. Platform restrictions remain mandatory.
  - Fallback metadata uses the database-level insert-only API, stores no invented artwork URL, and leaves artwork handling independent. A later confident TheGamesDB match can replace LaunchBox provenance with primary-provider metadata.
- [x] Keep the LaunchBox fallback metadata association free of invented artwork URLs; run a separate LaunchBox artwork manager only for saved LaunchBox associations, preserve existing cover paths, and gate image requests on the LaunchBox enablement preference. **Runtime verified:** the separate artwork manager completed library-scoped downloads successfully, and Metroid Prime 2 was confirmed with metadata, cover artwork, and gameplay screenshots.
- [ ] Add focused unit, parser, database, matching, archive/update, and end-to-end fallback regression coverage. Include disabled-provider, primary-match, genuine-no-match, provider-error, ambiguity, platform-collision, and no-overwrite cases.
- [ ] Build with `./scripts/build.sh --test`, run the complete regression suite, then perform local runtime checks for first-time enablement, download/cancellation, indexing, fallback matching, manual update, and disabled/default behavior before checking off implementation tasks.

**Scope and completion rule:** This work extends Milestone 4's automatic metadata maintenance. The checklist remains planned until the implementation, full automated regression suite, local build, and relevant runtime checks are completed and confirmed. Do not mark tasks complete based solely on code being committed.

- [x] Use one shared discovery/matching pipeline for manual and automatic scans (automatic post-scan scraping and the new manual **Refresh All Metadata** action use the same `LibraryMetadataMatcher` queue; user-confirmed runtime behavior)
- [x] Add recursive filesystem monitoring for configured platform folders, with a 750 ms debounce and automatic full-library reconciliation (implementation and regression tests committed; runtime scan/reconciliation behavior confirmed; latest full regression run still pending)
- [x] Detect added and removed games, including newly created nested folders; preserve unavailable database records and avoid duplicate registration (runtime scan behavior and duplicate-safe reconciliation confirmed; latest full regression run still pending)
- [x] Automatically identify metadata using conservative title and platform matching (runtime matching confirmed for representative titles; ambiguous matches remain unresolved for fallback handling and broader confidence-rule tests)
- [ ] Define and test conservative match-confidence rules; keep ambiguous matches unresolved
- [x] Add an isolated N64 ROM hashing utility with byte-order detection and MD5/CRC32 calculation. **Verified:** its regression coverage was included in the user-confirmed 18/18 CTest pass on 2026-11-20.
- [ ] Integrate TheGamesDB Games/ByGameHash requests (MD5 then CRC fallback) and preserve title search as the final fallback
- [ ] Replace the development-only environment-variable API-key workflow for production: review TheGamesDB licensing, shared-key policy, rate limits, attribution, and distribution terms; then choose an application-managed key/backend proxy or a secure user-key option
- [ ] Verify provider hash conventions against known-good N64 ROMs before enabling automatic hash association
- [x] Persist completed lookup outcomes and provider failures to avoid repeated automatic requests; explicit refresh can retry checked games (local validation pending)
- [ ] Further validate artwork cache behavior, provider rate-limit handling, and retry/backoff policy
- [ ] Surface scan status and exceptional review in the affected game/library context, not a separate Metadata Search page
- [ ] Add regression tests for matching, monitoring, duplicate events, API failures, and cache behavior

### Milestone 5: Settings and first-run experience

- [x] Move persistent preferences to `<dataDirectory>/config/settings.ini`, migrate existing QSettings values, and restore first-run setup when the file is absent.
- [x] Establish the first curated static theme foundation: Midnight Violet, Electric Blue, and Ember with semantic color tokens, coordinated surfaces/status colors, theme-aware background/artwork treatments, and live preview before applying. **Runtime verified:** selecting Electric Blue from Settings saved the preference and emitted the expected theme-change notification.
- [x] Add optional animated theme backgrounds and custom still-image backgrounds with preview/apply/restore behavior (initial lightweight wave ambience and image scaling implemented; further visual refinement remains)
- [x] Add compositor-respecting preferred window-size presets and safe windowed sizing (never force monitor-wide resolution changes; local Wayland validation pending)
- [ ] Make the UI adapt cleanly to 1080p (1920×1080), 1440p (2560×1440), and 4K (3840×2160), including Library presentations, Game Details, Settings, dialogs, overlays, and window resizing; preserve the intended spacious design at each resolution.
- [ ] Validate Qt high-DPI and desktop scaling behavior on the current KDE Plasma Wayland environment. Keep explicit monitor selection deferred; the compositor remains responsible for monitor placement and resolution.
- [x] Add a single configurable library root, catalog-derived platform folders, first-run setup, and startup scan preference
- [x] Replace the native folder chooser with the shared in-app folder browser; user confirmed folder creation works.
- [x] Style the first-run library-folder setup overlay and keep it scoped to the Library page.
- [x] Keep the setup overlay non-modal so the toolbar and Settings remain mouse-accessible; suppress it while the controller pause menu is open.
- [x] Add automatic filesystem monitoring for configured folders (runtime monitoring/reconciliation behavior confirmed; see Milestone 4 for implementation and validation details).
- [x] Cache gameplay screenshots independently of cover art for games with saved TheGamesDB associations.
- [x] Expose TheGamesDB provider status/enablement and one coordinated **Refresh All Metadata** action in Settings → Metadata; separate manual metadata and screenshot buttons have been retired.
- [ ] Revisit launcher installation/update preferences only when the underlying launcher workflows and higher-priority Settings work are settled; no broad all-launcher GitHub update polling is planned.
- [ ] Add useful contextual first-run guidance without forcing a lengthy setup wizard.
- [x] During initial setup, ask whether the user wants automatic RecompStation application-update checks and persist the choice; Yes, No, close, and Escape all complete the prompt, with dismissal saved as disabled. The preference is editable in Settings → Application. **Preference UI only:** release checking is not connected and makes no network requests.
- [ ] Audit Settings so every exposed control has implemented behavior, a clear purpose, and persistent state where appropriate; remove or defer nonfunctional controls.
- [ ] Implement the automatic application-update check workflow and a manual **Check for Updates Now** action; the opt-in preference UI and persistence are in place, but no release checks are performed yet.
- [ ] Check for application updates periodically (no more than once per day), compare the installed version with GitHub releases, and notify the user when a newer release is available.
- [ ] Update notifications must direct users to the GitHub release page for manual download; do not automatically download, replace, or install RecompStation. Handle API/network failures quietly and avoid repeating the same release notification.

### Milestone 6: Launcher experience

- [x] Present supported project discovery and installation contextually from game details. **Runtime verified:** the Banjo Recompiled workflow exposed the compatible launcher, completed installation, and successfully launched it through the game workflow.
- [ ] Derive and display one accurate Playable/Beta/Alpha status per game from curated project/launcher compatibility data (intentionally deferred until the core library experience is settled)
- [ ] Refine release/asset selection and architecture compatibility explanations
- [ ] Improve download progress, cancellation, extraction, installation, and failure feedback. **Partially runtime verified:** Banjo Recompiled archive download, extraction, installation, and Play Game were exercised successfully; cancellation and failure-path feedback still require dedicated validation.
- [ ] Improve installed-launcher status and confirmation-protected uninstall; make uninstall accessible from the relevant Game Details page as well as Settings, and never remove the user's ROM or unrelated game files.
- [ ] Make installed release/version information clear when available.
- [ ] Retain per-game launcher update checks in Game Details; defer a centralized check across every installed launcher to avoid unnecessary GitHub API traffic. A centralized launcher overview remains a future design discussion.

### Milestone 7: Input, accessibility, and regression pass

- [x] Add application-level arrow-key navigation for the Game Library Grid View and Enter/Return game selection
- [x] Support Escape to return from immersive game details to the Game Library
- [x] Add adaptive mouse, keyboard, and controller navigation hints with bundled icons
- [x] Detect controllers connected after application startup; switch hints when controller input is used
- [ ] Ensure keyboard navigation and visible focus across all screens (Library navigation works; Settings arrow-key routing and focus traversal have an initial fix committed, awaiting local verification)
- [x] Add controller-friendly category navigation and visible focus styling for Settings controls across the tested categories.
- [ ] Complete controller-only navigation across every settings control, dialog, dropdown, and long scrolling page.
- [ ] Add smooth right-analog-stick scrolling to the active Library Grid/List view, with a configurable-feeling dead zone, tilt-sensitive speed, and smooth deceleration on release; retain D-pad item-by-item navigation and existing mouse-wheel behavior.
- [ ] Add progressive navigation acceleration in Library List View for the left analog stick and D-pad: start at a deliberate, slow pace, then ramp up to much faster scrolling while the input is held for quicker movement through large libraries, while keeping selection stable and responsive.
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

- [ ] Decouple game-library platform discovery and folder creation from the recompilation-project catalog, so a platform can be scanned even when no native recompilation project exists for it
- [ ] Verify end-to-end GameCube game detection and ensure a dedicated GameCube folder is created under the configured RecompStation library root
- [ ] Verify end-to-end Wii game detection and ensure a dedicated Wii folder is created under the configured RecompStation library root
- [x] Add original Xbox as a first-class library platform with a dedicated folder and recursive .iso/.xiso candidate detection; local build/runtime verification pending. .iso must contain an Xbox XISO image, not an arbitrary or unconverted full-disc dump.
- [ ] Investigate Xbox 360 library support; first establish the correct supported disc/container format(s), naming/layout conventions, and reliable detection rules before implementing scanner support or creating the platform folder
- [ ] Define and test platform-specific file extensions, directory naming, and duplicate-detection rules; do not classify an unknown or unsupported file as a game
- [ ] Keep library detection support distinct from native recompilation-project availability and launcher compatibility
- [ ] Add curated projects only after validating release assets, package layout, executable path, and launch requirements
- [ ] Expand metadata-provider platform mappings and catalog coverage as supported platforms become available
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