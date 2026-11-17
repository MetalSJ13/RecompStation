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
    void defaultsToWindowedDisplayMode();
    void persistsDisplayMode();
    void rejectsUnsupportedDisplayMode();
    void persistsScanOnStartupPreference();
    void persistsPreferencesInExplicitConfigFile();
    void rejectsUnsupportedTheme();
    void rejectsUnsupportedPresentationSize();
    void defaultsToMediumFontAndFullMotion();
    void persistsAccessibilityPreferences();
    void rejectsUnsupportedFontSize();
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

void SettingsManagerTest::defaultsToWindowedDisplayMode()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");

    SettingsManager manager(settingsPath);
    QCOMPARE(manager.displayMode(), QStringLiteral("windowed"));
}

void SettingsManagerTest::persistsDisplayMode()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    {
        SettingsManager manager(settingsPath);
        QSignalSpy changed(&manager, &SettingsManager::displayModeChanged);
        manager.setRestoreDisplayMode(QStringLiteral("maximized"));
        manager.setDisplayMode(QStringLiteral("fullscreen"));
        QCOMPARE(manager.displayMode(), QStringLiteral("fullscreen"));
        QCOMPARE(manager.restoreDisplayMode(), QStringLiteral("maximized"));
        QCOMPARE(changed.count(), 1);
    }

    SettingsManager reloaded(settingsPath);
    QCOMPARE(reloaded.displayMode(), QStringLiteral("fullscreen"));
    QCOMPARE(reloaded.restoreDisplayMode(), QStringLiteral("maximized"));
}

void SettingsManagerTest::rejectsUnsupportedDisplayMode()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    QSignalSpy changed(&manager, &SettingsManager::displayModeChanged);
    manager.setDisplayMode(QStringLiteral("exclusive"));
    QCOMPARE(manager.displayMode(), QStringLiteral("windowed"));
    QCOMPARE(changed.count(), 0);
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
        manager.setDisplayMode(QStringLiteral("maximized"));
        QCOMPARE(manager.themeName(), QStringLiteral("ember"));
        QVERIFY(!manager.scanOnStartup());
        QCOMPARE(manager.preferredPresentationSize(), QStringLiteral("3440x1440"));
        QCOMPARE(manager.displayMode(), QStringLiteral("maximized"));
        QCOMPARE(manager.restoreDisplayMode(), QStringLiteral("maximized"));
    }

    SettingsManager reloaded(settingsPath);
    QCOMPARE(reloaded.themeName(), QStringLiteral("ember"));
    QVERIFY(!reloaded.scanOnStartup());
    QCOMPARE(reloaded.preferredPresentationSize(), QStringLiteral("3440x1440"));
    QCOMPARE(reloaded.displayMode(), QStringLiteral("maximized"));
    QCOMPARE(reloaded.restoreDisplayMode(), QStringLiteral("maximized"));
    QVERIFY(QFileInfo(settingsPath).isFile());
}

void SettingsManagerTest::rejectsUnsupportedTheme()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    QSignalSpy changed(&manager, &SettingsManager::themeNameChanged);
    manager.setThemeName(QStringLiteral("unsupported-theme"));

    QCOMPARE(manager.themeName(), QStringLiteral("midnight-violet"));
    QCOMPARE(changed.count(), 0);
}

void SettingsManagerTest::defaultsToMediumFontAndFullMotion()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    QCOMPARE(manager.fontSize(), QStringLiteral("medium"));
    QVERIFY(!manager.reducedMotion());
}

void SettingsManagerTest::persistsAccessibilityPreferences()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    {
        SettingsManager manager(settingsPath);
        QSignalSpy fontChanged(&manager, &SettingsManager::fontSizeChanged);
        QSignalSpy motionChanged(&manager, &SettingsManager::reducedMotionChanged);

        manager.setFontSize(QStringLiteral("large"));
        manager.setReducedMotion(true);

        QCOMPARE(manager.fontSize(), QStringLiteral("large"));
        QVERIFY(manager.reducedMotion());
        QCOMPARE(fontChanged.count(), 1);
        QCOMPARE(motionChanged.count(), 1);
    }

    SettingsManager reloaded(settingsPath);
    QCOMPARE(reloaded.fontSize(), QStringLiteral("large"));
    QVERIFY(reloaded.reducedMotion());
}

void SettingsManagerTest::rejectsUnsupportedFontSize()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    QSignalSpy changed(&manager, &SettingsManager::fontSizeChanged);
    manager.setFontSize(QStringLiteral("extra-large"));

    QCOMPARE(manager.fontSize(), QStringLiteral("medium"));
    QCOMPARE(changed.count(), 0);
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
