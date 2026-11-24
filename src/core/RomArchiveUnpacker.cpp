#include "RomArchiveUnpacker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <archive.h>
#include <archive_entry.h>

namespace {

constexpr int MaximumEntries = 200;

struct ArchiveReader {
    archive *handle = archive_read_new();
    ArchiveReader()
    {
        archive_read_support_filter_all(handle);
        archive_read_support_format_zip(handle);
        archive_read_support_format_7zip(handle);
    }
    ~ArchiveReader() { archive_read_free(handle); }
    bool open(const QString &path)
    {
        return archive_read_open_filename(handle, QFile::encodeName(path).constData(),
                                          64 * 1024) == ARCHIVE_OK;
    }
};

// The plain file name of an entry, or empty when it is not a usable name.
QString entryFileName(archive_entry *entry)
{
    const QString name = QFileInfo(QString::fromUtf8(archive_entry_pathname(entry))).fileName();
    if (name.isEmpty() || name.startsWith(QLatin1Char('.')))
        return {};
    return name;
}

} // namespace

bool RomArchiveUnpacker::isArchive(const QString &path)
{
    const QString extension = QFileInfo(path).suffix().toLower();
    return extension == QStringLiteral("zip") || extension == QStringLiteral("7z");
}

RomArchiveUnpacker::Outcome RomArchiveUnpacker::unpack(const QString &archivePath,
                                                       const QStringList &romExtensions,
                                                       QString *unpackedPath, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return Outcome::Failed;
    };

    // First pass: find the ROM and count the archive's other files.
    QString romName;
    qint64 romSize = -1;
    int romCount = 0;
    int otherFiles = 0;
    {
        ArchiveReader reader;
        if (!reader.open(archivePath))
            return fail(QStringLiteral("could not open the archive"));
        archive_entry *entry = nullptr;
        int scanned = 0;
        int status = ARCHIVE_OK;
        while ((status = archive_read_next_header(reader.handle, &entry)) == ARCHIVE_OK) {
            if (++scanned > MaximumEntries)
                return fail(QStringLiteral("the archive has too many entries"));
            if (archive_entry_filetype(entry) == AE_IFDIR) {
                archive_read_data_skip(reader.handle);
                continue;
            }
            const QString name = entryFileName(entry);
            const bool regular = archive_entry_filetype(entry) == AE_IFREG;
            if (regular && !name.isEmpty()
                && romExtensions.contains(QFileInfo(name).suffix().toLower())) {
                ++romCount;
                romName = name;
                romSize = archive_entry_size_is_set(entry) ? archive_entry_size(entry) : -1;
            } else {
                ++otherFiles;
            }
            archive_read_data_skip(reader.handle);
        }
        if (status != ARCHIVE_EOF)
            return fail(QStringLiteral("could not read the archive"));
    }
    if (romCount == 0)
        return Outcome::NoRom;
    if (romCount > 1)
        return Outcome::SeveralRoms;
    if (romSize > MaximumRomSize)
        return fail(QStringLiteral("the ROM inside is larger than any cartridge"));

    const QString destination = QFileInfo(archivePath).absoluteDir().filePath(romName);
    if (unpackedPath)
        *unpackedPath = destination;
    if (QFileInfo::exists(destination))
        return Outcome::AlreadyUnpacked;

    // Second pass: write the ROM to a temporary name, check it, then rename.
    const QString partial = destination + QStringLiteral(".part");
    qint64 written = 0;
    bool ok = false;
    {
        ArchiveReader reader;
        if (!reader.open(archivePath))
            return fail(QStringLiteral("could not open the archive"));
        archive_entry *entry = nullptr;
        while (archive_read_next_header(reader.handle, &entry) == ARCHIVE_OK) {
            if (archive_entry_filetype(entry) != AE_IFREG || entryFileName(entry) != romName) {
                archive_read_data_skip(reader.handle);
                continue;
            }
            QFile out(partial);
            if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
                return fail(QStringLiteral("could not write next to the archive"));
            char buffer[256 * 1024];
            ok = true;
            for (;;) {
                const la_ssize_t count = archive_read_data(reader.handle, buffer, sizeof(buffer));
                if (count == 0)
                    break;
                if (count < 0 || out.write(buffer, count) != count) {
                    ok = false;
                    break;
                }
                written += count;
                if (written > MaximumRomSize) {
                    ok = false;
                    break;
                }
            }
            ok = out.flush() && ok;
            out.close();
            break;
        }
    }
    if (ok && romSize >= 0 && written != romSize)
        ok = false;
    if (!ok || !QFile::rename(partial, destination)) {
        QFile::remove(partial);
        return fail(QStringLiteral("could not unpack the ROM"));
    }

    // The archive held only this ROM, now safely on disk: remove it so the
    // library does not keep two copies.
    if (otherFiles == 0) {
        if (!QFile::remove(archivePath)) {
            if (error)
                *error = QStringLiteral("the ROM was unpacked but the archive could not be removed");
            return Outcome::UnpackedKept;
        }
        return Outcome::Unpacked;
    }
    return Outcome::UnpackedKept;
}
