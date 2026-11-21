### First-run setup visibility and controller pause menu
- The first-run library setup overlay is visible only while the Library page is active and no library root is configured.
- Navigating to Settings hides the setup overlay; returning to Library restores it until setup is completed.
- The controller pause menu takes precedence over first-run setup. The setup overlay reappears after the pause menu closes if still required.
- Keep the pause panel centered and modestly enlarge its width and internal spacing without allowing it to dominate the screen.

# RecompStation UI and Experience Specification

## Product identity

RecompStation is a dedicated environment for browsing and launching the user's own collection of games that have supported recompilation projects. It is not a storefront and should not imitate an existing carousel dashboard. The experience should feel polished, responsive, immersive, and distinctly RecompStation.

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
   - Curated Grid/List/Carousel presentation and view-specific size controls live together in the Library header. Grid and Carousel use independent persistent card-size preferences (Compact, Comfortable, Spacious); List row size remains independent. Carousel uses a single horizontal row of centered cover cards, smooth horizontal navigation, selected-card emphasis, and progressive left/right edge fading. Persist the selected view and both size preferences.
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
   - All Settings categories share a 30 px content gutter, a consistent page-title/description/divider hierarchy, 16 px section spacing, and disabled horizontal scrolling; vertical scrolling appears only when content exceeds the viewport.
   - Back/Escape returns to the Library when no nested popup/dialog is active.

The obsolete Metadata Search, Platforms, and standalone Launcher Discovery page implementations have been removed from the active QML page stack. Their underlying metadata, platform-filtering, and launcher backend services remain available through the Library, Settings, and Game Details workflows.

## Game Library

The library should support:

- Browsing all games and filtering by platform.
- A prominent, responsive title-search field that filters the current collection as the user types, respects the selected platform filter, and provides a clear no-results state.
- Sorting options appropriate to a personal collection (for example title, platform, and recently added).
- Two curated presentation modes: a cover-art-forward Grid View (the signature default) and a compact, readable List View. Avoid arbitrary layout editors or unrestricted card resizing.
- Three curated Grid View density presets (Compact (220 px), Comfortable (270 px), Spacious (350 px)) that calculate responsive columns from target card widths and distribute the full available viewport width without a trailing empty strip. Comfortable is the default.
- Grid cards use a shared 2:3 artwork frame across platforms. Portrait covers remain uncropped and centered; landscape covers remain fully visible over a centered, aspect-preserving blurred/cropped backdrop, except N64 artwork, which is displayed cleanly without a blurred fill. Grid and List cards show the platform label without a redundant metadata badge.
- List View uses a left-aligned panel capped at approximately 52% of the available content width (with a responsive maximum), leaving the remaining library background visible for custom or animated backgrounds. Three independent row-size presets adjust row height and artwork/text scale.
- List rows use the same restrained selected/hover zoom treatment as Grid cards, with reduced-motion support and elevated stacking so neighboring rows do not visually cover the focused item.
- Switching between Grid, List, and Carousel uses a restrained cross-fade. On startup, Grid delegates begin in their hidden entrance state so they cannot flash fully visible before the reveal; the staggered reveal waits for fullscreen layout readiness. Returning to Grid and returning to the Game Library from Settings replay the same bottom-up fade, subtle scale-up, and upward movement from below the background; cards within a row appear from left to right. The sequence is based on the visible viewport rather than the full library, and is disabled by Reduced Motion. Searching, changing the platform filter, and opening or closing Game Details do not replay the entrance.
- Grid/List/Carousel and the active view-specific size selector in the Library header; do not duplicate browsing/presentation controls in Settings → Library. Persist the shared Grid/Carousel card density and List row size independently.
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

## Metadata and artwork maintenance

- LaunchBox catalog indexing may retain the full catalog for matching, but image-file downloads must be restricted to games present in the user's available local library.
- Resolve artwork records only through a saved, confident LaunchBox DatabaseID association in the local library. Do not infer an image association from title alone or download images for catalog-only games.
- Fetch one preferred `Box - Front` cover and up to three `Screenshot - Gameplay` images per matched local game. Prefer North American artwork when available, preserve an existing cover, and reuse valid cached files.
- Use the verified LaunchBox image CDN host `images.launchbox-app.com`. Keep requests sequential and pause 300 ms after every five image requests. Display artwork progress and downloaded/skipped/failed totals separately from catalog-indexing progress.

## Display and window behavior

- Display behavior must respect the desktop compositor; RecompStation must never change the monitor-wide display mode.
- The current foundation detects the screen attached to the application window and offers preferred application window sizes, including common 16:9, 4:3, and ultrawide presets.
- Preferred sizes are constrained to the available logical desktop area; the compositor remains responsible for window placement and may impose additional constraints, particularly on Wayland.
- The default “Follow desktop” option leaves the current window size unchanged.
- The implemented window modes are **Windowed**, **Maximized**, and **Fullscreen**. Every application launch requests **Fullscreen** by default, including first-run and existing installations. Fullscreen is declared as the initial native window state before the Wayland surface is mapped, avoiding a visible windowed-to-fullscreen transition. F11 can temporarily restore the previous non-fullscreen mode; the next application launch starts fullscreen again.
- The Display page may identify the screen associated with the application window and report its desktop/available dimensions. This is informational; it does not currently provide explicit monitor selection.
- Do not describe Maximized as Borderless. Borderless-windowed mode and explicit multi-monitor selection remain deferred until they can be implemented and validated reliably across supported compositors.
- Preferred application window sizes are separate from monitor resolution. Never expose a control that changes the monitor-wide mode.

## Settings organization

Current categories:

- **Appearance:** built-in theme, background treatment, accent preset, motion preferences.
- **Display:** implemented Windowed/Maximized/Fullscreen modes, current-screen information, and preferred application window-size presets. Explicit monitor selection and borderless mode are deferred.
- **Library:** configurable library/platform folders, startup scan preference, manual scan action, and library background selection. Grid/List and grid-density controls are exclusive to the Game Library page.
- **Metadata:** provider configuration/status and metadata refresh behavior. Additional providers, field selection, and independent artwork-source management remain future work.
  - Keep manual metadata refresh and gameplay-screenshot download actions in Settings → Metadata, alongside provider configuration. Settings → Library is limited to ROM folders and library scanning.
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
| Metadata Search | Removed from the page stack; metadata controls remain in Settings and game context | Aligned; obsolete presentation code removed |
| Launcher management | Available contextually from Game Details | Aligned |
| Game Details | Full-window modal overlay with Back to Library and Escape | Aligned |
| Settings hierarchy | Appearance, Display, Library, Metadata, Controller, Application | Aligned |
| Focus model | Library grid/list navigation and Settings category/content focus handling; visible control focus styling | Implemented baseline; full accessibility/controller-only pass remains later work |
| Dialogs and popups | First-run library setup, folder/background pickers, uninstall confirmation, project install prompt, ComboBox popups, and global system/quit overlay | Contextual overlays; must preserve parent context and close in Escape order |
| Display modes | Windowed, Maximized, Fullscreen; F11 restores prior non-fullscreen mode | Documented accurately |
| Monitor handling | Current screen is informational; no explicit monitor selector or monitor-mode changes | Safe compositor-respecting baseline; additional selection deferred |
| Library presentation | Title search, platform filter, Grid/List/Carousel, configurable density and backgrounds | Search and browsing controls are grouped in the Library; sorting and remaining library states tracked in later milestones |

Audit follow-up: finish complete keyboard/controller focus and accessibility validation under the dedicated input milestone; validate display behavior on supported compositors before adding more modes.


### Library toolbar
- Keep the collection heading on the left and use a compact, right-aligned single-row filter toolbar. The toolbar has a capped width rather than stretching across the entire window; search receives the flexible remaining space, while Platform, View, and Card/Row Size use deliberate proportional widths.

### List View presentation
- List rows use up to 68% of the available Library content width, capped at 1,200 px, rather than appearing as a narrow column on wide displays.
- Unselected rows use translucent surfaces with a horizontal fade toward the right edge so the selected background artwork remains visible.
- The selected row retains a stronger translucent surface and accent border for clear focus. Artwork and text remain fully opaque and readable.
- List rows remain left-aligned; the expanded width is not centered.


### List View density sizing
- Compact: 104 px row height, 68 × 82 px artwork frame, 14 px title text.
- Comfortable: 128 px row height, 86 × 104 px artwork frame, 16 px title text.
- Spacious: retain the existing 144 px row height, 94 × 116 px artwork frame, and 17 px title text.
- Keep the list density steps visually distinct while making Compact and Comfortable less undersized relative to Spacious.


### Library scroll-edge fades
- Grid View and List View apply a position-based opacity fade to cards near the top and bottom viewport edges.
- List View retains its existing 140 px fade behavior.
- Grid View uses a broader 240 px fade zone with a smoothstep easing curve and a subtle 0.18 minimum opacity for partially visible cards. This avoids the linear, abrupt-looking fade during fast scrolling.
- Grid View combines the scroll-edge opacity with its existing startup reveal opacity.
- Reduced Motion disables the scroll-edge fade by keeping cards fully opaque.


### Carousel / Carousel View
- Add a third Library presentation alongside Grid and List: a single horizontal row of cover cards, inspired by carousel game-library interfaces.
- Use a consistent, moderate card width so several titles are visible at once; center the selected card and keep its artwork/title treatment legible.
- Scroll horizontally with smooth flicking and animated directional navigation; selecting a card opens the same Game Details experience as the other views.
- Reuse the Library card-size preference with carousel-appropriate widths, while keeping card height consistent.
- Fade cards progressively near the left and right viewport edges to reinforce the carousel depth/continuity. Reduced Motion disables this fade.
- Keep the view choice persistent and reuse the same filtered library model, platform filter, and search results as Grid and List.
