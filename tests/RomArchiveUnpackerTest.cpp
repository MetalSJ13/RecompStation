#include <QtTest>

#include "core/RomArchiveUnpacker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <archive.h>
#include <archive_entry.h>

class RomArchiveUnpackerTest : public QObject
{
    Q_OBJECT

private:
    using Entry = QPair<QString, QByteArray>;

    static bool writeZip(const QString &path, const QList<Entry> &entries)
    {
        archive *writer = archive_write_new();
        archive_write_set_format_zip(writer);
        if (archive_write_open_filename(writer, QFile::encodeName(path).constData()) != ARCHIVE_OK) {
            archive_write_free(writer);
            return false;
        }
        for (const Entry &item : entries) {
            archive_entry *entry = archive_entry_new();
            archive_entry_set_pathname(entry, item.first.toUtf8().constData());
            archive_entry_set_size(entry, item.second.size());
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, 0644);
            archive_write_header(writer, entry);
            archive_write_data(writer, item.second.constData(), item.second.size());
            archive_entry_free(entry);
        }
        archive_write_close(writer);
        archive_write_free(writer);
        return true;
    }

    static QByteArray readAll(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    const QStringList n64{QStringLiteral("z64"), QStringLiteral("n64"), QStringLiteral("v64")};

private slots:
    void unpacksTheOnlyRomAndRemovesTheArchive()
    {
        QTemporaryDir directory;
        const QByteArray rom(64 * 1024, 'r');
        const QString archive = directory.filePath("Banjo.zip");
        // A folder inside the archive is ignored: the ROM lands next to it.
        QVERIFY(writeZip(archive, {{QStringLiteral("roms/Banjo-Kazooie (USA).z64"), rom}}));

        QString unpacked;
        QCOMPARE(RomArchiveUnpacker::unpack(archive, n64, &unpacked),
                 RomArchiveUnpacker::Outcome::Unpacked);
        QCOMPARE(unpacked, directory.filePath("Banjo-Kazooie (USA).z64"));
        QCOMPARE(readAll(unpacked), rom);
        QVERIFY(!QFileInfo::exists(archive));
        QVERIFY(!QFileInfo::exists(unpacked + ".part"));
    }

    void keepsArchivesWithOtherFiles()
    {
        QTemporaryDir directory;
        const QString archive = directory.filePath("Metroid.zip");
        QVERIFY(writeZip(archive, {{QStringLiteral("Super Metroid (USA).sfc"), QByteArray(4096, 's')},
                                   {QStringLiteral("readme.txt"), QByteArray("hello")}}));
        QCOMPARE(RomArchiveUnpacker::unpack(archive, {QStringLiteral("sfc"), QStringLiteral("smc")}),
                 RomArchiveUnpacker::Outcome::UnpackedKept);
        QVERIFY(QFileInfo::exists(directory.filePath("Super Metroid (USA).sfc")));
        QVERIFY(QFileInfo::exists(archive));

        // The next scan sees the ROM already there and leaves both alone.
        QCOMPARE(RomArchiveUnpacker::unpack(archive, {QStringLiteral("sfc")}),
                 RomArchiveUnpacker::Outcome::AlreadyUnpacked);
        QVERIFY(QFileInfo::exists(archive));
    }

    void neverReplacesAnExistingFile()
    {
        QTemporaryDir directory;
        const QString existing = directory.filePath("Mario.z64");
        {
            QFile file(existing);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("mine");
        }
        const QString archive = directory.filePath("Mario.zip");
        QVERIFY(writeZip(archive, {{QStringLiteral("Mario.z64"), QByteArray(1024, 'm')}}));
        QCOMPARE(RomArchiveUnpacker::unpack(archive, n64),
                 RomArchiveUnpacker::Outcome::AlreadyUnpacked);
        QCOMPARE(readAll(existing), QByteArray("mine"));
        QVERIFY(QFileInfo::exists(archive));
    }

    void leavesArchivesWithoutOneRom()
    {
        QTemporaryDir directory;
        const QString none = directory.filePath("notes.zip");
        QVERIFY(writeZip(none, {{QStringLiteral("notes.txt"), QByteArray("x")}}));
        QCOMPARE(RomArchiveUnpacker::unpack(none, n64), RomArchiveUnpacker::Outcome::NoRom);
        QVERIFY(QFileInfo::exists(none));

        // A Game Boy ROM in the N64 folder is not this system's ROM.
        const QString wrongSystem = directory.filePath("Tetris.zip");
        QVERIFY(writeZip(wrongSystem, {{QStringLiteral("Tetris.gb"), QByteArray(1024, 't')}}));
        QCOMPARE(RomArchiveUnpacker::unpack(wrongSystem, n64), RomArchiveUnpacker::Outcome::NoRom);

        const QString several = directory.filePath("pack.zip");
        QVERIFY(writeZip(several, {{QStringLiteral("A.z64"), QByteArray(16, 'a')},
                                   {QStringLiteral("B.z64"), QByteArray(16, 'b')}}));
        QCOMPARE(RomArchiveUnpacker::unpack(several, n64), RomArchiveUnpacker::Outcome::SeveralRoms);
        QVERIFY(QFileInfo::exists(several));
        QVERIFY(!QFileInfo::exists(directory.filePath("A.z64")));
    }

    void reportsUnreadableArchives()
    {
        QTemporaryDir directory;
        const QString broken = directory.filePath("broken.zip");
        {
            QFile file(broken);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("not an archive");
        }
        QString error;
        QCOMPARE(RomArchiveUnpacker::unpack(broken, n64, nullptr, &error),
                 RomArchiveUnpacker::Outcome::Failed);
        QVERIFY(!error.isEmpty());
        QVERIFY(QFileInfo::exists(broken));
    }
};

QTEST_MAIN(RomArchiveUnpackerTest)
#include "RomArchiveUnpackerTest.moc"
