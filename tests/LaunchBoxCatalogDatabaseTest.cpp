#include <QtTest>

#include "metadata/LaunchBoxCatalogDatabase.h"
#include "metadata/LaunchBoxCatalogMatcher.h"

#include <QFile>
#include <QTemporaryDir>
#include <atomic>

class LaunchBoxCatalogDatabaseTest : public QObject
{
    Q_OBJECT

private:
    static bool writeFile(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        return file.write(contents) == contents.size();
    }

    static bool writeCatalog(const QString &root, bool malformedMetadata = false)
    {
        const QByteArray platforms = R"xml(
            <LaunchBox>
                <Platform><Name>Nintendo 64</Name><Alternate>N64</Alternate></Platform>
                <Platform><Name>Sony Playstation</Name><Alternate>PS1</Alternate></Platform>
            </LaunchBox>
        )xml";
        const QByteArray files = R"xml(
            <LaunchBox>
                <File><Platform>Nintendo 64</Platform>
                    <FileName>Banjo-Kazooie (USA) (Rev A)</FileName>
                    <GameName>Banjo-Kazooie (USA) (Rev A)</GameName></File>
                <File><Platform>Nintendo 64</Platform>
                    <FileName>Banjo-Kazooie (USA) (Rev A)</FileName>
                    <GameName>Banjo-Kazooie (USA) (Rev A)</GameName></File>
            </LaunchBox>
        )xml";
        const QByteArray metadata = malformedMetadata
            ? QByteArray("<LaunchBox><Game><DatabaseID>broken</LaunchBox>")
            : QByteArray(R"xml(
                <LaunchBox>
                    <Game><DatabaseID>162</DatabaseID><Name>Banjo-Kazooie</Name>
                        <Platform>Nintendo 64</Platform><ReleaseYear>1998</ReleaseYear>
                        <Overview>Bear and bird adventure.</Overview><Developer>Rare</Developer>
                        <Publisher>Nintendo</Publisher></Game>
                    <Game><DatabaseID>163</DatabaseID><Name>Banjo-Kazooie</Name>
                        <Platform>Nintendo 64</Platform><ReleaseType>ROM Hack</ReleaseType></Game>
                    <GameImage><DatabaseID>162</DatabaseID>
                        <FileName>banjo-front-na.jpg</FileName>
                        <Type>Box - Front</Type><Region>North America</Region>
                        <CRC32>2977056290</CRC32></GameImage>
                    <GameImage><DatabaseID>162</DatabaseID>
                        <FileName>banjo-gameplay.png</FileName>
                        <Type>Screenshot - Gameplay</Type><Region>World</Region></GameImage>
                    <GameImage><DatabaseID>163</DatabaseID>
                        <FileName>romhack-cover.jpg</FileName>
                        <Type>Box - Front</Type><Region>Europe</Region></GameImage>
                    <Game><DatabaseID>164</DatabaseID><Name>Banjo-Kazooie</Name>
                        <Platform>Nintendo DS</Platform></Game>
                    <Game><DatabaseID>165</DatabaseID><Name>Banjo-Kazooie (Rev B)</Name>
                        <Platform>Nintendo 64</Platform></Game>
                </LaunchBox>
            )xml");
        return writeFile(root + "/Platforms.xml", platforms)
            && writeFile(root + "/Files.xml", files)
            && writeFile(root + "/Metadata.xml", metadata);
    }

private slots:
    void buildsIndexedCatalogAndSupportsIndexedLookups()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(writeCatalog(directory.path()));

        // Verify the exact fixture file independently before exercising the
        // catalog's SQL insertion path.
        QList<LaunchBoxGameImageRecord> parsedImages;
        QString imageParseError;
        QVERIFY2(LaunchBoxXmlParser::parseGameImagesFile(
                     directory.path() + "/Metadata.xml",
                     [&](const LaunchBoxGameImageRecord &image) {
                         parsedImages.append(image);
                         return true;
                     }, &imageParseError), qPrintable(imageParseError));
        QCOMPARE(parsedImages.size(), 3);

        LaunchBoxCatalogDatabase catalog;
        QVERIFY2(catalog.open(directory.path() + "/launchbox.sqlite"),
                 qPrintable(catalog.lastError()));
        QVERIFY2(catalog.rebuildFromXmlFiles(
                     directory.path() + "/Platforms.xml",
                     directory.path() + "/Files.xml",
                     directory.path() + "/Metadata.xml"),
                 qPrintable(catalog.lastError()));

        QCOMPARE(catalog.platformCount(), 2);
        QCOMPARE(catalog.fileAssociationCount(), 1);
        QCOMPARE(catalog.gameCount(), 4);
        QCOMPARE(catalog.gameImageCount(), 3);
        QVERIFY(!catalog.indexedAt().isEmpty());

        const auto associations = catalog.findFileAssociations(
            "N64", "Banjo-Kazooie (USA) (Rev A).z64");
        QCOMPARE(associations.size(), 1);
        QCOMPARE(associations.first().gameName,
                 QStringLiteral("Banjo-Kazooie (USA) (Rev A)"));
        QVERIFY(catalog.findFileAssociations(
            "N64", "Banjo-Kazooie (Europe) (Rev A).z64").isEmpty());
        QVERIFY(catalog.findFileAssociations(
            "N64", "Banjo-Kazooie (USA) (Rev B).z64").isEmpty());

        const auto matches = catalog.findGamesByTitle("N64", "Banjo-Kazooie");
        QCOMPARE(matches.size(), 2);
        QCOMPARE(matches.at(0).databaseId, 162);
        QCOMPARE(matches.at(0).releaseYear, 1998);
        QCOMPARE(matches.at(0).developer, QStringLiteral("Rare"));
        QCOMPARE(matches.at(1).databaseId, 163);

        const auto revisionInsensitive = catalog.findGamesByRevisionInsensitiveTitle(
            "N64", "Banjo-Kazooie (USA) (Rev A)");
        QCOMPARE(revisionInsensitive.size(), 3);
        QCOMPARE(revisionInsensitive.at(0).databaseId, 162);
        QCOMPARE(revisionInsensitive.at(1).databaseId, 163);
        QCOMPARE(revisionInsensitive.at(2).databaseId, 165);

        const auto images = catalog.imagesByDatabaseId(162);
        QCOMPARE(images.size(), 2);
        QCOMPARE(images.at(0).databaseId, 162);
        QCOMPARE(images.at(0).fileName, QStringLiteral("banjo-front-na.jpg"));
        QCOMPARE(images.at(0).type, QStringLiteral("Box - Front"));
        QCOMPARE(images.at(0).region, QStringLiteral("North America"));
        QCOMPARE(images.at(0).crc32, QStringLiteral("2977056290"));
        QCOMPARE(images.at(1).fileName, QStringLiteral("banjo-gameplay.png"));
        QVERIFY(catalog.imagesByDatabaseId(999).isEmpty());

        LaunchBoxGameRecord byId;
        QVERIFY(catalog.gameByDatabaseId(164, &byId));
        QCOMPARE(byId.platform, QStringLiteral("Nintendo DS"));
        QVERIFY(!catalog.gameByDatabaseId(999, &byId));
        QVERIFY(catalog.findGamesByTitle("PS1", "Banjo-Kazooie").isEmpty());
        QVERIFY(catalog.findGamesByTitle("64DD", "Banjo-Kazooie").isEmpty());
    }

    void failedRebuildPreservesPreviouslyIndexedCatalog()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(writeCatalog(directory.path()));

        LaunchBoxCatalogDatabase catalog;
        QVERIFY(catalog.open(directory.path() + "/launchbox.sqlite"));
        QVERIFY(catalog.rebuildFromXmlFiles(
            directory.path() + "/Platforms.xml",
            directory.path() + "/Files.xml",
            directory.path() + "/Metadata.xml"));
        const QString originalIndexedAt = catalog.indexedAt();
        QCOMPARE(catalog.gameCount(), 4);

        QVERIFY(writeCatalog(directory.path(), true));
        QVERIFY(!catalog.rebuildFromXmlFiles(
            directory.path() + "/Platforms.xml",
            directory.path() + "/Files.xml",
            directory.path() + "/Metadata.xml"));
        QVERIFY(!catalog.lastError().isEmpty());
        QCOMPARE(catalog.gameCount(), 4);
        QCOMPARE(catalog.gameImageCount(), 3);
        QCOMPARE(catalog.indexedAt(), originalIndexedAt);
        QCOMPARE(catalog.findGamesByTitle("N64", "Banjo-Kazooie").size(), 2);
    }

    void cancellationRollsBackTheCatalogTransaction()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(writeCatalog(directory.path()));

        LaunchBoxCatalogDatabase catalog;
        QVERIFY(catalog.open(directory.path() + "/launchbox.sqlite"));
        QVERIFY(catalog.rebuildFromXmlFiles(
            directory.path() + "/Platforms.xml",
            directory.path() + "/Files.xml",
            directory.path() + "/Metadata.xml"));
        const QString originalIndexedAt = catalog.indexedAt();

        std::atomic_bool cancelled{true};
        QVERIFY(!catalog.rebuildFromXmlFiles(
            directory.path() + "/Platforms.xml",
            directory.path() + "/Files.xml",
            directory.path() + "/Metadata.xml", &cancelled));
        QVERIFY(catalog.lastError().contains(QStringLiteral("cancelled")));
        QCOMPARE(catalog.gameCount(), 4);
        QCOMPARE(catalog.gameImageCount(), 3);
        QCOMPARE(catalog.indexedAt(), originalIndexedAt);
    }

    void rejectsMissingSourceWithoutDestroyingCatalog()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(writeCatalog(directory.path()));

        LaunchBoxCatalogDatabase catalog;
        QVERIFY(catalog.open(directory.path() + "/launchbox.sqlite"));
        QVERIFY(catalog.rebuildFromXmlFiles(
            directory.path() + "/Platforms.xml",
            directory.path() + "/Files.xml",
            directory.path() + "/Metadata.xml"));
        QVERIFY(!catalog.rebuildFromXmlFiles(
            directory.path() + "/Platforms.xml",
            directory.path() + "/missing.xml",
            directory.path() + "/Metadata.xml"));
        QCOMPARE(catalog.gameCount(), 4);
    }

    void requiresOpenDatabaseAndValidOutput()
    {
        LaunchBoxCatalogDatabase catalog;
        QVERIFY(!catalog.rebuildFromXmlFiles({}, {}, {}));
        QVERIFY(!catalog.gameByDatabaseId(1, nullptr));
    }
};

QTEST_GUILESS_MAIN(LaunchBoxCatalogDatabaseTest)
#include "LaunchBoxCatalogDatabaseTest.moc"
