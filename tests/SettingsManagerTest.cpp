#include <QtTest>

#include "core/SettingsManager.h"

#include <QSettings>

class SettingsManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void defaultsToFullscreen();
    void persistsSupportedDisplayModes();
    void rejectsUnsupportedDisplayModes();
};

void SettingsManagerTest::init()
{
    QCoreApplication::setOrganizationName(QStringLiteral("RecompStationTests"));
    QCoreApplication::setApplicationName(QStringLiteral("SettingsManagerTests"));
    QSettings().clear();
}

void SettingsManagerTest::defaultsToFullscreen()
{
    SettingsManager manager;
    QCOMPARE(manager.displayMode(), QStringLiteral("fullscreen"));
}

void SettingsManagerTest::persistsSupportedDisplayModes()
{
    SettingsManager manager;
    QSignalSpy changed(&manager, &SettingsManager::displayModeChanged);

    manager.setDisplayMode(QStringLiteral("windowed"));
    QCOMPARE(manager.displayMode(), QStringLiteral("windowed"));
    QCOMPARE(changed.count(), 1);

    SettingsManager reloaded;
    QCOMPARE(reloaded.displayMode(), QStringLiteral("windowed"));

    reloaded.setDisplayMode(QStringLiteral("borderless"));
    QCOMPARE(reloaded.displayMode(), QStringLiteral("borderless"));
    QCOMPARE(QSettings().value(QStringLiteral("display/mode")).toString(),
             QStringLiteral("borderless"));
}

void SettingsManagerTest::rejectsUnsupportedDisplayModes()
{
    SettingsManager manager;
    QSignalSpy changed(&manager, &SettingsManager::displayModeChanged);

    manager.setDisplayMode(QStringLiteral("unsupported-resolution"));
    QCOMPARE(manager.displayMode(), QStringLiteral("fullscreen"));
    QCOMPARE(changed.count(), 0);
}

QTEST_GUILESS_MAIN(SettingsManagerTest)
#include "SettingsManagerTest.moc"
