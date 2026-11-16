#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QHash>

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
    explicit PlatformManager(
        const QString &dataDirectory,
        QObject *parent = nullptr
    );

    bool initialize();

    QString dataDirectory() const;
    QString stagingDirectory() const;

    Q_INVOKABLE QStringList platforms() const;

    Q_INVOKABLE bool setPlatformDirectory(const QString &platform, const QUrl &directoryUrl);

    Q_INVOKABLE QString platformDirectory(
        const QString &platform
    ) const;

    Q_INVOKABLE QString launcherDirectory(
        const QString &projectId
    ) const;

    Q_INVOKABLE bool createLauncherDirectory(
        const QString &projectId
    );

signals:
    void platformDirectoryChanged(const QString &platform, const QString &directory);

private:
    QString m_dataDirectory;
    QHash<QString, QString> m_platformDirectories;
    QStringList m_platforms;

    bool createDirectory(const QString &path);
    static bool isValidProjectId(const QString &projectId);
};
