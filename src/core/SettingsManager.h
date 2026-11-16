#pragma once

#include <QObject>
#include <QString>

class SettingsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString themeName READ themeName WRITE setThemeName NOTIFY themeNameChanged)

public:
    explicit SettingsManager(QObject *parent = nullptr);

    QString themeName() const;
    Q_INVOKABLE void setThemeName(const QString &themeName);

signals:
    void themeNameChanged();

private:
    QString m_themeName;
    static bool isSupportedTheme(const QString &themeName);
};
