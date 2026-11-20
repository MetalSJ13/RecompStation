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
    Q_PROPERTY(bool theGamesDbEnabled READ theGamesDbEnabled NOTIFY theGamesDbEnabledChanged)
    Q_PROPERTY(bool launchBoxEnabled READ launchBoxEnabled NOTIFY launchBoxEnabledChanged)
    Q_PROPERTY(QString launchBoxLastUpdated READ launchBoxLastUpdated NOTIFY launchBoxLastUpdatedChanged)
    Q_PROPERTY(QString preferredPresentationSize READ preferredPresentationSize WRITE setPreferredPresentationSize NOTIFY preferredPresentationSizeChanged)
    Q_PROPERTY(QString displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_PROPERTY(QString restoreDisplayMode READ restoreDisplayMode WRITE setRestoreDisplayMode NOTIFY restoreDisplayModeChanged)
    Q_PROPERTY(QString fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(QString libraryView READ libraryView WRITE setLibraryView NOTIFY libraryViewChanged)
    Q_PROPERTY(QString libraryDensity READ libraryDensity WRITE setLibraryDensity NOTIFY libraryDensityChanged)
    Q_PROPERTY(QString libraryListDensity READ libraryListDensity WRITE setLibraryListDensity NOTIFY libraryListDensityChanged)
    Q_PROPERTY(QString libraryBackgroundType READ libraryBackgroundType WRITE setLibraryBackgroundType NOTIFY libraryBackgroundTypeChanged)
    Q_PROPERTY(QString customLibraryBackground READ customLibraryBackground WRITE setCustomLibraryBackground NOTIFY customLibraryBackgroundChanged)

public:
    explicit SettingsManager(QObject *parent = nullptr);
    explicit SettingsManager(const QString &settingsFilePath, QObject *parent = nullptr);

    QString themeName() const;
    Q_INVOKABLE void setThemeName(const QString &themeName);

    bool scanOnStartup() const;
    Q_INVOKABLE void setScanOnStartup(bool enabled);

    bool theGamesDbEnabled() const;
    Q_INVOKABLE void setTheGamesDbEnabled(bool enabled);

    bool launchBoxEnabled() const;
    Q_INVOKABLE void setLaunchBoxEnabled(bool enabled);
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

    bool reducedMotion() const;
    Q_INVOKABLE void setReducedMotion(bool enabled);

    QString libraryView() const;
    Q_INVOKABLE void setLibraryView(const QString &view);
    QString libraryDensity() const;
    Q_INVOKABLE void setLibraryDensity(const QString &density);
    QString libraryListDensity() const;
    Q_INVOKABLE void setLibraryListDensity(const QString &density);
    QString libraryBackgroundType() const;
    Q_INVOKABLE void setLibraryBackgroundType(const QString &type);
    QString customLibraryBackground() const;
    Q_INVOKABLE void setCustomLibraryBackground(const QString &path);

signals:
    void themeNameChanged();
    void scanOnStartupChanged();
    void theGamesDbEnabledChanged();
    void launchBoxEnabledChanged();
    void launchBoxLastUpdatedChanged();
    void preferredPresentationSizeChanged();
    void displayModeChanged();
    void restoreDisplayModeChanged();
    void fontSizeChanged();
    void reducedMotionChanged();
    void libraryViewChanged();
    void libraryDensityChanged();
    void libraryListDensityChanged();
    void libraryBackgroundTypeChanged();
    void customLibraryBackgroundChanged();

private:
    QString m_settingsFilePath;
    QString m_themeName;
    QString m_preferredPresentationSize = QStringLiteral("system");
    QString m_displayMode = QStringLiteral("fullscreen");
    QString m_restoreDisplayMode = QStringLiteral("windowed");
    QString m_fontSize = QStringLiteral("medium");
    bool m_reducedMotion = false;
    QString m_libraryView = QStringLiteral("grid");
    QString m_libraryDensity = QStringLiteral("comfortable");
    QString m_libraryListDensity = QStringLiteral("comfortable");
    QString m_libraryBackgroundType = QStringLiteral("static");
    QString m_customLibraryBackground;
    bool m_scanOnStartup = true;
    bool m_theGamesDbEnabled = true;
    bool m_launchBoxEnabled = false;
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
