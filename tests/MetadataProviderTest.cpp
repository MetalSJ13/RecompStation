#include <QtTest>

#include "metadata/TheGamesDbClient.h"
#include "metadata/MetadataManager.h"
#include "metadata/MetadataPlatformMapping.h"

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
            "include": {
                "boxart": {
                    "base_url": {"original": "https://cdn.example.test/original/"},
                    "data": {"4": [{"side": "front", "filename": "banjo-front.jpg"}]}
                }
            },
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
        QCOMPARE(game.artworkUrl,
                 QStringLiteral("https://cdn.example.test/original/banjo-front.jpg"));
        QCOMPARE(game.developerIds, QList<int>{6037});
        QCOMPARE(game.publisherIds, QList<int>{3});
        QCOMPARE(game.genreIds, (QList<int>{1, 8}));
    }

    void fallsBackToAvailableArtworkWhenFrontCoverIsMissing()
    {
        const QByteArray json = R"({
            "include": {
                "boxart": {
                    "base_url": {"thumb": "https://cdn.example.test/thumb/"},
                    "data": {"10": [{"side": "back", "filename": "game-back.jpg"}]}
                }
            },
            "data": {
                "games": [{"id": 10, "game_title": "Metroid Prime 3"}]
            }
        })";

        QList<GameMetadata> results;
        QString error;
        QVERIFY2(TheGamesDbClient::parseSearchResponse(json, &results, &error),
                 qPrintable(error));
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first().artworkUrl,
                 QStringLiteral("https://cdn.example.test/thumb/game-back.jpg"));
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

    void parsesTheGamesDbPlatformCatalog()
    {
        const QByteArray json = R"({
            "code": 200,
            "status": "Success",
            "data": {
                "count": 4,
                "platforms": {
                    "3": {"id": 3, "name": "Nintendo 64", "alias": "nintendo-64"},
                    "7": {"id": 7, "name": "Sony Playstation", "alias": "sony-playstation"},
                    "10": {"id": 10, "name": "Nintendo GameCube", "alias": "nintendo-gamecube"},
                    "11": {"id": 11, "name": "Nintendo Wii", "alias": "nintendo-wii"}
                }
            }
        })";

        QList<TheGamesDbPlatform> platforms;
        QString error;
        QVERIFY2(MetadataPlatformMapping::parsePlatformCatalog(json, &platforms, &error),
                 qPrintable(error));
        QCOMPARE(platforms.size(), 4);
        QCOMPARE(MetadataPlatformMapping::platformIdForInternalName(
                     QStringLiteral("N64"), platforms), 3);
        QCOMPARE(MetadataPlatformMapping::platformIdForInternalName(
                     QStringLiteral("PS1"), platforms), 7);
        QCOMPARE(MetadataPlatformMapping::platformIdForInternalName(
                     QStringLiteral("GameCube"), platforms), 10);
        QCOMPARE(MetadataPlatformMapping::platformIdForInternalName(
                     QStringLiteral("Wii"), platforms), 11);
    }

    void rejectsInvalidPlatformCatalog()
    {
        QList<TheGamesDbPlatform> platforms;
        QString error;
        QVERIFY(!MetadataPlatformMapping::parsePlatformCatalog(
            QByteArray(R"({"data":{}})"), &platforms, &error));
        QVERIFY(error.contains(QStringLiteral("data.platforms")));
    }

    void managerExposesProviderConfigurationAndErrors()
    {
        TheGamesDbClient client(QStringLiteral(""));
        MetadataManager manager(&client);

        QVERIFY(!manager.isConfigured());
        QVERIFY(!manager.isSearching());

        QSignalSpy failedSpy(&manager, &MetadataManager::searchFailed);
        QVERIFY(failedSpy.isValid());

        manager.searchGames(QStringLiteral("Super Mario 64"));

        QCOMPARE(failedSpy.count(), 1);
        QVERIFY(!manager.isSearching());
        QVERIFY(manager.errorString().contains(
            QStringLiteral("RECOMPSTATION_TGDB_API_KEY")));
        QVERIFY(manager.results().isEmpty());
    }
};

QTEST_GUILESS_MAIN(MetadataProviderTest)
#include "MetadataProviderTest.moc"
