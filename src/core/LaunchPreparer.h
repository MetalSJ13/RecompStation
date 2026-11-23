#pragma once

#include "RomInspector.h"

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// Hands the player's own game file to a launcher before it starts, so the
// launcher opens straight to the game instead of asking for the file.
//
// The project's catalog "launch" recipe says how each launcher finds its
// game data (see data/projects.json):
//   n64recomp       Copies the ROM, checked against the launcher's own XXH3
//                   checksum and converted to big-endian, to
//                   ~/.config/<dataFolder>/<romFileName>: where N64Recomp
//                   launchers keep the ROM after the first start.
//   harbourmasters  Points SHIP_HOME at ~/.local/share/<dataFolder> (outside
//                   the managed install folder, so saves survive updates and
//                   uninstalls) and places the ROM there, where the port
//                   looks for ROMs to extract.
//   manual          The launcher asks for the file itself.
//
// Existing files in a launcher's folder are never replaced, and the game
// file in the library is only read.
class LaunchPreparer
{
public:
    struct Options
    {
        bool prepareFiles = true;
        bool fullscreen = true;
        QString windowMode = QStringLiteral("default"); // default, fullscreen, windowed
        bool defaultFullscreenApplied = false; // "Start in fullscreen" already set once
        QString homeDirectory;  // empty: the user's home
        QString dataHome;       // empty: $XDG_DATA_HOME or ~/.local/share
    };

    struct Plan
    {
        bool ok = false;
        QString error;
        QHash<QString, QString> environment;
        QVariantList steps;     // {title, detail, warning} shown while the game starts
        bool defaultFullscreenApplied = false; // remember: don't set it again
    };

    enum class WindowModeResult { Unchanged, Changed, DefaultApplied, Pending };

    // What the line under Play says. Keys: state, tone (ok, warn, info),
    // title, detail, method, dataFolder, savesFolder, gameFile.
    QVariantMap readiness(const QVariantMap &project,
                          const QString &gamePath,
                          const QString &installDirectory,
                          const Options &options);

    Plan prepare(const QVariantMap &project,
                 const QString &gamePath,
                 const QString &installDirectory,
                 const Options &options);

    static QString dataFolder(const QVariantMap &recipe,
                              const QString &installDirectory,
                              const Options &options);
    static QString savesFolder(const QVariantMap &recipe,
                               const QString &installDirectory,
                               const Options &options);

    // Changes only "wm_option" in a launcher's own graphics file: once for
    // "Start games in fullscreen", or whenever the player chose a window mode
    // for this game. Never creates the file (Pending: the launcher writes it
    // on its first start), and leaves unreadable files alone.
    static WindowModeResult applyWindowMode(const QString &graphicsPath,
                                            const Options &options,
                                            QString *summary = nullptr);

private:
    N64RomInfo inspect(const QString &path);

    struct CacheEntry
    {
        qint64 size = -1;
        QDateTime modified;
        N64RomInfo info;
    };
    QHash<QString, CacheEntry> m_cache;
};
