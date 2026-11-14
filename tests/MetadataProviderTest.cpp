#include <QtTest>

#include "metadata/TheGamesDbClient.h"

#include <QSignalSpy>

class MetadataProviderTest : public QObject
{
    Q_OBJECT

private slots:
    void reportsMissingApiKeyWithoutMakingARequest()
    {
        TheGamesDbClient client(QStringLiteral(""));
        QVERIFY(!client.isConfigured());

        QSignalSpy failureSpy(&client, &MetadataProvider::requestFailed);
        QVERIFY(failureSpy.isValid());

        client.searchGames(QStringLiteral("Super Mario 64"));

        QCOMPARE(failureSpy.count(), 1);
        const QList<QVariant> arguments = failureSpy.takeFirst();
        QCOMPARE(arguments.at(0).toInt(), 1);
        QVERIFY(arguments.at(1).toString().contains(
            QStringLiteral("RECOMPSTATION_TGDB_API_KEY")));
    }

    void rejectsEmptySearchTerm()
    {
        TheGamesDbClient client(QStringLiteral("test-key"));
        QSignalSpy failureSpy(&client, &MetadataProvider::requestFailed);
        QVERIFY(failureSpy.isValid());

        client.searchGames(QStringLiteral("   "));

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(failureSpy.takeFirst().at(1).toString().contains(
            QStringLiteral("game title")));
    }

    void parsesGameSearchResponse()
    {
        const QByteArray json = R"({
            "code": 200,
            "status": "Success",
            "data": {
                "count": 1,
                "games": { "4": {
                    "id": 4,
                    "game_title": "Star Fox 64",
                    "release_date": "1997-06-30",
                    "platform": 3,
                    "overview": "A space combat game.",
                    "rating": "E - Everyone",
                    "developers": [6037],
                    "publishers": [3],
                    "genres": [1, 8]
                }}
            }
        })";

        QList<GameMetadata> results;
        QString error;
        QVERIFY2(TheGamesDbClient::parseSearchResponse(json, &results, &error),
                 qPrintable(error));

        QCOMPARE(results.size(), 1);
        const GameMetadata game = results.first();
        QCOMPARE(game.provider, QStringLiteral("thegamesdb"));
        QCOMPARE(game.providerGameId, 4);
        QCOMPARE(game.title, QStringLiteral("Star Fox 64"));
        QCOMPARE(game.releaseDate, QDate(1997, 6, 30));
        QCOMPARE(game.platformId, 3);
        QCOMPARE(game.overview, QStringLiteral("A space combat game."));
        QCOMPARE(game.developerIds, QList<int>{6037});
        QCOMPARE(game.publisherIds, QList<int>{3});
        QCOMPARE(game.genreIds, (QList<int>{1, 8}));
    }

    void acceptsArrayGamePayloadsToo()
    {
        const QByteArray json = R"({"data":{"games":[{"id":9,"game_title":"Example"}]}})";
        QList<GameMetadata> results;
        QString error;
        QVERIFY2(TheGamesDbClient::parseSearchResponse(json, &results, &error), qPrintable(error));
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first().providerGameId, 9);
    }

    void rejectsMalformedOrUnexpectedResponses()
    {
        QList<GameMetadata> results;
        QString error;

        QVERIFY(!TheGamesDbClient::parseSearchResponse(
            QByteArray("{invalid"), &results, &error));
        QVERIFY(!error.isEmpty());

        QVERIFY(!TheGamesDbClient::parseSearchResponse(
            QByteArray(R"({"data":{}})"), &results, &error));
        QVERIFY(error.contains(QStringLiteral("data.games")));
    }
};

QTEST_GUILESS_MAIN(MetadataProviderTest)
#include "MetadataProviderTest.moc"
