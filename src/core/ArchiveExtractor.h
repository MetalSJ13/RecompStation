#pragma once

#include <QString>
#include <QStringList>

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

    // The same for any single-file program (a portable .exe, a Linux
    // binary): copied under the catalog's name for it, which keeps the
    // download's extension (".exe" stays ".exe", none stays none).
    static Result stageExecutable(
        const QString &programPath,
        const QString &destinationDirectory,
        const QString &expectedExecutable
    );

    static Result stageAppImage(
        const QString &appImagePath,
        const QString &destinationDirectory,
        const QString &expectedExecutable,
        bool appImage
    );

    // Kept as a compatibility wrapper for existing callers and tests.
    static Result extractTarArchive(
        const QString &archivePath,
        const QString &destinationDirectory,
        const QString &expectedExecutable
    );

    // A Windows download with no program named in the catalog: the one
    // main .exe among relativePaths, leaving out helpers (uninstallers,
    // setup programs, crash reporters, redistributables) and preferring the
    // shallowest folder. Empty, with error set, when it isn't clear.
    static QString chooseWindowsProgram(const QStringList &relativePaths, QString *error = nullptr);

private:
    static bool isSafeRelativePath(const QString &path);
};
