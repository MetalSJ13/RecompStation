#include "ScreenshotManager.h"
#include "Logging.h"
#include "../database/DatabaseManager.h"

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

void ScreenshotManager::setProviderEnabled(bool enabled)
{
    m_providerEnabled = enabled;
}

void ScreenshotManager::refreshLibraryScreenshots()
{
    if (m_busy || !m_database) return;
    if (!m_providerEnabled) {
        setStatus("Screenshot downloads are disabled in Metadata Settings.");
        return;
    }
    if (m_apiKey.isEmpty()) {
        setStatus("Screenshot downloads require a configured TheGamesDB API key.");
        return;
    }

    m_queue.clear();
    const QVariantList games = m_database->gamesWithMetadata();
    for (const QVariant &entry : games)
        m_queue.enqueue(entry.toMap());

    m_downloaded = 0;
    m_withoutScreenshots = 0;
    m_failed = 0;
    if (m_queue.isEmpty()) {
        setStatus("No games with saved metadata are available for screenshot refresh.");
        emit refreshFinished(0, 0, 0);
        return;
    }
    setBusy(true);
    setStatus(QString("Checking screenshots for %1 metadata-matched game(s)...").arg(m_queue.size()));
    processNextGame();
}

void ScreenshotManager::processNextGame()
{
    if (m_queue.isEmpty()) {
        setBusy(false);
        setStatus(QString("Screenshot refresh complete: %1 downloaded, %2 had none, %3 failed.")
                      .arg(m_downloaded).arg(m_withoutScreenshots).arg(m_failed));
        emit refreshFinished(m_downloaded, m_withoutScreenshots, m_failed);
        return;
    }

    const QVariantMap game = m_queue.dequeue();
    m_currentGameId = game.value("id").toInt();
    m_currentProviderGameId = game.value("metadataGameId").toInt();
    const QString provider = game.value("metadataProvider").toString();
    if (provider != "thegamesdb" || m_currentGameId <= 0 || m_currentProviderGameId <= 0) {
        processNextGame();
        return;
    }

    QUrl url("https://api.thegamesdb.net/v1/Games/Images");
    QUrlQuery query;
    query.addQueryItem("apikey", m_apiKey);
    query.addQueryItem("games_id", QString::number(m_currentProviderGameId));
    query.addQueryItem("filter[type]", "screenshot");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcArtwork) << "Screenshot list request failed for game" << m_currentGameId
                                 << "HTTP" << status << reply->errorString();
            reply->deleteLater();
            ++m_failed;
            processNextGame();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
        reply->deleteLater();
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            ++m_failed;
            processNextGame();
            return;
        }

        const QJsonObject data = document.object().value("data").toObject();
        const QString base = data.value("base_url").toObject().value("original").toString();
        const QJsonArray images = data.value("images").toObject()
                                      .value(QString::number(m_currentProviderGameId)).toArray();
        m_pendingUrls.clear();
        for (const QJsonValue &entry : images) {
            if (!entry.isObject()) continue;
            const QJsonObject image = entry.toObject();
            if (image.value("type").toString() != "screenshot") continue;
            const QString filename = image.value("filename").toString().trimmed();
            if (filename.isEmpty() || base.isEmpty()) continue;
            QString root = base;
            if (!root.endsWith('/')) root += '/';
            const QUrl resolved = QUrl(root).resolved(QUrl(filename));
            if (resolved.scheme() == "https" && !resolved.host().isEmpty())
                m_pendingUrls.append(resolved.toString());
        }

        // Replace a game's old screenshot set only after a valid provider
        // response has arrived; a network failure leaves its existing cache intact.
        const QString cacheDirectory = QDir(m_artworkDirectory)
                                           .filePath(QString("screenshots/%1").arg(m_currentGameId));
        if (!m_database->clearGameScreenshots(m_currentGameId)) {
            ++m_failed;
            processNextGame();
            return;
        }
        QDir(cacheDirectory).removeRecursively();

        if (m_pendingUrls.isEmpty()) {
            ++m_withoutScreenshots;
            processNextGame();
            return;
        }
        downloadNextImage();
    });
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
            QString("%1.%2").arg(m_downloaded + 1).arg(extension));
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
        downloadNextImage();
    });
}
