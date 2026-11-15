#include <QtTest>

#include "core/ArchiveExtractor.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

class ArchiveExtractorTest : public QObject
{
    Q_OBJECT

private:
    bool createArchive(const QString &path, const QList<QVariantMap> &entries,
                       bool gzip = true);

private slots:
    void extractsTarGzAndPreservesStructure();
    void rejectsPathTraversalAndCleansOutput();
    void rejectsAbsolutePaths();
    void rejectsSymbolicLinks();
    void rejectsMissingExpectedExecutable();
    void refusesExistingDestination();
};

bool ArchiveExtractorTest::createArchive(
    const QString &path,
    const QList<QVariantMap> &entries,
    bool gzip
)
{
    struct archive *writer = archive_write_new();
    if (!writer)
        return false;

    archive_write_set_format_pax_restricted(writer);
    if (gzip)
        archive_write_add_filter_gzip(writer);
    else
        archive_write_add_filter_xz(writer);

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
    QVERIFY(QFile::permissions(result.executablePath) & QFile::ExeOwner);
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

QTEST_GUILESS_MAIN(ArchiveExtractorTest)
#include "ArchiveExtractorTest.moc"
