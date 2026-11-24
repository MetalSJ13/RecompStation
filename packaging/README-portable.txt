RecompStation — portable Linux build (x86_64)
=============================================

Start it with the "RecompStation" file in this folder, from a file manager or
a terminal:

    ./RecompStation

Qt and the other libraries RecompStation needs are included, so nothing has to
be installed. The folder can live anywhere and be moved; keep its contents
together.

Your settings, library database, covers and installed launchers are kept in
~/.local/share/RecompStation/, not in this folder, so you can replace this
folder with a newer version without losing anything.

To add RecompStation to your application menu, copy
share/applications/io.github.MetalSJ13.RecompStation.desktop to
~/.local/share/applications/ and change its Exec= line to the full path of the
RecompStation file in this folder.

RecompStation does not include, download or distribute any game files. You
need your own legally obtained copies of the games.

Project page: https://github.com/MetalSJ13/RecompStation
