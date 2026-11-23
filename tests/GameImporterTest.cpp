#include <QtTest>

#include "core/GameImporter.h"
#include "core/PlatformManager.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <archive.h>
#include <archive_entry.h>

class GameImporterTest : public QObject
{
    Q_OBJECT

private:
    // A file of `size` zero bytes with `bytes` written at `offset`.
    static bool writeImage(const QString &path, qint64 size,
                           const QList<QPair<qint64, QByteArray>> &patches)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || !file.resize(size))
            return false;
        for (const auto &patch : patches) {
            if (!file.seek(patch.first) || file.write(patch.second) != patch.second.size())
                return false;
        }
        return true;
    }

    static bool writeZip(const QString &path, const QString &entryName, const QByteArray &data)
    {
        archive *writer = archive_write_new();
        archive_write_set_format_zip(writer);
        if (archive_write_open_filename(writer, QFile::encodeName(path).constData()) != ARCHIVE_OK) {
            archive_write_free(writer);
            return false;
        }
        archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, entryName.toUtf8().constData());
        archive_entry_set_size(entry, data.size());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(writer, entry);
        archive_write_data(writer, data.constData(), data.size());
        archive_entry_free(entry);
        archive_write_close(writer);
        archive_write_free(writer);
        return true;
    }

private slots:
    void detectsSystemsFromExtensionsAndHeaders()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString root = directory.path();
        bool sure = false;

        QVERIFY(writeImage(root + "/a.z64", 1024, {}));
        QCOMPARE(GameImporter::detectPlatform(root + "/a.z64", &sure), QStringLiteral("N64"));
        QVERIFY(sure);

        QVERIFY(writeImage(root + "/gc.iso", 0x1000, {{0x1C, QByteArray::fromHex("c2339f3d")}}));
        QCOMPARE(GameImporter::detectPlatform(root + "/gc.iso", &sure), QStringLiteral("GameCube"));
        QVERIFY(sure);

        QVERIFY(writeImage(root + "/wii.iso", 0x1000, {{0x18, QByteArray::fromHex("5d1c9ea3")}}));
        QCOMPARE(GameImporter::detectPlatform(root + "/wii.iso", &sure), QStringLiteral("Wii"));

        QVERIFY(writeImage(root + "/wii.rvz", 0x1000,
                           {{0, QByteArray("RVZ\x01", 4)}, {0x58 + 0x18, QByteArray::fromHex("5d1c9ea3")}}));
        QCOMPARE(GameImporter::detectPlatform(root + "/wii.rvz", &sure), QStringLiteral("Wii"));
        QVERIFY(sure);

        QVERIFY(writeImage(root + "/xbox.iso", 0x11000, {{0x10000, QByteArray("MICROSOFT*XBOX*MEDIA")}}));
        QCOMPARE(GameImporter::detectPlatform(root + "/xbox.iso", &sure), QStringLiteral("Xbox"));

        QVERIFY(writeImage(root + "/ps1.iso", 0x9000,
                           {{0x8001, QByteArray("CD001")}, {0x8008, QByteArray("PLAYSTATION")}}));
        QCOMPARE(GameImporter::detectPlatform(root + "/ps1.iso", &sure), QStringLiteral("PS1"));
        QVERIFY(sure);

        QVERIFY(writeImage(root + "/unknown.iso", 0x9000, {}));
        QCOMPARE(GameImporter::detectPlatform(root + "/unknown.iso", &sure), QString());
        QVERIFY(!sure);
    }

    void groupsCueTracksAndUnpacksZippedN64Roms()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString source = directory.path() + "/downloads";
        const QString library = directory.path() + "/library";
        QVERIFY(QDir().mkpath(source + "/nested"));
        QVERIFY(QDir().mkpath(library));

        QVERIFY(writeImage(source + "/Spyro (Track 1).bin", 2048, {}));
        QVERIFY(writeImage(source + "/Spyro (Track 2).bin", 1024, {}));
        QFile cue(source + "/Spyro.cue");
        QVERIFY(cue.open(QIODevice::WriteOnly));
        cue.write("FILE \"Spyro (Track 1).bin\" BINARY\n  TRACK 01 MODE2/2352\n"
                  "FILE \"Spyro (Track 2).bin\" BINARY\n  TRACK 02 AUDIO\n");
        cue.close();
        const QByteArray rom(4096, 'r');
        QVERIFY(writeZip(source + "/nested/Banjo.zip", QStringLiteral("Banjo-Kazooie (USA).z64"), rom));
        QVERIFY(writeImage(source + "/notes.txt", 10, {}));

        const QVariantList found = GameImporter::findCandidates({source}, library);
        QCOMPARE(found.size(), 2);
        QVariantMap spyro, banjo;
        for (const QVariant &entry : found) {
            const QVariantMap item = entry.toMap();
            (item.value("platform").toString() == "PS1" ? spyro : banjo) = item;
        }
        QCOMPARE(spyro.value("files").toStringList().size(), 3);
        QCOMPARE(spyro.value("size").toLongLong(), qint64(3072 + cue.size()));
        QCOMPARE(banjo.value("platform").toString(), QStringLiteral("N64"));
        QCOMPARE(banjo.value("archiveEntry").toString(), QStringLiteral("Banjo-Kazooie (USA).z64"));
        QVERIFY(banjo.value("selected").toBool());

        // Import (copy) both into a library and check nothing is overwritten.
        PlatformManager platforms(directory.path() + "/data");
        QVERIFY(platforms.initialize({"N64", "PS1"}));
        QVERIFY(platforms.setLibraryDirectoryPath(library));
        GameImporter importer(&platforms);
        QSignalSpy finished(&importer, &GameImporter::importFinished);
        importer.setSources({source});
        QTRY_COMPARE(importer.candidates().size(), 2);
        QCOMPARE(importer.selectedCount(), 2);
        importer.importSelected(false);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(0).toInt(), 2);
        QFile unpacked(library + "/N64/Banjo-Kazooie (USA).z64");
        QVERIFY(unpacked.open(QIODevice::ReadOnly));
        QCOMPARE(unpacked.readAll(), rom);
        QVERIFY(QFileInfo::exists(library + "/PS1/Spyro.cue"));
        QVERIFY(QFileInfo::exists(library + "/PS1/Spyro (Track 2).bin"));
        QVERIFY(QFileInfo::exists(source + "/Spyro.cue"));  // copied, not moved

        // A second pass finds them already in the library and selects nothing.
        QCOMPARE(importer.candidates().size(), 0);
        importer.setSources({source});
        QTRY_COMPARE(importer.candidates().size(), 2);
        QCOMPARE(importer.selectedCount(), 0);
    }
};

QTEST_GUILESS_MAIN(GameImporterTest)
#include "GameImporterTest.moc"
