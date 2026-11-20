#include <QtTest>

#include "metadata/LaunchBoxArchiveExtractor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <atomic>

#include <archive.h>
#include <archive_entry.h>

class LaunchBoxArchiveExtractorTest : public QObject
{
    Q_OBJECT

private:
    static bool createZip(const QString &path,
                          const QList<QPair<QByteArray, QByteArray>> &entries)
    {
        struct archive *writer = archive_write_new();
        if (!writer)
            return false;
        archive_write_set_format_zip(writer);
        const QByteArray encodedPath = QFile::encodeName(path);
        if (archive_write_open_filename(writer, encodedPath.constData()) != ARCHIVE_OK) {
            archive_write_free(writer);
            return false;
        }
        bool success = true;
        for (const auto &item : entries) {
            struct archive_entry *entry = archive_entry_new();
            if (!entry) {
                success = false;
                break;
            }
            archive_entry_set_pathname(entry, item.first.constData());
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, 0644);
            archive_entry_set_size(entry, item.second.size());
            if (archive_write_header(writer, entry) != ARCHIVE_OK
                || archive_write_data(writer, item.second.constData(), item.second.size())
                    != item.second.size())
                success = false;
            archive_entry_free(entry);
            if (!success)
                break;
        }
        if (archive_write_close(writer) != ARCHIVE_OK)
            success = false;
        if (archive_write_free(writer) != ARCHIVE_OK)
            success = false;
        return success;
    }

    static QList<QPair<QByteArray, QByteArray>> validEntries()
    {
        return {
            {QByteArray("Metadata/Platforms.xml"), QByteArray("<LaunchBox/>")},
            {QByteArray("Metadata/Files.xml"), QByteArray("<LaunchBox/>")},
            {QByteArray("Metadata/Metadata.xml"), QByteArray("<LaunchBox/>")},
            {QByteArray("Metadata/readme.txt"), QByteArray("ignored")}
        };
    }

private slots:
    void extractsOnlyRequiredCatalogFiles();
    void rejectsArchiveMissingRequiredCatalog();
    void rejectsUnsafeArchivePaths();
    void rejectsDuplicateCatalogFiles();
    void preservesExistingDestination();
    void cancellationRemovesPartialExtraction();
};

void LaunchBoxArchiveExtractorTest::extractsOnlyRequiredCatalogFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString archivePath = directory.filePath("Metadata.zip");
    QVERIFY(createZip(archivePath, validEntries()));

    const QString destination = directory.filePath("extracted");
    const auto result =
        LaunchBoxArchiveExtractor::extractMetadataArchive(archivePath, destination);
    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(QFileInfo(result.files.platformsXmlPath).isFile());
    QVERIFY(QFileInfo(result.files.filesXmlPath).isFile());
    QVERIFY(QFileInfo(result.files.metadataXmlPath).isFile());
    QVERIFY(!QFileInfo::exists(QDir(destination).filePath("readme.txt")));
    QFile metadata(result.files.metadataXmlPath);
    QVERIFY(metadata.open(QIODevice::ReadOnly));
    QCOMPARE(metadata.readAll(), QByteArray("<LaunchBox/>"));
}

void LaunchBoxArchiveExtractorTest::rejectsArchiveMissingRequiredCatalog()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto entries = validEntries();
    entries.removeAt(1);
    const QString archivePath = directory.filePath("Metadata.zip");
    QVERIFY(createZip(archivePath, entries));

    const QString destination = directory.filePath("extracted");
    const auto result =
        LaunchBoxArchiveExtractor::extractMetadataArchive(archivePath, destination);
    QVERIFY(!result.success);
    QVERIFY(result.error.contains(QStringLiteral("Files.xml")));
    QVERIFY(!QFileInfo::exists(destination));
}

void LaunchBoxArchiveExtractorTest::rejectsUnsafeArchivePaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto entries = validEntries();
    entries.append({QByteArray("../escape.txt"), QByteArray("unsafe")});
    const QString archivePath = directory.filePath("Metadata.zip");
    QVERIFY(createZip(archivePath, entries));

    const QString destination = directory.filePath("extracted");
    const auto result =
        LaunchBoxArchiveExtractor::extractMetadataArchive(archivePath, destination);
    QVERIFY(!result.success);
    QVERIFY(!QFileInfo::exists(directory.filePath("escape.txt")));
    QVERIFY(!QFileInfo::exists(destination));
}

void LaunchBoxArchiveExtractorTest::rejectsDuplicateCatalogFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto entries = validEntries();
    entries.append({QByteArray("Another/Metadata.xml"), QByteArray("<LaunchBox/>")});
    const QString archivePath = directory.filePath("Metadata.zip");
    QVERIFY(createZip(archivePath, entries));

    const QString destination = directory.filePath("extracted");
    const auto result =
        LaunchBoxArchiveExtractor::extractMetadataArchive(archivePath, destination);
    QVERIFY(!result.success);
    QVERIFY(result.error.contains(QStringLiteral("more than one")));
    QVERIFY(!QFileInfo::exists(destination));
}

void LaunchBoxArchiveExtractorTest::preservesExistingDestination()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString archivePath = directory.filePath("Metadata.zip");
    QVERIFY(createZip(archivePath, validEntries()));

    const QString destination = directory.filePath("extracted");
    QVERIFY(QDir().mkpath(destination));
    QFile sentinel(QDir(destination).filePath("keep.txt"));
    QVERIFY(sentinel.open(QIODevice::WriteOnly));
    QCOMPARE(sentinel.write("keep"), qint64(4));
    sentinel.close();

    const auto result =
        LaunchBoxArchiveExtractor::extractMetadataArchive(archivePath, destination);
    QVERIFY(!result.success);
    QVERIFY(QFileInfo::exists(sentinel.fileName()));
}

void LaunchBoxArchiveExtractorTest::cancellationRemovesPartialExtraction()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString archivePath = directory.filePath("Metadata.zip");
    QVERIFY(createZip(archivePath, validEntries()));

    std::atomic_bool cancelled{true};
    const QString destination = directory.filePath("cancelled-extraction");
    const auto result = LaunchBoxArchiveExtractor::extractMetadataArchive(
        archivePath, destination, &cancelled);
    QVERIFY(!result.success);
    QVERIFY(result.error.contains(QStringLiteral("cancelled")));
    QVERIFY(!QFileInfo::exists(destination));
}

QTEST_GUILESS_MAIN(LaunchBoxArchiveExtractorTest)
#include "LaunchBoxArchiveExtractorTest.moc"
