# Third-party notices

RecompStation is free software under the GNU General Public License v3.0 or
later (see `LICENSE`). The release files (AppImage and portable zip) also
contain the libraries below, unmodified, so that RecompStation runs without
installing anything. Thank you to everyone who makes them.

## Qt

RecompStation is built with the Qt framework, used under the GNU Lesser
General Public License v3 (see `licenses/LGPL-3.0.txt`; the LGPL builds on the
GPL v3 in `LICENSE`).

- Copyright (C) The Qt Company Ltd. and other contributors.
- Modules included: Qt Core, Gui, Network, Sql (SQLite driver), Concurrent,
  DBus, OpenGL, Qml, Quick, Quick Controls, Quick Shapes, Svg and Wayland
  Client, with their plugins.
- Qt is linked dynamically and is not modified. The libraries are in
  `usr/lib` (`libQt6*.so*`); you can replace them with your own build of the
  same Qt version.
- Source code: https://download.qt.io/official_releases/qt/ (the version
  is listed at the end of this file), or https://code.qt.io/.
- Qt itself contains third-party code (for example PCRE2, HarfBuzz, libpng,
  double-conversion and md4c), listed with their licenses at
  https://doc.qt.io/qt-6/licenses-used-in-qt.html.
- Qt's prebuilt libraries use ICU (`libicu*.so*`), under the Unicode License:
  https://github.com/unicode-org/icu/blob/main/LICENSE.

## Other libraries

| Library | License | Project |
| --- | --- | --- |
| SDL2 | zlib License | https://www.libsdl.org/ |
| libarchive | BSD 2-Clause License | https://www.libarchive.org/ |
| xxHash (compiled into RecompStation) | BSD 2-Clause License | https://github.com/Cyan4973/xxHash |

libarchive, SDL2 and the libraries they and Qt use (compression, XML,
keyboard and X11 helpers) come from Ubuntu 22.04. Each one's copyright and
license file is included under `usr/share/doc/<package>/copyright`.

### Windows version

The Windows zip contains the same Qt modules (as `Qt6*.dll` beside
`RecompStation.exe`, replaceable with your own build of the same Qt version),
SDL2 and libarchive, plus the compression and encryption libraries libarchive
uses (bzip2, LZ4, XZ/liblzma, zlib, Zstandard and OpenSSL), all built
unmodified with vcpkg. Each one's copyright and license file is included in
`licenses\<library>.txt`. It also contains the Microsoft Visual C++ runtime
(`vcruntime140*.dll`, `msvcp140*.dll`, `concrt140.dll`), redistributed under
the Visual Studio license terms.

Each recompilation project installed through RecompStation is a separate
program under its own license; RecompStation does not include any of them.
