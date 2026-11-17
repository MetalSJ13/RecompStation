#pragma once

#include <QObject>
#include <QString>


class SettingsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString themeName READ themeName WRITE setThemeName NOTIFY themeNameChanged)
    Q_PROPERTY(bool scanOnStartup READ scanOnStartup WRITE setScanOnStartup NOTIFY scanOnStartupChanged)
    Q_PROPERTY(QString preferredPresentationSize READ preferredPresentationSize WRITE setPreferredPresentationSize NOTIFY preferredPresentationSizeChanged)
    Q_PROPERTY(QString displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_PROPERTY(QString restoreDisplayMode READ restoreDisplayMode WRITE setRestoreDisplayMode NOTIFY restoreDisplayModeChanged)
    Q_PROPERTY(QString fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)

public:
    explicit SettingsManager(QObject *parent = nullptr);
    explicit SettingsManager(const QString &settingsFilePath, QObject *parent = nullptr);

    QString themeName() const;
    Q_INVOKABLE void setThemeName(const QString &themeName);

    bool scanOnStartup() const;
    Q_INVOKABLE void setScanOnStartup(bool enabled);

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

signals:
    void themeNameChanged();
    void scanOnStartupChanged();
    void preferredPresentationSizeChanged();
    void displayModeChanged();
    void restoreDisplayModeChanged();
    void fontSizeChanged();
    void reducedMotionChanged();

private:
    QString m_settingsFilePath;
    QString m_themeName;
    QString m_preferredPresentationSize = QStringLiteral("system");
    QString m_displayMode = QStringLiteral("windowed");
    QString m_restoreDisplayMode = QStringLiteral("windowed");
    QString m_fontSize = QStringLiteral("medium");
    bool m_reducedMotion = false;
    bool m_scanOnStartup = true;
    static bool isSupportedTheme(const QString &themeName);
    static bool isSupportedPresentationSize(const QString &size);
    static bool isSupportedDisplayMode(const QString &mode);
    static bool isSupportedFontSize(const QString &size);
};
