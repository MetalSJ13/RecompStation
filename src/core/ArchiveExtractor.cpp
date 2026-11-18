#include "ArchiveExtractor.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStringList>

#include <sys/stat.h>

namespace {
constexpr qint64 MaximumExtractedBytes = 16LL * 1024LL * 1024LL * 1024LL;
constexpr int MaximumArchiveEntries = 100000;

QString archiveError(struct archive *reader, const QString &fallback)
{
    const char *message = archive_error_string(reader);
    return message ? QString::fromLocal8Bit(message) : fallback;
}

bool writeEntryData(struct archive *reader, QSaveFile &output,
                    qint64 declaredSize, qint64 *totalWritten,
                    QString *error)
{
    char buffer[64 * 1024];
    qint64 entryWritten = 0;

    while (true) {
        const la_ssize_t count = archive_read_data(reader, buffer, sizeof(buffer));
        if (count == 0)
            break;
        if (count < 0) {
            *error = QStringLiteral("Could not read an archive entry: %1")
                         .arg(archiveError(reader, QStringLiteral("Unknown archive read error")));
            return false;
        }

        if (*totalWritten > MaximumExtractedBytes - count
            || entryWritten > declaredSize - count) {
            *error = QStringLiteral("The archive expands beyond its permitted size or an entry exceeds its declared size.");
            return false;
        }

        if (output.write(buffer, count) != count) {
            *error = QStringLiteral("Could not write an extracted file: %1")
                         .arg(output.errorString());
            return false;
        }

        entryWritten += count;
        *totalWritten += count;
    }

    if (entryWritten != declaredSize) {
        *error = QStringLiteral("An archive entry did not contain its declared number of bytes.");
        return false;
    }

    return true;
}
}

ArchiveExtractor::Result ArchiveExtractor::extractArchive(
    const QString &archivePath,
    const QString &destinationDirectory,
    const QString &expectedExecutable
)
{
    Result result;

    const QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.isFile() || !archiveInfo.isReadable()) {
        result.error = QStringLiteral("The staged archive does not exist or cannot be read.");
        return result;
    }

    if (!isSafeRelativePath(expectedExecutable)) {
        result.error = QStringLiteral("The expected launcher executable path is invalid.");
        return result;
    }

    const QFileInfo destinationInfo(destinationDirectory);
    if (destinationInfo.exists()) {
        result.error = QStringLiteral("The extraction destination already exists; refusing to overwrite it.");
        return result;
    }

    const QString parentPath = QFileInfo(destinationDirectory).absolutePath();
    if (!QDir().mkpath(parentPath)) {
        result.error = QStringLiteral("Could not create the extraction parent directory.");
        return result;
    }

    const QString canonicalParent = QFileInfo(parentPath).canonicalFilePath();
    if (canonicalParent.isEmpty()) {
        result.error = QStringLiteral("Could not resolve the extraction parent directory.");
        return result;
    }

    const QString requestedDestination =
        QDir(canonicalParent).filePath(QFileInfo(destinationDirectory).fileName());

    if (!QDir().mkdir(requestedDestination)) {
        result.error = QStringLiteral("Could not create a new extraction directory.");
        return result;
    }

    const QString extractionRoot = QFileInfo(requestedDestination).canonicalFilePath();
    if (extractionRoot.isEmpty()
        || extractionRoot != QDir(canonicalParent).filePath(QFileInfo(destinationDirectory).fileName())) {
        QDir(requestedDestination).removeRecursively();
        result.error = QStringLiteral("The extraction destination resolves through an unexpected filesystem path.");
        return result;
    }

    struct archive *reader = archive_read_new();
    if (!reader) {
        QDir(extractionRoot).removeRecursively();
        result.error = QStringLiteral("Could not initialize the archive reader.");
        return result;
    }

    archive_read_support_filter_all(reader);
    archive_read_support_format_tar(reader);
    archive_read_support_format_zip(reader);

    const QByteArray archivePathBytes = QFile::encodeName(archivePath);
    if (archive_read_open_filename(reader, archivePathBytes.constData(), 64 * 1024) != ARCHIVE_OK) {
        result.error = archiveError(reader, QStringLiteral("Could not open the archive."));
        archive_read_free(reader);
        QDir(extractionRoot).removeRecursively();
        return result;
    }

    QSet<QString> seenPaths;
    qint64 totalWritten = 0;
    int entryCount = 0;
    QString exactExecutablePath;
    QStringList executableNameMatches;
    QString failure;

    struct archive_entry *entry = nullptr;
    int status = ARCHIVE_OK;

    while ((status = archive_read_next_header(reader, &entry)) == ARCHIVE_OK) {
        if (++entryCount > MaximumArchiveEntries) {
            failure = QStringLiteral("The archive contains too many entries.");
            break;
        }

        const char *rawPath = archive_entry_pathname(entry);
        if (!rawPath) {
            failure = QStringLiteral("The archive contains an entry without a filename.");
            break;
        }

        const QString relativePath = QString::fromUtf8(rawPath);
        if (!isSafeRelativePath(relativePath)) {
            failure = QStringLiteral("The archive contains an unsafe path: %1").arg(relativePath);
            break;
        }

        const QString normalizedPath = QDir::cleanPath(relativePath);
        // Tar archives commonly prefix entries with "./" and include a
        // trailing slash on directory entries. Both normalize safely.
        if (normalizedPath == QStringLiteral(".")) {
            if (archive_entry_filetype(entry) != AE_IFDIR) {
                failure = QStringLiteral("The archive contains an invalid root entry.");
                break;
            }
            archive_read_data_skip(reader);
            continue;
        }
        const mode_t fileType = archive_entry_filetype(entry);
        if (archive_entry_symlink(entry) || archive_entry_hardlink(entry)
            || (fileType != AE_IFREG && fileType != AE_IFDIR)) {
            failure = QStringLiteral("The archive contains a link or unsupported filesystem entry: %1")
                          .arg(relativePath);
            break;
        }

        if (seenPaths.contains(normalizedPath)) {
            failure = QStringLiteral("The archive contains a duplicate path: %1").arg(relativePath);
            break;
        }
        seenPaths.insert(normalizedPath);

        const QString outputPath = QDir(extractionRoot).filePath(normalizedPath);
        const QString absoluteOutput = QDir::cleanPath(QFileInfo(outputPath).absoluteFilePath());
        if (!absoluteOutput.startsWith(extractionRoot + QDir::separator())) {
            failure = QStringLiteral("An archive path resolves outside the extraction directory.");
            break;
        }

        if (fileType == AE_IFDIR) {
            if (!QDir().mkpath(outputPath)) {
                failure = QStringLiteral("Could not create extracted directory: %1").arg(relativePath);
                break;
            }
            archive_read_data_skip(reader);
            continue;
        }

        const qint64 declaredSize = archive_entry_size(entry);
        if (declaredSize < 0 || declaredSize > MaximumExtractedBytes
            || totalWritten > MaximumExtractedBytes - declaredSize) {
            failure = QStringLiteral("The archive contains a file with an invalid or excessive size.");
            break;
        }

        if (!QDir().mkpath(QFileInfo(outputPath).absolutePath())) {
            failure = QStringLiteral("Could not create a parent directory for: %1").arg(relativePath);
            break;
        }

        QSaveFile output(outputPath);
        if (!output.open(QIODevice::WriteOnly)) {
            failure = QStringLiteral("Could not create extracted file %1: %2")
                          .arg(relativePath, output.errorString());
            break;
        }

        if (!writeEntryData(reader, output, declaredSize, &totalWritten, &failure)) {
            output.cancelWriting();
            break;
        }

        if (!output.commit()) {
            failure = QStringLiteral("Could not finalize extracted file %1: %2")
                          .arg(relativePath, output.errorString());
            break;
        }

        if (normalizedPath == expectedExecutable)
            exactExecutablePath = normalizedPath;
        if (QFileInfo(normalizedPath).fileName() == QFileInfo(expectedExecutable).fileName())
            executableNameMatches.append(normalizedPath);

        // Do not preserve setuid/setgid/sticky bits or world-writable mode.
        QFile::Permissions permissions = QFile::ReadOwner | QFile::WriteOwner
            | QFile::ReadGroup | QFile::ReadOther;
        const mode_t archiveMode = archive_entry_perm(entry);
        if (archiveMode & S_IXUSR)
            permissions |= QFile::ExeOwner;
        if (archiveMode & S_IXGRP)
            permissions |= QFile::ExeGroup;
        if (archiveMode & S_IXOTH)
            permissions |= QFile::ExeOther;
        QFile::setPermissions(outputPath, permissions);
    }

    if (failure.isEmpty() && status != ARCHIVE_EOF)
        failure = archiveError(reader, QStringLiteral("The archive could not be read completely."));

    archive_read_close(reader);
    archive_read_free(reader);

    // Some Linux releases are ZIP wrappers around a TAR.GZ package.
    // Only unwrap this single supported nesting level.
    if (failure.isEmpty() && exactExecutablePath.isEmpty()
        && executableNameMatches.isEmpty()
        && QFileInfo(archivePath).suffix().compare(QStringLiteral("zip"), Qt::CaseInsensitive) == 0) {
        QStringList nestedArchives;
        QDirIterator iterator(extractionRoot, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QFileInfo candidate(iterator.next());
            if (candidate.isSymLink())
                continue;
            const QString name = candidate.fileName().toLower();
            if (name.endsWith(QStringLiteral(".tar.gz"))
                || name.endsWith(QStringLiteral(".tgz")))
                nestedArchives.append(candidate.absoluteFilePath());
        }

        if (nestedArchives.size() == 1) {
            const QString nestedDestination = extractionRoot + QStringLiteral(".nested");
            Result nestedResult = extractArchive(
                nestedArchives.constFirst(), nestedDestination, expectedExecutable);
            if (nestedResult.success) {
                QDir(extractionRoot).removeRecursively();
                return nestedResult;
            }
            failure = QStringLiteral("The ZIP contains a nested TAR.GZ package, but it could not be installed: %1")
                          .arg(nestedResult.error);
        } else if (nestedArchives.size() > 1) {
            failure = QStringLiteral("The ZIP contains multiple nested TAR.GZ packages; refusing to guess which one is the launcher.");
        }
    }

    QString selectedExecutablePath = exactExecutablePath;
    if (failure.isEmpty() && selectedExecutablePath.isEmpty()) {
        if (executableNameMatches.size() == 1) {
            selectedExecutablePath = executableNameMatches.constFirst();
        } else if (executableNameMatches.size() > 1) {
            failure = QStringLiteral("The archive contains multiple files named %1; refusing to guess which one is the launcher.")
                          .arg(QFileInfo(expectedExecutable).fileName());
        } else {
            failure = QStringLiteral("The archive does not contain the expected launcher executable: %1")
                          .arg(expectedExecutable);
        }
    }

    if (failure.isEmpty()) {
        const QString executablePath = QDir(extractionRoot).filePath(selectedExecutablePath);
        QFile::Permissions permissions = QFile::permissions(executablePath);
        permissions |= QFile::ExeOwner;
        if (!QFile::setPermissions(executablePath, permissions))
            failure = QStringLiteral("Could not mark the launcher executable as executable.");
        else {
            result.success = true;
            result.extractedDirectory = extractionRoot;
            result.executablePath = executablePath;
        }
    }

    if (!failure.isEmpty()) {
        QDir(extractionRoot).removeRecursively();
        result.error = failure;
    }

    return result;
}


ArchiveExtractor::Result ArchiveExtractor::extractTarArchive(
    const QString &archivePath,
    const QString &destinationDirectory,
    const QString &expectedExecutable
)
{
    return extractArchive(archivePath, destinationDirectory, expectedExecutable);
}

ArchiveExtractor::Result ArchiveExtractor::stageAppImage(
    const QString &appImagePath,
    const QString &destinationDirectory,
    const QString &expectedExecutable
)
{
    Result result;
    const QFileInfo sourceInfo(appImagePath);
    if (!sourceInfo.isFile() || !sourceInfo.isReadable()
        || sourceInfo.suffix().compare(QStringLiteral("AppImage"), Qt::CaseInsensitive) != 0) {
        result.error = QStringLiteral("The staged AppImage is missing, unreadable, or does not have an .AppImage extension.");
        return result;
    }

    if (!isSafeRelativePath(expectedExecutable)
        || QFileInfo(expectedExecutable).fileName() != expectedExecutable) {
        result.error = QStringLiteral("A standalone AppImage must use a safe filename without directory components.");
        return result;
    }

    const QFileInfo destinationInfo(destinationDirectory);
    if (destinationInfo.exists()) {
        result.error = QStringLiteral("The installation staging destination already exists; refusing to overwrite it.");
        return result;
    }

    const QString parentPath = QFileInfo(destinationDirectory).absolutePath();
    if (!QDir().mkpath(parentPath)) {
        result.error = QStringLiteral("Could not create the AppImage staging parent directory.");
        return result;
    }
    const QString canonicalParent = QFileInfo(parentPath).canonicalFilePath();
    if (canonicalParent.isEmpty()) {
        result.error = QStringLiteral("Could not resolve the AppImage staging parent directory.");
        return result;
    }

    const QString stagingRoot = QDir(canonicalParent).filePath(QFileInfo(destinationDirectory).fileName());
    if (!QDir().mkdir(stagingRoot)) {
        result.error = QStringLiteral("Could not create a new AppImage staging directory.");
        return result;
    }
    const QString canonicalRoot = QFileInfo(stagingRoot).canonicalFilePath();
    if (canonicalRoot.isEmpty() || canonicalRoot != stagingRoot) {
        QDir(stagingRoot).removeRecursively();
        result.error = QStringLiteral("The AppImage staging directory resolves through an unexpected filesystem path.");
        return result;
    }

    const QString executablePath = QDir(canonicalRoot).filePath(expectedExecutable);
    if (!QFile::copy(appImagePath, executablePath)) {
        QDir(canonicalRoot).removeRecursively();
        result.error = QStringLiteral("Could not copy the standalone AppImage into its staging directory.");
        return result;
    }

    QFile::Permissions permissions = QFile::permissions(executablePath);
    permissions |= QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
    if (!QFile::setPermissions(executablePath, permissions)) {
        QDir(canonicalRoot).removeRecursively();
        result.error = QStringLiteral("Could not mark the AppImage as executable.");
        return result;
    }

    result.success = true;
    result.extractedDirectory = canonicalRoot;
    result.executablePath = executablePath;
    return result;
}

bool ArchiveExtractor::isSafeRelativePath(const QString &path)
{
    if (path.isEmpty() || path.startsWith(QLatin1Char('/'))
        || path.contains(QLatin1Char('\\')) || path.contains(QChar::Null))
        return false;

    static const QRegularExpression drivePrefix(QStringLiteral("^[A-Za-z]:"));
    if (drivePrefix.match(path).hasMatch())
        return false;

    const QStringList components = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    for (const QString &component : components) {
        if (component == QStringLiteral(".."))
            return false;
    }

    return true;
}
