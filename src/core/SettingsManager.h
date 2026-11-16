#pragma once

#include <QObject>
#include <QString>


class SettingsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString themeName READ themeName WRITE setThemeName NOTIFY themeNameChanged)
    Q_PROPERTY(QString displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)

public:
    explicit SettingsManager(QObject *parent = nullptr);

    QString themeName() const;
    Q_INVOKABLE void setThemeName(const QString &themeName);

    QString displayMode() const;
    Q_INVOKABLE void setDisplayMode(const QString &displayMode);

signals:
    void themeNameChanged();
    void displayModeChanged();

private:
    QString m_themeName;
    QString m_displayMode;
    static bool isSupportedTheme(const QString &themeName);
    static bool isSupportedDisplayMode(const QString &displayMode);
};
