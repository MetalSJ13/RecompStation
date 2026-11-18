# RecompStation UI and Experience Specification

## Product identity

RecompStation is a dedicated environment for browsing and launching the user's own collection of games that have supported recompilation projects. It is not a storefront and should not imitate an existing console dashboard. The experience should feel polished, responsive, immersive, and distinctly RecompStation.

The library is the center of the application. Users should spend their time browsing and playing games, not maintaining metadata or navigating administrative screens.

## Primary navigation

The primary navigation contains only:

- **Game Library** — the default landing view and main application experience.
- **Settings** — appearance, display, library, metadata, launcher, and application configuration.

Platforms are filters or navigation choices inside the Game Library, not a separate top-level destination. Metadata Search is removed as a standalone page. Launcher discovery and installation should be presented contextually from a game's details or supported-project state rather than requiring a separate management destination. Existing backend capabilities must remain available during the transition.

## Game Library

The library should support:

- Browsing all games and filtering by platform.
- Optional text search for a specific game.
- Sorting options appropriate to a personal collection (for example title, platform, and recently added).
- Two curated presentation modes: a cover-art-forward Grid View (the signature default) and a compact, readable List View. Avoid arbitrary layout editors or unrestricted card resizing.
- Three curated Grid View density presets (Compact, Comfortable, Spacious) that adapt card sizing to available window width. Comfortable is the default.
- A small, immediate Grid/List selector in the library header; density remains in Settings → Library. Persist both preferences.
- A library-only background selector in Settings → Library: static theme-coordinated background (default), subtle optional animated ambience, or a user-selected custom still image (PNG/JPEG/WebP). Keep the background dark enough for legibility and preserve the active theme's accent and surface language.
- Animated ambience must pause when the window is hidden or minimized and be disabled when Reduced Motion is enabled.
- Custom backgrounds are still images only; do not turn this into a wallpaper editor or allow unrestricted colors/layout rearrangement.
- Cover-art-forward browsing with clear keyboard/controller focus and selection states in both presentation modes.
- A dedicated game-details experience with available metadata, artwork, platform, supported recompilation project information, launcher installation state, and launch action.
- Useful empty, loading, scanning, missing-artwork, metadata-unavailable, unsupported-project, and launch-error states.
- A single platform selector for narrowing the collection; library scanning remains in Settings rather than duplicating maintenance controls on the browsing page.

The library should not become a storefront: avoid promotional modules, sales-like badges, recommendations that imply products for purchase, or excessive marketing copy.

## Metadata and library maintenance

Metadata management is a background service, not a separate user workflow.

Use a combined approach:

1. (Planned) Monitor configured game folders for additions, removals, and relevant file changes, where supported by the platform and filesystem.
2. Provide a manual library scan action in Settings for initial setup and recovery. A configurable startup scan discovers new files; it must not re-query games that already have saved metadata or a completed lookup attempt.
3. Route discovery through one pipeline. Register newly found games first, then queue metadata only for games without a saved metadata association and without a completed lookup attempt.

The intended flow is:

1. Discover a ROM and identify its RecompStation platform.
2. Search the metadata provider using title and platform context.
3. Evaluate candidate matches and confidence.
4. Automatically associate a sufficiently reliable match.
5. Cache metadata and artwork locally.
6. Avoid repeatedly querying unchanged games; respect API limits and retry failures sensibly.
7. Keep uncertain matches unassociated and communicate that attention may be needed. Never silently attach a low-confidence or clearly incorrect match.

A user should not need to search a metadata database and manually associate every ROM. Any exceptional review should be presented in the context of the affected game, not as a separate metadata-management destination.

## Visual language

- Dark-themed surfaces by default, using layered charcoal/near-black tones rather than one flat black.
- Restrained accent colors, initially exploring blue-violet and electric-blue families.
- Strong typography hierarchy, comfortable spacing, consistent radii, and subtle surface separation.
- Game artwork is visually important, but layouts should remain readable and performant.
- Motion should be smooth and purposeful: game details rise into view from below, followed by a slightly slower artwork and information dissolve. All motion respects Reduced Motion.
- Use a consistent set of bundled SVG icon assets for primary navigation and common actions; avoid Unicode symbols as interface icons.
- Visible focus states and predictable interaction feedback are essential.
- Avoid copying Steam Big Picture or other existing launcher/dashboard layouts. Use them only as broad evidence that controller-friendly, immersive browsing can be engaging.

## Themes and backgrounds

Provide three distinct background/theme types:

1. **Static curated themes** — lightweight, coordinated palettes and surface treatments.
2. **Optional animated themes** — subtle ambient gradients or particles, with motion paused while the app is minimized or inactive and disabled when reduced motion is enabled.
3. **Custom still-image backgrounds** — user-selected PNG, JPEG, or WebP images, automatically scaled/cropped and previewed before applying.

A theme is a complete visual system, not just an accent-color choice. It coordinates the background, layered surfaces, controls, dividers, text hierarchy, status colors, and artwork overlays. Start with a small set of carefully designed presets and a clear default. The current static foundation provides Midnight Violet, Electric Blue, and Ember with coordinated semantic color tokens and a subtle background gradient. Selecting a preset previews it across the interface immediately; applying it saves the choice.

Keep theme selection and applying a preset explicit. Later background types should extend the same theme-selection experience rather than create a separate settings workflow. Animated backgrounds must remain optional and lightweight. Bundle only original, public-domain, or appropriately licensed artwork.

Do not begin with an overwhelming theme editor or dozens of arbitrary color controls.

## Display and window behavior

- Display behavior must respect the desktop compositor; RecompStation must never change the monitor-wide display mode.
- The current foundation detects the screen attached to the application window and offers preferred application window sizes, including common 16:9, 4:3, and ultrawide presets.
- Preferred sizes are constrained to the available logical desktop area; the compositor remains responsible for window placement and may impose additional constraints, particularly on Wayland.
- The default “Follow desktop” option leaves the current window size unchanged.
- Fullscreen/windowed/borderless mode controls and explicit multi-monitor selection will be added only when they can be implemented reliably across supported compositors.

## Settings organization

Initial categories:

- **Appearance:** built-in theme, background treatment, accent preset, motion preferences.
- **Display:** fullscreen/windowed/borderless mode, monitor selection, supported resolution.
- **Library:** configurable per-platform game folders, startup scan preference, and a manual scan action. Automatic monitoring remains a future capability until implemented.
- **Metadata:** provider configuration/status, cache management, refresh behavior.
- **Launchers:** managed installation location, installed launcher management, update behavior as it becomes available.
- **Application:** startup behavior, notifications, language where supported, diagnostics/logging.

Only expose settings that have real behavior. Avoid placeholder toggles.

## Input and accessibility

Mouse and keyboard remain supported. Controller navigation will support Xbox and PlayStation controllers with predefined mappings and user remapping. The frontend must suspend controller input while the launched game session owns the controller, then resume after the actual game process/session exits. Maintain readable contrast, accessible labels, predictable tab order, and scalable layouts.

## Implementation principles

- Keep backend and business logic in C++; keep presentation and navigation in QML.
- Extract reusable QML components when doing so reduces real duplication.
- Keep changes incremental and buildable.
- Preserve existing scanner, metadata provider, artwork, project catalog, download/install, uninstall, and process-launch behavior.
- Avoid mixing the visual redesign with a broad backend rewrite.
- Add filesystem monitoring and automatic matching as explicit, testable capabilities; do not imply that current one-shot scanning already provides continuous monitoring.
