#include <QtTest>

#include "core/SettingsManager.h"

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFileInfo>

class SettingsManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void defaultsToScanningOnStartup();
    void persistsScanOnStartupPreference();
    void persistsPreferencesInExplicitConfigFile();
};

void SettingsManagerTest::init()
{
    QCoreApplication::setOrganizationName(QStringLiteral("RecompStationSettingsTests"));
    QCoreApplication::setApplicationName(QStringLiteral("SettingsManagerTests"));
    QSettings().clear();
}

void SettingsManagerTest::defaultsToScanningOnStartup()
{
    SettingsManager manager;
    QVERIFY(manager.scanOnStartup());
}

void SettingsManagerTest::persistsScanOnStartupPreference()
{
    SettingsManager manager;
    QSignalSpy changed(&manager, &SettingsManager::scanOnStartupChanged);

    manager.setScanOnStartup(false);
    QVERIFY(!manager.scanOnStartup());
    QCOMPARE(changed.count(), 1);

    SettingsManager reloaded;
    QVERIFY(!reloaded.scanOnStartup());

    reloaded.setScanOnStartup(true);
    QVERIFY(reloaded.scanOnStartup());
    QCOMPARE(QSettings().value(QStringLiteral("library/scanOnStartup")).toBool(), true);
    QSettings().clear();
}


void SettingsManagerTest::persistsPreferencesInExplicitConfigFile()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");

    {
        SettingsManager manager(settingsPath);
        manager.setThemeName(QStringLiteral("ember"));
        manager.setScanOnStartup(false);
        QCOMPARE(manager.themeName(), QStringLiteral("ember"));
        QVERIFY(!manager.scanOnStartup());
    }

    SettingsManager reloaded(settingsPath);
    QCOMPARE(reloaded.themeName(), QStringLiteral("ember"));
    QVERIFY(!reloaded.scanOnStartup());
    QVERIFY(QFileInfo(settingsPath).isFile());
}

QTEST_GUILESS_MAIN(SettingsManagerTest)
#include "SettingsManagerTest.moc"
