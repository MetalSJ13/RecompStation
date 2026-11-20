#include <QtTest>

#include "database/DatabaseManager.h"

#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

class DatabaseManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void associatesMetadataWithSpecificLocalGame()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        const QString databasePath =
            temporaryDirectory.path() + "/database/library.sqlite";
        QVERIFY2(database.initialize(databasePath),
                 qPrintable(database.lastError()));

        QVERIFY(database.registerGame(
            QStringLiteral("Banjo-Kazooie"),
            QStringLiteral("N64"),
            temporaryDirectory.path() + "/games/N64/Banjo-Kazooie.z64"));

        const QVariantList before = database.games(QStringLiteral("N64"));
        QCOMPARE(before.size(), 1);
        const int gameId = before.first().toMap().value("id").toInt();
        QVERIFY(gameId > 0);
        QVERIFY(before.first().toMap().value("metadataTitle").toString().isEmpty());

        QVERIFY2(database.associateMetadata(
                     gameId,
                     QStringLiteral("thegamesdb"),
                     1234,
                     QStringLiteral("Banjo-Kazooie (North American release)"),
                     QStringLiteral("A bear and bird adventure."),
                     QStringLiteral("1998-06-29"),
                     3,
                     QStringLiteral("E - Everyone"),
                     QStringLiteral("https://cdn.example.test/banjo-front.jpg")),
                 qPrintable(database.lastError()));

        const QVariantMap saved = database.games(QStringLiteral("N64"))
                                      .first().toMap();
        QCOMPARE(saved.value("id").toInt(), gameId);
        QCOMPARE(saved.value("metadataProvider").toString(),
                 QStringLiteral("thegamesdb"));
        QCOMPARE(saved.value("metadataGameId").toInt(), 1234);
        QCOMPARE(saved.value("metadataTitle").toString(),
                 QStringLiteral("Banjo-Kazooie (North American release)"));
        QCOMPARE(saved.value("title").toString(),
                 QStringLiteral("Banjo-Kazooie"));
        QCOMPARE(saved.value("metadataOverview").toString(),
                 QStringLiteral("A bear and bird adventure."));
        QCOMPARE(saved.value("metadataReleaseDate").toString(),
                 QStringLiteral("1998-06-29"));
        QCOMPARE(saved.value("metadataPlatformId").toInt(), 3);
        QCOMPARE(saved.value("metadataRating").toString(),
                 QStringLiteral("E - Everyone"));
        QCOMPARE(saved.value("metadataArtworkUrl").toString(),
                 QStringLiteral("https://cdn.example.test/banjo-front.jpg"));
    }

    void reassociatingMetadataReplacesThePreviousMatch()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        QVERIFY2(database.initialize(
                     temporaryDirectory.path() + "/library.sqlite"),
                 qPrintable(database.lastError()));
        QVERIFY(database.registerGame(
            QStringLiteral("Game"), QStringLiteral("PS1"),
            temporaryDirectory.path() + "/Game.cue"));

        const int gameId = database.games().first().toMap().value("id").toInt();
        QVERIFY(database.associateMetadata(
            gameId, QStringLiteral("thegamesdb"), 100,
            QStringLiteral("First Match"), {}, {}, 7, {}, {}));
        QVERIFY(database.associateMetadata(
            gameId, QStringLiteral("thegamesdb"), 200,
            QStringLiteral("Corrected Match"), {}, {}, 7, {}, {}));

        const QVariantMap saved = database.games().first().toMap();
        QCOMPARE(saved.value("metadataGameId").toInt(), 200);
        QCOMPARE(saved.value("metadataTitle").toString(),
                 QStringLiteral("Corrected Match"));
    }

    void registersOriginalXboxAsLibraryPlatform()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        QVERIFY2(database.initialize(
                     temporaryDirectory.path() + "/library.sqlite"),
                 qPrintable(database.lastError()));

        QVERIFY2(database.registerGame(
                     QStringLiteral("Halo: Combat Evolved"),
                     QStringLiteral("Xbox"),
                     temporaryDirectory.path() + "/Xbox/Halo.iso"),
                 qPrintable(database.lastError()));

        const QVariantList xboxGames = database.games(QStringLiteral("Xbox"));
        QCOMPARE(xboxGames.size(), 1);
        QCOMPARE(xboxGames.first().toMap().value("platform").toString(),
                 QStringLiteral("Xbox"));
    }

    void storesCachedArtworkPath()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        QVERIFY(database.initialize(temporaryDirectory.path() + "/library.sqlite"));
        QVERIFY(database.registerGame(QStringLiteral("Game"), QStringLiteral("N64"),
                                      temporaryDirectory.path() + "/Game.z64"));
        const int gameId = database.games().first().toMap().value("id").toInt();
        const QString artworkPath = temporaryDirectory.path() + "/artwork/1.jpg";
        QVERIFY(database.setArtworkPath(gameId, artworkPath));
        QCOMPARE(database.games().first().toMap().value("artworkPath").toString(),
                 QUrl::fromLocalFile(artworkPath).toString());
    }


    void tracksMetadataLookupAttemptsWithoutRepeatingQueries()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        QVERIFY2(database.initialize(
                     temporaryDirectory.path() + "/library.sqlite"),
                 qPrintable(database.lastError()));
        QVERIFY(database.registerGame(
            QStringLiteral("Banjo-Kazooie"), QStringLiteral("N64"),
            temporaryDirectory.path() + "/Banjo-Kazooie.z64"));

        const QVariantList pending = database.gamesNeedingMetadata();
        QCOMPARE(pending.size(), 1);
        const int gameId = pending.first().toMap().value("id").toInt();

        QVERIFY(database.markMetadataLookupAttempt(
            gameId, QStringLiteral("no_match")));
        QVERIFY(database.gamesNeedingMetadata().isEmpty());

        QVERIFY(database.markMetadataLookupAttempt(
            gameId, QStringLiteral("ambiguous")));
        QVERIFY(database.gamesNeedingMetadata().isEmpty());

        QVERIFY(database.markMetadataLookupAttempt(
            gameId, QStringLiteral("matched")));
        QVERIFY(database.gamesNeedingMetadata().isEmpty());
    }

    void readdingUnavailableGameClearsStaleLookupAttempt()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        QVERIFY2(database.initialize(
                     temporaryDirectory.path() + "/library.sqlite"),
                 qPrintable(database.lastError()));

        const QString romPath = temporaryDirectory.path() + "/Banjo-Kazooie.z64";
        QVERIFY(database.registerGame(
            QStringLiteral("Banjo-Kazooie"), QStringLiteral("N64"), romPath));

        const int gameId = database.games().first().toMap().value("id").toInt();
        QVERIFY(database.markMetadataLookupAttempt(
            gameId, QStringLiteral("ambiguous")));
        QVERIFY(database.gamesNeedingMetadata().isEmpty());

        // A completed scan with no ROMs marks the existing record unavailable.
        QVERIFY(database.reconcilePlatformGames(QStringLiteral("N64"), {}));
        QVERIFY(database.games().isEmpty());

        // Reappearance reuses the original row but clears its stale failed lookup.
        QVERIFY(database.registerGame(
            QStringLiteral("Banjo-Kazooie"), QStringLiteral("N64"), romPath));
        const QVariantList pending = database.gamesNeedingMetadata();
        QCOMPARE(pending.size(), 1);
        QCOMPARE(pending.first().toMap().value("id").toInt(), gameId);
    }

    void providerFailuresAreSkippedByAutomaticScansButCanBeRetriedManually()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        QVERIFY2(database.initialize(
                     temporaryDirectory.path() + "/library.sqlite"),
                 qPrintable(database.lastError()));
        QVERIFY(database.registerGame(
            QStringLiteral("New Game"), QStringLiteral("N64"),
            temporaryDirectory.path() + "/New Game.z64"));

        const QVariantList pending = database.gamesNeedingMetadata();
        QCOMPARE(pending.size(), 1);
        const int gameId = pending.first().toMap().value("id").toInt();

        QVERIFY(database.markMetadataLookupAttempt(
            gameId, QStringLiteral("provider_error")));
        QVERIFY(database.gamesNeedingMetadata().isEmpty());

        const QVariantList manualRetry = database.gamesNeedingMetadata(true);
        QCOMPARE(manualRetry.size(), 1);
        QCOMPARE(manualRetry.first().toMap().value("id").toInt(), gameId);
    }

    void storesAndClearsGameplayScreenshots()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        QVERIFY2(database.initialize(temporaryDirectory.path() + "/library.sqlite"),
                 qPrintable(database.lastError()));
        QVERIFY(database.registerGame(QStringLiteral("Game"), QStringLiteral("N64"),
                                      temporaryDirectory.path() + "/Game.z64"));
        const int gameId = database.games().first().toMap().value("id").toInt();
        QVERIFY(database.associateMetadata(
            gameId, QStringLiteral("thegamesdb"), 4321,
            QStringLiteral("Game"), {}, {}, 3, {}, {}));

        const QVariantList matchedGames = database.gamesWithMetadata();
        QCOMPARE(matchedGames.size(), 1);
        QCOMPARE(matchedGames.first().toMap().value("metadataGameId").toInt(), 4321);

        const QString screenshot = temporaryDirectory.path() + "/screenshots/1.jpg";
        QVERIFY(database.addGameScreenshot(gameId, screenshot));
        QCOMPARE(database.gameScreenshots(gameId).size(), 0); // Path is not present yet.
        QVERIFY(QDir().mkpath(QFileInfo(screenshot).absolutePath()));
        QFile image(screenshot);
        QVERIFY(image.open(QIODevice::WriteOnly));
        image.write("test");
        image.close();
        QCOMPARE(database.gameScreenshots(gameId).size(), 1);

        QVERIFY(database.clearGameScreenshots(gameId));
        QVERIFY(database.gameScreenshots(gameId).isEmpty());
    }

    void rejectsAssociationForUnknownLocalGame()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        QVERIFY(database.initialize(
            temporaryDirectory.path() + "/library.sqlite"));

        QVERIFY(!database.associateMetadata(
            999, QStringLiteral("thegamesdb"), 123,
            QStringLiteral("Unknown Game"), {}, {}, 3, {}, {}));
        QVERIFY(database.lastError().contains(
            QStringLiteral("no longer exists")));
    }
};

QTEST_GUILESS_MAIN(DatabaseManagerTest)
#include "DatabaseManagerTest.moc"
