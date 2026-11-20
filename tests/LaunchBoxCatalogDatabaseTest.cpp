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
                    <Game><DatabaseID>164</DatabaseID><Name>Banjo-Kazooie</Name>
                        <Platform>Nintendo DS</Platform></Game>
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
        QCOMPARE(catalog.gameCount(), 3);
        QVERIFY(!catalog.indexedAt().isEmpty());

        const auto associations = catalog.findFileAssociations(
            "N64", "Banjo-Kazooie (USA) (Rev A).z64");
        QCOMPARE(associations.size(), 1);
        QCOMPARE(associations.first().gameName,
                 QStringLiteral("Banjo-Kazooie (USA) (Rev A)"));

        const auto matches = catalog.findGamesByTitle("N64", "Banjo-Kazooie");
        QCOMPARE(matches.size(), 2);
        QCOMPARE(matches.at(0).databaseId, 162);
        QCOMPARE(matches.at(0).releaseYear, 1998);
        QCOMPARE(matches.at(0).developer, QStringLiteral("Rare"));
        QCOMPARE(matches.at(1).databaseId, 163);

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
        QCOMPARE(catalog.gameCount(), 3);

        QVERIFY(writeCatalog(directory.path(), true));
        QVERIFY(!catalog.rebuildFromXmlFiles(
            directory.path() + "/Platforms.xml",
            directory.path() + "/Files.xml",
            directory.path() + "/Metadata.xml"));
        QVERIFY(!catalog.lastError().isEmpty());
        QCOMPARE(catalog.gameCount(), 3);
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
        QCOMPARE(catalog.gameCount(), 3);
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
        QCOMPARE(catalog.gameCount(), 3);
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
