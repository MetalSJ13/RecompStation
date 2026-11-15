#include <QtTest>

#include "core/LibraryMetadataMatcher.h"

class LibraryMetadataMatcherTest : public QObject
{
    Q_OBJECT

private slots:
    void matchesTitlesWithoutCaseOrPunctuationDifferences()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Banjo-Kazooie"}}
        };
        QCOMPARE(LibraryMetadataMatcher::exactMatchIndex(
                     QStringLiteral("BANJO KAZOOIE"), results), 0);
    }

    void doesNotAcceptSimilarButDifferentTitles()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Banjo-Tooie"}}
        };
        QCOMPARE(LibraryMetadataMatcher::exactMatchIndex(
                     QStringLiteral("Banjo-Kazooie"), results), -1);
    }

    void leavesDuplicateExactMatchesAmbiguous()
    {
        const QVariantList results = {
            QVariantMap{{"title", "The Legend of Zelda"}},
            QVariantMap{{"title", "The Legend of Zelda"}}
        };
        QCOMPARE(LibraryMetadataMatcher::exactMatchIndex(
                     QStringLiteral("The Legend of Zelda"), results), -2);
    }

    void ignoresCommonRomRegionTags()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Donkey Kong 64"}}
        };
        QCOMPARE(LibraryMetadataMatcher::exactMatchIndex(
                     QStringLiteral("Donkey Kong 64 (USA) [!]"), results), 0);
    }

    void ignoresCommonRegionCodesInParenthesesAndBrackets()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Metroid Prime 3: Corruption"}}
        };
        for (const QString &localTitle : {
                 QStringLiteral("Metroid Prime 3: Corruption (USA)"),
                 QStringLiteral("Metroid Prime 3: Corruption [USA]"),
                 QStringLiteral("Metroid Prime 3: Corruption (Europe)"),
                 QStringLiteral("Metroid Prime 3: Corruption [J]"),
                 QStringLiteral("Metroid Prime 3: Corruption (World)")}) {
            QCOMPARE(LibraryMetadataMatcher::exactMatchIndex(localTitle, results), 0);
        }
    }

    void preservesRevisionInformation()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Game Title Rev 1"}},
            QVariantMap{{"title", "Game Title"}}
        };
        QCOMPARE(LibraryMetadataMatcher::exactMatchIndex(
                     QStringLiteral("Game Title (Rev 1)"), results), 0);
    }

    void ignoresDiacriticDifferences()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Pokemon Stadium"}}
        };
        QCOMPARE(LibraryMetadataMatcher::exactMatchIndex(
                     QString::fromUtf8("Pokémon Stadium"), results), 0);
    }

    void ranksDuplicateExactTitlesByArtworkResolution()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Donkey Kong 64"}, {"platformId", 3},
                        {"artworkPixelCount", 0}},
            QVariantMap{{"title", "Donkey Kong 64"}, {"platformId", 3},
                        {"artworkPixelCount", 682000}},
            QVariantMap{{"title", "Donkey Kong 64"}, {"platformId", 3},
                        {"artworkPixelCount", 0}}
        };
        QCOMPARE(LibraryMetadataMatcher::bestMatchIndex(
                     QStringLiteral("Donkey Kong 64"), 3, results), 1);
    }

    void acceptsAColonSubtitleWhenPlatformAndArtworkAreClear()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Metroid Prime 3: Corruption"},
                        {"platformId", 9}, {"artworkPixelCount", 2292884}}
        };
        QCOMPARE(LibraryMetadataMatcher::bestMatchIndex(
                     QStringLiteral("Metroid Prime 3"), 9, results), 0);
    }

    void leavesEqualRankedCandidatesAmbiguous()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Donkey Kong 64"}, {"platformId", 3},
                        {"artworkPixelCount", 682000}},
            QVariantMap{{"title", "Donkey Kong 64"}, {"platformId", 3},
                        {"artworkPixelCount", 682000}}
        };
        QCOMPARE(LibraryMetadataMatcher::bestMatchIndex(
                     QStringLiteral("Donkey Kong 64"), 3, results), -2);
    }

    void rejectsCandidatesFromAnotherPlatform()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Donkey Kong 64"}, {"platformId", 9},
                        {"artworkPixelCount", 2000000}}
        };
        QCOMPARE(LibraryMetadataMatcher::bestMatchIndex(
                     QStringLiteral("Donkey Kong 64"), 3, results), -1);
    }
};

QTEST_GUILESS_MAIN(LibraryMetadataMatcherTest)
#include "LibraryMetadataMatcherTest.moc"
