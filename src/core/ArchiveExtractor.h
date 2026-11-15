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

    // Extracts supported tar or ZIP archives into a new directory. Only
    // regular files and directories are accepted; links and special
    // filesystem entries are rejected.
    static Result extractArchive(
        const QString &archivePath,
        const QString &destinationDirectory,
        const QString &expectedExecutable
    );

    // Copies a standalone AppImage into a new managed staging directory.
    static Result stageAppImage(
        const QString &appImagePath,
        const QString &destinationDirectory,
        const QString &expectedExecutable
    );

    // Kept as a compatibility wrapper for existing callers and tests.
    static Result extractTarArchive(
        const QString &archivePath,
        const QString &destinationDirectory,
        const QString &expectedExecutable
    );

private:
    static bool isSafeRelativePath(const QString &path);
};
