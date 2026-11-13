
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class PlatformManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(
        QString dataDirectory
        READ dataDirectory
        CONSTANT
    )

public:
    explicit PlatformManager(QObject *parent = nullptr);

    bool initialize();

    QString dataDirectory() const;

    Q_INVOKABLE QStringList platforms() const;

    Q_INVOKABLE QString platformDirectory(
        const QString &platform
    ) const;

private:
    QString m_dataDirectory;
    QStringList m_platforms;

    bool createDirectory(const QString &path);
};
