# RecompStation logo

A d-pad whose left arm breaks up into pixels: an old game, rebuilt as a
modern native program. Designed for RecompStation; it uses no console
maker's shapes, symbols or colors.

| File | Use |
| --- | --- |
| `recompstation-mark.svg` | The logo itself: app header, first-time setup, README |
| `recompstation-mark-white.svg` | One-color version for dark or busy backgrounds |
| `recompstation-icon.svg` | App icon (the mark on a dark tile): window, taskbar, desktop |
| `png/recompstation-icon-<size>.png` | The app icon at 16–512 px: window icon; 16–256 px are installed as the desktop icon |
| `png/recompstation-mark-256.png` | The mark as a bitmap, for places that don't take SVG |

Colors: blue `#55a8f5` to violet `#8174e8`, the pixel trail in blue at
fading opacity; the icon tile is `#2d2754` to `#14151f`.

The PNGs are rendered from the SVGs with
`rsvg-convert -w <size> -h <size> recompstation-icon.svg`.

The release files and `cmake --install` install the icon as
`io.github.MetalSJ13.RecompStation` (the 16–256 px PNGs under
`share/icons/hicolor/<size>/apps/` and `recompstation-mark.svg` as the
scalable icon), matching the desktop entry in `packaging/`.
