#include "ArtworkManager.h"

#include "../database/DatabaseManager.h"

#include <QDir>\n#include <QBuffer>\n#include <QImageReader>
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
    const QUrl url(artworkUrl);
    if (!m_database || gameId <= 0 || !url.isValid()
        || url.scheme() != QStringLiteral("https") || url.host().isEmpty()) {
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
            emit artworkFailed(gameId, message);
            return;
        }

        const QByteArray bytes = reply->readAll();
        const QString extension = imageExtension(reply->header(QNetworkRequest::ContentTypeHeader).toByteArray(), bytes);
        reply->deleteLater();

        if (bytes.isEmpty() || bytes.size() > 20 * 1024 * 1024 || extension.isEmpty()) {
            emit artworkFailed(gameId, QStringLiteral("The artwork response was empty, too large, or not a supported image."));
            return;
        }

        if (!QDir().mkpath(m_artworkDirectory)) {
            emit artworkFailed(gameId, QStringLiteral("Unable to create the local artwork cache directory."));
            return;
        }

        const QString path = QDir(m_artworkDirectory)
                                 .filePath(QString::number(gameId) + "." + extension);
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()
            || !file.commit()) {
            const QString error = file.errorString();
            emit artworkFailed(gameId, QStringLiteral("Unable to save artwork: %1").arg(error));
            return;
        }

        if (!m_database->setArtworkPath(gameId, path)) {
            QFile::remove(path);
            emit artworkFailed(gameId, m_database->lastError());
            return;
        }

        emit artworkSaved(gameId, path);
    });
}
