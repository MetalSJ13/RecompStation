#include <QtTest>

#include "core/LibraryScanner.h"
#include "core/PlatformManager.h"
#include "database/DatabaseManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QSettings>
#include <QUrl>

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
        QCoreApplication::setOrganizationName(QStringLiteral("RecompStationLibraryScannerTests"));
        QCoreApplication::setApplicationName(QStringLiteral("LibraryScannerTests"));
        QSettings().clear();

        m_tempDirectory = std::make_unique<QTemporaryDir>();
        QVERIFY(m_tempDirectory->isValid());

        m_platformManager =
            std::make_unique<PlatformManager>(m_tempDirectory->path());
        QVERIFY(m_platformManager->initialize());
        const QString libraryRoot = m_tempDirectory->path() + "/library";
        QVERIFY(QDir().mkpath(libraryRoot));
        QVERIFY(m_platformManager->setLibraryDirectory(QUrl::fromLocalFile(libraryRoot)));

        m_databaseManager = std::make_unique<DatabaseManager>();
        const QString databasePath =
            m_tempDirectory->path() + "/database/test.sqlite";
        QVERIFY2(m_databaseManager->initialize(databasePath),
                 qPrintable(m_databaseManager->lastError()));

        m_scanner = std::make_unique<LibraryScanner>(
            m_platformManager.get(), m_databaseManager.get());
        // The production debounce (750 ms) only slows the test down; the
        // behaviour under test is the same with a short one.
        m_scanner->setWatchDebounceInterval(50);
    }

    void cleanup()
    {
        m_scanner.reset();
        m_databaseManager.reset();
        m_platformManager.reset();
        m_tempDirectory.reset();
    }

    void asynchronousScanReportsLifecycleAndUnsupportedFiles()
    {
        QVERIFY(writeGameFile("N64", "Mario.z64"));
        QVERIFY(writeGameFile("N64", "notes.txt"));
        QVERIFY(writeGameFile("PS1", "FinalFantasy.chd"));

        QSignalSpy startedSpy(m_scanner.get(), &LibraryScanner::scanStarted);
        QSignalSpy platformStartedSpy(m_scanner.get(), &LibraryScanner::platformScanStarted);
        QSignalSpy platformCompletedSpy(m_scanner.get(), &LibraryScanner::platformScanCompleted);
        QSignalSpy completedSpy(m_scanner.get(), &LibraryScanner::scanCompleted);
        QVERIFY(startedSpy.isValid());
        QVERIFY(platformStartedSpy.isValid());
        QVERIFY(platformCompletedSpy.isValid());
        QVERIFY(completedSpy.isValid());

        QVERIFY(m_scanner->scanAllAsync());
        QVERIFY(m_scanner->scanning());
        QVERIFY(!m_scanner->scanAllAsync());
        QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 5000);

        QCOMPARE(startedSpy.count(), 1);
        QCOMPARE(platformStartedSpy.count(), 6);
        QCOMPARE(platformCompletedSpy.count(), 6);
        QCOMPARE(completedSpy.takeFirst().at(0).toInt(), 2);
        QCOMPARE(m_scanner->lastUnsupportedCount(), 1);
        QVERIFY(!m_scanner->scanning());
        QCOMPARE(m_databaseManager->games().size(), 2);
    }

    void scansExistingGamesImmediatelyWhenLibraryFolderIsSelected()
    {
        auto dataDirectory = std::make_unique<QTemporaryDir>();
        QVERIFY(dataDirectory->isValid());

        PlatformManager platformManager(dataDirectory->path());
        QVERIFY(platformManager.initialize());

        const QString libraryRoot = dataDirectory->path() + "/existing-library";
        const QString n64Directory = libraryRoot + "/N64";
        QVERIFY(QDir().mkpath(n64Directory));

        QFile gameFile(n64Directory + "/Banjo-Kazooie (USA).z64");
        QVERIFY(gameFile.open(QIODevice::WriteOnly));
        gameFile.write("test-rom");
        gameFile.close();

        DatabaseManager databaseManager;
        QVERIFY2(databaseManager.initialize(
                     dataDirectory->path() + "/database/first-run.sqlite"),
                 qPrintable(databaseManager.lastError()));

        LibraryScanner scanner(&platformManager, &databaseManager);
        scanner.setWatchDebounceInterval(50);
        QSignalSpy scanSpy(&scanner, &LibraryScanner::scanCompleted);
        QVERIFY(scanSpy.isValid());
        QVERIFY(!platformManager.libraryConfigured());

        QVERIFY(platformManager.setLibraryDirectoryPath(libraryRoot));

        QTRY_COMPARE_WITH_TIMEOUT(scanSpy.count(), 1, 5000);
        QCOMPARE(scanSpy.takeFirst().at(0).toInt(), 1);

        const QVariantList games = databaseManager.games("N64");
        QCOMPARE(games.size(), 1);
        const QVariantMap game = games.first().toMap();
        QCOMPARE(game.value("title").toString(), QString("Banjo-Kazooie (USA)"));
        QCOMPARE(game.value("path").toString(), QFileInfo(gameFile).absoluteFilePath());
    }

    void discoversSupportedExtensionsForEveryPlatform()
    {
        const QList<QPair<QString, QStringList>> filesByPlatform = {
            {"N64", {"Mario.z64", "Zelda.N64", "Pilotwings.v64"}},
            {"PS1", {"Game.cue", "Game.chd", "Game.pbp", "Game.iso",
                     "Standalone/Standalone.bin", "Standalone/Standalone.img"}},
            {"GameCube", {"Mario.iso", "Luigi.gcm", "Metroid.rvz", "Zelda.ciso"}},
            {"Wii", {"Galaxy.iso", "Kart.wbfs", "Zelda.rvz"}},
            {"Wii U", {"Kart.wua", "Sports.wud", "Adventure.wux"}},
            {"GB", {"Tetris.gb"}},
            {"GBC", {"Oracle.gbc", "Pinball.gb"}},
            {"GBA", {"Minish.gba"}},
            {"SNES", {"Metroid.sfc", "Aladdin.smc"}},
            {"PS2", {"Ratchet.iso", "Jak.chd"}},
            {"Xbox", {"Halo.iso", "Fable.xiso"}}
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

    void removesGamesThatAreNoLongerInTheScannedFolder()
    {
        QVERIFY(writeGameFile("N64", "Mario.z64"));
        QCOMPARE(m_scanner->scanPlatform("N64"), 1);
        QCOMPARE(m_databaseManager->games("N64").size(), 1);

        const QString gamePath =
            m_platformManager->platformDirectory("N64") + "/Mario.z64";
        QVERIFY(QFile::remove(gamePath));

        QCOMPARE(m_scanner->scanPlatform("N64"), 0);
        QVERIFY(m_databaseManager->games("N64").isEmpty());

        // A game that returns to the folder should become visible again.
        QVERIFY(writeGameFile("N64", "Mario.z64"));
        QCOMPARE(m_scanner->scanPlatform("N64"), 1);
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

    void automaticallyDiscoversFilesInNewNestedDirectories()
    {
        QSignalSpy spy(m_scanner.get(), &LibraryScanner::scanCompleted);
        QVERIFY(spy.isValid());
        QVERIFY(m_scanner->monitoringActive());

        QVERIFY(writeGameFile("N64", "NewFolder/FirstGame.z64"));
        QTRY_COMPARE_WITH_TIMEOUT(m_databaseManager->games("N64").size(), 1, 5000);

        QVERIFY(writeGameFile("N64", "NewFolder/Deeper/SecondGame.n64"));
        QTRY_COMPARE_WITH_TIMEOUT(m_databaseManager->games("N64").size(), 2, 5000);
        QVERIFY(spy.count() >= 2);
    }

    void automaticallyMarksRemovedFilesUnavailable()
    {
        QVERIFY(writeGameFile("N64", "RemoveMe.z64"));
        QCOMPARE(m_scanner->scanPlatform("N64"), 1);
        QCOMPARE(m_databaseManager->games("N64").size(), 1);

        const QString gamePath =
            m_platformManager->platformDirectory("N64") + "/RemoveMe.z64";
        QVERIFY(QFile::remove(gamePath));

        QTRY_VERIFY_WITH_TIMEOUT(m_databaseManager->games("N64").isEmpty(), 5000);
        // The database keeps the record, but unavailable games are omitted from the library view.
        QVERIFY(m_databaseManager->games().isEmpty());
    }

    void automaticMonitoringCanBeStoppedAndRestarted()
    {
        QVERIFY(m_scanner->monitoringActive());
        m_scanner->stopMonitoring();
        QVERIFY(!m_scanner->monitoringActive());

        QVERIFY(writeGameFile("N64", "WhileStopped.z64"));
        // Several debounce intervals: a running watcher would have rescanned.
        QTest::qWait(300);
        QVERIFY(m_databaseManager->games("N64").isEmpty());

        QVERIFY(m_scanner->startMonitoring());
        QVERIFY(writeGameFile("N64", "AfterRestart.z64"));
        QTRY_COMPARE_WITH_TIMEOUT(m_databaseManager->games("N64").size(), 2, 5000);
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
