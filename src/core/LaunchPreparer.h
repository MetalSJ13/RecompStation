#pragma once

#include "RomInspector.h"

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// Hands the player's own game file to a launcher before it starts, so the
// launcher never asks for the file: some open straight to the game, others
// (N64Recomp launchers) open their own menu with the game file found.
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
//   romfolder       Like n64recomp, but the launcher's folder is
//                   ~/.local/share/<dataFolder>, outside the install folder:
//                   the checked ROM goes to <dataFolder>/<romFileName>,
//                   "environment" values point the launcher there and
//                   "arguments" pass the placed ROM ({dataFolder} and
//                   {romPath} are filled in). "dataFiles" in the install
//                   folder are copied over the first time, so saves made
//                   there are kept. Pokémon Snap, Snowboard Kids.
//   discargument    Passes the game file itself in "arguments" ({gamePath},
//                   e.g. --disc <file>) when its extension is one of
//                   "fileExtensions"; nothing is copied. Luigi's Mansion.
//   fileargument    The same for cartridge ROMs. Super Metroid and Mega Man
//                   X get --launcher --rom <file>: their launcher opens with
//                   the ROM selected, so settings and mods stay reachable.
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
        // Which system's folder rules apply: empty means this one ("linux"
        // or "windows"; tests set it).
        QString system;
        QString localAppData;   // Windows: empty means %LOCALAPPDATA%
    };

    struct Plan
    {
        bool ok = false;
        QString error;
        QHash<QString, QString> environment;
        QStringList arguments;  // passed to the launcher
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

    // With background reads on, readiness() never reads a game file it
    // hasn't seen: it answers "checking" and lists the file in
    // takePendingReads(), for the caller to inspect off the UI thread and hand
    // back with storeInspection(). Off (the default, used by tests), it reads
    // the file right away.
    void setBackgroundReads(bool enabled) { m_backgroundReads = enabled; }
    QStringList takePendingReads();
    void storeInspection(const QString &path, const N64RomInfo &info);

private:
    N64RomInfo inspect(const QString &path);
    bool isInspected(const QString &path) const;
    bool m_backgroundReads = false;
    QStringList m_pendingReads;

    struct CacheEntry
    {
        qint64 size = -1;
        QDateTime modified;
        N64RomInfo info;
    };
    QHash<QString, CacheEntry> m_cache;
};
