#include <QtTest>

#include "metadata/N64RomHash.h"

class N64RomHashTest : public QObject
{
    Q_OBJECT

private slots:
    void calculatesHashesForBigEndianRom()
    {
        const QByteArray rom = QByteArray::fromHex("8037124001020304");
        N64RomHashes hashes;
        QString error;

        QVERIFY2(N64RomHash::calculate(rom, &hashes, &error), qPrintable(error));
        QCOMPARE(hashes.md5, QStringLiteral("fe3613018f8377b35378dbf62d641b57"));
        QCOMPARE(hashes.crc32, QStringLiteral("da481e67"));
    }

    void normalizesByteSwappedRomToSameHashes()
    {
        const QByteArray rom = QByteArray::fromHex("8037124001020304");
        const QByteArray byteSwapped = QByteArray::fromHex("3780401202010403");
        N64RomHashes expected;
        N64RomHashes actual;
        QString error;

        QVERIFY2(N64RomHash::calculate(rom, &expected, &error), qPrintable(error));
        QVERIFY2(N64RomHash::calculate(byteSwapped, &actual, &error), qPrintable(error));
        QCOMPARE(actual.md5, expected.md5);
        QCOMPARE(actual.crc32, expected.crc32);
    }

    void normalizesLittleEndianRomToSameHashes()
    {
        const QByteArray rom = QByteArray::fromHex("8037124001020304");
        const QByteArray littleEndian = QByteArray::fromHex("4012378004030201");
        N64RomHashes expected;
        N64RomHashes actual;
        QString error;

        QVERIFY2(N64RomHash::calculate(rom, &expected, &error), qPrintable(error));
        QVERIFY2(N64RomHash::calculate(littleEndian, &actual, &error), qPrintable(error));
        QCOMPARE(actual.md5, expected.md5);
        QCOMPARE(actual.crc32, expected.crc32);
    }

    void rejectsUnknownByteOrder()
    {
        QByteArray canonical;
        QString error;
        QVERIFY(!N64RomHash::toBigEndian(QByteArray::fromHex("00000000"),
                                         &canonical, &error));
        QVERIFY(error.contains(QStringLiteral("signature")));
    }

    void rejectsInvalidRomSizes()
    {
        QByteArray canonical;
        QString error;
        QVERIFY(!N64RomHash::toBigEndian(QByteArray::fromHex("803712"),
                                         &canonical, &error));
        QVERIFY(error.contains(QStringLiteral("size")));
    }

    void rejectsMissingOutput()
    {
        QString error;
        QVERIFY(!N64RomHash::calculate(QByteArray::fromHex("80371240"),
                                       nullptr, &error));
        QVERIFY(error.contains(QStringLiteral("destination")));
    }
};

QTEST_GUILESS_MAIN(N64RomHashTest)
#include "N64RomHashTest.moc"
