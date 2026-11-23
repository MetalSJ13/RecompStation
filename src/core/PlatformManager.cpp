#include "PlatformManager.h"

#include <QDir>
#include <QRegularExpression>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QDebug>
#include <QDirIterator>
#include <QVariantMap>

PlatformManager::PlatformManager(QObject *parent)
    : PlatformManager(QString(), parent)
{
}

PlatformManager::PlatformManager(const QString &dataDirectory, QObject *parent)
    : QObject(parent)
{
    m_platforms = {
        "N64",
        "PS1",
        "GameCube",
        "Wii",
        "Xbox"
    };

    m_dataDirectory = dataDirectory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : QDir(dataDirectory).absolutePath();
}

bool PlatformManager::initialize()
{
    const QStringList defaultPlatforms = m_platforms;
    return initialize(defaultPlatforms);
}

bool PlatformManager::initialize(const QStringList &supportedPlatforms)
{
    m_platforms.clear();
    for (const QString &platform : supportedPlatforms) {
        const QString normalized = platform.trimmed();
        if (!normalized.isEmpty() && !m_platforms.contains(normalized))
            m_platforms.append(normalized);
    }
    if (m_dataDirectory.isEmpty())
        return false;

    if (!createDirectory(m_dataDirectory))
        return false;

    const QStringList directories = {
        "config",
        "launchers",
        "downloads/staging",
        "artwork",
        "database"
    };

    for (const QString &directory : directories) {
        if (!createDirectory(QDir(m_dataDirectory).filePath(directory)))
            return false;
    }

    m_settingsFilePath = QDir(m_dataDirectory).filePath(
        QStringLiteral("config/settings.ini"));

    // Migrate existing QSettings preferences into the application data directory
    // once. This preserves the library root and preferences from older versions.
    if (!QFileInfo::exists(m_settingsFilePath)) {
        QSettings legacySettings;
        QSettings migratedSettings(m_settingsFilePath, QSettings::IniFormat);
        for (const QString &key : legacySettings.allKeys())
            migratedSettings.setValue(key, legacySettings.value(key));
        migratedSettings.sync();
        if (migratedSettings.status() != QSettings::NoError) {
            qWarning() << "Could not migrate legacy settings to" << m_settingsFilePath;
            return false;
        }
        // Clear the old location after a successful migration. This prevents a
        // deleted settings.ini from being silently recreated from stale settings.
        legacySettings.clear();
        legacySettings.sync();
    }

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    m_libraryDirectory = QDir::cleanPath(settings.value(
        QStringLiteral("library/root")).toString());
    if (!m_libraryDirectory.isEmpty()) {
        const QFileInfo rootInfo(m_libraryDirectory);
        if (!rootInfo.exists() || !rootInfo.isDir() || !rootInfo.isReadable())
            m_libraryDirectory.clear();
        else
            m_libraryDirectory = rootInfo.absoluteFilePath();
    }

    for (const QString &platform : m_platforms) {
        const QString launcherPath =
            QDir(m_dataDirectory).filePath("launchers/" + platform);

        if ((!m_libraryDirectory.isEmpty()
             && !createDirectory(QDir(m_libraryDirectory).filePath(platform)))
            || !createDirectory(launcherPath))
            return false;
    }

    return true;
}

QString PlatformManager::dataDirectory() const
{
    return m_dataDirectory;
}

QString PlatformManager::settingsFilePath() const
{
    return m_settingsFilePath;
}

QString PlatformManager::libraryDirectory() const
{
    return m_libraryDirectory;
}

bool PlatformManager::libraryConfigured() const
{
    return !m_libraryDirectory.isEmpty();
}

QString PlatformManager::stagingDirectory() const
{
    if (m_dataDirectory.isEmpty())
        return {};

    return QDir(m_dataDirectory).filePath("downloads/staging");
}

QStringList PlatformManager::platforms() const
{
    return m_platforms;
}

bool PlatformManager::setLibraryDirectory(const QUrl &directoryUrl)
{
    if (!directoryUrl.isLocalFile())
        return false;

    const QFileInfo selectedInfo(QDir::cleanPath(directoryUrl.toLocalFile()));
    if (!selectedInfo.exists() || !selectedInfo.isDir() || !selectedInfo.isReadable())
        return false;

    const QString selectedPath = selectedInfo.absoluteFilePath();
    for (const QString &platform : m_platforms) {
        if (!createDirectory(QDir(selectedPath).filePath(platform)))
            return false;
    }

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("library/root"), selectedPath);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return false;

    m_libraryDirectory = selectedPath;
    emit libraryDirectoryChanged();
    return true;
}

bool PlatformManager::setLibraryDirectoryPath(const QString &directoryPath)
{
    if (directoryPath.trimmed().isEmpty())
        return false;

    return setLibraryDirectory(QUrl::fromLocalFile(directoryPath));
}

QString PlatformManager::homeDirectory() const
{
    return QDir::homePath();
}

QVariantMap PlatformManager::suggestedLibraryLocation() const
{
    const QDir home(QDir::homePath());
    const QString games = home.filePath(QStringLiteral("Games"));
    const QString path = QFileInfo(games).isDir()
        ? QDir(games).filePath(QStringLiteral("RecompStation"))
        : home.filePath(QStringLiteral("RecompStation"));
    QString existing = path;
    while (!QFileInfo::exists(existing) && existing != QDir::rootPath())
        existing = QFileInfo(existing).absolutePath();
    return QVariantMap{
        {QStringLiteral("path"), path},
        {QStringLiteral("exists"), QFileInfo(path).isDir()},
        {QStringLiteral("freeBytes"), QStorageInfo(existing).bytesAvailable()},
    };
}

bool PlatformManager::useLibraryLocation(const QString &directoryPath)
{
    const QString path = QDir::cleanPath(directoryPath.trimmed());
    if (path.isEmpty() || !QDir().mkpath(path))
        return false;
    return setLibraryDirectoryPath(path);
}

QString PlatformManager::parentDirectory(const QString &directoryPath) const
{
    const QFileInfo info(directoryPath);
    if (!info.exists() || !info.isDir())
        return {};

    QDir parent(info.absoluteFilePath());
    if (!parent.cdUp())
        return info.absoluteFilePath();

    return parent.absolutePath();
}

QVariantList PlatformManager::libraryDirectories(const QString &directoryPath) const
{
    QVariantList result;
    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.exists() || !directoryInfo.isDir() || !directoryInfo.isReadable())
        return result;

    const QDir directory(directoryInfo.absoluteFilePath());
    const QFileInfoList entries = directory.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable | QDir::Hidden,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo &entry : entries) {
        if (!entry.isDir() || !entry.isReadable())
            continue;

        QVariantMap item;
        item.insert(QStringLiteral("name"), entry.fileName());
        item.insert(QStringLiteral("path"), entry.absoluteFilePath());
        result.append(item);
    }

    return result;
}

bool PlatformManager::createLibraryFolder(
    const QString &parentPath,
    const QString &folderName)
{
    const QString name = folderName.trimmed();
    if (name.isEmpty() || name == QStringLiteral(".")
        || name == QStringLiteral("..")
        || name.contains(QLatin1Char('/'))
        || name.contains(QLatin1Char('\\'))
        || name.contains(QChar::Null))
        return false;

    const QFileInfo parentInfo(parentPath);
    if (!parentInfo.exists() || !parentInfo.isDir() || !parentInfo.isWritable())
        return false;

    QDir parent(parentInfo.absoluteFilePath());
    if (parent.exists(name))
        return false;

    return parent.mkdir(name);
}

QString PlatformManager::platformDirectory(const QString &platform) const
{
    if (!m_platforms.contains(platform) || m_libraryDirectory.isEmpty())
        return {};

    return QDir(m_libraryDirectory).filePath(platform);
}

QString PlatformManager::launcherDirectory(const QString &projectId) const
{
    if (m_dataDirectory.isEmpty() || !isValidProjectId(projectId))
        return {};

    return QDir(m_dataDirectory).filePath("launchers/" + projectId);
}

bool PlatformManager::createLauncherDirectory(const QString &projectId)
{
    const QString path = launcherDirectory(projectId);
    if (path.isEmpty())
        return false;

    return createDirectory(path);
}

bool PlatformManager::createDirectory(const QString &path)
{
    QDir directory;
    return directory.mkpath(path);
}

bool PlatformManager::isValidProjectId(const QString &projectId)
{
    static const QRegularExpression projectIdPattern(
        QStringLiteral("^[a-z0-9][a-z0-9-]*$")
    );

    return projectIdPattern.match(projectId).hasMatch();
}
