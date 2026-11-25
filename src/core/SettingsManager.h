#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>


class SettingsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString themeName READ themeName WRITE setThemeName NOTIFY themeNameChanged)
    Q_PROPERTY(bool scanOnStartup READ scanOnStartup WRITE setScanOnStartup NOTIFY scanOnStartupChanged)
    Q_PROPERTY(bool automaticUpdateChecksEnabled READ automaticUpdateChecksEnabled NOTIFY automaticUpdateChecksEnabledChanged)
    Q_PROPERTY(bool updatePreferencePromptCompleted READ updatePreferencePromptCompleted NOTIFY updatePreferencePromptCompletedChanged)
    Q_PROPERTY(bool automaticMetadataMatchingEnabled READ automaticMetadataMatchingEnabled NOTIFY automaticMetadataMatchingEnabledChanged)
    Q_PROPERTY(bool automaticArtworkDownloadsEnabled READ automaticArtworkDownloadsEnabled NOTIFY automaticArtworkDownloadsEnabledChanged)
    Q_PROPERTY(bool theGamesDbEnabled READ theGamesDbEnabled NOTIFY theGamesDbEnabledChanged)
    Q_PROPERTY(bool launchBoxEnabled READ launchBoxEnabled NOTIFY launchBoxEnabledChanged)
    Q_PROPERTY(QString metadataProviderOrder READ metadataProviderOrder WRITE setMetadataProviderOrder NOTIFY metadataProviderOrderChanged)
    Q_PROPERTY(QString theGamesDbKeySource READ theGamesDbKeySource NOTIFY theGamesDbApiKeyChanged)
    Q_PROPERTY(QString theGamesDbKeyHint READ theGamesDbKeyHint NOTIFY theGamesDbApiKeyChanged)
    Q_PROPERTY(QString launchBoxLastUpdated READ launchBoxLastUpdated NOTIFY launchBoxLastUpdatedChanged)
    Q_PROPERTY(QString preferredPresentationSize READ preferredPresentationSize WRITE setPreferredPresentationSize NOTIFY preferredPresentationSizeChanged)
    Q_PROPERTY(QString displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_PROPERTY(QString restoreDisplayMode READ restoreDisplayMode WRITE setRestoreDisplayMode NOTIFY restoreDisplayModeChanged)
    Q_PROPERTY(QString fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(QString interfaceScale READ interfaceScale WRITE setInterfaceScale NOTIFY interfaceScaleChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    // Interface sounds (Settings > Appearance > Sounds): on by default, 0-100.
    Q_PROPERTY(bool soundsEnabled READ soundsEnabled WRITE setSoundsEnabled NOTIFY soundsEnabledChanged)
    Q_PROPERTY(int soundVolume READ soundVolume WRITE setSoundVolume NOTIFY soundVolumeChanged)
    Q_PROPERTY(QString libraryView READ libraryView WRITE setLibraryView NOTIFY libraryViewChanged)
    Q_PROPERTY(QString libraryDensity READ libraryDensity WRITE setLibraryDensity NOTIFY libraryDensityChanged)
    Q_PROPERTY(QString libraryListDensity READ libraryListDensity WRITE setLibraryListDensity NOTIFY libraryListDensityChanged)
    Q_PROPERTY(QString libraryConsoleDensity READ libraryConsoleDensity WRITE setLibraryConsoleDensity NOTIFY libraryConsoleDensityChanged)
    Q_PROPERTY(QString libraryBackgroundType READ libraryBackgroundType WRITE setLibraryBackgroundType NOTIFY libraryBackgroundTypeChanged)
    Q_PROPERTY(bool detailedLogging READ detailedLogging WRITE setDetailedLogging NOTIFY detailedLoggingChanged)
    Q_PROPERTY(bool prepareGameFiles READ prepareGameFiles WRITE setPrepareGameFiles NOTIFY launchSettingsChanged)
    Q_PROPERTY(bool startGamesFullscreen READ startGamesFullscreen WRITE setStartGamesFullscreen NOTIFY launchSettingsChanged)
    // Linux only: offer Windows launchers, run through the system's Wine.
    Q_PROPERTY(bool windowsLaunchersThroughWine READ windowsLaunchersThroughWine WRITE setWindowsLaunchersThroughWine NOTIFY launchSettingsChanged)
    Q_PROPERTY(int launchSettingsRevision READ launchSettingsRevision NOTIFY launchSettingsChanged)
    Q_PROPERTY(QString customLibraryBackground READ customLibraryBackground WRITE setCustomLibraryBackground NOTIFY customLibraryBackgroundChanged)

public:
    explicit SettingsManager(QObject *parent = nullptr);
    explicit SettingsManager(const QString &settingsFilePath, QObject *parent = nullptr);

    QString themeName() const;
    Q_INVOKABLE void setThemeName(const QString &themeName);

    bool scanOnStartup() const;
    Q_INVOKABLE void setScanOnStartup(bool enabled);

    bool automaticUpdateChecksEnabled() const;
    bool updatePreferencePromptCompleted() const;
    Q_INVOKABLE void setAutomaticUpdateChecksEnabled(bool enabled);
    Q_INVOKABLE void completeUpdatePreferencePrompt(bool enabled);

    bool automaticMetadataMatchingEnabled() const;
    Q_INVOKABLE void setAutomaticMetadataMatchingEnabled(bool enabled);
    bool automaticArtworkDownloadsEnabled() const;
    Q_INVOKABLE void setAutomaticArtworkDownloadsEnabled(bool enabled);

    bool theGamesDbEnabled() const;
    Q_INVOKABLE void setTheGamesDbEnabled(bool enabled);

    bool launchBoxEnabled() const;
    Q_INVOKABLE void setLaunchBoxEnabled(bool enabled);
    // "launchbox" (default: offline catalog first) or "thegamesdb".
    QString metadataProviderOrder() const;
    Q_INVOKABLE void setMetadataProviderOrder(const QString &order);

    // TheGamesDB API key. RECOMPSTATION_TGDB_API_KEY wins when set; otherwise
    // the key saved from Settings is used. The saved key lives in its own
    // file next to settings.ini, readable and writable only by the user.
    QString theGamesDbApiKey() const;
    QString theGamesDbKeySource() const;   // "environment", "saved" or "none"
    QString theGamesDbKeyHint() const;     // last four characters, or empty
    QString theGamesDbKeyFilePath() const;
    Q_INVOKABLE bool saveTheGamesDbApiKey(const QString &key);
    Q_INVOKABLE bool clearTheGamesDbApiKey();
    static bool isPlausibleTheGamesDbApiKey(const QString &key);

    QString launchBoxLastUpdated() const;
    Q_INVOKABLE void setLaunchBoxLastUpdated(const QString &timestamp);

    Q_INVOKABLE QVariantMap controllerProfile(const QString &guid) const;
    Q_INVOKABLE bool saveControllerProfile(const QString &guid, const QVariantMap &profile);
    Q_INVOKABLE bool resetControllerProfile(const QString &guid);
    Q_INVOKABLE QStringList controllerProfileIds() const;

    QString preferredPresentationSize() const;
    Q_INVOKABLE void setPreferredPresentationSize(const QString &size);

    QString displayMode() const;
    Q_INVOKABLE void setDisplayMode(const QString &mode);

    QString restoreDisplayMode() const;
    Q_INVOKABLE void setRestoreDisplayMode(const QString &mode);

    QString fontSize() const;
    Q_INVOKABLE void setFontSize(const QString &size);

    // "auto" or a whole percentage from interfaceScalePercents(). Applied
    // through QT_SCALE_FACTOR when the application starts, so a change takes
    // effect after a restart.
    QString interfaceScale() const;
    Q_INVOKABLE void setInterfaceScale(const QString &scale);
    Q_INVOKABLE QVariantList interfaceScaleOptions() const;
    static QList<int> interfaceScalePercents();
    static bool isSupportedInterfaceScale(const QString &scale);
    // Auto scale for a screen's logical height (after desktop scaling).
    static int autoInterfaceScalePercent(int logicalScreenHeight);
    // Reads the saved setting straight from the settings file. Used by main()
    // before QGuiApplication exists.
    static QString storedInterfaceScale(const QString &settingsFilePath);
    // Asks main() to restart RecompStation and reopen Settings at a section.
    Q_INVOKABLE void requestApplicationRestart(const QString &settingsSection);
    // Restores appearance, display, library-view, controller and metadata
    // behaviour settings to their defaults. Keeps the library folder, the
    // LaunchBox catalog state, the TheGamesDB key and first-run answers.
    // Takes full effect after a restart.
    Q_INVOKABLE bool resetPreferences();

    bool reducedMotion() const;
    Q_INVOKABLE void setReducedMotion(bool enabled);

    bool soundsEnabled() const;
    Q_INVOKABLE void setSoundsEnabled(bool enabled);
    int soundVolume() const;
    Q_INVOKABLE void setSoundVolume(int percent);

    QString libraryView() const;
    Q_INVOKABLE void setLibraryView(const QString &view);
    QString libraryDensity() const;
    Q_INVOKABLE void setLibraryDensity(const QString &density);
    QString libraryListDensity() const;
    Q_INVOKABLE void setLibraryListDensity(const QString &density);
    QString libraryConsoleDensity() const;
    Q_INVOKABLE void setLibraryConsoleDensity(const QString &density);
    QString libraryBackgroundType() const;
    Q_INVOKABLE void setLibraryBackgroundType(const QString &type);
    QString customLibraryBackground() const;
    Q_INVOKABLE void setCustomLibraryBackground(const QString &path);

    // Application › Diagnostics: debug lines in runtime.log.
    bool detailedLogging() const;
    Q_INVOKABLE void setDetailedLogging(bool enabled);

    // Playing: handing game files to launchers (see LaunchPreparer).
    bool prepareGameFiles() const;
    Q_INVOKABLE void setPrepareGameFiles(bool enabled);
    bool startGamesFullscreen() const;
    bool windowsLaunchersThroughWine() const;
    Q_INVOKABLE void setWindowsLaunchersThroughWine(bool enabled);
    Q_INVOKABLE void setStartGamesFullscreen(bool enabled);
    // Per game: "default", "fullscreen" or "windowed".
    Q_INVOKABLE QString launchWindowMode(const QString &projectId) const;
    Q_INVOKABLE void setLaunchWindowMode(const QString &projectId, const QString &mode);
    int launchSettingsRevision() const;
    // "Start games in fullscreen" is set once per launcher, then left alone.
    bool defaultFullscreenApplied(const QString &projectId) const;
    void markDefaultFullscreenApplied(const QString &projectId);

signals:
    void launchSettingsChanged();
    void detailedLoggingChanged();
    void themeNameChanged();
    void scanOnStartupChanged();
    void automaticUpdateChecksEnabledChanged();
    void updatePreferencePromptCompletedChanged();
    void automaticMetadataMatchingEnabledChanged();
    void automaticArtworkDownloadsEnabledChanged();
    void theGamesDbEnabledChanged();
    void launchBoxEnabledChanged();
    void launchBoxLastUpdatedChanged();
    void metadataProviderOrderChanged();
    void theGamesDbApiKeyChanged();
    void preferredPresentationSizeChanged();
    void displayModeChanged();
    void restoreDisplayModeChanged();
    void fontSizeChanged();
    void interfaceScaleChanged();
    void applicationRestartRequested(const QString &settingsSection);
    void reducedMotionChanged();
    void soundsEnabledChanged();
    void soundVolumeChanged();
    void libraryViewChanged();
    void libraryDensityChanged();
    void libraryListDensityChanged();
    void libraryConsoleDensityChanged();
    void libraryBackgroundTypeChanged();
    void customLibraryBackgroundChanged();

private:
    QString m_settingsFilePath;
    QString m_themeName;
    QString m_preferredPresentationSize = QStringLiteral("system");
    QString m_displayMode = QStringLiteral("fullscreen");
    QString m_restoreDisplayMode = QStringLiteral("windowed");
    QString m_fontSize = QStringLiteral("medium");
    QString m_interfaceScale = QStringLiteral("auto");
    QString m_metadataProviderOrder = QStringLiteral("launchbox");
    bool m_reducedMotion = false;
    bool m_soundsEnabled = true;
    int m_soundVolume = 60;
    QString m_libraryView = QStringLiteral("carousel");
    QString m_libraryDensity = QStringLiteral("comfortable");
    QString m_libraryListDensity = QStringLiteral("comfortable");
    QString m_libraryConsoleDensity = QStringLiteral("spacious");
    QString m_libraryBackgroundType = QStringLiteral("static");
    QString m_customLibraryBackground;
    bool m_scanOnStartup = true;
    bool m_automaticUpdateChecksEnabled = false;
    bool m_updatePreferencePromptCompleted = false;
    bool m_automaticMetadataMatchingEnabled = true;
    bool m_automaticArtworkDownloadsEnabled = true;
    bool m_theGamesDbEnabled = true;
    bool m_launchBoxEnabled = false;
    bool m_prepareGameFiles = true;
    bool m_detailedLogging = false;
    bool m_startGamesFullscreen = true;
    bool m_windowsLaunchersThroughWine = false;
    int m_launchSettingsRevision = 0;
    QString m_launchBoxLastUpdated;
    static QVariantMap defaultControllerProfile();
    static bool isValidControllerGuid(const QString &guid);
    static bool isSupportedTheme(const QString &themeName);
    static bool isSupportedPresentationSize(const QString &size);
    static bool isSupportedDisplayMode(const QString &mode);
    static bool isSupportedFontSize(const QString &size);
    static bool isSupportedLibraryView(const QString &view);
    static bool isSupportedLibraryDensity(const QString &density);
    static bool isSupportedLibraryListDensity(const QString &density);
    static bool isSupportedLibraryBackgroundType(const QString &type);
};
