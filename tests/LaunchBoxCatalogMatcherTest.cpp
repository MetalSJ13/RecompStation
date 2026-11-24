#include <QtTest>

#include "metadata/LaunchBoxCatalogMatcher.h"

class LaunchBoxCatalogMatcherTest : public QObject
{
    Q_OBJECT

private:
    static LaunchBoxGameRecord game(int id, const QString &name,
                                    const QString &platform)
    {
        LaunchBoxGameRecord record;
        record.databaseId = id;
        record.name = name;
        record.platform = platform;
        return record;
    }

private slots:
    void mapsOnlyExplicitCanonicalPlatforms()
    {
        QCOMPARE(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("N64"),
                 QStringLiteral("Nintendo 64"));
        QCOMPARE(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("PS1"),
                 QStringLiteral("Sony Playstation"));
        QCOMPARE(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("GameCube"),
                 QStringLiteral("Nintendo GameCube"));
        QCOMPARE(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("Wii"),
                 QStringLiteral("Nintendo Wii"));
        QCOMPARE(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("Xbox"),
                 QStringLiteral("Microsoft Xbox"));
        QCOMPARE(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("Xbox 360"),
                 QStringLiteral("Microsoft Xbox 360"));
        QCOMPARE(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("GB"),
                 QStringLiteral("Nintendo Game Boy"));
        QCOMPARE(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("GBC"),
                 QStringLiteral("Nintendo Game Boy Color"));
        QCOMPARE(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("GBA"),
                 QStringLiteral("Nintendo Game Boy Advance"));
        QCOMPARE(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("SNES"),
                 QStringLiteral("Super Nintendo Entertainment System"));
        QCOMPARE(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("PS2"),
                 QStringLiteral("Sony Playstation 2"));
        QCOMPARE(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("Wii U"),
                 QStringLiteral("Nintendo Wii U"));
        QVERIFY(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("Nintendo 64").isEmpty());
        QVERIFY(LaunchBoxCatalogMatcher::canonicalPlatformForInternalName("64DD").isEmpty());
    }

    void matchesFilenameAssociationBeforeTitleFallback()
    {
        const QList<LaunchBoxFileAssociation> associations = {
            {QStringLiteral("Nintendo 64"),
             QStringLiteral("Banjo-Kazooie (USA) (Rev A)"),
             QStringLiteral("Banjo-Kazooie (USA) (Rev A)")}
        };
        const QList<LaunchBoxGameRecord> games = {
            game(162, "Banjo-Kazooie (USA) (Rev A)", "Nintendo 64"),
            game(163, "Banjo-Kazooie", "Nintendo 64")
        };

        const auto result = LaunchBoxCatalogMatcher::matchByFileAssociation(
            "N64", "Banjo-Kazooie (USA) (Rev A).z64",
            associations, games);
        QVERIFY(result.isMatch());
        QCOMPARE(result.index, 0);
        QCOMPARE(games.at(result.index).databaseId, 162);
    }

    void fileAssociationRequiresMatchingRegionAndRevisionTokens()
    {
        const QList<LaunchBoxFileAssociation> associations = {
            {"Nintendo 64", "Game (USA) (Rev A)", "Game (USA) (Rev A)"}
        };
        const QList<LaunchBoxGameRecord> games = {
            game(1, "Game", "Nintendo 64")
        };
        const auto wrongRegion = LaunchBoxCatalogMatcher::matchByFileAssociation(
            "N64", "Game (Europe) (Rev A).z64", associations, games);
        QCOMPARE(wrongRegion.status, LaunchBoxMatchResult::Status::NoMatch);
        const auto wrongRevision = LaunchBoxCatalogMatcher::matchByFileAssociation(
            "N64", "Game (USA) (Rev B).z64", associations, games);
        QCOMPARE(wrongRevision.status, LaunchBoxMatchResult::Status::NoMatch);
    }

    void resolvesRevisionedFileAssociationToUniqueCanonicalMetadataTitle()
    {
        const QList<LaunchBoxFileAssociation> associations = {
            {"Nintendo 64", "Banjo-Kazooie (USA) (Rev A)",
             "Banjo-Kazooie (USA) (Rev A)"}
        };
        const QList<LaunchBoxGameRecord> games = {
            game(162, "Banjo-Kazooie", "Nintendo 64")
        };
        const auto result = LaunchBoxCatalogMatcher::matchByFileAssociation(
            "N64", "Banjo-Kazooie (USA) (Rev A).z64",
            associations, games);
        QVERIFY(result.isMatch());
        QCOMPARE(games.at(result.index).databaseId, 162);
    }

    void leavesRevisionFallbackAmbiguousWhenMultipleRecordsExist()
    {
        const QList<LaunchBoxFileAssociation> associations = {
            {"Nintendo 64", "Game (USA) (Rev A)", "Game (Rev A)"}
        };
        const QList<LaunchBoxGameRecord> games = {
            game(1, "Game", "Nintendo 64"),
            game(2, "Game (Rev B)", "Nintendo 64")
        };
        const auto result = LaunchBoxCatalogMatcher::matchByFileAssociation(
            "N64", "Game (USA) (Rev A).z64", associations, games);
        QCOMPARE(result.status, LaunchBoxMatchResult::Status::Ambiguous);
    }

    void preservesRevisionDistinctionsDuringFallback()
    {
        const QList<LaunchBoxGameRecord> games = {
            game(1, "Game Title", "Nintendo 64"),
            game(2, "Game Title (Rev 1)", "Nintendo 64")
        };
        const auto result = LaunchBoxCatalogMatcher::matchByTitle(
            "N64", "Game Title (Rev 1) (USA).z64", games);
        QVERIFY(result.isMatch());
        QCOMPARE(result.index, 1);
    }

    void requiresExactPlatformAndTitleForFallback()
    {
        const QList<LaunchBoxGameRecord> games = {
            game(1, "Super Mario 64", "Nintendo 64"),
            game(2, "Super Mario 64", "Nintendo DS"),
            game(3, "Super Mario 64 DS", "Nintendo 64")
        };
        const auto result = LaunchBoxCatalogMatcher::matchByTitle(
            "N64", "Super Mario 64", games);
        QVERIFY(result.isMatch());
        QCOMPARE(result.index, 0);
    }

    void rejectsAmbiguousFilenameAssociations()
    {
        const QList<LaunchBoxFileAssociation> associations = {
            {"Nintendo 64", "Game (USA)", "Game A"},
            {"Nintendo 64", "Game (USA)", "Game B"}
        };
        const QList<LaunchBoxGameRecord> games = {
            game(1, "Game A", "Nintendo 64"),
            game(2, "Game B", "Nintendo 64")
        };
        const auto result = LaunchBoxCatalogMatcher::matchByFileAssociation(
            "N64", "Game (USA).z64", associations, games);
        QCOMPARE(result.status, LaunchBoxMatchResult::Status::Ambiguous);
        QVERIFY(!result.isMatch());
    }

    void rejectsDuplicateMetadataMatches()
    {
        const QList<LaunchBoxFileAssociation> associations = {
            {"Nintendo 64", "Game (USA)", "Game"}
        };
        const QList<LaunchBoxGameRecord> games = {
            game(1, "Game", "Nintendo 64"),
            game(2, "Game", "Nintendo 64")
        };
        const auto result = LaunchBoxCatalogMatcher::matchByFileAssociation(
            "N64", "Game (USA).z64", associations, games);
        QCOMPARE(result.status, LaunchBoxMatchResult::Status::Ambiguous);
    }

    void doesNotUseCrossPlatformOrAmbiguousAliases()
    {
        const QList<LaunchBoxFileAssociation> associations = {
            {"Nintendo 64DD", "Game (USA)", "Game"},
            {"Nintendo DS", "Game (USA)", "Game"}
        };
        const QList<LaunchBoxGameRecord> games = {
            game(1, "Game", "Nintendo 64DD")
        };
        const auto result = LaunchBoxCatalogMatcher::matchByFileAssociation(
            "N64", "Game (USA).z64", associations, games);
        QCOMPARE(result.status, LaunchBoxMatchResult::Status::NoMatch);
    }

    void rejectsUnsupportedPlatforms()
    {
        const auto result = LaunchBoxCatalogMatcher::matchByTitle(
            "64DD", "Game", {});
        QCOMPARE(result.status, LaunchBoxMatchResult::Status::UnsupportedPlatform);
    }

    void exactFilenameNormalizationPreservesRegionLanguageAndRevision()
    {
        const QString usaRevA = LaunchBoxCatalogMatcher::normalizedFileName(
            "Banjo-Kazooie (USA) (Rev A).z64");
        QCOMPARE(usaRevA, LaunchBoxCatalogMatcher::normalizedFileName(
                             "BANJO KAZOOIE USA REV A"));
        QVERIFY(usaRevA != LaunchBoxCatalogMatcher::normalizedFileName(
                              "Banjo-Kazooie (Europe) (Rev A).z64"));
        QVERIFY(usaRevA != LaunchBoxCatalogMatcher::normalizedFileName(
                              "Banjo-Kazooie (USA) (Rev B).z64"));
    }

    void stripsRegionsAndLanguageTagsButPreservesRevisionTags()
    {
        QCOMPARE(LaunchBoxCatalogMatcher::normalizedTitle("Banjo-Kazooie (USA).z64"),
                 LaunchBoxCatalogMatcher::normalizedTitle("Banjo Kazooie"));
        QCOMPARE(LaunchBoxCatalogMatcher::normalizedTitle("Mario Kart Wii (USA) (En,Fr,Es).rvz"),
                 LaunchBoxCatalogMatcher::normalizedTitle("Mario Kart Wii"));
        QVERIFY(LaunchBoxCatalogMatcher::normalizedTitle("Game (Rev A)")
                != LaunchBoxCatalogMatcher::normalizedTitle("Game"));
    }

    void titleFallbackCanResolveUniqueCanonicalTitleWhenRevisionIsMissing()
    {
        const QList<LaunchBoxGameRecord> games = {
            game(1, "Banjo-Kazooie", "Nintendo 64")
        };
        const auto result = LaunchBoxCatalogMatcher::matchByTitle(
            "N64", "Banjo-Kazooie (USA) (Rev A)", games);
        QVERIFY(result.isMatch());
        QCOMPARE(games.at(result.index).databaseId, 1);
    }

    void titleFallbackPrefersExactRevisionWhenBothExist()
    {
        const QList<LaunchBoxGameRecord> games = {
            game(1, "Game", "Nintendo 64"),
            game(2, "Game (Rev 1)", "Nintendo 64")
        };
        const auto result = LaunchBoxCatalogMatcher::matchByTitle(
            "N64", "Game (Rev 1) (USA)", games);
        QVERIFY(result.isMatch());
        QCOMPARE(games.at(result.index).databaseId, 2);
    }

    void titleFallbackKeepsRevisionInsensitiveDuplicatesAmbiguous()
    {
        const QList<LaunchBoxGameRecord> games = {
            game(1, "Game", "Nintendo 64"),
            game(2, "Game (Rev 2)", "Nintendo 64")
        };
        const auto result = LaunchBoxCatalogMatcher::matchByTitle(
            "N64", "Game (Rev 1) (USA)", games);
        QCOMPARE(result.status, LaunchBoxMatchResult::Status::Ambiguous);
    }
};

QTEST_GUILESS_MAIN(LaunchBoxCatalogMatcherTest)
#include "LaunchBoxCatalogMatcherTest.moc"
