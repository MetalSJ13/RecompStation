#include <QtTest>

#include "database/DatabaseManager.h"

#include <QTemporaryDir>

class DatabaseManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void associatesMetadataWithSpecificLocalGame()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        const QString databasePath =
            temporaryDirectory.path() + "/database/library.sqlite";
        QVERIFY2(database.initialize(databasePath),
                 qPrintable(database.lastError()));

        QVERIFY(database.registerGame(
            QStringLiteral("Banjo-Kazooie"),
            QStringLiteral("N64"),
            temporaryDirectory.path() + "/games/N64/Banjo-Kazooie.z64"));

        const QVariantList before = database.games(QStringLiteral("N64"));
        QCOMPARE(before.size(), 1);
        const int gameId = before.first().toMap().value("id").toInt();
        QVERIFY(gameId > 0);
        QVERIFY(before.first().toMap().value("metadataTitle").toString().isEmpty());

        QVERIFY2(database.associateMetadata(
                     gameId,
                     QStringLiteral("thegamesdb"),
                     1234,
                     QStringLiteral("Banjo-Kazooie (North American release)"),
                     QStringLiteral("A bear and bird adventure."),
                     QStringLiteral("1998-06-29"),
                     3,
                     QStringLiteral("E - Everyone")),
                 qPrintable(database.lastError()));

        const QVariantMap saved = database.games(QStringLiteral("N64"))
                                      .first().toMap();
        QCOMPARE(saved.value("id").toInt(), gameId);
        QCOMPARE(saved.value("metadataProvider").toString(),
                 QStringLiteral("thegamesdb"));
        QCOMPARE(saved.value("metadataGameId").toInt(), 1234);
        QCOMPARE(saved.value("metadataTitle").toString(),
                 QStringLiteral("Banjo-Kazooie (North American release)"));
        QCOMPARE(saved.value("title").toString(),
                 QStringLiteral("Banjo-Kazooie"));
        QCOMPARE(saved.value("metadataOverview").toString(),
                 QStringLiteral("A bear and bird adventure."));
        QCOMPARE(saved.value("metadataReleaseDate").toString(),
                 QStringLiteral("1998-06-29"));
        QCOMPARE(saved.value("metadataPlatformId").toInt(), 3);
        QCOMPARE(saved.value("metadataRating").toString(),
                 QStringLiteral("E - Everyone"));
    }

    void reassociatingMetadataReplacesThePreviousMatch()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        QVERIFY2(database.initialize(
                     temporaryDirectory.path() + "/library.sqlite"),
                 qPrintable(database.lastError()));
        QVERIFY(database.registerGame(
            QStringLiteral("Game"), QStringLiteral("PS1"),
            temporaryDirectory.path() + "/Game.cue"));

        const int gameId = database.games().first().toMap().value("id").toInt();
        QVERIFY(database.associateMetadata(
            gameId, QStringLiteral("thegamesdb"), 100,
            QStringLiteral("First Match"), {}, {}, 7, {}));
        QVERIFY(database.associateMetadata(
            gameId, QStringLiteral("thegamesdb"), 200,
            QStringLiteral("Corrected Match"), {}, {}, 7, {}));

        const QVariantMap saved = database.games().first().toMap();
        QCOMPARE(saved.value("metadataGameId").toInt(), 200);
        QCOMPARE(saved.value("metadataTitle").toString(),
                 QStringLiteral("Corrected Match"));
    }

    void rejectsAssociationForUnknownLocalGame()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        DatabaseManager database;
        QVERIFY(database.initialize(
            temporaryDirectory.path() + "/library.sqlite"));

        QVERIFY(!database.associateMetadata(
            999, QStringLiteral("thegamesdb"), 123,
            QStringLiteral("Unknown Game"), {}, {}, 3, {}));
        QVERIFY(database.lastError().contains(
            QStringLiteral("no longer exists")));
    }
};

QTEST_GUILESS_MAIN(DatabaseManagerTest)
#include "DatabaseManagerTest.moc"
