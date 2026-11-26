#include <QtTest>

#include "core/ArchiveExtractor.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <sys/stat.h>

// Unix execute permission; Windows has none (programs are .exe files).
#ifdef Q_OS_WIN
#define QVERIFY_UNIX_EXECUTABLE(path) do { } while (false)
#else
#define QVERIFY_UNIX_EXECUTABLE(path) QVERIFY(QFileInfo(path).isExecutable())
#endif

class ArchiveExtractorTest : public QObject
{
    Q_OBJECT

private:
    bool createArchive(const QString &path, const QList<QVariantMap> &entries,
                       bool gzip = true, bool zip = false);

private slots:
    void extractsTarGzAndPreservesStructure();
    void extractsZipAndPreservesStructure();
    void extractsZipContainingNestedTarGz();
    void findsExecutableInsideSingleTopLevelDirectory();
    void stagesStandaloneAppImage();
    void rejectsAppImageWithUnsafeFilename();
    void rejectsPathTraversalAndCleansOutput();
    void rejectsAbsolutePaths();
    void rejectsSymbolicLinks();
    void rejectsMissingExpectedExecutable();
    void matchesExecutableNameWithoutCase();
    void refusesExistingDestination();
    void rejectsHardLinks();
    void rejectsSpecialFiles();
    void rejectsDuplicatePaths();
    void rejectsWindowsStylePaths();
    void dropsSetuidAndWorldWritableModes();
    void unwrapsOnlyOneNestingLevel();
    void refusesZipWithSeveralNestedPackages();
    void choosesTheMainWindowsProgram();
    void extractsWindowsZipPickingItsProgram();
};

bool ArchiveExtractorTest::createArchive(
    const QString &path,
    const QList<QVariantMap> &entries,
    bool gzip,
    bool zip
)
{
    struct archive *writer = archive_write_new();
    if (!writer)
        return false;

    if (zip) {
        archive_write_set_format_zip(writer);
    } else {
        archive_write_set_format_pax_restricted(writer);
        if (gzip)
            archive_write_add_filter_gzip(writer);
        else
            archive_write_add_filter_xz(writer);
    }

    const QByteArray encodedPath = QFile::encodeName(path);
    if (archive_write_open_filename(writer, encodedPath.constData()) != ARCHIVE_OK) {
        archive_write_free(writer);
        return false;
    }

    bool success = true;
    for (const QVariantMap &item : entries) {
        struct archive_entry *entry = archive_entry_new();
        const QByteArray name = item.value(QStringLiteral("name")).toString().toUtf8();
        archive_entry_set_pathname(entry, name.constData());

        const QString type = item.value(QStringLiteral("type"), QStringLiteral("file")).toString();
        if (type == QStringLiteral("directory")) {
            archive_entry_set_filetype(entry, AE_IFDIR);
            archive_entry_set_perm(entry, 0755);
            archive_entry_set_size(entry, 0);
        } else if (type == QStringLiteral("hardlink")) {
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, 0644);
            const QByteArray target = item.value(QStringLiteral("target")).toString().toUtf8();
            archive_entry_set_hardlink(entry, target.constData());
            archive_entry_set_size(entry, 0);
        } else if (type == QStringLiteral("fifo")) {
            archive_entry_set_filetype(entry, AE_IFIFO);
            archive_entry_set_perm(entry, 0644);
            archive_entry_set_size(entry, 0);
        } else if (type == QStringLiteral("symlink")) {
            archive_entry_set_filetype(entry, AE_IFLNK);
            archive_entry_set_perm(entry, 0777);
            const QByteArray target = item.value(QStringLiteral("target")).toString().toUtf8();
            archive_entry_set_symlink(entry, target.constData());
            archive_entry_set_size(entry, 0);
        } else {
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, item.value(QStringLiteral("mode"), 0644).toInt());
            const QByteArray data = item.value(QStringLiteral("data")).toByteArray();
            archive_entry_set_size(entry, data.size());
        }

        if (archive_write_header(writer, entry) != ARCHIVE_OK) {
            success = false;
            archive_entry_free(entry);
            break;
        }

        if (type == QStringLiteral("file")) {
            const QByteArray data = item.value(QStringLiteral("data")).toByteArray();
            if (!data.isEmpty()
                && archive_write_data(writer, data.constData(), data.size()) != data.size()) {
                success = false;
                archive_entry_free(entry);
                break;
            }
        }
        archive_entry_free(entry);
    }

    if (archive_write_close(writer) != ARCHIVE_OK)
        success = false;
    archive_write_free(writer);
    return success;
}

void ArchiveExtractorTest::extractsTarGzAndPreservesStructure()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("launcher.tar.gz");
    const QString destination = QDir(temp.path()).filePath("extracted");

    QVERIFY(createArchive(archivePath, {
        {{QStringLiteral("name"), QStringLiteral("./")}, {QStringLiteral("type"), QStringLiteral("directory")}},
        {{QStringLiteral("name"), QStringLiteral("./BanjoRecompiled/")}, {QStringLiteral("type"), QStringLiteral("directory")}},
        {{QStringLiteral("name"), QStringLiteral("./BanjoRecompiled/BanjoRecompiled")},
         {QStringLiteral("data"), QByteArray("#!/bin/sh\necho ok\n")},
         {QStringLiteral("mode"), 0644}},
        {{QStringLiteral("name"), QStringLiteral("./BanjoRecompiled/libsupport.so")},
         {QStringLiteral("data"), QByteArray("library")}}
    }));

    const auto result = ArchiveExtractor::extractTarArchive(
        archivePath, destination, QStringLiteral("BanjoRecompiled/BanjoRecompiled"));
    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(QFileInfo(result.executablePath).isFile());
    QVERIFY(QFileInfo(result.extractedDirectory).isDir());
    QVERIFY(QFileInfo(QDir(result.extractedDirectory).filePath(
        QStringLiteral("BanjoRecompiled/libsupport.so"))).isFile());
#ifndef Q_OS_WIN
    QVERIFY(QFile::permissions(result.executablePath) & QFile::ExeOwner);
#endif
}


void ArchiveExtractorTest::extractsZipAndPreservesStructure()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("launcher.zip");
    const QString destination = QDir(temp.path()).filePath("extracted");

    QVERIFY(createArchive(archivePath, {
        {{QStringLiteral("name"), QStringLiteral("SpaghettiKart/")}, {QStringLiteral("type"), QStringLiteral("directory")}},
        {{QStringLiteral("name"), QStringLiteral("SpaghettiKart/SpaghettiKart.AppImage")},
         {QStringLiteral("data"), QByteArray("appimage-binary")},
         {QStringLiteral("mode"), 0755}},
        {{QStringLiteral("name"), QStringLiteral("SpaghettiKart/resources/data.bin")},
         {QStringLiteral("data"), QByteArray("companion-data")}}
    }, true, true));

    const auto result = ArchiveExtractor::extractArchive(
        archivePath, destination,
        QStringLiteral("SpaghettiKart/SpaghettiKart.AppImage"));
    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY_UNIX_EXECUTABLE(result.executablePath);
    QFile companion(QDir(result.extractedDirectory).filePath(
        QStringLiteral("SpaghettiKart/resources/data.bin")));
    QVERIFY(companion.open(QIODevice::ReadOnly));
    QCOMPARE(companion.readAll(), QByteArray("companion-data"));
}

void ArchiveExtractorTest::extractsZipContainingNestedTarGz()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString innerArchivePath = QDir(temp.path()).filePath("starfox.tar.gz");
    const QString outerArchivePath = QDir(temp.path()).filePath("starfox-release.zip");
    const QString destination = QDir(temp.path()).filePath("installed");

    QVERIFY(createArchive(innerArchivePath, {
        {{QStringLiteral("name"), QStringLiteral("Starfox64Recompiled")},
         {QStringLiteral("data"), QByteArray("#!/bin/sh\\necho starfox\\n")},
         {QStringLiteral("mode"), 0755}},
        {{QStringLiteral("name"), QStringLiteral("assets/")}, {QStringLiteral("type"), QStringLiteral("directory")}},
        {{QStringLiteral("name"), QStringLiteral("assets/launcher.rml")},
         {QStringLiteral("data"), QByteArray("launcher-ui")}},
        {{QStringLiteral("name"), QStringLiteral("recompcontrollerdb.txt")},
         {QStringLiteral("data"), QByteArray("controller-db")}}
    }));

    QFile innerArchive(innerArchivePath);
    QVERIFY(innerArchive.open(QIODevice::ReadOnly));
    const QByteArray innerArchiveBytes = innerArchive.readAll();
    innerArchive.close();

    QVERIFY(createArchive(outerArchivePath, {
        {{QStringLiteral("name"), QStringLiteral("Starfox64Recompiled.tar.gz")},
         {QStringLiteral("data"), innerArchiveBytes}}
    }, true, true));

    const auto result = ArchiveExtractor::extractArchive(
        outerArchivePath, destination, QStringLiteral("Starfox64Recompiled"));
    QVERIFY2(result.success, qPrintable(result.error));
    QCOMPARE(QFileInfo(result.executablePath).fileName(), QStringLiteral("Starfox64Recompiled"));
    QVERIFY_UNIX_EXECUTABLE(result.executablePath);
    QVERIFY(QFileInfo(QDir(result.extractedDirectory).filePath(
        QStringLiteral("assets/launcher.rml"))).isFile());
    QVERIFY(QFileInfo(QDir(result.extractedDirectory).filePath(
        QStringLiteral("recompcontrollerdb.txt"))).isFile());
    QCOMPARE(QFileInfo(result.extractedDirectory).absoluteFilePath(),
             QFileInfo(destination).absoluteFilePath());
}

void ArchiveExtractorTest::findsExecutableInsideSingleTopLevelDirectory()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("conker.tar.gz");
    const QString destination = QDir(temp.path()).filePath("extracted");

    QVERIFY(createArchive(archivePath, {
        {{QStringLiteral("name"), QStringLiteral("ConkerRecompiled/")}, {QStringLiteral("type"), QStringLiteral("directory")}},
        {{QStringLiteral("name"), QStringLiteral("ConkerRecompiled/ConkerRecomp")},
         {QStringLiteral("data"), QByteArray("#!/bin/sh\necho ok\n")},
         {QStringLiteral("mode"), 0644}},
        {{QStringLiteral("name"), QStringLiteral("ConkerRecompiled/assets/thumbnail.png")},
         {QStringLiteral("data"), QByteArray("artwork")}}
    }));

    const auto result = ArchiveExtractor::extractArchive(
        archivePath, destination, QStringLiteral("ConkerRecomp"));
    QVERIFY2(result.success, qPrintable(result.error));
    QCOMPARE(QDir(result.extractedDirectory).relativeFilePath(result.executablePath),
             QStringLiteral("ConkerRecompiled/ConkerRecomp"));
    QVERIFY_UNIX_EXECUTABLE(result.executablePath);
    QVERIFY(QFileInfo(QDir(result.extractedDirectory).filePath(
        QStringLiteral("ConkerRecompiled/assets/thumbnail.png"))).isFile());
}

void ArchiveExtractorTest::stagesStandaloneAppImage()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString sourcePath = QDir(temp.path()).filePath("Standalone.AppImage");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("appimage-content"), qint64(16));
    source.close();

    const QString destination = QDir(temp.path()).filePath("installed");
    const auto result = ArchiveExtractor::stageAppImage(
        sourcePath, destination, QStringLiteral("Standalone.AppImage"));
    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(QFileInfo(result.executablePath).isFile());
    QVERIFY_UNIX_EXECUTABLE(result.executablePath);
    QFile installed(result.executablePath);
    QVERIFY(installed.open(QIODevice::ReadOnly));
    QCOMPARE(installed.readAll(), QByteArray("appimage-content"));
}

void ArchiveExtractorTest::rejectsAppImageWithUnsafeFilename()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString sourcePath = QDir(temp.path()).filePath("Standalone.AppImage");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    source.write("content");
    source.close();

    const auto result = ArchiveExtractor::stageAppImage(
        sourcePath, QDir(temp.path()).filePath("installed"),
        QStringLiteral("../escape.AppImage"));
    QVERIFY(!result.success);
    QVERIFY(!QFileInfo::exists(QDir(temp.path()).filePath("installed")));
}

void ArchiveExtractorTest::rejectsPathTraversalAndCleansOutput()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("malicious.tar.gz");
    const QString destination = QDir(temp.path()).filePath("extracted");
    QVERIFY(createArchive(archivePath, {
        {{QStringLiteral("name"), QStringLiteral("../escaped")},
         {QStringLiteral("data"), QByteArray("bad")}},
        {{QStringLiteral("name"), QStringLiteral("BanjoRecompiled")},
         {QStringLiteral("data"), QByteArray("binary")}}
    }));

    const auto result = ArchiveExtractor::extractTarArchive(
        archivePath, destination, QStringLiteral("BanjoRecompiled"));
    QVERIFY(!result.success);
    QVERIFY(!QFileInfo::exists(QDir(temp.path()).filePath("escaped")));
    QVERIFY(!QFileInfo::exists(destination));
}

void ArchiveExtractorTest::rejectsAbsolutePaths()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("absolute.tar.gz");
    QVERIFY(createArchive(archivePath, {
        {{QStringLiteral("name"), QStringLiteral("/tmp/escaped")},
         {QStringLiteral("data"), QByteArray("bad")}}
    }));

    const auto result = ArchiveExtractor::extractTarArchive(
        archivePath, QDir(temp.path()).filePath("out"), QStringLiteral("launcher"));
    QVERIFY(!result.success);
}

void ArchiveExtractorTest::rejectsSymbolicLinks()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("link.tar.gz");
    QVERIFY(createArchive(archivePath, {
        {{QStringLiteral("name"), QStringLiteral("link")},
         {QStringLiteral("type"), QStringLiteral("symlink")},
         {QStringLiteral("target"), QStringLiteral("../../outside")}}
    }));

    const auto result = ArchiveExtractor::extractTarArchive(
        archivePath, QDir(temp.path()).filePath("out"), QStringLiteral("launcher"));
    QVERIFY(!result.success);
    QVERIFY(!QFileInfo::exists(QDir(temp.path()).filePath("outside")));
}

void ArchiveExtractorTest::rejectsMissingExpectedExecutable()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("missing.tar.xz");
    QVERIFY(createArchive(archivePath, {
        {{QStringLiteral("name"), QStringLiteral("readme.txt")},
         {QStringLiteral("data"), QByteArray("not a launcher")}}
    }, false));

    const QString destination = QDir(temp.path()).filePath("out");
    const auto result = ArchiveExtractor::extractTarArchive(
        archivePath, destination, QStringLiteral("BanjoRecompiled"));
    QVERIFY(!result.success);
    QVERIFY(!QFileInfo::exists(destination));
}

void ArchiveExtractorTest::matchesExecutableNameWithoutCase()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("ghostship.zip");
    QVERIFY(createArchive(archivePath, {
        {{QStringLiteral("name"), QStringLiteral("ghostship.appimage")},
         {QStringLiteral("data"), QByteArray("appimage-binary")},
         {QStringLiteral("mode"), 0755}},
        {{QStringLiteral("name"), QStringLiteral("config.yml")},
         {QStringLiteral("data"), QByteArray("config")}}
    }, true, true));

    const auto result = ArchiveExtractor::extractArchive(
        archivePath, QDir(temp.path()).filePath("out"), QStringLiteral("Ghostship.appimage"));
    QVERIFY2(result.success, qPrintable(result.error));
    QCOMPARE(QFileInfo(result.executablePath).fileName(), QStringLiteral("ghostship.appimage"));

    // Two files that differ only in case: still refuses to guess.
    const QString ambiguousPath = QDir(temp.path()).filePath("ambiguous.zip");
    QVERIFY(createArchive(ambiguousPath, {
        {{QStringLiteral("name"), QStringLiteral("a/ghostship.appimage")},
         {QStringLiteral("data"), QByteArray("one")}},
        {{QStringLiteral("name"), QStringLiteral("b/GHOSTSHIP.appimage")},
         {QStringLiteral("data"), QByteArray("two")}}
    }, true, true));
    const auto ambiguous = ArchiveExtractor::extractArchive(
        ambiguousPath, QDir(temp.path()).filePath("out2"), QStringLiteral("Ghostship.appimage"));
    QVERIFY(!ambiguous.success);
}

void ArchiveExtractorTest::refusesExistingDestination()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("launcher.tar.gz");
    QVERIFY(createArchive(archivePath, {
        {{QStringLiteral("name"), QStringLiteral("launcher")},
         {QStringLiteral("data"), QByteArray("binary")}}
    }));

    const QString destination = QDir(temp.path()).filePath("out");
    QVERIFY(QDir().mkpath(destination));
    QFile marker(QDir(destination).filePath("keep.txt"));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("keep");
    marker.close();

    const auto result = ArchiveExtractor::extractTarArchive(
        archivePath, destination, QStringLiteral("launcher"));
    QVERIFY(!result.success);
    QVERIFY(QFileInfo::exists(marker.fileName()));
}

namespace {
QVariantMap fileEntry(const QString &name, const QByteArray &data = QByteArray("x"), int mode = 0755)
{
    return {{QStringLiteral("name"), name}, {QStringLiteral("data"), data}, {QStringLiteral("mode"), mode}};
}

QByteArray fileBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Nothing is left behind next to the destination after a refusal.
bool onlyTheArchiveRemains(const QString &folder, const QString &archiveName)
{
    const QStringList entries = QDir(folder).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    return entries.size() == 1 && entries.first() == archiveName;
}
}

void ArchiveExtractorTest::rejectsHardLinks()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("hardlink.tar.gz");
    QVERIFY(createArchive(archivePath, {
        fileEntry(QStringLiteral("launcher")),
        {{QStringLiteral("name"), QStringLiteral("copy")}, {QStringLiteral("type"), QStringLiteral("hardlink")},
         {QStringLiteral("target"), QStringLiteral("launcher")}}
    }));
    const auto result = ArchiveExtractor::extractArchive(
        archivePath, QDir(temp.path()).filePath("out"), QStringLiteral("launcher"));
    QVERIFY(!result.success);
    QVERIFY(result.error.contains(QStringLiteral("link")));
    QVERIFY(onlyTheArchiveRemains(temp.path(), QStringLiteral("hardlink.tar.gz")));
}

void ArchiveExtractorTest::rejectsSpecialFiles()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("fifo.tar.gz");
    QVERIFY(createArchive(archivePath, {
        fileEntry(QStringLiteral("launcher")),
        {{QStringLiteral("name"), QStringLiteral("pipe")}, {QStringLiteral("type"), QStringLiteral("fifo")}}
    }));
    const auto result = ArchiveExtractor::extractArchive(
        archivePath, QDir(temp.path()).filePath("out"), QStringLiteral("launcher"));
    QVERIFY(!result.success);
    QVERIFY(onlyTheArchiveRemains(temp.path(), QStringLiteral("fifo.tar.gz")));
}

void ArchiveExtractorTest::rejectsDuplicatePaths()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    // A second entry for the same file could replace the checked one.
    const QString archivePath = QDir(temp.path()).filePath("duplicate.tar.gz");
    QVERIFY(createArchive(archivePath, {
        fileEntry(QStringLiteral("launcher"), QByteArray("first")),
        fileEntry(QStringLiteral("./launcher"), QByteArray("second"))
    }));
    const auto result = ArchiveExtractor::extractArchive(
        archivePath, QDir(temp.path()).filePath("out"), QStringLiteral("launcher"));
    QVERIFY(!result.success);
    QVERIFY(result.error.contains(QStringLiteral("duplicate")));
    QVERIFY(onlyTheArchiveRemains(temp.path(), QStringLiteral("duplicate.tar.gz")));
}

void ArchiveExtractorTest::rejectsWindowsStylePaths()
{
    // ("..\\evil" stays refused even if the reader turns \\ into /.)
    for (const QString &name : {QStringLiteral("..\\evil"), QStringLiteral("C:/evil")}) {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString archivePath = QDir(temp.path()).filePath("windows.zip");
        QVERIFY(createArchive(archivePath, {fileEntry(QStringLiteral("launcher")), fileEntry(name)}, true, true));
        const auto result = ArchiveExtractor::extractArchive(
            archivePath, QDir(temp.path()).filePath("out"), QStringLiteral("launcher"));
        QVERIFY2(!result.success, qPrintable(name));
        QVERIFY2(onlyTheArchiveRemains(temp.path(), QStringLiteral("windows.zip")), qPrintable(name));
    }
}

void ArchiveExtractorTest::dropsSetuidAndWorldWritableModes()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("modes.tar.gz");
    QVERIFY(createArchive(archivePath, {
        fileEntry(QStringLiteral("launcher"), QByteArray("exe"), 06777),
        fileEntry(QStringLiteral("data.bin"), QByteArray("data"), 0666)
    }));
    const auto result = ArchiveExtractor::extractArchive(
        archivePath, QDir(temp.path()).filePath("out"), QStringLiteral("launcher"));
    QVERIFY2(result.success, qPrintable(result.error));
#ifndef Q_OS_WIN // Unix permission bits; Windows files have none.
    for (const QString &name : {QStringLiteral("launcher"), QStringLiteral("data.bin")}) {
        struct stat info {};
        QCOMPARE(::stat(QFile::encodeName(QDir(result.extractedDirectory).filePath(name)).constData(), &info), 0);
        QCOMPARE(int(info.st_mode & (S_ISUID | S_ISGID | S_ISVTX)), 0);
        QCOMPARE(int(info.st_mode & (S_IWGRP | S_IWOTH)), 0);
    }
#endif
    QVERIFY_UNIX_EXECUTABLE(result.executablePath);
}

void ArchiveExtractorTest::unwrapsOnlyOneNestingLevel()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    // launcher.zip > inner.tar.gz > deeper.zip > launcher: the second level
    // stays packed, so the launcher isn't found.
    const QString deeper = QDir(temp.path()).filePath("deeper.zip");
    QVERIFY(createArchive(deeper, {fileEntry(QStringLiteral("launcher"))}, true, true));
    const QString inner = QDir(temp.path()).filePath("inner.tar.gz");
    QVERIFY(createArchive(inner, {fileEntry(QStringLiteral("deeper.zip"), fileBytes(deeper), 0644)}));
    const QString work = QDir(temp.path()).filePath("work");
    QVERIFY(QDir().mkpath(work));
    const QString outer = QDir(work).filePath("launcher.zip");
    QVERIFY(createArchive(outer, {fileEntry(QStringLiteral("inner.tar.gz"), fileBytes(inner), 0644)}, true, true));

    const auto result = ArchiveExtractor::extractArchive(outer, QDir(work).filePath("out"), QStringLiteral("launcher"));
    QVERIFY(!result.success);
    QVERIFY(onlyTheArchiveRemains(work, QStringLiteral("launcher.zip")));
}

void ArchiveExtractorTest::refusesZipWithSeveralNestedPackages()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString package = QDir(temp.path()).filePath("package.tar.gz");
    QVERIFY(createArchive(package, {fileEntry(QStringLiteral("launcher"))}));
    const QString work = QDir(temp.path()).filePath("work");
    QVERIFY(QDir().mkpath(work));
    const QString outer = QDir(work).filePath("launcher.zip");
    QVERIFY(createArchive(outer, {fileEntry(QStringLiteral("a.tar.gz"), fileBytes(package), 0644),
                                  fileEntry(QStringLiteral("b.tar.gz"), fileBytes(package), 0644)}, true, true));
    const auto result = ArchiveExtractor::extractArchive(outer, QDir(work).filePath("out"), QStringLiteral("launcher"));
    QVERIFY(!result.success);
    QVERIFY(result.error.contains(QStringLiteral("multiple")));
    QVERIFY(onlyTheArchiveRemains(work, QStringLiteral("launcher.zip")));
}


void ArchiveExtractorTest::choosesTheMainWindowsProgram()
{
    QString error;
    // One program beside its helpers.
    QCOMPARE(ArchiveExtractor::chooseWindowsProgram({
        QStringLiteral("Zelda64Recompiled.exe"), QStringLiteral("crashpad_handler.exe"),
        QStringLiteral("unins000.exe"), QStringLiteral("assets/readme.txt")}, &error),
        QStringLiteral("Zelda64Recompiled.exe"));
    // The shallowest folder wins; tools deeper inside are left alone.
    QCOMPARE(ArchiveExtractor::chooseWindowsProgram({
        QStringLiteral("Game/Game.exe"), QStringLiteral("Game/tools/texture-pack-tool.exe")}, &error),
        QStringLiteral("Game/Game.exe"));
    QCOMPARE(ArchiveExtractor::chooseWindowsProgram({
        QStringLiteral("Game.exe"), QStringLiteral("vc_redist.x64.exe"), QStringLiteral("Game-debug.exe")}, &error),
        QStringLiteral("Game.exe"));
    // Unclear or nothing to start: refused with a reason.
    QVERIFY(ArchiveExtractor::chooseWindowsProgram({
        QStringLiteral("GameA.exe"), QStringLiteral("GameB.exe")}, &error).isEmpty());
    QVERIFY(error.contains(QStringLiteral("several Windows programs")));
    QVERIFY(ArchiveExtractor::chooseWindowsProgram({QStringLiteral("setup.exe")}, &error).isEmpty());
    QVERIFY(error.contains(QStringLiteral("no Windows program")));
    QVERIFY(ArchiveExtractor::chooseWindowsProgram({QStringLiteral("../evil.exe")}, &error).isEmpty());
}

void ArchiveExtractorTest::extractsWindowsZipPickingItsProgram()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archivePath = QDir(temp.path()).filePath("Game-Windows.zip");
    const QString destination = QDir(temp.path()).filePath("extracted");
    QVERIFY(createArchive(archivePath, {
        {{QStringLiteral("name"), QStringLiteral("Game/")}, {QStringLiteral("type"), QStringLiteral("directory")}},
        {{QStringLiteral("name"), QStringLiteral("Game/Game.exe")}, {QStringLiteral("data"), QByteArray("MZ")}},
        {{QStringLiteral("name"), QStringLiteral("Game/crashpad_handler.exe")}, {QStringLiteral("data"), QByteArray("MZ")}},
        {{QStringLiteral("name"), QStringLiteral("Game/SDL2.dll")}, {QStringLiteral("data"), QByteArray("dll")}}
    }, true, true));
    const auto result = ArchiveExtractor::extractArchive(archivePath, destination, QString());
    QVERIFY2(result.success, qPrintable(result.error));
    QCOMPARE(QDir(result.extractedDirectory).relativeFilePath(result.executablePath), QStringLiteral("Game/Game.exe"));
}

QTEST_GUILESS_MAIN(ArchiveExtractorTest)
#include "ArchiveExtractorTest.moc"
