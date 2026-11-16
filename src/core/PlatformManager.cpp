#include "PlatformManager.h"

#include <QDir>
#include <QRegularExpression>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

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
        "Wii"
    };

    m_dataDirectory = dataDirectory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : QDir(dataDirectory).absolutePath();
}

bool PlatformManager::initialize()
{
    if (m_dataDirectory.isEmpty())
        return false;

    if (!createDirectory(m_dataDirectory))
        return false;

    const QStringList directories = {
        "games",
        "launchers",
        "downloads/staging",
        "artwork",
        "database"
    };

    for (const QString &directory : directories) {
        if (!createDirectory(QDir(m_dataDirectory).filePath(directory)))
            return false;
    }

    QSettings settings;
    for (const QString &platform : m_platforms) {
        const QString defaultGamePath =
            QDir(m_dataDirectory).filePath("games/" + platform);
        const QString configuredPath = settings.value(
            QStringLiteral("library/folders/") + platform,
            defaultGamePath
        ).toString();
        m_platformDirectories.insert(platform, QDir(configuredPath).absolutePath());
        const QString gamePath = defaultGamePath;
        const QString launcherPath =
            QDir(m_dataDirectory).filePath("launchers/" + platform);

        if (!createDirectory(gamePath) || !createDirectory(launcherPath))
            return false;
    }

    return true;
}

QString PlatformManager::dataDirectory() const
{
    return m_dataDirectory;
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

bool PlatformManager::setPlatformDirectory(const QString &platform,
                                            const QUrl &directoryUrl)
{
    if (!m_platforms.contains(platform) || !directoryUrl.isLocalFile())
        return false;

    const QFileInfo selectedInfo(QDir::cleanPath(directoryUrl.toLocalFile()));
    if (!selectedInfo.exists() || !selectedInfo.isDir() || !selectedInfo.isReadable())
        return false;

    const QString absolutePath = selectedInfo.absoluteFilePath();
    if (m_platformDirectories.value(platform) == absolutePath)
        return true;

    QSettings settings;
    settings.setValue(QStringLiteral("library/folders/") + platform, absolutePath);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return false;

    m_platformDirectories.insert(platform, absolutePath);
    emit platformDirectoryChanged(platform, absolutePath);
    return true;
}

QString PlatformManager::platformDirectory(const QString &platform) const
{
    if (!m_platforms.contains(platform))
        return {};

    return m_platformDirectories.value(platform);
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
