#include <QtTest>

#include "core/SettingsManager.h"

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QUrl>

class SettingsManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void defaultsToScanningOnStartup();
    void defaultsToSystemPresentationSize();
    void interfaceSoundsDefaultOnAndPersist();
    void defaultsToFullscreenDisplayMode();
    void persistsDisplayMode();
    void rejectsUnsupportedDisplayMode();
    void persistsScanOnStartupPreference();
    void defaultsToUpdateChecksDisabledAndPromptPending();
    void persistsUpdatePreferenceAndPromptCompletion();
    void defaultsToAutomaticMetadataMatchingEnabled();
    void persistsAutomaticMetadataMatchingPreference();
    void defaultsToAutomaticArtworkDownloadsEnabled();
    void persistsAutomaticArtworkDownloadsPreference();
    void defaultsToTheGamesDbEnabled();
    void persistsTheGamesDbEnabledPreference();
    void defaultsToLaunchBoxDisabled();
    void persistsLaunchBoxEnabledPreference();
    void persistsLaunchBoxLastUpdatedTimestamp();
    void controllerProfilesDefaultPerGuidAndPersistIndependently();
    void rejectsInvalidControllerProfiles();
    void persistsTriggerControllerBindings();
    void persistsStickClickControllerBindings();
    void persistsPreferencesInExplicitConfigFile();
    void rejectsUnsupportedTheme();
    void rejectsUnsupportedPresentationSize();
    void defaultsToMediumFontAndFullMotion();
    void persistsAccessibilityPreferences();
    void rejectsUnsupportedFontSize();
    void defaultsToCuratedLibraryPresentation();
    void persistsCarouselLibraryView();
    void persistsShelvesLibraryView();
    void defaultControllerProfileMapsDetailsToY();
    void legacyControllerProfileGainsDetailsDefault();
    void rejectsUnknownLibraryView();
    void persistsLibraryPresentationPreferences();
    void rejectsUnsupportedLibraryPresentationPreferences();
    void migratesRetiredEmberThemeToElectricBlue();
    void acceptsAuroraAndCarbonThemes();
    void defaultsToAutomaticInterfaceScale();
    void persistsInterfaceScale();
    void rejectsUnsupportedInterfaceScale();
    void autoInterfaceScaleFollowsScreenHeight();
    void defaultsToLaunchBoxFirstProviderOrder();
    void persistsProviderOrderAndRejectsUnknownValues();
    void savesTheGamesDbKeyPrivately();
    void rejectsImplausibleTheGamesDbKeys();
    void environmentTheGamesDbKeyTakesPriority();
    void resetPreferencesKeepsLibraryAndKey();
};

void SettingsManagerTest::init()
{
    QCoreApplication::setOrganizationName(QStringLiteral("RecompStationSettingsTests"));
    QCoreApplication::setApplicationName(QStringLiteral("SettingsManagerTests"));
    QSettings().clear();
    qunsetenv("RECOMPSTATION_TGDB_API_KEY");
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

void SettingsManagerTest::interfaceSoundsDefaultOnAndPersist()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    {
        SettingsManager manager(settingsPath);
        QVERIFY(manager.soundsEnabled());
        QCOMPARE(manager.soundVolume(), 60);
        QSignalSpy volumeSpy(&manager, &SettingsManager::soundVolumeChanged);
        manager.setSoundsEnabled(false);
        manager.setSoundVolume(140);  // Clamped.
        QCOMPARE(manager.soundVolume(), 100);
        QCOMPARE(volumeSpy.size(), 1);
    }
    SettingsManager reloaded(settingsPath);
    QVERIFY(!reloaded.soundsEnabled());
    QCOMPARE(reloaded.soundVolume(), 100);
}

void SettingsManagerTest::defaultsToFullscreenDisplayMode()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");

    SettingsManager manager(settingsPath);
    QCOMPARE(manager.displayMode(), QStringLiteral("fullscreen"));
}

void SettingsManagerTest::persistsDisplayMode()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    // Start from a saved non-fullscreen mode so this test exercises an
    // actual transition to fullscreen with the new fullscreen default.
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("display/windowMode"), QStringLiteral("windowed"));
        settings.sync();
        QCOMPARE(settings.status(), QSettings::NoError);
    }

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
    QCOMPARE(manager.displayMode(), QStringLiteral("fullscreen"));
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


void SettingsManagerTest::defaultsToUpdateChecksDisabledAndPromptPending()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");

    SettingsManager manager(settingsPath);
    QVERIFY(!manager.automaticUpdateChecksEnabled());
    QVERIFY(!manager.updatePreferencePromptCompleted());
}

void SettingsManagerTest::persistsUpdatePreferenceAndPromptCompletion()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    {
        SettingsManager manager(settingsPath);
        QSignalSpy enabledChanged(&manager, &SettingsManager::automaticUpdateChecksEnabledChanged);
        QSignalSpy completedChanged(&manager, &SettingsManager::updatePreferencePromptCompletedChanged);

        manager.completeUpdatePreferencePrompt(false);
        QVERIFY(!manager.automaticUpdateChecksEnabled());
        QVERIFY(manager.updatePreferencePromptCompleted());
        QCOMPARE(enabledChanged.count(), 0);
        QCOMPARE(completedChanged.count(), 1);
    }

    {
        SettingsManager reloaded(settingsPath);
        QVERIFY(!reloaded.automaticUpdateChecksEnabled());
        QVERIFY(reloaded.updatePreferencePromptCompleted());
        reloaded.setAutomaticUpdateChecksEnabled(true);
        QVERIFY(reloaded.automaticUpdateChecksEnabled());
        QVERIFY(reloaded.updatePreferencePromptCompleted());
    }

    SettingsManager finalReload(settingsPath);
    QVERIFY(finalReload.automaticUpdateChecksEnabled());
    QVERIFY(finalReload.updatePreferencePromptCompleted());
}

void SettingsManagerTest::defaultsToAutomaticMetadataMatchingEnabled()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");

    SettingsManager manager(settingsPath);
    QVERIFY(manager.automaticMetadataMatchingEnabled());
}

void SettingsManagerTest::persistsAutomaticMetadataMatchingPreference()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    {
        SettingsManager manager(settingsPath);
        QSignalSpy changed(&manager, &SettingsManager::automaticMetadataMatchingEnabledChanged);
        manager.setAutomaticMetadataMatchingEnabled(false);
        QVERIFY(!manager.automaticMetadataMatchingEnabled());
        QCOMPARE(changed.count(), 1);
    }

    SettingsManager reloaded(settingsPath);
    QVERIFY(!reloaded.automaticMetadataMatchingEnabled());
    QSignalSpy changed(&reloaded, &SettingsManager::automaticMetadataMatchingEnabledChanged);
    reloaded.setAutomaticMetadataMatchingEnabled(false);
    QCOMPARE(changed.count(), 0);
    reloaded.setAutomaticMetadataMatchingEnabled(true);
    QVERIFY(reloaded.automaticMetadataMatchingEnabled());
    QCOMPARE(changed.count(), 1);
}

void SettingsManagerTest::defaultsToAutomaticArtworkDownloadsEnabled()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");

    SettingsManager manager(settingsPath);
    QVERIFY(manager.automaticArtworkDownloadsEnabled());
}

void SettingsManagerTest::persistsAutomaticArtworkDownloadsPreference()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    {
        SettingsManager manager(settingsPath);
        QSignalSpy changed(&manager, &SettingsManager::automaticArtworkDownloadsEnabledChanged);
        manager.setAutomaticArtworkDownloadsEnabled(false);
        QVERIFY(!manager.automaticArtworkDownloadsEnabled());
        QCOMPARE(changed.count(), 1);
    }

    SettingsManager reloaded(settingsPath);
    QVERIFY(!reloaded.automaticArtworkDownloadsEnabled());
    QSignalSpy changed(&reloaded, &SettingsManager::automaticArtworkDownloadsEnabledChanged);
    reloaded.setAutomaticArtworkDownloadsEnabled(false);
    QCOMPARE(changed.count(), 0);
    reloaded.setAutomaticArtworkDownloadsEnabled(true);
    QVERIFY(reloaded.automaticArtworkDownloadsEnabled());
    QCOMPARE(changed.count(), 1);
}

void SettingsManagerTest::defaultsToTheGamesDbEnabled()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");

    SettingsManager manager(settingsPath);
    QVERIFY(manager.theGamesDbEnabled());
}

void SettingsManagerTest::persistsTheGamesDbEnabledPreference()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    {
        SettingsManager manager(settingsPath);
        QSignalSpy changed(&manager, &SettingsManager::theGamesDbEnabledChanged);
        manager.setTheGamesDbEnabled(false);
        QVERIFY(!manager.theGamesDbEnabled());
        QCOMPARE(changed.count(), 1);
    }

    SettingsManager reloaded(settingsPath);
    QVERIFY(!reloaded.theGamesDbEnabled());
    QSignalSpy changed(&reloaded, &SettingsManager::theGamesDbEnabledChanged);
    reloaded.setTheGamesDbEnabled(false);
    QCOMPARE(changed.count(), 0);
    reloaded.setTheGamesDbEnabled(true);
    QVERIFY(reloaded.theGamesDbEnabled());
    QCOMPARE(changed.count(), 1);
}


void SettingsManagerTest::defaultsToLaunchBoxDisabled()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");

    SettingsManager manager(settingsPath);
    QVERIFY(!manager.launchBoxEnabled());
}

void SettingsManagerTest::persistsLaunchBoxEnabledPreference()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    {
        SettingsManager manager(settingsPath);
        QSignalSpy changed(&manager, &SettingsManager::launchBoxEnabledChanged);
        manager.setLaunchBoxEnabled(true);
        QVERIFY(manager.launchBoxEnabled());
        QCOMPARE(changed.count(), 1);
    }

    SettingsManager reloaded(settingsPath);
    QVERIFY(reloaded.launchBoxEnabled());
    QSignalSpy changed(&reloaded, &SettingsManager::launchBoxEnabledChanged);
    reloaded.setLaunchBoxEnabled(true);
    QCOMPARE(changed.count(), 0);
    reloaded.setLaunchBoxEnabled(false);
    QVERIFY(!reloaded.launchBoxEnabled());
    QCOMPARE(changed.count(), 1);
}

void SettingsManagerTest::persistsLaunchBoxLastUpdatedTimestamp()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    const QString timestamp = QStringLiteral("2026-11-20T14:30:00-05:00");
    {
        SettingsManager manager(settingsPath);
        QSignalSpy changed(&manager, &SettingsManager::launchBoxLastUpdatedChanged);
        manager.setLaunchBoxLastUpdated(timestamp);
        QCOMPARE(manager.launchBoxLastUpdated(), timestamp);
        QCOMPARE(changed.count(), 1);
        manager.setLaunchBoxLastUpdated(QString());
        QCOMPARE(manager.launchBoxLastUpdated(), timestamp);
        QCOMPARE(changed.count(), 1);
    }

    SettingsManager reloaded(settingsPath);
    QCOMPARE(reloaded.launchBoxLastUpdated(), timestamp);
}

void SettingsManagerTest::controllerProfilesDefaultPerGuidAndPersistIndependently()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));
    const QString xboxGuid = QStringLiteral("030000005e0400008e02000000000000");
    const QString secondGuid = QStringLiteral("030000005e0400008e02000000000001");

    SettingsManager manager(settingsPath);
    const QVariantMap defaults = manager.controllerProfile(xboxGuid);
    QVERIFY(!defaults.isEmpty());
    QCOMPARE(defaults.value(QStringLiteral("select")).toList().first().toString(),
             QStringLiteral("a"));

    QVariantMap customized = defaults;
    customized[QStringLiteral("select")] = QVariantList{QStringLiteral("x")};
    QVERIFY(manager.saveControllerProfile(xboxGuid, customized));
    QVERIFY(manager.controllerProfileIds().contains(xboxGuid));

    const QVariantMap secondProfile = manager.controllerProfile(secondGuid);
    QCOMPARE(secondProfile.value(QStringLiteral("select")).toList().first().toString(),
             QStringLiteral("a"));

    SettingsManager reloaded(settingsPath);
    QCOMPARE(reloaded.controllerProfile(xboxGuid).value(QStringLiteral("select")).toList().first().toString(),
             QStringLiteral("x"));
    QCOMPARE(reloaded.controllerProfile(secondGuid).value(QStringLiteral("select")).toList().first().toString(),
             QStringLiteral("a"));

    QVERIFY(reloaded.resetControllerProfile(xboxGuid));
    QCOMPARE(reloaded.controllerProfile(xboxGuid).value(QStringLiteral("select")).toList().first().toString(),
             QStringLiteral("a"));
    QVERIFY(!reloaded.controllerProfileIds().contains(xboxGuid));
    // Merely reading an untouched default profile must not create a saved
    // profile entry; the second controller remains on its implicit defaults.
    QVERIFY(!reloaded.controllerProfileIds().contains(secondGuid));
}

void SettingsManagerTest::rejectsInvalidControllerProfiles()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    QVERIFY(manager.controllerProfile(QStringLiteral("not-a-guid")).isEmpty());
    QVERIFY(!manager.saveControllerProfile(QStringLiteral("not-a-guid"), {}));

    const QString guid = QStringLiteral("030000005e0400008e02000000000000");
    QVariantMap invalid = manager.controllerProfile(guid);
    invalid[QStringLiteral("select")] = QVariantList{QStringLiteral("unknown-button")};
    QVERIFY(!manager.saveControllerProfile(guid, invalid));
    QVERIFY(!manager.saveControllerProfile(guid, {}));
}

void SettingsManagerTest::persistsTriggerControllerBindings()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    const QString guid = QStringLiteral("0123456789abcdef0123456789abcdef");
    SettingsManager manager(settingsPath);
    QVariantMap profile = manager.controllerProfile(guid);
    profile[QStringLiteral("previousSection")] = QVariantList{QStringLiteral("lefttrigger")};
    profile[QStringLiteral("nextSection")] = QVariantList{QStringLiteral("righttrigger")};
    QVERIFY(manager.saveControllerProfile(guid, profile));

    const QVariantMap reloaded = manager.controllerProfile(guid);
    QCOMPARE(reloaded.value(QStringLiteral("previousSection")).toStringList(),
             QStringList{QStringLiteral("lefttrigger")});
    QCOMPARE(reloaded.value(QStringLiteral("nextSection")).toStringList(),
             QStringList{QStringLiteral("righttrigger")});
}

void SettingsManagerTest::persistsStickClickControllerBindings()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));
    const QString guid = QStringLiteral("abcdef0123456789abcdef0123456789");
    SettingsManager manager(settingsPath);
    QVariantMap profile = manager.controllerProfile(guid);
    profile[QStringLiteral("select")] = QVariantList{QStringLiteral("leftstick-click")};
    profile[QStringLiteral("back")] = QVariantList{QStringLiteral("rightstick-click")};
    QVERIFY(manager.saveControllerProfile(guid, profile));
    const QVariantMap reloaded = manager.controllerProfile(guid);
    QCOMPARE(reloaded.value(QStringLiteral("select")).toStringList(),
             QStringList{QStringLiteral("leftstick-click")});
    QCOMPARE(reloaded.value(QStringLiteral("back")).toStringList(),
             QStringList{QStringLiteral("rightstick-click")});
}

void SettingsManagerTest::persistsPreferencesInExplicitConfigFile()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    {
        SettingsManager manager(settingsPath);
        manager.setThemeName(QStringLiteral("carbon"));
        manager.setScanOnStartup(false);
        manager.setPreferredPresentationSize(QStringLiteral("3440x1440"));
        manager.setDisplayMode(QStringLiteral("maximized"));
        QCOMPARE(manager.themeName(), QStringLiteral("carbon"));
        QVERIFY(!manager.scanOnStartup());
        QCOMPARE(manager.preferredPresentationSize(), QStringLiteral("3440x1440"));
        QCOMPARE(manager.displayMode(), QStringLiteral("maximized"));
        QCOMPARE(manager.restoreDisplayMode(), QStringLiteral("maximized"));
    }

    SettingsManager reloaded(settingsPath);
    QCOMPARE(reloaded.themeName(), QStringLiteral("carbon"));
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

void SettingsManagerTest::defaultsToCuratedLibraryPresentation()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");

    SettingsManager manager(settingsPath);
    QCOMPARE(manager.libraryView(), QStringLiteral("carousel"));
    QCOMPARE(manager.libraryDensity(), QStringLiteral("comfortable"));
    QCOMPARE(manager.libraryListDensity(), QStringLiteral("comfortable"));
    QCOMPARE(manager.libraryConsoleDensity(), QStringLiteral("spacious"));
    QCOMPARE(manager.libraryBackgroundType(), QStringLiteral("static"));
    QVERIFY(manager.customLibraryBackground().isEmpty());
}

void SettingsManagerTest::persistsCarouselLibraryView()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    {
        SettingsManager manager(settingsPath);
        manager.setLibraryView(QStringLiteral("grid"));
        QSignalSpy changed(&manager, &SettingsManager::libraryViewChanged);
        manager.setLibraryView(QStringLiteral("carousel"));
        QCOMPARE(manager.libraryView(), QStringLiteral("carousel"));
        QCOMPARE(changed.count(), 1);
    }

    SettingsManager reloaded(settingsPath);
    QCOMPARE(reloaded.libraryView(), QStringLiteral("carousel"));
}

void SettingsManagerTest::persistsShelvesLibraryView()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    {
        SettingsManager manager(settingsPath);
        manager.setLibraryView(QStringLiteral("grid"));
        QSignalSpy changed(&manager, &SettingsManager::libraryViewChanged);
        manager.setLibraryView(QStringLiteral("shelves"));
        QCOMPARE(manager.libraryView(), QStringLiteral("shelves"));
        QCOMPARE(changed.count(), 1);
    }

    SettingsManager reloaded(settingsPath);
    QCOMPARE(reloaded.libraryView(), QStringLiteral("shelves"));
}

void SettingsManagerTest::defaultControllerProfileMapsDetailsToY()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    // An unsaved controller receives the default profile.
    const QVariantMap defaults = manager.controllerProfile(
        QStringLiteral("030000005e0400008e02000000000000"));
    QCOMPARE(defaults.value(QStringLiteral("details")).toStringList(),
             QStringList{QStringLiteral("y")});
}

void SettingsManagerTest::legacyControllerProfileGainsDetailsDefault()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));
    const QString guid = QStringLiteral("030000005e0400008e02000000000000");

    // A profile saved before the details action existed, with a custom select.
    QVariantMap legacy;
    {
        SettingsManager defaultsSource(settingsPath);
        legacy = defaultsSource.controllerProfile(guid);
    }
    legacy.remove(QStringLiteral("details"));
    legacy[QStringLiteral("select")] = QVariantList{QStringLiteral("x")};
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("controllers/profiles/") + guid
                              + QStringLiteral("/mappings"), legacy);
        settings.sync();
    }

    SettingsManager manager(settingsPath);
    const QVariantMap profile = manager.controllerProfile(guid);
    QCOMPARE(profile.value(QStringLiteral("select")).toStringList(),
             QStringList{QStringLiteral("x")});
    QCOMPARE(profile.value(QStringLiteral("details")).toStringList(),
             QStringList{QStringLiteral("y")});
}

void SettingsManagerTest::rejectsUnknownLibraryView()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    manager.setLibraryView(QStringLiteral("grid"));
    QSignalSpy changed(&manager, &SettingsManager::libraryViewChanged);
    manager.setLibraryView(QStringLiteral("mosaic"));
    QCOMPARE(manager.libraryView(), QStringLiteral("grid"));
    QCOMPARE(changed.count(), 0);
}

void SettingsManagerTest::persistsLibraryPresentationPreferences()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    const QString sourceImagePath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("wallpaper.webp"));
    QFile sourceImage(sourceImagePath);
    QVERIFY(sourceImage.open(QIODevice::WriteOnly));
    QCOMPARE(sourceImage.write("test-image"), qint64(10));
    sourceImage.close();

    QString savedBackground;
    {
        SettingsManager manager(settingsPath);
        manager.setLibraryView(QStringLiteral("grid"));
        QSignalSpy viewChanged(&manager, &SettingsManager::libraryViewChanged);
        QSignalSpy densityChanged(&manager, &SettingsManager::libraryDensityChanged);
        QSignalSpy listDensityChanged(&manager, &SettingsManager::libraryListDensityChanged);
        QSignalSpy consoleDensityChanged(&manager, &SettingsManager::libraryConsoleDensityChanged);
        QSignalSpy backgroundChanged(&manager, &SettingsManager::libraryBackgroundTypeChanged);
        QSignalSpy imageChanged(&manager, &SettingsManager::customLibraryBackgroundChanged);

        manager.setLibraryView(QStringLiteral("carousel"));
        manager.setLibraryDensity(QStringLiteral("spacious"));
        manager.setLibraryListDensity(QStringLiteral("compact"));
        manager.setLibraryConsoleDensity(QStringLiteral("comfortable"));
        manager.setLibraryBackgroundType(QStringLiteral("custom"));
        manager.setCustomLibraryBackground(sourceImagePath);

        QCOMPARE(viewChanged.count(), 1);
        QCOMPARE(densityChanged.count(), 1);
        QCOMPARE(listDensityChanged.count(), 1);
        QCOMPARE(consoleDensityChanged.count(), 1);
        QCOMPARE(backgroundChanged.count(), 1);
        QCOMPARE(imageChanged.count(), 1);
        savedBackground = manager.customLibraryBackground();
        QVERIFY(savedBackground.startsWith(QStringLiteral("file://")));
        const QString copiedImagePath = QUrl(savedBackground).toLocalFile();
        QVERIFY(QFileInfo(copiedImagePath).isFile());
        QVERIFY(copiedImagePath != sourceImagePath);
    }

    SettingsManager reloaded(settingsPath);
    QCOMPARE(reloaded.libraryView(), QStringLiteral("carousel"));
    QCOMPARE(reloaded.libraryDensity(), QStringLiteral("spacious"));
    QCOMPARE(reloaded.libraryListDensity(), QStringLiteral("compact"));
    QCOMPARE(reloaded.libraryConsoleDensity(), QStringLiteral("comfortable"));
    QCOMPARE(reloaded.libraryBackgroundType(), QStringLiteral("custom"));
    QCOMPARE(reloaded.customLibraryBackground(), savedBackground);
}

void SettingsManagerTest::rejectsUnsupportedLibraryPresentationPreferences()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    QSignalSpy viewChanged(&manager, &SettingsManager::libraryViewChanged);
    QSignalSpy densityChanged(&manager, &SettingsManager::libraryDensityChanged);
    QSignalSpy listDensityChanged(&manager, &SettingsManager::libraryListDensityChanged);
    QSignalSpy backgroundChanged(&manager, &SettingsManager::libraryBackgroundTypeChanged);

    manager.setLibraryView(QStringLiteral("unsupported"));
    manager.setLibraryDensity(QStringLiteral("extreme"));
    manager.setLibraryListDensity(QStringLiteral("extreme"));
    manager.setLibraryBackgroundType(QStringLiteral("video"));

    QCOMPARE(manager.libraryView(), QStringLiteral("carousel"));
    QCOMPARE(manager.libraryDensity(), QStringLiteral("comfortable"));
    QCOMPARE(manager.libraryListDensity(), QStringLiteral("comfortable"));
    QCOMPARE(manager.libraryBackgroundType(), QStringLiteral("static"));
    QCOMPARE(viewChanged.count(), 0);
    QCOMPARE(densityChanged.count(), 0);
    QCOMPARE(listDensityChanged.count(), 0);
    QCOMPARE(backgroundChanged.count(), 0);
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

void SettingsManagerTest::migratesRetiredEmberThemeToElectricBlue()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/theme"), QStringLiteral("ember"));
    }

    SettingsManager manager(settingsPath);
    QCOMPARE(manager.themeName(), QStringLiteral("electric-blue"));
    QSettings stored(settingsPath, QSettings::IniFormat);
    QCOMPARE(stored.value(QStringLiteral("appearance/theme")).toString(),
             QStringLiteral("electric-blue"));

    manager.setThemeName(QStringLiteral("ember"));
    QCOMPARE(manager.themeName(), QStringLiteral("electric-blue"));
}

void SettingsManagerTest::acceptsAuroraAndCarbonThemes()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    manager.setThemeName(QStringLiteral("aurora"));
    QCOMPARE(manager.themeName(), QStringLiteral("aurora"));
    manager.setThemeName(QStringLiteral("carbon"));
    QCOMPARE(SettingsManager(settingsPath).themeName(), QStringLiteral("carbon"));
}

void SettingsManagerTest::defaultsToAutomaticInterfaceScale()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    QCOMPARE(manager.interfaceScale(), QStringLiteral("auto"));
    QCOMPARE(SettingsManager::storedInterfaceScale(settingsPath), QStringLiteral("auto"));
    QCOMPARE(manager.interfaceScaleOptions().first().toString(), QStringLiteral("auto"));
}

void SettingsManagerTest::persistsInterfaceScale()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    QSignalSpy changed(&manager, &SettingsManager::interfaceScaleChanged);
    manager.setInterfaceScale(QStringLiteral("150"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(SettingsManager(settingsPath).interfaceScale(), QStringLiteral("150"));
    QCOMPARE(SettingsManager::storedInterfaceScale(settingsPath), QStringLiteral("150"));
}

void SettingsManagerTest::rejectsUnsupportedInterfaceScale()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    SettingsManager manager(settingsPath);
    QSignalSpy changed(&manager, &SettingsManager::interfaceScaleChanged);
    manager.setInterfaceScale(QStringLiteral("140"));
    manager.setInterfaceScale(QStringLiteral("huge"));
    QCOMPARE(manager.interfaceScale(), QStringLiteral("auto"));
    QCOMPARE(changed.count(), 0);

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("display/interfaceScale"), QStringLiteral("999"));
    }
    QCOMPARE(SettingsManager::storedInterfaceScale(settingsPath), QStringLiteral("auto"));
}

void SettingsManagerTest::autoInterfaceScaleFollowsScreenHeight()
{
    QCOMPARE(SettingsManager::autoInterfaceScalePercent(0), 100);
    QCOMPARE(SettingsManager::autoInterfaceScalePercent(720), 85);
    QCOMPARE(SettingsManager::autoInterfaceScalePercent(800), 85);
    QCOMPARE(SettingsManager::autoInterfaceScalePercent(1080), 100);
    QCOMPARE(SettingsManager::autoInterfaceScalePercent(1200), 100);
    QCOMPARE(SettingsManager::autoInterfaceScalePercent(1440), 125);
    QCOMPARE(SettingsManager::autoInterfaceScalePercent(1800), 150);
    QCOMPARE(SettingsManager::autoInterfaceScalePercent(2160), 175);
    QCOMPARE(SettingsManager::autoInterfaceScalePercent(2880), 200);
}

void SettingsManagerTest::defaultsToLaunchBoxFirstProviderOrder()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    SettingsManager manager(QDir(temporaryDirectory.path()).filePath("settings.ini"));
    QCOMPARE(manager.metadataProviderOrder(), QStringLiteral("launchbox"));
}

void SettingsManagerTest::persistsProviderOrderAndRejectsUnknownValues()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("settings.ini");
    SettingsManager manager(settingsPath);
    QSignalSpy changed(&manager, &SettingsManager::metadataProviderOrderChanged);
    manager.setMetadataProviderOrder(QStringLiteral("thegamesdb"));
    manager.setMetadataProviderOrder(QStringLiteral("screenscraper"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(SettingsManager(settingsPath).metadataProviderOrder(), QStringLiteral("thegamesdb"));
}

void SettingsManagerTest::savesTheGamesDbKeyPrivately()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));
    SettingsManager manager(settingsPath);
    QCOMPARE(manager.theGamesDbKeySource(), QStringLiteral("none"));

    QSignalSpy changed(&manager, &SettingsManager::theGamesDbApiKeyChanged);
    const QString key = QStringLiteral("0123456789abcdef0123456789ABCDEF3f9a");
    QVERIFY(manager.saveTheGamesDbApiKey(QStringLiteral("  ") + key + QStringLiteral("\n")));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(manager.theGamesDbApiKey(), key);
    QCOMPARE(manager.theGamesDbKeySource(), QStringLiteral("saved"));
    QCOMPARE(manager.theGamesDbKeyHint(), QStringLiteral("3f9a"));

    // Not in settings.ini, and only the owner can read or write the key file.
    QVERIFY(!QSettings(settingsPath, QSettings::IniFormat).allKeys().join(",").contains("apiKey"));
    const QFileInfo keyFile(manager.theGamesDbKeyFilePath());
    QVERIFY(keyFile.exists());
    QCOMPARE(keyFile.permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther
                                      | QFileDevice::WriteGroup | QFileDevice::WriteOther),
             QFileDevice::Permissions());

    QVERIFY(manager.clearTheGamesDbApiKey());
    QCOMPARE(manager.theGamesDbKeySource(), QStringLiteral("none"));
    QVERIFY(!QFileInfo::exists(manager.theGamesDbKeyFilePath()));
}

void SettingsManagerTest::rejectsImplausibleTheGamesDbKeys()
{
    QVERIFY(!SettingsManager::isPlausibleTheGamesDbApiKey(QString()));
    QVERIFY(!SettingsManager::isPlausibleTheGamesDbApiKey(QStringLiteral("short")));
    QVERIFY(!SettingsManager::isPlausibleTheGamesDbApiKey(QStringLiteral("has spaces in the middle of it")));
    QVERIFY(!SettingsManager::isPlausibleTheGamesDbApiKey(QStringLiteral("key;rm -rf /;0123456789")));
    QVERIFY(SettingsManager::isPlausibleTheGamesDbApiKey(
        QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")));
}

void SettingsManagerTest::environmentTheGamesDbKeyTakesPriority()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    SettingsManager manager(QDir(temporaryDirectory.path()).filePath("settings.ini"));
    QVERIFY(manager.saveTheGamesDbApiKey(QStringLiteral("savedkey0123456789savedkey")));
    qputenv("RECOMPSTATION_TGDB_API_KEY", "environmentkey0123456789env");
    QCOMPARE(manager.theGamesDbApiKey(), QStringLiteral("environmentkey0123456789env"));
    QCOMPARE(manager.theGamesDbKeySource(), QStringLiteral("environment"));
    qunsetenv("RECOMPSTATION_TGDB_API_KEY");
    QCOMPARE(manager.theGamesDbKeySource(), QStringLiteral("saved"));
}

void SettingsManagerTest::resetPreferencesKeepsLibraryAndKey()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString settingsPath = QDir(temporaryDirectory.path()).filePath("config/settings.ini");
    QVERIFY(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("library/root"), QStringLiteral("/games"));
        settings.setValue(QStringLiteral("metadata/launchBoxEnabled"), true);
    }
    {
        SettingsManager manager(settingsPath);
        manager.setThemeName(QStringLiteral("carbon"));
        manager.setInterfaceScale(QStringLiteral("150"));
        manager.setMetadataProviderOrder(QStringLiteral("thegamesdb"));
        QVERIFY(manager.saveTheGamesDbApiKey(QStringLiteral("keepthiskey0123456789")));
        QVERIFY(manager.resetPreferences());
    }
    SettingsManager reloaded(settingsPath);
    QCOMPARE(reloaded.themeName(), QStringLiteral("midnight-violet"));
    QCOMPARE(reloaded.interfaceScale(), QStringLiteral("auto"));
    QCOMPARE(reloaded.metadataProviderOrder(), QStringLiteral("launchbox"));
    QVERIFY(reloaded.launchBoxEnabled());
    QCOMPARE(reloaded.theGamesDbKeySource(), QStringLiteral("saved"));
    QCOMPARE(QSettings(settingsPath, QSettings::IniFormat).value(QStringLiteral("library/root")).toString(),
             QStringLiteral("/games"));
}

QTEST_GUILESS_MAIN(SettingsManagerTest)
#include "SettingsManagerTest.moc"
