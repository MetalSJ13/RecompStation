#include <QtTest>

#include "core/GameImporter.h"
#include "core/PlatformManager.h"

#include <QDir>
#include <QFile>
#include <QMap>
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

        // Trimmed Xbox 360 image: the root directory (sector 0x22) lists default.xex.
        QVERIFY(writeImage(root + "/x360.iso", 0x11800,
                           {{0x10000, QByteArray("MICROSOFT*XBOX*MEDIA")},
                            {0x10014, QByteArray::fromHex("22000000")},
                            {0x10018, QByteArray::fromHex("00080000")},
                            {0x11000 + 14, QByteArray("default.xex")}}));
        QCOMPARE(GameImporter::detectPlatform(root + "/x360.iso", &sure), QStringLiteral("Xbox 360"));
        QVERIFY(sure);

        // Full (redump) images: the game partition's offset tells the console.
        QVERIFY(writeImage(root + "/x360-xgd3.iso", 0x2090000 + 0x800,
                           {{0x2090000, QByteArray("MICROSOFT*XBOX*MEDIA")}}));
        QCOMPARE(GameImporter::detectPlatform(root + "/x360-xgd3.iso", &sure), QStringLiteral("Xbox 360"));

        QVERIFY(writeImage(root + "/ps1.iso", 0x9000,
                           {{0x8001, QByteArray("CD001")}, {0x8008, QByteArray("PLAYSTATION")}}));
        QCOMPARE(GameImporter::detectPlatform(root + "/ps1.iso", &sure), QStringLiteral("PS1"));
        QVERIFY(sure);

        // PS2 DVD: same "PLAYSTATION" volume, but SYSTEM.CNF says BOOT2.
        // Root directory at sector 20 lists SYSTEM.CNF at sector 21.
        {
            QByteArray record(46, '\0');
            record[0] = char(46);
            record[2] = char(21);           // extent LBA (LE)
            record[10] = char(0x40);        // size
            record[32] = char(12);          // name length
            record.replace(33, 12, "SYSTEM.CNF;1");
            QByteArray rootRecord(34, '\0');
            rootRecord[2] = char(20);
            rootRecord[11] = char(0x08);    // 2048 bytes
            QVERIFY(writeImage(root + "/ps2.iso", 22 * 2048,
                               {{0x8001, QByteArray("CD001")}, {0x8008, QByteArray("PLAYSTATION")},
                                {0x8000 + 156, rootRecord}, {20 * 2048, record},
                                {21 * 2048, QByteArray("BOOT2 = cdrom0:\\SCUS_971.99;1\r\nVER = 1.00\r\n")}}));
            QCOMPARE(GameImporter::detectPlatform(root + "/ps2.iso", &sure), QStringLiteral("PS2"));
            QVERIFY(sure);

            // The same layout with a PS1 SYSTEM.CNF stays PS1.
            QVERIFY(writeImage(root + "/ps1-cnf.iso", 22 * 2048,
                               {{0x8001, QByteArray("CD001")}, {0x8008, QByteArray("PLAYSTATION")},
                                {0x8000 + 156, rootRecord}, {20 * 2048, record},
                                {21 * 2048, QByteArray("BOOT = cdrom:\\SCUS_944.26;1\r\n")}}));
            QCOMPARE(GameImporter::detectPlatform(root + "/ps1-cnf.iso", &sure), QStringLiteral("PS1"));
        }

        // CHD: a DVD metadata entry means PS2, a CD track entry PS1.
        {
            QByteArray header(124, '\0');
            header.replace(0, 8, "MComprHD");
            header[15] = char(5);                       // version 5 (BE)
            header[48 + 7] = char(124);                 // metadata offset (BE u64)
            QVERIFY(writeImage(root + "/ps2.chd", 256, {{0, header}, {124, QByteArray("DVD ")}}));
            QCOMPARE(GameImporter::detectPlatform(root + "/ps2.chd", &sure), QStringLiteral("PS2"));
            QVERIFY(sure);
            QVERIFY(writeImage(root + "/ps1.chd", 256, {{0, header}, {124, QByteArray("CHT2")}}));
            QCOMPARE(GameImporter::detectPlatform(root + "/ps1.chd", &sure), QStringLiteral("PS1"));
            QVERIFY(sure);
        }

        // Game Boy: the CGB flag decides GB or GBC; a dual-mode ROM follows
        // its extension.
        const QByteArray logo = QByteArray::fromHex("ceed6666");
        QVERIFY(writeImage(root + "/mono.gb", 0x8000, {{0x104, logo}}));
        QCOMPARE(GameImporter::detectPlatform(root + "/mono.gb", &sure), QStringLiteral("GB"));
        QVERIFY(sure);
        QVERIFY(writeImage(root + "/colour-only.gb", 0x8000, {{0x104, logo}, {0x143, QByteArray(1, char(0xC0))}}));
        QCOMPARE(GameImporter::detectPlatform(root + "/colour-only.gb", &sure), QStringLiteral("GBC"));
        QVERIFY(writeImage(root + "/dual.gbc", 0x8000, {{0x104, logo}, {0x143, QByteArray(1, char(0x80))}}));
        QCOMPARE(GameImporter::detectPlatform(root + "/dual.gbc", &sure), QStringLiteral("GBC"));
        QVERIFY(writeImage(root + "/dual.gb", 0x8000, {{0x104, logo}, {0x143, QByteArray(1, char(0x80))}}));
        QCOMPARE(GameImporter::detectPlatform(root + "/dual.gb", &sure), QStringLiteral("GB"));
        QVERIFY(writeImage(root + "/nologo.gb", 0x8000, {}));
        QCOMPARE(GameImporter::detectPlatform(root + "/nologo.gb", &sure), QStringLiteral("GB"));
        QVERIFY(!sure);

        QVERIFY(writeImage(root + "/a.gba", 0x1000, {}));
        QCOMPARE(GameImporter::detectPlatform(root + "/a.gba", &sure), QStringLiteral("GBA"));
        QVERIFY(sure);
        QVERIFY(writeImage(root + "/a.sfc", 0x1000, {}));
        QCOMPARE(GameImporter::detectPlatform(root + "/a.sfc", &sure), QStringLiteral("SNES"));
        QVERIFY(writeImage(root + "/a.wua", 0x1000, {}));
        QCOMPARE(GameImporter::detectPlatform(root + "/a.wua", &sure), QStringLiteral("Wii U"));
        QVERIFY(writeImage(root + "/a.wud", 0x1000, {}));
        QCOMPARE(GameImporter::detectPlatform(root + "/a.wud", &sure), QStringLiteral("Wii U"));

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

    void detectsCartridgeSystemsInsideArchives()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString source = directory.path() + "/downloads";
        QVERIFY(QDir().mkpath(source));

        // A .gb ROM whose header marks it Game Boy Color only.
        QByteArray colour(0x8000, '\0');
        colour.replace(0x104, 4, QByteArray::fromHex("ceed6666"));
        colour[0x143] = char(0xC0);
        QVERIFY(writeZip(source + "/Oracle.zip", QStringLiteral("Oracle of Seasons (USA).gb"), colour));
        QVERIFY(writeZip(source + "/Metroid.zip", QStringLiteral("Super Metroid (USA).sfc"),
                         QByteArray(0x8000, 'm')));
        QVERIFY(writeZip(source + "/Minish.zip", QStringLiteral("Minish Cap (USA).gba"),
                         QByteArray(0x8000, 'g')));

        const QVariantList found = GameImporter::findCandidates({source}, QString());
        QCOMPARE(found.size(), 3);
        QMap<QString, QString> platformByEntry;
        for (const QVariant &entry : found) {
            const QVariantMap item = entry.toMap();
            platformByEntry.insert(item.value("archiveEntry").toString(), item.value("platform").toString());
            QVERIFY(item.value("selected").toBool());
        }
        QCOMPARE(platformByEntry.value("Oracle of Seasons (USA).gb"), QStringLiteral("GBC"));
        QCOMPARE(platformByEntry.value("Super Metroid (USA).sfc"), QStringLiteral("SNES"));
        QCOMPARE(platformByEntry.value("Minish Cap (USA).gba"), QStringLiteral("GBA"));
    }
};

QTEST_GUILESS_MAIN(GameImporterTest)
#include "GameImporterTest.moc"
