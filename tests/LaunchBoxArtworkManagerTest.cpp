#include "../src/core/LaunchBoxArtworkManager.h"
#include "../src/metadata/LaunchBoxXmlParser.h"

#include <QtTest>

class LaunchBoxArtworkManagerTest : public QObject
{
    Q_OBJECT
private slots:
    void buildsOnlyTrustedCdnUrls()
    {
        QCOMPARE(LaunchBoxArtworkManager::imageUrl("cover.png"),
                 QString("https://images.launchbox-app.com/cover.png"));
        QVERIFY(LaunchBoxArtworkManager::imageUrl("../cover.png").isEmpty());
        QVERIFY(LaunchBoxArtworkManager::imageUrl("folder/cover.png").isEmpty());
    }

    void prefersNorthAmericanCoverAndGameplayScreenshots()
    {
        QList<LaunchBoxGameImageRecord> images{
            {42, "europe.png", "Box - Front", "Europe", ""},
            {42, "usa.png", "Box - Front", "North America", ""},
            {42, "logo.png", "Clear Logo", "North America", ""},
            {42, "gameplay1.png", "Screenshot - Gameplay", "World", ""},
            {42, "title.png", "Screenshot - Game Title", "North America", ""},
            {42, "gameplay2.png", "Screenshot - Gameplay", "North America", ""}
        };
        const auto selected = LaunchBoxArtworkManager::selectRequests(7, 42, images, true, 1);
        QCOMPARE(selected.size(), 2);
        QCOMPARE(selected.at(0).fileName, QString("usa.png"));
        QCOMPARE(selected.at(1).fileName, QString("gameplay2.png"));
    }

    void skipsCoverWhenAlreadyPresent()
    {
        QList<LaunchBoxGameImageRecord> images{
            {42, "cover.png", "Box - Front", "North America", ""}
        };
        QVERIFY(LaunchBoxArtworkManager::selectRequests(7, 42, images, false).isEmpty());
    }
};

QTEST_GUILESS_MAIN(LaunchBoxArtworkManagerTest)
#include "LaunchBoxArtworkManagerTest.moc"
