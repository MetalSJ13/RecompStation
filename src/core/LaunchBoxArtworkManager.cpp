#include "LaunchBoxArtworkManager.h"

#include "Logging.h"
#include "../database/DatabaseManager.h"
#include "../metadata/LaunchBoxCatalogDatabase.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>
#include <algorithm>

namespace {
QString supportedImageExtension(const QByteArray &contentType, const QByteArray &bytes)
{
    const QByteArray mime = contentType.split(';').first().trimmed().toLower();
    if (mime == "image/jpeg" || mime == "image/jpg"
        || bytes.startsWith(QByteArray::fromHex("ffd8ff"))) return QStringLiteral("jpg");
    if (mime == "image/png"
        || bytes.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) return QStringLiteral("png");
    if (mime == "image/webp"
        || (bytes.size() >= 12 && bytes.startsWith("RIFF") && bytes.mid(8, 4) == "WEBP"))
        return QStringLiteral("webp");
    return {};
}

bool isGameplayScreenshot(const QString &type)
{
    return type.compare(QStringLiteral("Screenshot - Gameplay"),
                        Qt::CaseInsensitive) == 0;
}

bool isFrontCover(const QString &type)
{
    return type.compare(QStringLiteral("Box - Front"), Qt::CaseInsensitive) == 0;
}
}

LaunchBoxArtworkManager::LaunchBoxArtworkManager(
    DatabaseManager *database, const QString &dataDirectory, QObject *parent)
    : QObject(parent),
      m_database(database),
      m_artworkDirectory(QDir(dataDirectory).filePath(QStringLiteral("artwork/launchbox"))),
      m_catalogPath(QDir(dataDirectory).filePath(QStringLiteral("metadata/launchbox/catalog.sqlite")))
{
    m_batchPause.setSingleShot(true);
    m_batchPause.setInterval(300);
    connect(&m_batchPause, &QTimer::timeout, this, &LaunchBoxArtworkManager::processNext);
}

bool LaunchBoxArtworkManager::busy() const { return m_busy; }
QString LaunchBoxArtworkManager::status() const { return m_status; }

void LaunchBoxArtworkManager::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!m_enabled && !m_busy)
        setStatus(QStringLiteral("LaunchBox artwork downloads are disabled in Metadata Settings."));
}

QString LaunchBoxArtworkManager::imageUrl(const QString &fileName)
{
    const QString safeName = fileName.trimmed();
    if (safeName.isEmpty() || safeName.contains(QLatin1Char('/'))
        || safeName.contains(QLatin1Char('\\')) || safeName == QStringLiteral(".")
        || safeName == QStringLiteral(".."))
        return {};
    return QStringLiteral("https://images.launchbox-app.com/")
        + QString::fromLatin1(QUrl::toPercentEncoding(safeName));
}

int LaunchBoxArtworkManager::regionPreference(const QString &region)
{
    const QString value = region.trimmed().toCaseFolded();
    if (value == QStringLiteral("north america") || value == QStringLiteral("usa")
        || value == QStringLiteral("us")) return 0;
    if (value == QStringLiteral("world")) return 1;
    if (value == QStringLiteral("europe") || value == QStringLiteral("eur")) return 2;
    if (value == QStringLiteral("japan") || value == QStringLiteral("jp")) return 3;
    return 4;
}

QList<LaunchBoxArtworkRequest> LaunchBoxArtworkManager::selectRequests(
    int libraryGameId, int launchBoxGameId,
    const QList<LaunchBoxGameImageRecord> &images,
    bool includeCover, int maximumScreenshots)
{
    QList<LaunchBoxGameImageRecord> covers;
    QList<LaunchBoxGameImageRecord> screenshots;
    for (const auto &image : images) {
        if (image.databaseId != launchBoxGameId || image.fileName.trimmed().isEmpty())
            continue;
        if (isFrontCover(image.type))
            covers.append(image);
        else if (isGameplayScreenshot(image.type))
            screenshots.append(image);
    }
    const auto preferred = [](const LaunchBoxGameImageRecord &a,
                              const LaunchBoxGameImageRecord &b) {
        const int regionA = regionPreference(a.region);
        const int regionB = regionPreference(b.region);
        return regionA == regionB ? a.fileName < b.fileName : regionA < regionB;
    };
    std::sort(covers.begin(), covers.end(), preferred);
    std::sort(screenshots.begin(), screenshots.end(), preferred);

    QList<LaunchBoxArtworkRequest> requests;
    if (includeCover && !covers.isEmpty()) {
        const auto &image = covers.first();
        requests.append({libraryGameId, image.fileName, image.type,
                         QStringLiteral("cover.") + QFileInfo(image.fileName).suffix().toLower()});
    }
    const int count = qBound(0, maximumScreenshots, screenshots.size());
    for (int i = 0; i < count; ++i) {
        const auto &image = screenshots.at(i);
        requests.append({libraryGameId, image.fileName, image.type,
                         QStringLiteral("screenshots/%1.%2").arg(i + 1)
                             .arg(QFileInfo(image.fileName).suffix().toLower())});
    }
    return requests;
}

void LaunchBoxArtworkManager::refreshLibraryArtwork()
{
    if (!m_database)
        return;
    if (!m_enabled) {
        setStatus(QStringLiteral("LaunchBox artwork downloads are disabled in Metadata Settings."));
        return;
    }
    if (m_busy) {
        m_refreshPending = true;
        return;
    }

    m_queue.clear();
    m_downloaded = m_skipped = m_failed = m_batchRequests = m_totalRequests = 0;

    LaunchBoxCatalogDatabase catalog;
    if (!catalog.open(m_catalogPath)) {
        setStatus(QStringLiteral("LaunchBox artwork skipped: catalog could not be opened."));
        emit refreshFinished(0, 0, 0);
        return;
    }

    const QVariantList games = m_database->games();
    int eligibleGames = 0;
    for (const QVariant &entry : games) {
        const QVariantMap game = entry.toMap();
        if (game.value(QStringLiteral("metadataProvider")).toString()
                != QStringLiteral("launchbox"))
            continue;
        const int localId = game.value(QStringLiteral("id")).toInt();
        const int catalogId = game.value(QStringLiteral("metadataGameId")).toInt();
        if (localId <= 0 || catalogId <= 0)
            continue;
        ++eligibleGames;

        const QString currentArtwork = QUrl(game.value(QStringLiteral("artworkPath")).toString())
                                           .toLocalFile();
        const bool coverExists = !currentArtwork.isEmpty() && QFileInfo(currentArtwork).isFile();
        const auto requests = selectRequests(localId, catalogId,
                                             catalog.imagesByDatabaseId(catalogId),
                                             !coverExists, 3);
        for (const auto &request : requests) {
            const QString url = imageUrl(request.fileName);
            if (url.isEmpty()) {
                ++m_skipped;
                continue;
            }
            const QString destination = QDir(m_artworkDirectory)
                                            .filePath(QString::number(localId) + QLatin1Char('/')
                                                      + request.destination);
            if (QFileInfo(destination).isFile()) {
                bool linked = true;
                if (request.type == QStringLiteral("Box - Front"))
                    linked = m_database->setArtworkPath(localId, destination);
                else if (request.type == QStringLiteral("Screenshot - Gameplay"))
                    linked = m_database->addGameScreenshot(localId, destination);
                if (linked)
                    ++m_skipped;
                else
                    ++m_failed;
                continue;
            }
            m_queue.enqueue({localId, destination, request.type, url});
        }
    }
    catalog.close();

    if (m_queue.isEmpty()) {
        setStatus(QStringLiteral("LaunchBox artwork refresh: nothing to download for %1 library game(s).")
                      .arg(eligibleGames));
        emit refreshFinished(0, m_skipped, 0);
        return;
    }
    m_totalRequests = m_queue.size();
    setBusy(true);
    setStatus(QStringLiteral("Queued %1 LaunchBox images for %2 matched library game(s).")
                  .arg(m_totalRequests).arg(eligibleGames));
    qCInfo(lcArtwork) << "LaunchBox artwork queue contains" << m_queue.size()
                      << "images for" << eligibleGames << "library games.";
    processNext();
}

void LaunchBoxArtworkManager::processNext()
{
    if (!m_enabled) {
        m_queue.clear();
        setBusy(false);
        setStatus(QStringLiteral("LaunchBox artwork paused: disabled in Metadata Settings (%1 downloaded, %2 failed).")
                      .arg(m_downloaded).arg(m_failed));
        emit refreshFinished(m_downloaded, m_skipped, m_failed);
        return;
    }
    if (m_queue.isEmpty()) {
        setBusy(false);
        setStatus(QStringLiteral("LaunchBox artwork complete: %1 downloaded, %2 skipped, %3 failed.")
                      .arg(m_downloaded).arg(m_skipped).arg(m_failed));
        qCInfo(lcArtwork) << m_status;
        emit refreshFinished(m_downloaded, m_skipped, m_failed);
        if (m_refreshPending) {
            m_refreshPending = false;
            QTimer::singleShot(0, this, &LaunchBoxArtworkManager::refreshLibraryArtwork);
        }
        return;
    }
    if (m_batchRequests >= 5) {
        m_batchRequests = 0;
        m_batchPause.start();
        return;
    }

    const PendingRequest pending = m_queue.dequeue();
    const QUrl url(pending.url);
    if (!url.isValid() || url.scheme() != QStringLiteral("https")
        || url.host() != QStringLiteral("images.launchbox-app.com")) {
        ++m_failed;
        ++m_batchRequests;
        processNext();
        return;
    }

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "image/*");
    request.setRawHeader("User-Agent", "RecompStation");
    request.setTransferTimeout(20000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, pending]() {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->readAll();
        const QString extension = supportedImageExtension(
            reply->header(QNetworkRequest::ContentTypeHeader).toByteArray(), bytes);
        const QUrl finalUrl = reply->url();
        const bool networkOk = reply->error() == QNetworkReply::NoError
            && statusCode >= 200 && statusCode < 300
            && finalUrl.scheme() == QStringLiteral("https")
            && finalUrl.host() == QStringLiteral("images.launchbox-app.com");
        const QString networkError = reply->errorString();
        reply->deleteLater();

        bool saved = false;
        if (networkOk && !bytes.isEmpty() && bytes.size() <= 20 * 1024 * 1024
            && !extension.isEmpty()) {
            const QString destination = QDir(m_artworkDirectory)
                .filePath(QString::number(pending.gameId) + QLatin1Char('/')
                          + pending.destination);
            if (QDir().mkpath(QFileInfo(destination).absolutePath())) {
                QSaveFile file(destination);
                saved = file.open(QIODevice::WriteOnly)
                    && file.write(bytes) == bytes.size() && file.commit();
                if (saved && pending.type == QStringLiteral("Box - Front")) {
                    if (!m_database->setArtworkPath(pending.gameId, destination)) {
                        QFile::remove(destination);
                        saved = false;
                    }
                } else if (saved && pending.type == QStringLiteral("Screenshot - Gameplay")) {
                    if (!m_database->addGameScreenshot(pending.gameId, destination)) {
                        QFile::remove(destination);
                        saved = false;
                    }
                }
            }
        }
        if (saved) {
            ++m_downloaded;
        } else {
            ++m_failed;
            qCWarning(lcArtwork) << "LaunchBox image failed for library game"
                                 << pending.gameId << "HTTP" << statusCode
                                 << networkError;
        }
        ++m_batchRequests;
        setStatus(QStringLiteral("LaunchBox artwork: %1/%2 processed, %3 skipped, %4 failed.")
                      .arg(m_downloaded + m_failed).arg(m_totalRequests)
                      .arg(m_skipped).arg(m_failed));
        processNext();
    });
}

void LaunchBoxArtworkManager::setBusy(bool value)
{
    if (m_busy == value) return;
    m_busy = value;
    emit busyChanged();
}

void LaunchBoxArtworkManager::setStatus(const QString &value)
{
    if (m_status == value) return;
    m_status = value;
    emit statusChanged();
}
