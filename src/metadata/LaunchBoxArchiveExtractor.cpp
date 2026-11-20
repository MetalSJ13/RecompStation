#include "LaunchBoxArchiveExtractor.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>

namespace {
constexpr qint64 MaximumExtractedBytes = 1024LL * 1024LL * 1024LL;
constexpr qint64 MaximumCatalogFileBytes = 768LL * 1024LL * 1024LL;
constexpr int MaximumArchiveEntries = 100000;
constexpr qint64 BufferSize = 64 * 1024;

QString archiveError(struct archive *reader)
{
    const char *message = archive_error_string(reader);
    return message ? QString::fromLocal8Bit(message)
                   : QStringLiteral("Unknown archive error.");
}

bool safeArchivePath(const QString &path)
{
    if (path.isEmpty() || path.startsWith(QLatin1Char('/'))
        || path.contains(QLatin1Char('\\')) || path.contains(QChar::Null))
        return false;
    const QStringList components = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    for (const QString &component : components) {
        if (component == QStringLiteral(".."))
            return false;
    }
    return true;
}

QString requiredCatalogName(const QString &path)
{
    const QString baseName = QFileInfo(path).fileName();
    if (baseName == QStringLiteral("Platforms.xml")
        || baseName == QStringLiteral("Files.xml")
        || baseName == QStringLiteral("Metadata.xml"))
        return baseName;
    return {};
}
}

LaunchBoxArchiveExtractor::Result LaunchBoxArchiveExtractor::extractMetadataArchive(
    const QString &archivePath,
    const QString &destinationDirectory)
{
    Result result;
    const QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.isFile() || archiveInfo.isSymLink() || !archiveInfo.isReadable()) {
        result.error = QStringLiteral("The LaunchBox archive is missing, unreadable, or a symbolic link.");
        return result;
    }
    if (QFileInfo::exists(destinationDirectory)) {
        result.error = QStringLiteral("The LaunchBox extraction destination already exists.");
        return result;
    }
    const QString parent = QFileInfo(destinationDirectory).absolutePath();
    if (!QDir().mkpath(parent)) {
        result.error = QStringLiteral("Could not create the LaunchBox extraction parent directory.");
        return result;
    }
    const QString canonicalParent = QFileInfo(parent).canonicalFilePath();
    if (canonicalParent.isEmpty()) {
        result.error = QStringLiteral("Could not resolve the LaunchBox extraction parent directory.");
        return result;
    }
    const QString root = QDir(canonicalParent).filePath(QFileInfo(destinationDirectory).fileName());
    if (!QDir().mkdir(root)) {
        result.error = QStringLiteral("Could not create a fresh LaunchBox extraction directory.");
        return result;
    }
    const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
    if (canonicalRoot.isEmpty() || canonicalRoot != root) {
        QDir(root).removeRecursively();
        result.error = QStringLiteral("The extraction directory resolves through an unexpected filesystem path.");
        return result;
    }

    struct archive *reader = archive_read_new();
    if (!reader) {
        QDir(root).removeRecursively();
        result.error = QStringLiteral("Could not initialize the LaunchBox archive reader.");
        return result;
    }
    archive_read_support_filter_all(reader);
    archive_read_support_format_zip(reader);
    archive_read_support_format_tar(reader);

    const QByteArray encodedPath = QFile::encodeName(archivePath);
    if (archive_read_open_filename(reader, encodedPath.constData(), BufferSize) != ARCHIVE_OK) {
        result.error = archiveError(reader);
        archive_read_free(reader);
        QDir(root).removeRecursively();
        return result;
    }

    QSet<QString> seen;
    qint64 totalWritten = 0;
    int entryCount = 0;
    QString failure;
    struct archive_entry *entry = nullptr;
    int status = ARCHIVE_OK;

    while ((status = archive_read_next_header(reader, &entry)) == ARCHIVE_OK) {
        if (++entryCount > MaximumArchiveEntries) {
            failure = QStringLiteral("The LaunchBox archive contains too many entries.");
            break;
        }
        const char *rawPath = archive_entry_pathname(entry);
        if (!rawPath) {
            failure = QStringLiteral("The LaunchBox archive contains an entry without a path.");
            break;
        }
        const QString relativePath = QString::fromUtf8(rawPath);
        if (!safeArchivePath(relativePath)) {
            failure = QStringLiteral("The LaunchBox archive contains an unsafe path.");
            break;
        }
        const mode_t type = archive_entry_filetype(entry);
        if (archive_entry_symlink(entry) || archive_entry_hardlink(entry)
            || (type != AE_IFREG && type != AE_IFDIR)) {
            failure = QStringLiteral("The LaunchBox archive contains a link or unsupported filesystem entry.");
            break;
        }
        if (type == AE_IFDIR) {
            archive_read_data_skip(reader);
            continue;
        }

        const QString catalogName = requiredCatalogName(relativePath);
        if (catalogName.isEmpty()) {
            archive_read_data_skip(reader);
            continue;
        }
        if (seen.contains(catalogName)) {
            failure = QStringLiteral("The LaunchBox archive contains more than one %1 file.")
                          .arg(catalogName);
            break;
        }
        const qint64 declaredSize = archive_entry_size(entry);
        if (declaredSize < 0 || declaredSize > MaximumCatalogFileBytes
            || totalWritten > MaximumExtractedBytes - declaredSize) {
            failure = QStringLiteral("A LaunchBox catalog file exceeds the permitted size.");
            break;
        }

        QSaveFile output(QDir(root).filePath(catalogName));
        if (!output.open(QIODevice::WriteOnly)) {
            failure = QStringLiteral("Could not create %1: %2")
                          .arg(catalogName, output.errorString());
            break;
        }
        char buffer[BufferSize];
        qint64 entryWritten = 0;
        while (true) {
            const la_ssize_t count = archive_read_data(reader, buffer, sizeof(buffer));
            if (count == 0)
                break;
            if (count < 0) {
                failure = archiveError(reader);
                break;
            }
            if (entryWritten > declaredSize - count
                || totalWritten > MaximumExtractedBytes - count) {
                failure = QStringLiteral("A LaunchBox catalog entry exceeded its declared size.");
                break;
            }
            if (output.write(buffer, count) != count) {
                failure = output.errorString();
                break;
            }
            entryWritten += count;
            totalWritten += count;
        }
        if (!failure.isEmpty()) {
            output.cancelWriting();
            break;
        }
        if (entryWritten != declaredSize) {
            output.cancelWriting();
            failure = QStringLiteral("A LaunchBox catalog file was truncated.");
            break;
        }
        if (!output.commit()) {
            failure = output.errorString();
            break;
        }
        seen.insert(catalogName);
    }

    if (failure.isEmpty() && status != ARCHIVE_EOF)
        failure = archiveError(reader);
    archive_read_close(reader);
    archive_read_free(reader);

    const QStringList required{
        QStringLiteral("Platforms.xml"),
        QStringLiteral("Files.xml"),
        QStringLiteral("Metadata.xml")
    };
    for (const QString &name : required) {
        if (failure.isEmpty() && !seen.contains(name))
            failure = QStringLiteral("The LaunchBox archive is missing %1.").arg(name);
    }
    if (!failure.isEmpty()) {
        QDir(root).removeRecursively();
        result.error = failure;
        return result;
    }

    result.success = true;
    result.extractedDirectory = root;
    result.files.platformsXmlPath = QDir(root).filePath(QStringLiteral("Platforms.xml"));
    result.files.filesXmlPath = QDir(root).filePath(QStringLiteral("Files.xml"));
    result.files.metadataXmlPath = QDir(root).filePath(QStringLiteral("Metadata.xml"));
    return result;
}
