#include "ScreenshotManager.h"
#include "Logging.h"
#include "../database/DatabaseManager.h"
#include "../metadata/TheGamesDbAllowance.h"
#include "../metadata/TheGamesDbClient.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>
#include <QUrlQuery>

namespace {
QString screenshotExtension(const QByteArray &contentType, const QByteArray &bytes)
{
    const QByteArray mime = contentType.split(';').first().trimmed().toLower();
    if (mime == "image/jpeg" || mime == "image/jpg" || bytes.startsWith(QByteArray::fromHex("ffd8ff"))) return "jpg";
    if (mime == "image/png" || bytes.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) return "png";
    if (mime == "image/webp" || (bytes.size() >= 12 && bytes.startsWith("RIFF") && bytes.mid(8, 4) == "WEBP")) return "webp";
    return {};
}
}

ScreenshotManager::ScreenshotManager(DatabaseManager *database,
                                     const QString &artworkDirectory,
                                     const QString &apiKey,
                                     QObject *parent)
    : QObject(parent), m_database(database),
      m_artworkDirectory(artworkDirectory),
      m_apiKey(apiKey.trimmed())
{
}

bool ScreenshotManager::isBusy() const { return m_busy; }
QString ScreenshotManager::status() const { return m_status; }

void ScreenshotManager::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged();
}

void ScreenshotManager::setStatus(const QString &status)
{
    if (m_status == status) return;
    m_status = status;
    emit statusChanged();
}

void ScreenshotManager::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey.trimmed();
}

void ScreenshotManager::setProviderEnabled(bool enabled)
{
    m_providerEnabled = enabled;
}

void ScreenshotManager::setAllowance(TheGamesDbAllowance *allowance)
{
    m_allowance = allowance;
}

bool ScreenshotManager::parseImagesResponse(const QByteArray &response,
                                            QHash<int, QStringList> *screenshots,
                                            int *nextPage,
                                            QString *errorMessage)
{
    if (nextPage)
        *nextPage = 0;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid TheGamesDB images response.");
        return false;
    }
    const QJsonObject root = document.object();
    const QJsonObject data = root.value("data").toObject();
    const QString base = data.value("base_url").toObject().value("original").toString();
    const QJsonObject images = data.value("images").toObject();
    for (auto it = images.begin(); it != images.end(); ++it) {
        bool idOk = false;
        const int providerGameId = it.key().toInt(&idOk);
        if (!idOk || providerGameId <= 0 || !it.value().isArray())
            continue;
        QStringList urls;
        for (const QJsonValue &entry : it.value().toArray()) {
            if (!entry.isObject()) continue;
            const QJsonObject image = entry.toObject();
            if (image.value("type").toString() != "screenshot") continue;
            const QString filename = image.value("filename").toString().trimmed();
            if (filename.isEmpty() || base.isEmpty()) continue;
            QString rootUrl = base;
            if (!rootUrl.endsWith('/')) rootUrl += '/';
            const QUrl resolved = QUrl(rootUrl).resolved(QUrl(filename));
            if (resolved.scheme() == "https" && !resolved.host().isEmpty())
                urls.append(resolved.toString());
        }
        if (screenshots && !urls.isEmpty())
            (*screenshots)[providerGameId].append(urls);
    }
    const QString next = root.value("pages").toObject().value("next").toString();
    if (nextPage && !next.isEmpty()) {
        const int page = QUrlQuery(QUrl(next).query()).queryItemValue("page").toInt();
        if (page > 0)
            *nextPage = page;
    }
    if (errorMessage)
        errorMessage->clear();
    return true;
}

void ScreenshotManager::refreshLibraryScreenshots(bool recheckAll)
{
    if (m_busy || !m_database) return;
    if (!m_providerEnabled) {
        setStatus("Screenshot downloads are disabled in Metadata Settings.");
        emit refreshFinished(0, 0, 0, 0);
        return;
    }
    if (m_apiKey.isEmpty()) {
        setStatus("Screenshot downloads need a TheGamesDB API key (Settings > Metadata).");
        emit refreshFinished(0, 0, 0, 0);
        return;
    }

    m_queue.clear();
    m_batch.clear();
    m_batchScreenshots.clear();
    m_downloadQueue.clear();
    m_downloaded = 0;
    m_skipped = 0;
    m_withoutScreenshots = 0;
    m_failed = 0;
    const QVariantList games = m_database->gamesWithMetadata();
    for (const QVariant &entry : games) {
        const QVariantMap game = entry.toMap();
        if (game.value(QStringLiteral("metadataProvider")).toString() != "thegamesdb"
                || game.value(QStringLiteral("id")).toInt() <= 0
                || game.value(QStringLiteral("metadataGameId")).toInt() <= 0)
            continue;
        if (!game.value(QStringLiteral("screenshots")).toList().isEmpty()
                || (!recheckAll && game.value(QStringLiteral("screenshotsCheckedRecently")).toBool())) {
            ++m_skipped;
            continue;
        }
        m_queue.enqueue(game);
    }

    if (m_queue.isEmpty()) {
        setStatus(QString("Screenshot refresh complete: nothing new to look up (%1 game(s) already have screenshots or were checked recently).")
                      .arg(m_skipped));
        emit refreshFinished(0, m_skipped, 0, 0);
        return;
    }
    setBusy(true);
    setStatus(QString("Checking screenshots for %1 game(s)...").arg(m_queue.size()));
    requestNextBatch();
}

void ScreenshotManager::finishRefresh(const QString &note)
{
    setBusy(false);
    QString message = QString("Screenshot refresh complete: %1 downloaded, %2 skipped, %3 had none, %4 failed.")
                          .arg(m_downloaded).arg(m_skipped).arg(m_withoutScreenshots).arg(m_failed);
    if (!note.isEmpty())
        message += QStringLiteral(" ") + note;
    setStatus(message);
    m_queue.clear();
    m_batch.clear();
    m_downloadQueue.clear();
    emit refreshFinished(m_downloaded, m_skipped, m_withoutScreenshots, m_failed);
}

void ScreenshotManager::requestNextBatch()
{
    if (m_queue.isEmpty()) {
        finishRefresh();
        return;
    }
    if (m_allowance && !m_allowance->automaticLookupsAllowed()) {
        const int left = m_queue.size();
        finishRefresh(QString("%1 game(s) left for later: %2")
                          .arg(left).arg(m_allowance->pauseReason()));
        return;
    }
    m_batch.clear();
    m_batchScreenshots.clear();
    while (!m_queue.isEmpty() && m_batch.size() < GamesPerRequest)
        m_batch.append(m_queue.dequeue());
    requestImagesPage(1);
}

void ScreenshotManager::requestImagesPage(int page)
{
    QStringList ids;
    for (const QVariantMap &game : std::as_const(m_batch)) {
        const QString id = QString::number(game.value("metadataGameId").toInt());
        if (!ids.contains(id))
            ids.append(id);
    }

    QUrl url("https://api.thegamesdb.net/v1/Games/Images");
    QUrlQuery query;
    query.addQueryItem("apikey", m_apiKey);
    query.addQueryItem("games_id", ids.join(','));
    query.addQueryItem("filter[type]", "screenshot");
    if (page > 1)
        query.addQueryItem("page", QString::number(page));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, page]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const QNetworkReply::NetworkError error = reply->error();
        const QString errorString = reply->errorString();
        reply->deleteLater();

        if (error != QNetworkReply::NoError) {
            qCWarning(lcArtwork) << "Screenshot list request failed for" << m_batch.size()
                                 << "game(s): HTTP" << status << errorString;
            m_failed += m_batch.size();
            if (status == 403) {
                // No requests left: stop instead of failing every batch.
                if (m_allowance)
                    m_allowance->reportRejected();
                finishRefresh(QString("TheGamesDB has no requests left right now; "
                                      "%1 game(s) left for later.").arg(m_queue.size()));
                return;
            }
            m_batch.clear();
            requestNextBatch();
            return;
        }

        const int allowance = TheGamesDbClient::remainingAllowance(body);
        if (allowance >= 0 && m_allowance)
            m_allowance->report(allowance);

        int nextPage = 0;
        if (!parseImagesResponse(body, &m_batchScreenshots, &nextPage)) {
            m_failed += m_batch.size();
            m_batch.clear();
            requestNextBatch();
            return;
        }
        // Follow extra result pages, with a cap against a misbehaving reply.
        if (nextPage > page && nextPage <= 10) {
            requestImagesPage(nextPage);
            return;
        }
        finishBatch();
    });
}

void ScreenshotManager::finishBatch()
{
    for (const QVariantMap &game : std::as_const(m_batch)) {
        const int gameId = game.value("id").toInt();
        const int providerGameId = game.value("metadataGameId").toInt();
        const QStringList urls = m_batchScreenshots.value(providerGameId);
        if (urls.isEmpty()) {
            ++m_withoutScreenshots;
            // Remember the answer so the next refresh does not ask again.
            m_database->markScreenshotsChecked(gameId, providerGameId);
            continue;
        }
        QVariantMap pending = game;
        pending.insert(QStringLiteral("urls"), urls);
        m_downloadQueue.enqueue(pending);
    }
    m_batch.clear();
    m_batchScreenshots.clear();
    processNextGame();
}

void ScreenshotManager::processNextGame()
{
    if (m_downloadQueue.isEmpty()) {
        requestNextBatch();
        return;
    }
    const QVariantMap game = m_downloadQueue.dequeue();
    m_currentGameId = game.value("id").toInt();
    m_currentProviderGameId = game.value("metadataGameId").toInt();
    m_currentGameImageCount = 0;
    m_pendingUrls = game.value("urls").toStringList();
    downloadNextImage();
}

void ScreenshotManager::downloadNextImage()
{
    if (m_pendingUrls.isEmpty()) {
        processNextGame();
        return;
    }
    const QString imageUrl = m_pendingUrls.takeFirst();
    QNetworkRequest request{QUrl(imageUrl)};
    request.setRawHeader("Accept", "image/*");
    request.setTransferTimeout(20000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->readAll();
        const QString extension = screenshotExtension(
            reply->header(QNetworkRequest::ContentTypeHeader).toByteArray(), bytes);
        reply->deleteLater();

        if (bytes.isEmpty() || bytes.size() > 20 * 1024 * 1024 || extension.isEmpty()) {
            qCWarning(lcArtwork) << "Rejected screenshot image for game" << m_currentGameId
                                 << "HTTP" << status << "bytes" << bytes.size();
            ++m_failed;
            downloadNextImage();
            return;
        }

        const QString directory = QDir(m_artworkDirectory)
                                      .filePath(QString("screenshots/%1").arg(m_currentGameId));
        if (!QDir().mkpath(directory)) {
            ++m_failed;
            downloadNextImage();
            return;
        }

        const QString path = QDir(directory).filePath(
            QString("%1.%2").arg(m_currentGameImageCount + 1).arg(extension));
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            ++m_failed;
            downloadNextImage();
            return;
        }
        if (!m_database->addGameScreenshot(m_currentGameId, path)) {
            QFile::remove(path);
            ++m_failed;
            downloadNextImage();
            return;
        }
        ++m_downloaded;
        ++m_currentGameImageCount;
        downloadNextImage();
    });
}
