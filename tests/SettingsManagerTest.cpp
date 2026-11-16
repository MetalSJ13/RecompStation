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
    void defaultsToSystemPresentationSize();
    void persistsScanOnStartupPreference();
    void persistsPreferencesInExplicitConfigFile();
    void rejectsUnsupportedPresentationSize();
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

void SettingsManagerTest::defaultsToSystemPresentationSize()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");

    SettingsManager manager(settingsPath);
    QCOMPARE(manager.preferredPresentationSize(), QStringLiteral("system"));
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
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    {
        SettingsManager manager(settingsPath);
        manager.setThemeName(QStringLiteral("ember"));
        manager.setScanOnStartup(false);
        manager.setPreferredPresentationSize(QStringLiteral("3440x1440"));
        QCOMPARE(manager.themeName(), QStringLiteral("ember"));
        QVERIFY(!manager.scanOnStartup());
        QCOMPARE(manager.preferredPresentationSize(), QStringLiteral("3440x1440"));
    }

    SettingsManager reloaded(settingsPath);
    QCOMPARE(reloaded.themeName(), QStringLiteral("ember"));
    QVERIFY(!reloaded.scanOnStartup());
    QCOMPARE(reloaded.preferredPresentationSize(), QStringLiteral("3440x1440"));
    QVERIFY(QFileInfo(settingsPath).isFile());
}

void SettingsManagerTest::rejectsUnsupportedPresentationSize()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    QSignalSpy changed(&manager, &SettingsManager::preferredPresentationSizeChanged);
    manager.setPreferredPresentationSize(QStringLiteral("800x600"));

    QCOMPARE(manager.preferredPresentationSize(), QStringLiteral("system"));
    QCOMPARE(changed.count(), 0);
}

QTEST_GUILESS_MAIN(SettingsManagerTest)
#include "SettingsManagerTest.moc"
