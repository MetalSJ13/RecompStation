
#include "PlatformManager.h"

#include <QDir>
#include <QStandardPaths>

PlatformManager::PlatformManager(QObject *parent)
: QObject(parent)
{
    m_platforms = {
        "N64",
        "PS1",
        "GameCube",
        "Wii"
    };

    m_dataDirectory =
    QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation
    );
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
        "artwork",
        "database"
    };

    for (const QString &directory : directories) {
        if (!createDirectory(
            m_dataDirectory + "/" + directory
        )) {
            return false;
        }
    }

    for (const QString &platform : m_platforms) {

        const QString gamePath =
        m_dataDirectory + "/games/" + platform;

        const QString launcherPath =
        m_dataDirectory + "/launchers/" + platform;

        if (!createDirectory(gamePath))
            return false;

        if (!createDirectory(launcherPath))
            return false;
    }

    return true;
}

QString PlatformManager::dataDirectory() const
{
    return m_dataDirectory;
}

QStringList PlatformManager::platforms() const
{
    return m_platforms;
}

QString PlatformManager::platformDirectory(
    const QString &platform
) const
{
    if (!m_platforms.contains(platform))
        return {};

    return m_dataDirectory +
    "/games/" + platform;
}

bool PlatformManager::createDirectory(
    const QString &path
)
{
    QDir directory;

    return directory.mkpath(path);
}
