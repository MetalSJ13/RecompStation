#pragma once

#include <QObject>
#include <QString>


class SettingsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString themeName READ themeName WRITE setThemeName NOTIFY themeNameChanged)
    Q_PROPERTY(bool scanOnStartup READ scanOnStartup WRITE setScanOnStartup NOTIFY scanOnStartupChanged)

public:
    explicit SettingsManager(QObject *parent = nullptr);

    QString themeName() const;
    Q_INVOKABLE void setThemeName(const QString &themeName);

    bool scanOnStartup() const;
    Q_INVOKABLE void setScanOnStartup(bool enabled);


signals:
    void themeNameChanged();
    void scanOnStartupChanged();

private:
    QString m_themeName;
    bool m_scanOnStartup = true;
    static bool isSupportedTheme(const QString &themeName);
};
