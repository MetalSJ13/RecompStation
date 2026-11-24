#pragma once

#include <QString>
#include <QStringList>

// Unpacks a cartridge ROM that was placed in a library folder inside a .zip
// or .7z archive, so launchers (which read the ROM file itself) can use it.
//
// The ROM is written next to the archive under its own file name. The
// archive is removed only when the ROM was its only file and the written
// size matches the archive's record; archives with other files (a readme,
// several ROMs) are kept. Existing files are never replaced.
class RomArchiveUnpacker
{
public:
    enum class Outcome {
        Unpacked,          // ROM written, archive removed
        UnpackedKept,      // ROM written, archive kept (it holds other files)
        AlreadyUnpacked,   // a file with the ROM's name is already there
        NoRom,             // no ROM for this system inside
        SeveralRoms,       // more than one ROM: left for Add games to sort out
        Failed             // unreadable archive, write error, size mismatch
    };

    // Largest ROM accepted (N64 ROMs top out at 64 MiB; this leaves room).
    static constexpr qint64 MaximumRomSize = 128LL * 1024 * 1024;

    static bool isArchive(const QString &path);

    // `romExtensions` are lower-case extensions without the dot, e.g. the
    // library scanner's list for the system whose folder holds the archive.
    // `unpackedPath` receives the ROM's path for Unpacked, UnpackedKept and
    // AlreadyUnpacked.
    static Outcome unpack(const QString &archivePath, const QStringList &romExtensions,
                          QString *unpackedPath = nullptr, QString *error = nullptr);
};
