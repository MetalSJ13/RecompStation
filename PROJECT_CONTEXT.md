# RecompStation — Project Context

## Objective

RecompStation is a native desktop environment for browsing and launching the user's own game collection through supported native recompilation projects. It manages game libraries and launchers; it is not an emulator frontend or a storefront.

## Technology and development environment

- **Language:** C++20
- **UI:** Qt 6.5+, Qt Quick / QML
- **Build:** CMake and Ninja
- **Data:** SQLite
- **System integration:** Qt Network and QProcess
- **Primary development environment:** Arch Linux / KDE Plasma

## Supported library platforms

- Nintendo 64
- PlayStation 1
- Nintendo GameCube
- Nintendo Wii

Platform folders are derived from platforms represented in the curated recompilation project catalog. Scanner support does not imply that every game has a compatible recompilation project. Launchability depends on catalog coverage and project-specific ROM requirements.

## Current project status

### Branch and merge policy

- **Active branch:** \`ui/interface-redesign\`
- **Backup branch:** \`feature/launcher-download-staging\`
- **Merge policy:** Keep the active branch separate from \`main\` until the user explicitly approves a merge.

### Current milestone

**Milestone 2 — Application shell and navigation.** The core navigation, Library browsing controls, contextual launcher workflow, Settings layout consistency, and Grid card entrance behavior are implemented. The remaining milestone exit task is a full navigation/backend regression pass. See \`DEVELOPMENT.md\` for the authoritative checklist and milestone status.

### Implemented capabilities

- **Library:** One configurable library root, catalog-derived platform subfolders, first-run setup, recursive scanning, persistent startup-scan preference, and debounced filesystem monitoring/reconciliation.
- **Metadata:** TheGamesDB asynchronous search and platform mapping, persistent provider enablement, local game-metadata associations, automatic conservative title/platform matching, persisted lookup outcomes, explicit retry support, and artwork caching.
- **Artwork:** Cover-art retrieval, local caching, and library thumbnails. Gameplay screenshots have a separate refresh/cache workflow under Settings → Metadata.
- **Game browsing:** Curated Grid/List presentation, title search, platform filtering, independent view sizing, theme-aware backgrounds, and keyboard/controller navigation hints.
- **Settings:** Appearance, Display, Library, Metadata, Controller, and Application categories. TheGamesDB provider state and metadata/screenshot refresh actions are grouped in Metadata settings.
- **Controller:** Live connection state, device name/type, input enablement, and read-only navigation-binding presentation. Stable SDL GUIDs and per-device profile persistence have backend foundations; applying mappings and interactive remapping are not complete.
- **Recompilation catalog:** Curated N64 project catalog, including Star Fox 64, Donkey Kong 64, Mario Kart 64, the Mario Party titles, Super Smash Bros., Ghostship, Banjo-Tooie, and PaperBoat (Paper Mario). Multiple projects can be offered for one game where applicable.
- **Linux launchers:** GitHub release discovery, architecture-aware Linux asset filtering, staged downloads, safe archive extraction, installation validation, installed-launcher discovery, managed uninstall, and native process launch/state reporting.
- **Tests:** Regression coverage exists for scanner, database, metadata, artwork, GitHub, catalog, archive, launcher-management, and process-launch components.

### Validation notes and known limitations

- Recent filesystem-monitoring, automatic metadata-matching, Settings, and database work has implementation and regression-test coverage in the repository, but local build/runtime validation must be confirmed by the user before being described as complete.
- Representative Linux launcher workflows have been user-tested successfully for Star Fox 64, Donkey Kong 64, Banjo-Tooie, and PaperBoat.
- Controller remapping is not yet implemented; current navigation mappings are informational/read-only.
- Full accessibility/controller-only validation and the Milestone 2 navigation/backend regression pass remain outstanding.
- The TheGamesDB API key is currently read from the development process environment variable \`RECOMPSTATION_TGDB_API_KEY\`. This is a temporary development mechanism, not the intended production experience. Production authentication must be designed after reviewing TheGamesDB licensing, shared-key rules, rate limits, attribution, and distribution requirements. A backend proxy/application-managed key or secure user-provided key are candidates; no production approach has been selected.

## Product and UI direction

The detailed visual and interaction specification is maintained in \`docs/UI_DESIGN.md\`.

### Primary navigation

1. **Game Library** — default landing view for browsing, title search, platform filtering, Grid/List presentation, game details, and game-context actions.
2. **Settings** — application configuration.

Metadata Search, Platforms, and standalone Launcher Discovery are not primary destinations. Platform filtering belongs in the Library; launcher discovery and management are contextual to Game Details.

### Visual language and interaction

- Dark, immersive personal-collection presentation; do not imitate a storefront or Steam Big Picture.
- Restrained blue-violet/electric-blue accents, strong artwork presentation, consistent typography and spacing.
- Smooth, purposeful transitions and clear focus/selection states.
- Grid is the default Library view; List remains available with independent row sizing.
- Library backgrounds support a coordinated static theme treatment, optional animated ambience, and a user-selected still image. Reduced Motion must be respected.
- Grid cards use a staggered bottom-up entrance on startup, List-to-Grid switching, and return from Settings. Do not replay it for ordinary search/filter changes or opening/closing Game Details.
- Keyboard and controller navigation should remain clear and consistent. Controller input must be relinquished while a launched game session is active and restored after that process/session exits.

### Settings structure

Settings categories are Appearance, Display, Library, Metadata, Controller, and Application. Keep category pages visually consistent, scrollable when needed, and equipped with visible focus styling.

- **Library:** Library root, platform folders, scanning, and monitoring configuration.
- **Metadata:** TheGamesDB enablement/configuration state, metadata refresh, and gameplay-screenshot refresh.
- **Controller:** Device state and current input settings; do not present planned remapping as implemented.
- **Display:** Window sizing only. Respect the desktop compositor; preferred window-size presets must never change monitor-wide display modes.

### Display and desktop compatibility

The initial compatibility target is KDE Plasma on Wayland. Display settings detect the current screen and persist preferred window-size presets constrained to available desktop space. These are application-window dimensions, not monitor resolution changes. The system preset leaves the current window size unchanged. X11/Xlibre and other desktop environments can be investigated later.

## Metadata automation direction

Metadata is a background library service, not a manual chore. Manual scans and automatic folder monitoring should use a shared discovery/matching pipeline.

Current matching behavior uses conservative normalized-title matching within the mapped platform. Ambiguous and missing results remain unresolved. Lookup outcomes and provider failures are persisted to avoid redundant automatic requests; explicit refresh can retry previously checked games.

N64 hash groundwork exists in \`N64RomHash\`:
- Detect ROM byte order from magic.
- Normalize ROM data to canonical big-endian order.
- Calculate MD5 and CRC32.

The next hash-related work is to integrate TheGamesDB \`Games/ByGameHash\` requests, verify provider hash conventions against known-good ROMs, and preserve title search as a fallback. Hash-based association must not override conservative matching until verified.

The metadata workflow should:
1. Discover a ROM and platform.
2. Search using hash/title and platform context as supported.
3. Associate only sufficiently confident matches.
4. Leave ambiguous or low-confidence results unresolved.
5. Cache metadata and artwork, avoid redundant calls, and respect provider limits.
6. Surface exceptional review in the context of the affected game.

Provider priority/fallback and independent artwork-source selection remain future work.

## Development workflow

From the repository root:

\`\`\`bash
./scripts/build.sh
./scripts/build.sh --test
./scripts/run.sh
\`\`\`

The build script reuses the existing build directory and generator. Prefer incremental builds; do not routinely delete or recreate the local build directory.

Keep business logic in C++ and presentation/navigation in QML. Make small, buildable changes, preserve existing backend workflows, and add regression tests for backend changes. Keep \`DEVELOPMENT.md\` as the detailed implementation roadmap and authoritative checklist.
