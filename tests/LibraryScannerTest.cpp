#include <QtTest>

#include "core/LibraryScanner.h"
#include "core/PlatformManager.h"
#include "database/DatabaseManager.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

class LibraryScannerTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_tempDirectory;
    std::unique_ptr<PlatformManager> m_platformManager;
    std::unique_ptr<DatabaseManager> m_databaseManager;
    std::unique_ptr<LibraryScanner> m_scanner;

    bool writeGameFile(const QString &platform, const QString &relativePath)
    {
        const QString fullPath =
            m_platformManager->platformDirectory(platform) + "/" + relativePath;

        if (!QDir().mkpath(QFileInfo(fullPath).absolutePath()))
            return false;

        QFile file(fullPath);
        if (!file.open(QIODevice::WriteOnly))
            return false;

        file.write("test");
        file.close();
        return true;
    }

private slots:
    void init()
    {
        m_tempDirectory = std::make_unique<QTemporaryDir>();
        QVERIFY(m_tempDirectory->isValid());

        m_platformManager =
            std::make_unique<PlatformManager>(m_tempDirectory->path());
        QVERIFY(m_platformManager->initialize());

        m_databaseManager = std::make_unique<DatabaseManager>();
        const QString databasePath =
            m_tempDirectory->path() + "/database/test.sqlite";
        QVERIFY2(m_databaseManager->initialize(databasePath),
                 qPrintable(m_databaseManager->lastError()));

        m_scanner = std::make_unique<LibraryScanner>(
            m_platformManager.get(), m_databaseManager.get());
    }

    void cleanup()
    {
        m_scanner.reset();
        m_databaseManager.reset();
        m_platformManager.reset();
        m_tempDirectory.reset();
    }

    void discoversSupportedExtensionsForEveryPlatform()
    {
        const QList<QPair<QString, QStringList>> filesByPlatform = {
            {"N64", {"Mario.z64", "Zelda.N64", "Pilotwings.v64"}},
            {"PS1", {"Game.cue", "Game.chd", "Game.pbp", "Game.iso",
                     "Standalone.bin", "Standalone.img"}},
            {"GameCube", {"Mario.iso", "Luigi.gcm", "Metroid.rvz", "Zelda.ciso"}},
            {"Wii", {"Galaxy.iso", "Kart.wbfs", "Zelda.rvz", "Sports.wud",
                     "Adventure.wux"}}
        };

        int expectedTotal = 0;
        for (const auto &entry : filesByPlatform) {
            for (const QString &fileName : entry.second)
                QVERIFY(writeGameFile(entry.first, fileName));
            expectedTotal += entry.second.size();
        }

        QCOMPARE(m_scanner->scanAll(), expectedTotal);
        QCOMPARE(m_databaseManager->games().size(), expectedTotal);
        QVERIFY(m_scanner->lastError().isEmpty());
    }

    void scansRecursivelyAndIgnoresUnsupportedFiles()
    {
        QVERIFY(writeGameFile("N64", "Mario/Mario.Z64"));
        QVERIFY(writeGameFile("N64", "Zelda.n64"));
        QVERIFY(writeGameFile("N64", "notes.txt"));

        QCOMPARE(m_scanner->scanPlatform("N64"), 2);
        QCOMPARE(m_databaseManager->games("N64").size(), 2);
    }

    void repeatedScansDoNotDuplicateGames()
    {
        QVERIFY(writeGameFile("N64", "Mario.z64"));

        QCOMPARE(m_scanner->scanPlatform("N64"), 1);
        QCOMPARE(m_scanner->scanPlatform("N64"), 0);
        QCOMPARE(m_databaseManager->games("N64").size(), 1);
    }

    void skipsBinAndImgWhenCueSheetExistsRegardlessOfCueCase()
    {
        QVERIFY(writeGameFile("PS1", "Disc.Cue"));
        QVERIFY(writeGameFile("PS1", "Disc.bin"));
        QVERIFY(writeGameFile("PS1", "Cover.img"));

        QCOMPARE(m_scanner->scanPlatform("PS1"), 1);
        const QVariantList games = m_databaseManager->games("PS1");
        QCOMPARE(games.size(), 1);
        QCOMPARE(games.first().toMap().value("title").toString(), QString("Disc"));
    }

    void scanAllEmitsOneCompletionSignal()
    {
        QVERIFY(writeGameFile("N64", "Mario.z64"));
        QVERIFY(writeGameFile("PS1", "FinalFantasy.chd"));
        QVERIFY(writeGameFile("GameCube", "Metroid.rvz"));
        QVERIFY(writeGameFile("Wii", "Galaxy.wbfs"));

        QSignalSpy spy(m_scanner.get(), &LibraryScanner::scanCompleted);
        QVERIFY(spy.isValid());

        QCOMPARE(m_scanner->scanAll(), 4);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), 4);
    }

    void unknownPlatformReportsErrorAndCompletion()
    {
        QSignalSpy spy(m_scanner.get(), &LibraryScanner::scanCompleted);
        QVERIFY(spy.isValid());

        QCOMPARE(m_scanner->scanPlatform("Dreamcast"), 0);
        QCOMPARE(spy.count(), 1);
        QVERIFY(m_scanner->lastError().contains("Unknown platform"));
    }
};

QTEST_GUILESS_MAIN(LibraryScannerTest)
#include "LibraryScannerTest.moc"
