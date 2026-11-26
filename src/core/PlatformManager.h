#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>

class PlatformManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(
        QString dataDirectory
        READ dataDirectory
        CONSTANT
    )
    Q_PROPERTY(QString settingsFilePath READ settingsFilePath CONSTANT)
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
    // Linux: Qt's application data folder; Windows: %LOCALAPPDATA%\RecompStation.
    static QString defaultDataDirectory();
    // A path as people see it: Windows "C:\\Users\\Sam\\RecompStation",
    // Linux "~/RecompStation".
    Q_INVOKABLE QString displayPath(const QString &path) const;

    QString dataDirectory() const;
    QString settingsFilePath() const;
    QString stagingDirectory() const;
    QString libraryDirectory() const;
    bool libraryConfigured() const;

    Q_INVOKABLE QStringList platforms() const;

    Q_INVOKABLE bool setLibraryDirectory(const QUrl &directoryUrl);
    Q_INVOKABLE bool setLibraryDirectoryPath(const QString &directoryPath);
    Q_INVOKABLE QString homeDirectory() const;
    Q_INVOKABLE bool pathExists(const QString &path) const;
    // Default library location for first-time setup: ~/Games/RecompStation
    // when ~/Games already exists (a new folder inside it, so nothing there is
    // touched), otherwise ~/RecompStation. Returns { path, exists, freeBytes }.
    Q_INVOKABLE QVariantMap suggestedLibraryLocation() const;
    // Creates the folder if needed and makes it the library.
    Q_INVOKABLE bool useLibraryLocation(const QString &directoryPath);
    Q_INVOKABLE QString parentDirectory(const QString &directoryPath) const;
    Q_INVOKABLE QVariantList libraryDirectories(const QString &directoryPath) const;
    Q_INVOKABLE QVariantList browserPlaces() const;
    Q_INVOKABLE bool createLibraryFolder(const QString &parentPath, const QString &folderName);

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
    QString m_settingsFilePath;
    QString m_libraryDirectory;
    QStringList m_platforms;

    bool createDirectory(const QString &path);
    static bool isValidProjectId(const QString &projectId);
};
