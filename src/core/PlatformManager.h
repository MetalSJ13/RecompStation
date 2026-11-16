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
    Q_PROPERTY(QString libraryDirectory READ libraryDirectory NOTIFY libraryDirectoryChanged)
    Q_PROPERTY(bool libraryConfigured READ libraryConfigured NOTIFY libraryDirectoryChanged)

public:
    explicit PlatformManager(QObject *parent = nullptr);
    explicit PlatformManager(
        const QString &dataDirectory,
        QObject *parent = nullptr
    );

    bool initialize();
    bool initialize(const QStringList &supportedPlatforms);

    QString dataDirectory() const;
    QString stagingDirectory() const;
    QString libraryDirectory() const;
    bool libraryConfigured() const;

    Q_INVOKABLE QStringList platforms() const;

    Q_INVOKABLE bool setLibraryDirectory(const QUrl &directoryUrl);

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
    void libraryDirectoryChanged();

private:
    QString m_dataDirectory;
    QString m_libraryDirectory;
    QStringList m_platforms;

    bool createDirectory(const QString &path);
    static bool isValidProjectId(const QString &projectId);
};
