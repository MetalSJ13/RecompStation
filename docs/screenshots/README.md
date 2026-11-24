# Screenshots

Images shown in the main README, captured in the app at 3440×1440 and scaled
to 1920 px wide. Metadata (screen name, position, DPI) is removed before
committing; the files hold image data only.

| File | What it shows |
| --- | --- |
| `game-details.png` | Game Details of an installed game (Diddy Kong Racing) with its recompilation project, version and launch settings |
| `library-shelves.png` | Library, Shelves view, All systems, spotlight on the selected game, wave background |
| `library-grid.png` | Library, Grid view, sections per system, custom wallpaper |
| `library-list.png` | Library, List view with the preview pane, wave background |
| `library-carousel.png` | Library, Carousel view, Aurora theme |

To add or replace one, strip metadata first, for example:
`convert input.png -strip -resize 1920x output.png` (ImageMagick).

Covers and screenshots of games belong to their publishers and are shown only
to illustrate the app.
