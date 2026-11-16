#include "LibraryScanner.h"

#include "PlatformManager.h"
#include "../database/DatabaseManager.h"
#include "Logging.h"

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

int LibraryScanner::lastScanCount() const
{
    return m_lastScanCount;
}

QString LibraryScanner::lastError() const
{
    return m_lastError;
}

QStringList LibraryScanner::supportedExtensions(const QString &platform) const
{
    if (platform == "N64")
        return {"z64", "n64", "v64"};
    if (platform == "PS1")
        return {"cue", "chd", "pbp", "iso", "bin", "img"};
    if (platform == "GameCube")
        return {"iso", "gcm", "rvz", "ciso"};
    if (platform == "Wii")
        return {"iso", "wbfs", "rvz", "wud", "wux"};

    return {};
}

bool LibraryScanner::isDiscCompanionFile(const QString &filePath) const
{
    const QFileInfo info(filePath);
    const QString extension = info.suffix().toLower();

    if (extension != "bin" && extension != "img")
        return false;

    const QStringList files = info.dir().entryList(QDir::Files);
    for (const QString &fileName : files) {
        if (QFileInfo(fileName).suffix().compare(
                "cue", Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

int LibraryScanner::scanAll()
{
    qCInfo(lcLibrary) << "Starting full library scan.";
    m_scanningAll = true;

    int totalFound = 0;
    QStringList errors;

    for (const QString &platform : m_platformManager->platforms()) {
        totalFound += scanPlatform(platform);

        if (!m_lastError.isEmpty())
            errors.append(m_lastError);
    }

    m_scanningAll = false;
    m_lastScanCount = totalFound;
    m_lastError = errors.join("\n");

    qCInfo(lcLibrary) << "Full library scan finished. Registered games:" << m_lastScanCount;
    if (!m_lastError.isEmpty())
        qCWarning(lcLibrary) << "Full library scan reported errors:" << m_lastError;
    emit scanCompleted(m_lastScanCount);
    return m_lastScanCount;
}

int LibraryScanner::scanPlatform(const QString &platform)
{
    qCDebug(lcLibrary) << "Scanning platform:" << platform;
    m_lastScanCount = 0;
    m_lastError.clear();

    const QString directoryPath =
        m_platformManager->platformDirectory(platform);
    const QStringList extensions = supportedExtensions(platform);

    if (directoryPath.isEmpty() || extensions.isEmpty()) {
        m_lastError = "Unknown platform: " + platform;
        qCWarning(lcLibrary) << m_lastError;
        if (!m_scanningAll)
            emit scanCompleted(m_lastScanCount);
        return m_lastScanCount;
    }

    if (!QDir(directoryPath).exists()) {
        m_lastError = "Game directory does not exist: " + directoryPath;
        qCWarning(lcLibrary) << m_lastError;
        if (!m_scanningAll)
            emit scanCompleted(m_lastScanCount);
        return m_lastScanCount;
    }

    QSet<QString> acceptedExtensions;
    for (const QString &extension : extensions)
        acceptedExtensions.insert(extension);

    QStringList discoveredPaths;

    QDirIterator iterator(
        directoryPath,
        QDir::Files | QDir::Readable | QDir::NoSymLinks,
        QDirIterator::Subdirectories
    );

    while (iterator.hasNext()) {
        const QString filePath = iterator.next();
        const QFileInfo info(filePath);
        const QString extension = info.suffix().toLower();

        if (!acceptedExtensions.contains(extension))
            continue;

        if (isDiscCompanionFile(filePath))
            continue;

        const QString title = info.completeBaseName().trimmed();
        if (title.isEmpty())
            continue;

        const QString absolutePath = info.absoluteFilePath();
        discoveredPaths.append(absolutePath);

        if (m_databaseManager->registerGame(
                title, platform, absolutePath)) {
            ++m_lastScanCount;
        } else if (!m_databaseManager->lastError().isEmpty()) {
            m_lastError = m_databaseManager->lastError();
        }
    }

    // Reconcile only after a complete scan without database errors.
    if (m_lastError.isEmpty()
        && !m_databaseManager->reconcilePlatformGames(platform, discoveredPaths)) {
        m_lastError = m_databaseManager->lastError();
    }

    qCInfo(lcLibrary) << "Platform scan finished:" << platform
                      << "registered:" << m_lastScanCount
                      << "discovered:" << discoveredPaths.size();
    if (!m_lastError.isEmpty())
        qCWarning(lcLibrary) << "Platform scan error:" << platform << m_lastError;
    if (!m_scanningAll)
        emit scanCompleted(m_lastScanCount);

    return m_lastScanCount;
}
