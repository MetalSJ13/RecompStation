#include <QtTest>

#include "core/LibraryMetadataMatcher.h"
#include "core/SettingsManager.h"
#include "database/DatabaseManager.h"
#include "metadata/MetadataManager.h"
#include "metadata/MetadataProvider.h"
#include "metadata/LaunchBoxCatalogManager.h"
#include "metadata/LaunchBoxCatalogDatabase.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>


class FakeMetadataProvider : public MetadataProvider
{
    Q_OBJECT
public:
    QList<GameMetadata> results;
    bool failRequest = false;

    QString providerId() const override { return QStringLiteral("fake"); }
    bool isConfigured() const override { return true; }
    void fetchPlatforms() override
    {
        emit platformsLoaded({TheGamesDbPlatform{3, QStringLiteral("Nintendo 64"),
                                                 QStringLiteral("nintendo-64")}});
    }
    void searchGames(const QString &, int) override
    {
        if (failRequest) {
            emit requestFailed(1, QStringLiteral("simulated provider failure"));
            return;
        }
        emit searchCompleted(1, results);
    }
};

class LibraryMetadataMatcherTest : public QObject
{
    Q_OBJECT

private:

    static bool writeFile(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(contents) == contents.size();
    }

    static bool prepareLaunchBoxCatalog(const QString &dataDirectory,
                                        LaunchBoxCatalogManager *manager,
                                        bool duplicateMetadata = false,
                                        bool includeFileAssociation = true)
    {
        if (!manager)
            return false;
        const QString root = dataDirectory + QStringLiteral("/catalog-source");
        if (!QDir().mkpath(root))
            return false;
        const QByteArray platforms = R"xml(
            <LaunchBox><Platform><Name>Nintendo 64</Name></Platform></LaunchBox>
        )xml";
        const QByteArray files = includeFileAssociation
            ? QByteArray(R"xml(
                <LaunchBox><File><Platform>Nintendo 64</Platform>
                    <FileName>Banjo-Kazooie (USA) (Rev A)</FileName>
                    <GameName>Banjo-Kazooie (USA) (Rev A)</GameName>
                </File></LaunchBox>
            )xml")
            : QByteArray("<LaunchBox/>");
        QByteArray metadata = R"xml(
            <LaunchBox><Game><DatabaseID>162</DatabaseID><Name>Banjo-Kazooie</Name>
                <Platform>Nintendo 64</Platform><ReleaseYear>1998</ReleaseYear>
                <Overview>Bear and bird adventure.</Overview></Game>
        )xml";
        if (duplicateMetadata) {
            metadata += R"xml(
                <Game><DatabaseID>163</DatabaseID><Name>Banjo-Kazooie</Name>
                    <Platform>Nintendo 64</Platform></Game>
            )xml";
        }
        metadata += "</LaunchBox>";
        if (!writeFile(root + "/Platforms.xml", platforms)
            || !writeFile(root + "/Files.xml", files)
            || !writeFile(root + "/Metadata.xml", metadata))
            return false;

        LaunchBoxCatalogDatabase catalog;
        return catalog.open(manager->catalogPath())
            && catalog.rebuildFromXmlFiles(root + "/Platforms.xml",
                                            root + "/Files.xml",
                                            root + "/Metadata.xml");
    }

    static bool prepareLibraryGame(const QString &root, DatabaseManager *database)
    {
        if (!database || !database->initialize(root + "/library.sqlite"))
            return false;
        return database->registerGame(
            QStringLiteral("Banjo-Kazooie (USA) (Rev A)"), QStringLiteral("N64"),
            root + "/roms/Banjo-Kazooie (USA) (Rev A).z64");
    }


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

    void ignoresCommonRomLanguageTags()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Mario Kart Wii"}}
        };
        QCOMPARE(LibraryMetadataMatcher::exactMatchIndex(
                     QStringLiteral("Mario Kart Wii (USA) (En,Fr,Es)"), results), 0);
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

    void movesTrailingTheArticleToTheFront()
    {
        const QVariantList results = {
            QVariantMap{{"title", "The Legend of Zelda: Majora's Mask"}},
            QVariantMap{{"title", "The Legend of Zelda: Ocarina of Time"}}
        };

        QCOMPARE(LibraryMetadataMatcher::exactMatchIndex(
                     QStringLiteral("Legend of Zelda, The - Majora's Mask (USA)"),
                     results), 0);
        QCOMPARE(LibraryMetadataMatcher::exactMatchIndex(
                     QStringLiteral("Legend of Zelda, The - Ocarina of Time (USA)"),
                     results), 1);
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

    void matchesUniquePrimaryCandidateWithoutRomRevisionSuffix()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Banjo-Kazooie"}, {"platformId", 3},
                        {"artworkPixelCount", 0}}
        };
        QCOMPARE(LibraryMetadataMatcher::bestMatchIndex(
                     QStringLiteral("Banjo-Kazooie (USA) (Rev A)"), 3, results), 0);
    }

    void prefersExactPrimaryRevisionOverCanonicalTitle()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Banjo-Kazooie"}, {"platformId", 3},
                        {"artworkPixelCount", 900000}},
            QVariantMap{{"title", "Banjo-Kazooie (Rev A)"}, {"platformId", 3},
                        {"artworkPixelCount", 0}}
        };
        QCOMPARE(LibraryMetadataMatcher::bestMatchIndex(
                     QStringLiteral("Banjo-Kazooie (USA) (Rev A)"), 3, results), 1);
    }

    void leavesMultipleRevisionlessPrimaryCandidatesAmbiguous()
    {
        const QVariantList results = {
            QVariantMap{{"title", "Banjo-Kazooie"}, {"platformId", 3}},
            QVariantMap{{"title", "Banjo-Kazooie (Rev B)"}, {"platformId", 3}}
        };
        QCOMPARE(LibraryMetadataMatcher::bestMatchIndex(
                     QStringLiteral("Banjo-Kazooie (USA) (Rev A)"), 3, results), -2);
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

    void usesLaunchBoxAfterSuccessfulEmptyTheGamesDbResponse()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DatabaseManager database;
        QVERIFY(prepareLibraryGame(directory.path(), &database));
        SettingsManager settings(directory.path() + "/settings.ini");
        settings.setLaunchBoxEnabled(true);
        LaunchBoxCatalogManager catalogManager(directory.path());
        QVERIFY(prepareLaunchBoxCatalog(directory.path(), &catalogManager));
        FakeMetadataProvider provider;
        MetadataManager metadata(&provider, nullptr, true);
        LibraryMetadataMatcher matcher(&database, &metadata, nullptr,
                                       &catalogManager, &settings);
        QSignalSpy finished(&matcher, &LibraryMetadataMatcher::scrapeFinished);

        matcher.scrapeLibrary();
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(0).toInt(), 1);
        const QVariantMap game = database.games().first().toMap();
        QCOMPARE(game.value("metadataProvider").toString(), QStringLiteral("launchbox"));
        QCOMPARE(game.value("metadataGameId").toInt(), 162);
        QCOMPARE(game.value("metadataTitle").toString(), QStringLiteral("Banjo-Kazooie"));
        QCOMPARE(game.value("metadataReleaseDate").toString(), QStringLiteral("1998"));
        QVERIFY(game.value("metadataArtworkUrl").toString().isEmpty());
        QVERIFY(game.value("artworkPath").toString().isEmpty());
    }

    void usesLaunchBoxTitleFallbackWhenNoFilenameAssociationExists()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DatabaseManager database;
        QVERIFY(prepareLibraryGame(directory.path(), &database));
        SettingsManager settings(directory.path() + "/settings.ini");
        settings.setLaunchBoxEnabled(true);
        LaunchBoxCatalogManager catalogManager(directory.path());
        QVERIFY(prepareLaunchBoxCatalog(directory.path(), &catalogManager, false, false));
        FakeMetadataProvider provider;
        MetadataManager metadata(&provider, nullptr, true);
        LibraryMetadataMatcher matcher(&database, &metadata, nullptr,
                                       &catalogManager, &settings);

        matcher.scrapeLibrary();
        const QVariantMap game = database.games().first().toMap();
        QCOMPARE(game.value("metadataProvider").toString(), QStringLiteral("launchbox"));
        QCOMPARE(game.value("metadataGameId").toInt(), 162);
    }

    void usesLaunchBoxWhenPrimaryResultsHaveNoConfidentMatch()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DatabaseManager database;
        QVERIFY(prepareLibraryGame(directory.path(), &database));
        SettingsManager settings(directory.path() + "/settings.ini");
        settings.setLaunchBoxEnabled(true);
        LaunchBoxCatalogManager catalogManager(directory.path());
        QVERIFY(prepareLaunchBoxCatalog(directory.path(), &catalogManager));
        FakeMetadataProvider provider;
        GameMetadata unrelated;
        unrelated.provider = QStringLiteral("fake");
        unrelated.providerGameId = 77;
        unrelated.title = QStringLiteral("Banjo-Tooie");
        unrelated.platformId = 3;
        provider.results.append(unrelated);
        MetadataManager metadata(&provider, nullptr, true);
        LibraryMetadataMatcher matcher(&database, &metadata, nullptr,
                                       &catalogManager, &settings);

        matcher.scrapeLibrary();
        const QVariantMap game = database.games().first().toMap();
        QCOMPARE(game.value("metadataProvider").toString(), QStringLiteral("launchbox"));
        QCOMPARE(game.value("metadataGameId").toInt(), 162);
    }

    void usesLaunchBoxWhenPrimaryResultsAreAmbiguous()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DatabaseManager database;
        QVERIFY(prepareLibraryGame(directory.path(), &database));
        SettingsManager settings(directory.path() + "/settings.ini");
        settings.setLaunchBoxEnabled(true);
        LaunchBoxCatalogManager catalogManager(directory.path());
        QVERIFY(prepareLaunchBoxCatalog(directory.path(), &catalogManager));
        FakeMetadataProvider provider;
        for (int id : {77, 78}) {
            GameMetadata candidate;
            candidate.provider = QStringLiteral("thegamesdb");
            candidate.providerGameId = id;
            candidate.title = QStringLiteral("Banjo-Kazooie");
            candidate.platformId = 3;
            provider.results.append(candidate);
        }
        MetadataManager metadata(&provider, nullptr, true);
        LibraryMetadataMatcher matcher(&database, &metadata, nullptr,
                                       &catalogManager, &settings);

        matcher.scrapeLibrary();
        const QVariantMap game = database.games().first().toMap();
        QCOMPARE(game.value("metadataProvider").toString(), QStringLiteral("launchbox"));
        QCOMPARE(game.value("metadataGameId").toInt(), 162);
    }

    void doesNotUseLaunchBoxAfterPrimaryProviderFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DatabaseManager database;
        QVERIFY(prepareLibraryGame(directory.path(), &database));
        SettingsManager settings(directory.path() + "/settings.ini");
        settings.setLaunchBoxEnabled(true);
        LaunchBoxCatalogManager catalogManager(directory.path());
        QVERIFY(prepareLaunchBoxCatalog(directory.path(), &catalogManager));
        FakeMetadataProvider provider;
        provider.failRequest = true;
        MetadataManager metadata(&provider, nullptr, true);
        LibraryMetadataMatcher matcher(&database, &metadata, nullptr,
                                       &catalogManager, &settings);

        matcher.scrapeLibrary();
        QVERIFY(database.games().first().toMap().value("metadataTitle").toString().isEmpty());
    }

    void doesNotUseLaunchBoxWhenDisabled()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DatabaseManager database;
        QVERIFY(prepareLibraryGame(directory.path(), &database));
        SettingsManager settings(directory.path() + "/settings.ini");
        settings.setLaunchBoxEnabled(false);
        LaunchBoxCatalogManager catalogManager(directory.path());
        QVERIFY(prepareLaunchBoxCatalog(directory.path(), &catalogManager));
        FakeMetadataProvider provider;
        MetadataManager metadata(&provider, nullptr, true);
        LibraryMetadataMatcher matcher(&database, &metadata, nullptr,
                                       &catalogManager, &settings);

        matcher.scrapeLibrary();
        QVERIFY(database.games().first().toMap().value("metadataTitle").toString().isEmpty());
    }

    void primaryMatchRemainsPreferredOverLaunchBox()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DatabaseManager database;
        QVERIFY(prepareLibraryGame(directory.path(), &database));
        SettingsManager settings(directory.path() + "/settings.ini");
        settings.setLaunchBoxEnabled(true);
        LaunchBoxCatalogManager catalogManager(directory.path());
        QVERIFY(prepareLaunchBoxCatalog(directory.path(), &catalogManager));
        FakeMetadataProvider provider;
        GameMetadata primary;
        primary.provider = QStringLiteral("thegamesdb");
        primary.providerGameId = 987;
        primary.title = QStringLiteral("Banjo-Kazooie (USA) (Rev A)");
        primary.platformId = 3;
        provider.results.append(primary);
        MetadataManager metadata(&provider, nullptr, true);
        LibraryMetadataMatcher matcher(&database, &metadata, nullptr,
                                       &catalogManager, &settings);

        matcher.scrapeLibrary();
        const QVariantMap game = database.games().first().toMap();
        QCOMPARE(game.value("metadataProvider").toString(), QStringLiteral("thegamesdb"));
        QCOMPARE(game.value("metadataGameId").toInt(), 987);
    }

    void ambiguousLaunchBoxAssociationRemainsUnresolved()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DatabaseManager database;
        QVERIFY(prepareLibraryGame(directory.path(), &database));
        SettingsManager settings(directory.path() + "/settings.ini");
        settings.setLaunchBoxEnabled(true);
        LaunchBoxCatalogManager catalogManager(directory.path());
        QVERIFY(prepareLaunchBoxCatalog(directory.path(), &catalogManager, true));
        FakeMetadataProvider provider;
        MetadataManager metadata(&provider, nullptr, true);
        LibraryMetadataMatcher matcher(&database, &metadata, nullptr,
                                       &catalogManager, &settings);

        matcher.scrapeLibrary();
        QVERIFY(database.games().first().toMap().value("metadataTitle").toString().isEmpty());
    }

};

QTEST_GUILESS_MAIN(LibraryMetadataMatcherTest)
#include "LibraryMetadataMatcherTest.moc"
