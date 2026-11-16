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

    QSettings settings;
    settings.setValue(QStringLiteral("library/root"), selectedPath);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return false;

    m_libraryDirectory = selectedPath;
    emit libraryDirectoryChanged();
    return true;
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
