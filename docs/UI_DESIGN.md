# RecompStation UI and Experience Specification

## Product identity

RecompStation is a dedicated environment for browsing and launching the user's own collection of games that have supported recompilation projects. It is not a storefront and should not imitate an existing console dashboard. The experience should feel polished, responsive, immersive, and distinctly RecompStation.

The library is the center of the application. Users should spend their time browsing and playing games, not maintaining metadata or navigating administrative screens.

## Primary navigation

The primary navigation contains only:

- **Game Library** — the default landing view and main application experience.
- **Settings** — appearance, display, library, metadata, controller, and application configuration.

Platforms are filters or navigation choices inside the Game Library, not a separate top-level destination. Metadata Search is not an active destination; metadata association and refresh belong to the library/game context. Launcher discovery, installation, launch, and uninstall are presented contextually from a game's details or supported-project state rather than requiring a separate management destination. Existing backend capabilities must remain available during the transition.

### Page hierarchy and navigation model

1. **Application shell**
   - Persistent compact primary navigation: Game Library and Settings.
   - A global system menu is an overlay for application-level actions such as fullscreen and quit; it is not a third content destination.
2. **Game Library** (default)
   - Collection header with an always-available game-title search field and platform filter. Search applies to the current platform selection, or the entire collection when All Platforms is selected.
   - Curated Grid/List presentation and view-specific size controls live together in the Library header. Grid density (Compact, Comfortable, Spacious) controls the number/size of cover cards; List row size (Compact, Comfortable, Spacious) independently controls row height, artwork size, and typography. Persist both preferences independently.
   - Game selection opens the immersive Game Details experience.
3. **Game Details** (full-window modal experience over the Library)
   - Back to Library is always visible.
   - Escape closes Details and returns to the prior Library context.
   - Present game metadata/artwork, platform, supported recompilation projects, release/install state, and relevant launch/install/uninstall actions together.
   - Project selection and installation prompts remain inside this context.
4. **Settings**
   - Category navigation: Appearance, Display, Library, Metadata, Controller, Application.
   - Category selection changes the content panel without creating another primary destination.
   - Settings content is scrollable; category navigation and content controls have distinct focus ownership.
   - Back/Escape returns to the Library when no nested popup/dialog is active.

Legacy Metadata Search, Platforms, and Launchers page implementations may remain temporarily in the QML stack during migration, but must not be exposed as primary navigation destinations. Retire their unreachable presentation code only after confirming that no current action or backend workflow depends on it.

## Game Library

The library should support:

- Browsing all games and filtering by platform.
- A prominent, responsive title-search field that filters the current collection as the user types, respects the selected platform filter, and provides a clear no-results state.
- Sorting options appropriate to a personal collection (for example title, platform, and recently added).
- Two curated presentation modes: a cover-art-forward Grid View (the signature default) and a compact, readable List View. Avoid arbitrary layout editors or unrestricted card resizing.
- Three curated Grid View density presets (Compact, Comfortable, Spacious) that adapt card sizing to available window width. Comfortable is the default.
- List View uses a left-aligned panel capped at approximately 52% of the available content width (with a responsive maximum), leaving the remaining library background visible for custom or animated backgrounds. Three independent row-size presets adjust row height and artwork/text scale.
- List rows use the same restrained selected/hover zoom treatment as Grid cards, with reduced-motion support and elevated stacking so neighboring rows do not visually cover the focused item.
- Grid/List and the active view-specific size selector in the Library header; do not duplicate browsing/presentation controls in Settings → Library. Persist Grid density and List row size independently.
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
- Selection and focus are separate concepts: the selected game remains selected when pointer/controller focus moves elsewhere; focus indicates the next keyboard/controller action target.
- The active primary destination and active Settings category must be visibly distinct from hover. Interactive controls must show a clear keyboard/controller focus treatment, not rely on color changes from hover alone.
- In the Library, directional input moves between game tiles and preserves a sensible current selection when entering/leaving Details. Enter/Return opens the selected game; Escape closes Details. Mouse selection and activation remain available.
- In Settings, Left/Right may move between category navigation and the content region; Up/Down move among controls in the active region. Popup controls (such as ComboBox menus) own focus while open; Escape closes the popup before leaving the current page.
- Focus must remain visible at all supported window sizes and in both Grid and List modes. Reduced Motion removes/shortens transitions without changing navigation outcomes.
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
- The implemented window modes are **Windowed**, **Maximized**, and **Fullscreen**. Fullscreen preserves the previous non-fullscreen mode so F11 can restore it.
- The Display page may identify the screen associated with the application window and report its desktop/available dimensions. This is informational; it does not currently provide explicit monitor selection.
- Do not describe Maximized as Borderless. Borderless-windowed mode and explicit multi-monitor selection remain deferred until they can be implemented and validated reliably across supported compositors.
- Preferred application window sizes are separate from monitor resolution. Never expose a control that changes the monitor-wide mode.

## Settings organization

Current categories:

- **Appearance:** built-in theme, background treatment, accent preset, motion preferences.
- **Display:** implemented Windowed/Maximized/Fullscreen modes, current-screen information, and preferred application window-size presets. Explicit monitor selection and borderless mode are deferred.
- **Library:** configurable library/platform folders, startup scan preference, manual scan action, and library background selection. Grid/List and grid-density controls are exclusive to the Game Library page.
- **Metadata:** provider configuration/status and metadata refresh behavior. Additional providers, field selection, and independent artwork-source management remain future work.
- **Controller:** connected-device status, controller-input enablement, active/default profile presentation, and navigation binding summary. Interactive remapping and per-device application remain future work.
- **Application:** application-level preferences and diagnostics/logging where implemented.

Launcher installation and management are contextual to Game Details rather than a Settings destination. A dedicated launcher Settings category may be added only when there are meaningful global launcher preferences to expose.

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


## Milestone 1 design audit (2026-11-18)

The current implementation was reviewed against this specification. This is a design/structure audit, not a claim that every later milestone feature is complete.

| Area | Current state | Audit outcome |
|---|---|---|
| Primary navigation | Library and Settings only | Aligned |
| Platform navigation | Filter inside Library | Aligned |
| Metadata Search | No longer a primary destination; legacy stack implementation remains | Aligned at UX level; legacy code cleanup deferred |
| Launcher management | Available contextually from Game Details | Aligned |
| Game Details | Full-window modal overlay with Back to Library and Escape | Aligned |
| Settings hierarchy | Appearance, Display, Library, Metadata, Controller, Application | Aligned |
| Focus model | Library grid/list navigation and Settings category/content focus handling; visible control focus styling | Implemented baseline; full accessibility/controller-only pass remains later work |
| Dialogs and popups | First-run library setup, folder/background pickers, uninstall confirmation, project install prompt, ComboBox popups, and global system/quit overlay | Contextual overlays; must preserve parent context and close in Escape order |
| Display modes | Windowed, Maximized, Fullscreen; F11 restores prior non-fullscreen mode | Documented accurately |
| Monitor handling | Current screen is informational; no explicit monitor selector or monitor-mode changes | Safe compositor-respecting baseline; additional selection deferred |
| Library presentation | Title search, platform filter, Grid/List, configurable density and backgrounds | Search and browsing controls are grouped in the Library; sorting and remaining library states tracked in later milestones |

Audit follow-up: remove obsolete unreachable Metadata Search/Platforms/Launchers presentation code only after checking dependencies; finish complete keyboard/controller focus and accessibility validation under the dedicated input milestone; validate display behavior on supported compositors before adding more modes.
