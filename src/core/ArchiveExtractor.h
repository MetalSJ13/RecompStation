#pragma once

#include <QString>

class ArchiveExtractor
{
public:
    struct Result
    {
        bool success = false;
        QString extractedDirectory;
        QString executablePath;
        QString error;
    };

    // Extracts a gzip- or xz-compressed tar archive into a new directory.
    // Only regular files and directories are accepted; links and special
    // filesystem entries are rejected.
    static Result extractTarArchive(
        const QString &archivePath,
        const QString &destinationDirectory,
        const QString &expectedExecutable
    );

private:
    static bool isSafeRelativePath(const QString &path);
};
