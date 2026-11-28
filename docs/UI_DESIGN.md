# RecompStation UI and Experience Specification

Last reviewed: 2026-11-25. This document describes the interface as it is built, and the rules new work follows. The current status and roadmap are in [`DEVELOPMENT.md`](../DEVELOPMENT.md); earlier design audits and step-by-step history are in [`development-history.md`](development-history.md).

## Product identity

RecompStation is a dedicated environment for browsing and playing the user's own game collection through native recompilation projects. It is not a storefront and does not imitate an existing dashboard (RecompLauncher, Steam Big Picture and others were references only). It should feel polished, responsive, immersive and distinctly RecompStation.

The library is the centre of the application. Players spend their time browsing and playing, not maintaining metadata or managing launchers: scanning, matching, artwork, launcher installs and updates happen in the background or in the context of a game.

Avoid storefront patterns: promotional modules, sales-like badges, recommendations, marketing copy.

## Application shell

- **Two destinations:** Library (the default) and Settings, as tabs at the right of the header. Platforms, metadata and launchers are not destinations of their own.
- **Header:** the logo and name; the system picker; on the Library, a search button, the view switch (Shelves, Grid, List, Carousel) and the current view's size toggle; then Library and Settings.
- **Hint bar:** a small bar at the bottom shows the actions available right now. It follows the last input used: controller glyphs that match the connected controller (Xbox or PlayStation layout, and the user's remapping), keyboard keys, or mouse hints.
- **System menu:** an overlay opened with the controller's Menu/Start button, with Resume, Settings and Quit RecompStation; Ctrl+Q opens it at the quit confirmation. It takes precedence over everything else and closes back to where the user was.
- **Window:** fullscreen on every launch, declared before the window appears so there is no windowed-to-fullscreen jump. F11 toggles fullscreen for the session. Windowed and Maximized are available in Settings › Display.
- **First-time setup:** a full-window flow shown until a library folder is chosen: library folder (suggests `~/Games/RecompStation` or `~/RecompStation`), Add games, covers and info (the LaunchBox catalog download with stages, Cancel and Retry), ready.

## Library

### System picker and search

- One chip in the header: "All systems" with the game count, or the selected system. Its list shows only systems that have games, grouped by maker. LB/RB (Q/E) step through them; the controller's Select button opens the list. Picking a system plays the switch sound; opening the list is silent.
- Twelve systems, ordered by maker and generation: Game Boy, Game Boy Color, Game Boy Advance, Super Nintendo, Nintendo 64, GameCube, Wii, Wii U, PlayStation, PlayStation 2, Xbox, Xbox 360.
- Search is a button that expands into a field. It filters by title as the user types, within the selected system, and shows a clear no-results state.
- Sorting is not implemented yet; games are in title order within each system.

### Views

Four views share the same filtered games, selection and system choice. The view and each view's size preset persist. Carousel is the default for new installs.

**Shelves.** A spotlight panel at the top for the selected game: system chip, title, year and rating, a short overview, Details, and the cover at its true shape on the right over a blurred backdrop. Below it, one horizontal shelf per system. Shelf covers have no captions; a placeholder cover shows the title instead.

**Grid.** Artwork-forward: the cover is the card, with no surface or padding, and the focus ring sits outside the artwork. Size presets set the target card width (Compact 220 px, Comfortable 270 px, Spacious 350 px); columns fill the full width without a trailing empty strip. With All systems, the grid is sectioned by system, each section with a header (name and game count). Captions show the matched title, then one status line: "Recomp installed", "Recompilation available", or the system name. Both grids leave a 14 px left inset so the grown selected card and its ring are never clipped.

**List.** Rows on the left (about 45% of the width, 420–900 px) and a preview pane on the right. Rows: a platform-shaped thumbnail in a fixed column so titles align, the title, system and year, and the recompilation status (hidden at Compact). Size presets set the thumbnail height (60 / 84 / 108 px). With All systems, rows are grouped under system headers. The selected row has an elevated surface, an accent border and a slight scale. The preview pane: a screenshot header (blurred cover when there is none), the cover, system and status chips, title, year and rating, Play (when installed) and Details, a four-line overview and up to three screenshots.

**Carousel.** The selected game's first screenshot fills the window behind the page (a blurred, dimmed cover when there is none), cross-fading between selections under a gradient that keeps text legible; the collection heading is hidden. One edge-to-edge row of covers in their platform shapes, the selected one centred and about 1.43× larger with the focus ring, fading toward the window edges. Size presets set the row height (170 / 210 / 250 px, scaled gently with window height). Beneath it: system and position ("N of M") chips, title, year and rating, Play when installed, and Details.

### Covers

- Each cover uses its system's box shape: N64 and SNES 7:5; PlayStation 1:1; GameCube, Wii, Wii U, PS2 and Xbox 5:7; Game Boy, Game Boy Color and Game Boy Advance 0.92. A cover whose real shape differs by more than 12% (for example a PAL portrait N64 box) keeps its own shape.
- Artwork comes from LaunchBox and TheGamesDB only.
- A game without a cover gets a placeholder at its system's shape: a plain dark tile with the logo faintly in the middle.

### Interaction

- Directional input moves the selection; Enter/A plays an installed recompilation or opens Game Details; the Details action (Y by default, `I` on the keyboard) opens Game Details from any view. Clicking a cover opens Game Details; in the Carousel, hover highlights without changing the selection.
- Switching views cross-fades. Grid plays a staggered bottom-up entrance (cards in a row appear left to right) on startup, when switching to Grid and when returning from Settings; searching, filtering and opening or closing Game Details do not replay it.
- Grid and List fade cards near the top and bottom edges while scrolling (smoothstep, minimum opacity 0.18).
- Selection and focus are separate: the selected game stays selected while focus moves to the header or a dialog.
- Screen readers hear the selected game and its system in every view.

### Backgrounds

Settings › Background, Library only:
- **Still:** one tile with four patterns in the theme's colours, previewed when the tile is opened: Gradient, CRT glow (the default for new installs), Dithered sunset and Circuit board (`shaders/retro.frag`, `RetroBackground.qml`, drawn once into a cached layer).
- **Moving:** one tile with four scenes drawn live by GPU shaders (`WaveBackground.qml`): Waves (two ribbons of thin glowing lines), Twinkling stars, Floating lights and Colour flow (`shaders/waves.frag`, `stars.frag`, `lights.frag`, `flow.frag`). Colour presets, the theme accent or a custom hue; Motion (Calm / Normal / Lively); Quality (Auto, High, Balanced at 75% resolution, Low at half resolution and 30 fps). It pauses on its current frame under Reduced motion, while a game runs, and when the window is inactive or minimized. Qt's software renderer falls back to the gradient.
- **Image:** the user's still image (PNG, JPEG, WebP), decoded at screen size; Fill (crop) or Fit (whole image over a blurred copy) and an adjustable Dim.

No wallpaper editor and no arbitrary colour or layout controls.

## Game Details

A full-window overlay over the Library. Back to Library (B, Esc or the button) returns to the same selection; LB/RB switch to the previous or next game, sliding the page in from that side with the shoulder-button sound, with the position ("N of M") in the corner. Menu/Start opens the system menu on top of Game Details; closing it returns to the same game and button.

**Hero.** The first screenshot as a backdrop, the cover at its true shape, chips for the system and the recompilation project with its state ("Installed", "Available"), the title, year and rating.

**Actions.** One primary action that follows the launcher's state: Find compatible launcher → Install → Downloading/Installing → Play. Beside it: Check for updates, which becomes "Update to <version>" when a newer release is out (not while the game runs), and ⋯, a drop-down with Show install folder, Open launcher log and Uninstall… (set apart in red, with a confirmation).

**Status line.** One line under the actions with a coloured dot: ready to play, checking your copy, wrong version or wrong game (region and revision read from the ROM), update available ("your saves are kept"), up to date, or why no package fits this system (Windows or macOS only, ARM only, Flatpak only, no downloads).

**About and screenshots.** The overview, and up to three selectable screenshots that open a full-screen viewer.

**Recompilation card.** Kept short for players:
- Project name and repository.
- Status (Installed, Not installed, Downloading…), Version (the installed version, plus "· vX available"; or the latest release when not installed), and Requires (the game file it needs, e.g. "Banjo-Tooie (USA) ROM").
- Launch section, when the project has a launch recipe: the game file from the library, a per-game Window choice (◀ Default (fullscreen) ▶, applied from the game's second start), and the saves folder with Open folder.
- Package file names, install paths and checksum details are deliberately not shown; the install folder is in the ⋯ menu.

**Without game info.** A "No game info" chip, the file name under the title, Find game info and Metadata settings buttons, an About card explaining what happened, empty screenshot frames, and an accent glow behind the page when there is neither a screenshot nor a cover.

Opening plays the Open sound in every view, closing plays Back once.

## Settings

Layout: a category rail on the left, each entry with a one-line summary of its current values; setting rows in the middle, capped at 880 px; and, when the window is wide enough, a preview or reference pane on the right (Appearance, Display, Background, Metadata). Choices are ◀ value ▶ rows (`ValueCycler.qml`: left at the first value returns to the rail), switches (`ToggleSwitch.qml`), tiles or Settings-style buttons (`SettingsButton.qml`). No dropdowns and no Apply buttons; changes save immediately. LB/RB (Q/E) switch categories; Back returns to the Library when nothing nested is open.

- **Appearance:** theme tiles (each a miniature Library in its colours; selecting applies and saves), Text size, Reduced motion, and Sounds (Interface sounds on/off, volume).
- **Display:** Interface scale (Auto from the screen's logical height, or 75–200%; applied at startup, so changing it offers a restart that reopens this page), Windowed / Maximized / Fullscreen, and window-size presets for Windowed mode. The screen is shown for information; RecompStation never changes the monitor's mode.
- **Background:** Still, Moving or Image, with their options (see Backgrounds).
- **Library:** library folder and system folders, Add games, startup scan and manual scan, and Playing (prepare games for their launcher, start games in fullscreen).
- **Metadata:** provider cards (LaunchBox with catalog download, update, size and any systems missing from an old catalog; TheGamesDB with the user's own key, stored privately, its connection and requests left), which provider is asked first (LaunchBox by default), automatic matching and artwork switches, and Refresh all metadata.
- **Controller:** connected devices, a drawing of the controller with a live button test, button symbols, and remapping. Capturing a button shows clear states: let go first, listening with a 15-second countdown, replacing (old inputs struck out), added (with a warning when another action uses it), and nothing pressed; other rows dim. No sounds while mapping.
- **Application:** about (version), Updates (status, Check now, Update…, Check automatically, a skipped version), storage use, data and log folders, detailed logging, launchers whose game is gone, and reset settings.

Only settings with real behaviour are shown; no placeholder controls. Descriptions are written for players: what the setting does and what it keeps, in a sentence or two, without internals (file layouts, environment variables, render percentages, retry timings).

## Application updates

- Checked at most once a day at startup (Settings › Application › Check automatically, on by default) or with Check now; RecompStation always asks before updating.
- A newer version is offered once, when nothing else is in the way: title, "You have 0.1.0 · released … · 61 MB", What's new (the Changes section of the release notes), a one-line promise that games, settings and launchers stay as they are, and Update now / Remind me later / Skip <version>. A line under the buttons explains the focused one; skipping shows "Skipped <version> · Undo".
- The same panel then shows downloading (Hide keeps it going, with a header chip), checking, ready (Restart now / Later) and failed (nothing changed).
- Wording avoids internals (checksums, file types, build steps). Release notes describe what improved in RecompStation, never "this game works now".
- Copies built from source say they can't update themselves and link to the website.

## Visual language

- Dark, layered surfaces rather than flat black; strong typography, consistent spacing and radii, subtle separation.
- Themes are complete visual systems (background, surfaces, controls, text, status colours, artwork overlays): Electric Blue (default), Midnight Violet, Aurora (deep teal, mint accent) and Carbon (graphite, crimson accent). Ember was retired in November 2026; saved Ember settings move to Electric Blue.
- Muted text meets WCAG AA (4.5:1) on panels in every theme.
- Bundled SVG icons for navigation and actions; no Unicode symbols as icons (the ◀ ▶ arrows in value rows are the exception).
- Focus is always visible and distinct from hover; the active tab and Settings category are clearly marked.
- Motion is smooth and purposeful (Game Details rises into view, then its artwork and text dissolve in) and respects Reduced motion everywhere.

## Sounds

The "Glass, refined" set, original and synthesized (`tools/sounds/`), played through SDL2 audio:
- Move when focus moves, Open a game, Back, a swoosh when cycling with the shoulder buttons, a switch sound for picking a system, menus, switches on and off, Play and a failed launch.
- Rapid repeats are thinned so held directions don't stack; silent while a game runs, and never while mapping buttons.
- On by default at 60%; Settings › Appearance › Sounds.

## Input and accessibility

- Controller, keyboard and mouse all reach everything. Xbox and PlayStation controllers have default mappings and can be remapped; a second controller takes over when it is used.
- While a launched game runs, RecompStation releases the controller and resumes after the game's process exits.
- Popups and dialogs own focus while open; Back closes the innermost one first; focus returns to the control that opened them.
- Accessible names on the header tabs, Game Details' back button and mapping chips; every control is reachable in tab order.

## Metadata and library maintenance

Metadata is a background service, not a workflow:
1. The library folder is watched; added and removed games are reconciled automatically, and a manual scan is in Settings › Library for recovery.
2. New games are registered first, then matched: LaunchBox's offline catalog (by file name, then title) and TheGamesDB, in the order chosen in Settings.
3. A match is accepted only when one result clearly fits; uncertain games stay unmatched rather than getting the wrong cover.
4. Games with saved metadata or a finished lookup are not asked again; provider errors are retried an hour later.
5. Covers and up to three gameplay screenshots are cached locally, only for games in the library.

Fixing a wrong or missing match belongs in the game's context (a Fix match action in Game Details is planned), never in a separate metadata page.

## Display and window behaviour

- RecompStation respects the compositor and never changes the monitor-wide display mode.
- Window modes: Windowed, Maximized, Fullscreen. Preferred window sizes (16:9, 4:3, ultrawide presets) apply in Windowed mode and are limited to the available desktop area; the compositor decides placement, particularly on Wayland.
- On Wayland the app identifies itself as `io.github.MetalSJ13.RecompStation`, matching its desktop entry, so the taskbar shows its icon.
- Explicit monitor selection and borderless windowed mode are deferred until they can be validated across compositors.

## Implementation principles

- Business logic in C++, presentation and navigation in QML; reusable components where they remove real duplication.
- Keep changes incremental and buildable; UI changes are mocked up on the design canvas and approved before they are built.
- Every new `.qml` or `.js` file is listed in `QML_FILES` in `CMakeLists.txt`.
