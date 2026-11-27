## Downloads (64-bit, x86_64)

| File | What it is |
| --- | --- |
| `RecompStation-VERSION-x86_64.AppImage` | Linux, one file: make it executable and run it. |
| `RecompStation-VERSION-linux-x86_64.zip` | Linux, portable folder: unzip anywhere and run `./RecompStation`. |

Each includes Qt, so nothing else needs installing. A Windows version is on its way: it follows in a later release, once it is signed and has an installer. Each file's SHA-256 is listed in `SHA256SUMS` and shown next to the file on this page.

If the AppImage doesn't start, your system may be missing FUSE 2 (`libfuse2`, or `libfuse2t64` on Ubuntu 24.04); install it, run the AppImage with `--appimage-extract-and-run`, or use the zip.

> [!IMPORTANT]
> RecompStation does not include, host or distribute any game files, ROMs or copyrighted assets. You need your own legally obtained copies of the games.

## Changes

The catalog grows from 42 to 119 projects, now covering Super Nintendo, Game Boy Advance, GameCube and Wii as well as Nintendo 64, PlayStation, Xbox and Xbox 360. [Compatibility.md](https://github.com/MetalSJ13/RecompStation/blob/main/Compatibility.md) lists the 50 games tested so far and the ones waiting for a test or a launcher.

### Playing
- Play hands your game to each launcher the way that launcher expects: started straight away, opened in its own launcher with your game already chosen, or placed where the launcher looks for it. Launchers that find or build the game themselves are left to do so.
- Games with more than one launcher show them side by side, and RecompStation remembers the one you pick (at most two per game).
- Windows-only launchers can run through Wine on Linux. Turn it on in Settings; RecompStation uses the Wine already on your system, and keeps their saves on Wine's C: drive.
- Launchers start in their own folder, which fixes ones that couldn't find their files (BattleTanx).
- Some launchers check your ROM's exact version before playing, so a wrong dump is caught early.

### Installing launchers
- Downloads that contain links (Eternal Darkness) and downloads that are a single program now install.
- Each project shows the builds available for your system, found when RecompStation starts.

### Library
- Faster with large libraries: games are found in the background and the library updates in place instead of reloading.
- The system filter is remembered, and Find game info shows the new cover straight away.
- LaunchBox: Download in setup really downloads now, and the right entry is picked when one game has two with the same name.

### Desktop
- Works on more computers: a software-drawing fallback when graphics drivers are missing, a lighter background on integrated graphics, and a Renderer setting.
- Windowed sizes open centred on X11, and file managers show the AppImage's logo.
- The system menu pauses the sound, is larger, and shows your controller's real buttons, including PlayStation symbols.
- Setup scripts support Debian and Ubuntu, and `uninstall_dependencies.sh` removes only what the installer added.

