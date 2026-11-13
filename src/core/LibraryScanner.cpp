#include "LibraryScanner.h"

#include "PlatformManager.h"
#include "../database/DatabaseManager.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

LibraryScanner::LibraryScanner(PlatformManager *platformManager,
                               DatabaseManager *databaseManager,
                               QObject *parent)
    : QObject(parent),
      m_platformManager(platformManager),
      m_databaseManager(databaseManager)
{
}

int LibraryScanner::lastScanCount() const { return m_lastScanCount; }
QString LibraryScanner::lastError() const { return m_lastError; }

QStringList LibraryScanner::supportedExtensions(const QString &platform) const
{
    if (platform == "N64") return {"z64", "n64", "v64"};
    if (platform == "PS1") return {"cue", "chd", "pbp", "iso", "bin", "img"};
    if (platform == "GameCube") return {"iso", "gcm", "rvz", "ciso"};
    if (platform == "Wii") return {"iso", "wbfs", "rvz", "wud", "wux"};
    return {};
}

bool LibraryScanner::isDiscCompanionFile(const QString &filePath) const
{
    const QFileInfo info(filePath);
    const QString extension = info.suffix().toLower();
    if (extension != "bin" && extension != "img") return false;

    return !info.dir().entryList({"*.cue", "*.CUE"}, QDir::Files).isEmpty();
}

int LibraryScanner::scanAll()
{
    int totalFound = 0;
    QStringList errors;

    for (const QString &platform : m_platformManager->platforms()) {
        const int platformCount = scanPlatform(platform);
        totalFound += platformCount;
        if (!m_lastError.isEmpty())
            errors.append(m_lastError);
    }

    m_lastScanCount = totalFound;
    m_lastError = errors.join("\\n");
    emit scanCompleted(m_lastScanCount);
    return m_lastScanCount;
}

int LibraryScanner::scanPlatform(const QString &platform)
{
    m_lastScanCount = 0;
    m_lastError.clear();

    const QString directoryPath = m_platformManager->platformDirectory(platform);
    const QStringList extensions = supportedExtensions(platform);
    if (directoryPath.isEmpty() || extensions.isEmpty()) {
        m_lastError = "Unknown platform: " + platform;
        emit scanCompleted(m_lastScanCount);
        return m_lastScanCount;
    }

    if (!QDir(directoryPath).exists()) {
        m_lastError = "Game directory does not exist: " + directoryPath;
        emit scanCompleted(m_lastScanCount);
        return m_lastScanCount;
    }

    QSet<QString> acceptedExtensions;
    for (const QString &extension : extensions) acceptedExtensions.insert(extension);

    QDirIterator iterator(directoryPath, QDir::Files | QDir::Readable | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString filePath = iterator.next();
        const QFileInfo info(filePath);
        if (!acceptedExtensions.contains(info.suffix().toLower())) continue;
        if (isDiscCompanionFile(filePath)) continue;

        const QString title = info.completeBaseName().trimmed();
        if (title.isEmpty()) continue;

        if (m_databaseManager->registerGame(title, platform, info.absoluteFilePath())) {
            ++m_lastScanCount;
        } else if (!m_databaseManager->lastError().isEmpty()) {
            m_lastError = m_databaseManager->lastError();
        }
    }

    emit scanCompleted(m_lastScanCount);
    return m_lastScanCount;
}
