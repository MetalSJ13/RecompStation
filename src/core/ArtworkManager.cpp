#include "ArtworkManager.h"
#include "Logging.h"

#include "../database/DatabaseManager.h"

#include <QDir>
#include <QByteArray>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>

namespace {
QString imageExtension(const QByteArray &contentType, const QByteArray &bytes)
{
    const QByteArray mime = contentType.split(';').first().trimmed().toLower();
    if (mime == "image/jpeg" || mime == "image/jpg") return QStringLiteral("jpg");
    if (mime == "image/png") return QStringLiteral("png");
    if (mime == "image/webp") return QStringLiteral("webp");
    if (mime == "image/gif") return QStringLiteral("gif");

    if (bytes.startsWith(QByteArray::fromHex("ffd8ff"))) return QStringLiteral("jpg");
    if (bytes.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) return QStringLiteral("png");
    if (bytes.startsWith("GIF87a") || bytes.startsWith("GIF89a")) return QStringLiteral("gif");
    if (bytes.size() >= 12 && bytes.startsWith("RIFF")
        && bytes.mid(8, 4) == "WEBP") return QStringLiteral("webp");
    return {};
}
}

ArtworkManager::ArtworkManager(DatabaseManager *database,
                               const QString &artworkDirectory,
                               QObject *parent)
    : QObject(parent),
      m_database(database),
      m_artworkDirectory(artworkDirectory)
{
}

void ArtworkManager::downloadArtwork(int gameId, const QString &artworkUrl)
{
    qCDebug(lcArtwork) << "Artwork download requested for library game:" << gameId;
    const QUrl url(artworkUrl);
    if (!m_database || gameId <= 0 || !url.isValid()
        || url.scheme() != QStringLiteral("https") || url.host().isEmpty()) {
        qCWarning(lcArtwork) << "Rejected invalid artwork request for game" << gameId;
        emit artworkFailed(gameId, QStringLiteral("A valid HTTPS artwork URL and local game are required."));
        return;
    }

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "image/*");
    request.setTransferTimeout(20000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gameId]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            const QString message = QStringLiteral("Artwork download failed (HTTP %1): %2")
                                        .arg(status).arg(reply->errorString());
            reply->deleteLater();
            qCWarning(lcArtwork) << "Artwork request failed for game" << gameId << ":" << message;
            emit artworkFailed(gameId, message);
            return;
        }

        const QByteArray bytes = reply->readAll();
        const QString extension = imageExtension(reply->header(QNetworkRequest::ContentTypeHeader).toByteArray(), bytes);
        reply->deleteLater();

        if (bytes.isEmpty() || bytes.size() > 20 * 1024 * 1024 || extension.isEmpty()) {
            qCWarning(lcArtwork) << "Artwork response rejected for game" << gameId
                                 << "bytes:" << bytes.size();
            emit artworkFailed(gameId, QStringLiteral("The artwork response was empty, too large, or not a supported image."));
            return;
        }

        if (!QDir().mkpath(m_artworkDirectory)) {
            qCCritical(lcArtwork) << "Could not create artwork cache directory:" << m_artworkDirectory;
            emit artworkFailed(gameId, QStringLiteral("Unable to create the local artwork cache directory."));
            return;
        }

        const QString path = QDir(m_artworkDirectory)
                                 .filePath(QString::number(gameId) + "." + extension);
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()
            || !file.commit()) {
            const QString error = file.errorString();
            qCCritical(lcArtwork) << "Could not save artwork for game" << gameId << ":" << error;
            emit artworkFailed(gameId, QStringLiteral("Unable to save artwork: %1").arg(error));
            return;
        }

        if (!m_database->setArtworkPath(gameId, path)) {
            QFile::remove(path);
            qCCritical(lcArtwork) << "Could not update artwork path for game" << gameId
                                  << ":" << m_database->lastError();
            emit artworkFailed(gameId, m_database->lastError());
            return;
        }

        qCDebug(lcArtwork) << "Artwork saved for game" << gameId;
        emit artworkSaved(gameId, path);
    });
}
