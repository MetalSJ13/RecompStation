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
- Cover-art-forward browsing with clear keyboard/controller focus and selection states.
- A dedicated game-details experience with available metadata, artwork, platform, supported recompilation project information, launcher installation state, and launch action.
- Useful empty, loading, scanning, missing-artwork, metadata-unavailable, unsupported-project, and launch-error states.
- A visible **Scan / Refresh Library** action that can initiate a user-requested scan and metadata scrape.

The library should not become a storefront: avoid promotional modules, sales-like badges, recommendations that imply products for purchase, or excessive marketing copy.

## Metadata and library maintenance

Metadata management is a background service, not a separate user workflow.

Use a combined approach:

1. Monitor configured game folders for additions, removals, and relevant file changes, where supported by the platform and filesystem.
2. Provide a manual library scan/scrape action for initial setup, recovery, and user-requested refreshes.
3. Route both paths through the same discovery and metadata-matching pipeline.

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
- Motion should be smooth and purposeful: page transitions, selection changes, and details presentation must never impede navigation.
- Visible focus states and predictable interaction feedback are essential.
- Avoid copying Steam Big Picture or other existing launcher/dashboard layouts. Use them only as broad evidence that controller-friendly, immersive browsing can be engaging.

## Themes and backgrounds

Provide a curated set of built-in appearance presets. A preset should coordinate the background, accent, and surface palette so the application remains coherent. Users may choose a preset and optionally a supported background treatment or bundled background image. Custom user-supplied images can be considered later.

Do not begin with an overwhelming theme editor or dozens of arbitrary color controls. The first implementation should have a small, high-quality selection of presets and a clear default.

## Display and window behavior

- Fullscreen is the default launch mode.
- Provide supported alternatives such as windowed and borderless fullscreen.
- Display configuration must respect the desktop compositor and expose only modes that can be applied reliably; fullscreen remains the intended default.
- Display selection and resolution options must be based on actual modes reported by the system; do not offer arbitrary unsupported resolutions.
- Retain sensible minimum window dimensions when windowed.
- Display changes must be recoverable if a selected mode cannot be applied.

## Settings organization

Initial categories:

- **Appearance:** built-in theme, background treatment, accent preset, motion preferences.
- **Display:** fullscreen/windowed/borderless mode, monitor selection, supported resolution.
- **Library:** configured game folders, automatic monitoring, scan behavior, sorting defaults.
- **Metadata:** provider configuration/status, cache management, refresh behavior.
- **Launchers:** managed installation location, installed launcher management, update behavior as it becomes available.
- **Application:** startup behavior, notifications, language where supported, diagnostics/logging.

Only expose settings that have real behavior. Avoid placeholder toggles.

## Input and accessibility

Mouse and keyboard remain supported. The design should be built with a clear focus model so full controller navigation can be added without redesigning every screen. Controller support is a planned experience goal, not a claim of current functionality. Maintain readable contrast, accessible labels, predictable tab order, and scalable layouts.

## Implementation principles

- Keep backend and business logic in C++; keep presentation and navigation in QML.
- Extract reusable QML components when doing so reduces real duplication.
- Keep changes incremental and buildable.
- Preserve existing scanner, metadata provider, artwork, project catalog, download/install, uninstall, and process-launch behavior.
- Avoid mixing the visual redesign with a broad backend rewrite.
- Add filesystem monitoring and automatic matching as explicit, testable capabilities; do not imply that current one-shot scanning already provides continuous monitoring.
