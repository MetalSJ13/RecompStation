#include <QtTest>

#include "core/ArtworkManager.h"
#include "database/DatabaseManager.h"

#include <QSignalSpy>
#include <QTemporaryDir>

class ArtworkManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsNonHttpsArtworkUrls()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        QVERIFY(database.initialize(temporaryDirectory.path() + "/library.sqlite"));
        QVERIFY(database.registerGame(QStringLiteral("Test Game"),
                                      QStringLiteral("N64"),
                                      temporaryDirectory.path() + "/Test.z64"));
        const int gameId = database.games().first().toMap().value("id").toInt();

        ArtworkManager artworkManager(&database,
                                      temporaryDirectory.path() + "/artwork");
        QSignalSpy failedSpy(&artworkManager, &ArtworkManager::artworkFailed);
        QVERIFY(failedSpy.isValid());

        artworkManager.downloadArtwork(gameId, QStringLiteral("http://example.test/cover.jpg"));

        QCOMPARE(failedSpy.count(), 1);
        QVERIFY(failedSpy.first().at(1).toString().contains(QStringLiteral("HTTPS")));
        QVERIFY(database.games().first().toMap().value("artworkPath").toString().isEmpty());
    }

    void rejectsInvalidGameIds()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        QVERIFY(database.initialize(temporaryDirectory.path() + "/library.sqlite"));
        ArtworkManager artworkManager(&database,
                                      temporaryDirectory.path() + "/artwork");
        QSignalSpy failedSpy(&artworkManager, &ArtworkManager::artworkFailed);
        QVERIFY(failedSpy.isValid());

        artworkManager.downloadArtwork(0, QStringLiteral("https://cdn.example.test/cover.jpg"));

        QCOMPARE(failedSpy.count(), 1);
        QVERIFY(failedSpy.first().at(1).toString().contains(QStringLiteral("valid HTTPS")));
    }
};

QTEST_GUILESS_MAIN(ArtworkManagerTest)
#include "ArtworkManagerTest.moc"
