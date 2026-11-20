#include <QtTest>

#include "metadata/LaunchBoxXmlParser.h"

class LaunchBoxXmlParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesPlatformNamesAndAliases()
    {
        const QByteArray xml = R"xml(
            <LaunchBox>
                <Platform>
                    <Name>Nintendo 64</Name>
                    <Alternate>N64</Alternate>
                    <Alternate>Nintendo N64</Alternate>
                    <PlatformAlternateName>N64</PlatformAlternateName>
                </Platform>
                <Platform>
                    <Name>Nintendo GameCube</Name>
                    <Alternate>GameCube</Alternate>
                </Platform>
            </LaunchBox>
        )xml";

        QList<LaunchBoxPlatform> platforms;
        QString error;
        QVERIFY2(LaunchBoxXmlParser::parsePlatforms(xml, &platforms, &error),
                 qPrintable(error));
        QCOMPARE(platforms.size(), 2);
        QCOMPARE(platforms.at(0).name, QStringLiteral("Nintendo 64"));
        QCOMPARE(platforms.at(0).alternateNames,
                 QStringList({QStringLiteral("N64"),
                              QStringLiteral("Nintendo N64")}));
        QCOMPARE(platforms.at(1).alternateNames,
                 QStringList({QStringLiteral("GameCube")}));
        QVERIFY(error.isEmpty());
    }

    void skipsIncompleteAndDuplicatePlatformRecords()
    {
        const QByteArray xml = R"xml(
            <LaunchBox>
                <Platform><Name>Nintendo 64</Name><Alternate>N64</Alternate></Platform>
                <Platform><Name> nintendo-64 </Name><Alternate>Other alias</Alternate></Platform>
                <Platform><Alternate>Missing name</Alternate></Platform>
            </LaunchBox>
        )xml";

        QList<LaunchBoxPlatform> platforms;
        QString error;
        QVERIFY2(LaunchBoxXmlParser::parsePlatforms(xml, &platforms, &error),
                 qPrintable(error));
        QCOMPARE(platforms.size(), 1);
        QCOMPARE(platforms.first().name, QStringLiteral("Nintendo 64"));
    }

    void parsesFileAssociationsAndPreservesConflictingMappings()
    {
        const QByteArray xml = R"xml(
            <LaunchBox>
                <File>
                    <Platform>Nintendo 64</Platform>
                    <FileName>Banjo-Kazooie (USA) (Rev A)</FileName>
                    <GameName>Banjo-Kazooie (USA) (Rev A)</GameName>
                </File>
                <File>
                    <Platform>Nintendo 64</Platform>
                    <FileName>Banjo-Kazooie (USA) (Rev A)</FileName>
                    <GameName>Banjo-Kazooie</GameName>
                </File>
                <File>
                    <Platform>Nintendo 64</Platform>
                    <FileName>Banjo-Kazooie (USA) (Rev A)</FileName>
                    <GameName>Banjo-Kazooie</GameName>
                </File>
                <File><Platform>Nintendo 64</Platform><FileName>Incomplete</FileName></File>
            </LaunchBox>
        )xml";

        QList<LaunchBoxFileAssociation> files;
        QString error;
        QVERIFY2(LaunchBoxXmlParser::parseFiles(xml, &files, &error),
                 qPrintable(error));
        QCOMPARE(files.size(), 2);
        QCOMPARE(files.at(0).platform, QStringLiteral("Nintendo 64"));
        QCOMPARE(files.at(0).fileName,
                 QStringLiteral("Banjo-Kazooie (USA) (Rev A)"));
        QCOMPARE(files.at(1).gameName, QStringLiteral("Banjo-Kazooie"));
    }

    void parsesMetadataAndAllowsMissingOptionalFields()
    {
        const QByteArray xml = R"xml(
            <LaunchBox>
                <Game>
                    <Name>Banjo-Kazooie</Name>
                    <ReleaseDate>1998-06-30</ReleaseDate>
                    <ReleaseYear>1998</ReleaseYear>
                    <Overview>A bear and bird adventure.</Overview>
                    <MaxPlayers>1</MaxPlayers>
                    <ReleaseType>Released</ReleaseType>
                    <Cooperative>No</Cooperative>
                    <VideoURL>https://example.test/video</VideoURL>
                    <DatabaseID>162</DatabaseID>
                    <CommunityRating>4.47</CommunityRating>
                    <WikipediaURL>https://en.wikipedia.org/wiki/Banjo-Kazooie</WikipediaURL>
                    <Platform>Nintendo 64</Platform>
                    <ESRB>E</ESRB>
                    <CommunityRatingCount>100</CommunityRatingCount>
                    <Genres>Action, Adventure</Genres>
                    <Developer>Rare</Developer>
                    <Publisher>Nintendo</Publisher>
                </Game>
                <Game>
                    <Name>Minimal Game</Name>
                    <DatabaseID>163</DatabaseID>
                    <Platform>Nintendo 64</Platform>
                </Game>
            </LaunchBox>
        )xml";

        QList<LaunchBoxGameRecord> games;
        QString error;
        QVERIFY2(LaunchBoxXmlParser::parseMetadata(xml, &games, &error),
                 qPrintable(error));
        QCOMPARE(games.size(), 2);
        const LaunchBoxGameRecord &banjo = games.at(0);
        QCOMPARE(banjo.databaseId, 162);
        QCOMPARE(banjo.name, QStringLiteral("Banjo-Kazooie"));
        QCOMPARE(banjo.platform, QStringLiteral("Nintendo 64"));
        QCOMPARE(banjo.releaseDate, QDate(1998, 6, 30));
        QCOMPARE(banjo.releaseYear, 1998);
        QCOMPARE(banjo.overview, QStringLiteral("A bear and bird adventure."));
        QCOMPARE(banjo.releaseType, QStringLiteral("Released"));
        QCOMPARE(banjo.communityRating, QStringLiteral("4.47"));
        QCOMPARE(banjo.genres, QStringLiteral("Action, Adventure"));
        QCOMPARE(banjo.developer, QStringLiteral("Rare"));
        QCOMPARE(banjo.publisher, QStringLiteral("Nintendo"));

        const LaunchBoxGameRecord &minimal = games.at(1);
        QCOMPARE(minimal.databaseId, 163);
        QVERIFY(minimal.overview.isEmpty());
        QVERIFY(!minimal.releaseDate.isValid());
    }

    void skipsInvalidAndDuplicateMetadataRecords()
    {
        const QByteArray xml = R"xml(
            <LaunchBox>
                <Game><Name>Missing ID</Name><Platform>Nintendo 64</Platform></Game>
                <Game><DatabaseID>0</DatabaseID><Name>Zero ID</Name><Platform>Nintendo 64</Platform></Game>
                <Game><DatabaseID>10</DatabaseID><Name>Missing platform</Name></Game>
                <Game><DatabaseID>11</DatabaseID><Name>Valid Game</Name><Platform>Nintendo 64</Platform></Game>
                <Game><DatabaseID>11</DatabaseID><Name>Duplicate ID</Name><Platform>Nintendo 64</Platform></Game>
            </LaunchBox>
        )xml";

        QList<LaunchBoxGameRecord> games;
        QString error;
        QVERIFY2(LaunchBoxXmlParser::parseMetadata(xml, &games, &error),
                 qPrintable(error));
        QCOMPARE(games.size(), 1);
        QCOMPARE(games.first().name, QStringLiteral("Valid Game"));
    }

    void rejectsMalformedXmlWithoutReplacingOutput()
    {
        QList<LaunchBoxPlatform> platforms = {
            {QStringLiteral("Existing platform"), {QStringLiteral("Existing alias")}}
        };
        QString error;
        const QByteArray malformed =
            "<LaunchBox><Platform><Name>Partial</Name></Platform>";
        QVERIFY(!LaunchBoxXmlParser::parsePlatforms(
            malformed, &platforms, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(platforms.size(), 1);
        QCOMPARE(platforms.first().name, QStringLiteral("Existing platform"));
    }

    void rejectsWrongRootAndEmptyCatalog()
    {
        QList<LaunchBoxFileAssociation> files;
        QString error;
        QVERIFY(!LaunchBoxXmlParser::parseFiles(
            QByteArray("<Other><File/></Other>"), &files, &error));
        QVERIFY(!error.isEmpty());

        QVERIFY(!LaunchBoxXmlParser::parseFiles(
            QByteArray("<LaunchBox><File><Platform>N64</Platform></File></LaunchBox>"),
            &files, &error));
        QVERIFY(error.contains(QStringLiteral("no usable records")));
    }

    void rejectsNullDestinations()
    {
        QString error;
        QVERIFY(!LaunchBoxXmlParser::parsePlatforms(
            QByteArray("<LaunchBox/>"), nullptr, &error));
        QVERIFY(error.contains(QStringLiteral("destination")));
        QVERIFY(!LaunchBoxXmlParser::parseFiles(
            QByteArray("<LaunchBox/>"), nullptr, &error));
        QVERIFY(!LaunchBoxXmlParser::parseMetadata(
            QByteArray("<LaunchBox/>"), nullptr, &error));
    }
};

QTEST_GUILESS_MAIN(LaunchBoxXmlParserTest)
#include "LaunchBoxXmlParserTest.moc"
