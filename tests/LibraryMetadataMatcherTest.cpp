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

    void ignoresCommonRomRegionTags()\n    {\n        const QVariantList results = {\n            QVariantMap{{"title", "Donkey Kong 64"}}\n        };\n        QCOMPARE(LibraryMetadataMatcher::exactMatchIndex(\n                     QStringLiteral("Donkey Kong 64 (USA) [! ]"), results), 0);\n    }\n\n    void ignoresDiacriticDifferences()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Pokemon Stadium"}}
        };
        QCOMPARE(LibraryMetadataMatcher::exactMatchIndex(
                     QString::fromUtf8("Pokémon Stadium"), results), 0);
    }
};

QTEST_GUILESS_MAIN(LibraryMetadataMatcherTest)
#include "LibraryMetadataMatcherTest.moc"
