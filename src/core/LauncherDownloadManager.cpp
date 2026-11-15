#include "LauncherDownloadManager.h"

#include "PlatformManager.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>

#include <limits>

namespace {
constexpr qint64 MaximumDownloadSize = 8LL * 1024LL * 1024LL * 1024LL;
}

struct LauncherDownloadManager::Download
{
    int requestId = 0;
    QString projectId;
    QString assetName;
    QString targetPath;
    qint64 expectedSize = -1;
    qint64 bytesReceived = 0;
    QSaveFile file;
    QPointer<QNetworkReply> reply;
};

LauncherDownloadManager::LauncherDownloadManager(
    PlatformManager *platformManager,
    QObject *parent
)
    : QObject(parent),
      m_platformManager(platformManager)
{
}

LauncherDownloadManager::~LauncherDownloadManager()
{
    const auto downloads = m_downloads;
    m_downloads.clear();
    for (Download *download : downloads) {
        if (download->reply)
            download->reply->abort();
        download->file.cancelWriting();
        delete download;
    }
}

int LauncherDownloadManager::downloadAsset(
    const QString &projectId,
    const QString &assetName,
    const QString &downloadUrl,
    qint64 expectedSize
)
{
    const int requestId = m_nextRequestId++;

    QString validationError;
    if (!m_platformManager) {
        validationError = QStringLiteral("The application storage manager is unavailable.");
    } else if (!isValidProjectId(projectId)) {
        validationError = QStringLiteral("The project identifier is invalid.");
    } else if (!isValidAssetName(assetName)) {
        validationError = QStringLiteral("The release asset filename is invalid.");
    } else if (!isValidHttpsUrl(downloadUrl)) {
        validationError = QStringLiteral("Only valid HTTPS release asset URLs are allowed.");
    } else if (expectedSize < -1 || expectedSize > MaximumDownloadSize) {
        validationError = QStringLiteral("The release asset size is outside the supported limit.");
    }

    if (!validationError.isEmpty()) {
        QTimer::singleShot(0, this, [this, requestId, validationError]() {
            emit downloadFailed(requestId, validationError);
        });
        return requestId;
    }

    const QString projectStagingPath =
        QDir(m_platformManager->stagingDirectory()).filePath(projectId);
    if (!QDir().mkpath(projectStagingPath)) {
        QTimer::singleShot(0, this, [this, requestId]() {
            emit downloadFailed(
                requestId,
                QStringLiteral("Could not create the launcher download staging directory.")
            );
        });
        return requestId;
    }

    const QString targetPath = QDir(projectStagingPath).filePath(assetName);
    if (m_activeTargets.contains(targetPath)) {
        QTimer::singleShot(0, this, [this, requestId]() {
            emit downloadFailed(
                requestId,
                QStringLiteral("This release asset is already being downloaded.")
            );
        });
        return requestId;
    }

    auto *download = new Download;
    download->requestId = requestId;
    download->projectId = projectId;
    download->assetName = assetName;
    download->targetPath = targetPath;
    download->expectedSize = expectedSize;
    download->file.setFileName(targetPath);

    if (!download->file.open(QIODevice::WriteOnly)) {
        const QString message = QStringLiteral("Could not create the temporary download file: %1")
                                    .arg(download->file.errorString());
        delete download;
        QTimer::singleShot(0, this, [this, requestId, message]() {
            emit downloadFailed(requestId, message);
        });
        return requestId;
    }

    QNetworkRequest request{QUrl(downloadUrl)};
    request.setTransferTimeout(30000);
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
    );
    request.setRawHeader("User-Agent", "RecompStation");

    QNetworkReply *reply = m_network.get(request);
    download->reply = reply;
    m_downloads.insert(requestId, download);
    m_activeTargets.insert(targetPath);

    connect(reply, &QIODevice::readyRead, this, [this, requestId]() {
        receiveDownloadData(requestId);
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, requestId](qint64 received, qint64 total) {
        if (m_downloads.contains(requestId))
            emit downloadProgress(requestId, received, total);
    });
    connect(reply, &QNetworkReply::finished, this, [this, requestId]() {
        finishDownload(requestId);
    });

    return requestId;
}

void LauncherDownloadManager::cancelDownload(int requestId)
{
    if (!m_downloads.contains(requestId))
        return;

    Download *download = m_downloads.value(requestId);
    if (download->reply)
        download->reply->abort();
    else
        failDownload(requestId, QStringLiteral("The download was cancelled."));
}

void LauncherDownloadManager::receiveDownloadData(int requestId)
{
    Download *download = m_downloads.value(requestId, nullptr);
    if (!download || !download->reply)
        return;

    const QByteArray data = download->reply->readAll();
    if (data.isEmpty())
        return;

    if (download->bytesReceived > MaximumDownloadSize - data.size()) {
        failDownload(requestId, QStringLiteral("The release asset exceeds the 8 GiB download limit."));
        return;
    }

    if (download->expectedSize >= 0
        && download->bytesReceived + data.size() > download->expectedSize) {
        failDownload(requestId, QStringLiteral("The downloaded data is larger than GitHub's reported asset size."));
        return;
    }

    const qint64 written = download->file.write(data);
    if (written != data.size()) {
        const QString message = QStringLiteral("Could not write the downloaded asset: %1")
                                    .arg(download->file.errorString());
        failDownload(requestId, message);
        return;
    }

    download->bytesReceived += written;
}

void LauncherDownloadManager::finishDownload(int requestId)
{
    Download *download = m_downloads.value(requestId, nullptr);
    if (!download || !download->reply)
        return;

    receiveDownloadData(requestId);
    download = m_downloads.value(requestId, nullptr);
    if (!download || !download->reply)
        return;

    QNetworkReply *reply = download->reply;
    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QUrl finalUrl = reply->url();

    if (reply->error() != QNetworkReply::NoError) {
        const QString message = QStringLiteral("Launcher download failed: %1")
                                    .arg(reply->errorString());
        reply->deleteLater();
        download->reply = nullptr;
        failDownload(requestId, message);
        return;
    }

    if (statusCode < 200 || statusCode >= 300
        || finalUrl.scheme() != QStringLiteral("https")) {
        reply->deleteLater();
        download->reply = nullptr;
        failDownload(
            requestId,
            QStringLiteral("The release server returned an unsuccessful or insecure response.")
        );
        return;
    }

    if (download->expectedSize >= 0
        && download->bytesReceived != download->expectedSize) {
        const QString message = QStringLiteral(
            "The downloaded asset size does not match GitHub's reported size "
            "(received %1 bytes, expected %2 bytes)."
        ).arg(download->bytesReceived).arg(download->expectedSize);
        reply->deleteLater();
        download->reply = nullptr;
        failDownload(requestId, message);
        return;
    }

    if (!download->file.commit()) {
        const QString message = QStringLiteral("Could not finalize the downloaded asset: %1")
                                    .arg(download->file.errorString());
        reply->deleteLater();
        download->reply = nullptr;
        failDownload(requestId, message);
        return;
    }

    const QString projectId = download->projectId;
    const QString assetName = download->assetName;
    const QString filePath = download->targetPath;
    m_activeTargets.remove(download->targetPath);
    m_downloads.remove(requestId);
    reply->deleteLater();
    delete download;

    emit downloadCompleted(requestId, projectId, assetName, filePath);
}

void LauncherDownloadManager::failDownload(
    int requestId,
    const QString &message
)
{
    Download *download = m_downloads.take(requestId);
    if (!download)
        return;

    m_activeTargets.remove(download->targetPath);
    if (download->reply) {
        disconnect(download->reply, nullptr, this, nullptr);
        download->reply->abort();
        download->reply->deleteLater();
    }
    download->file.cancelWriting();
    delete download;

    emit downloadFailed(requestId, message);
}

bool LauncherDownloadManager::isValidProjectId(const QString &projectId)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[a-z0-9][a-z0-9-]*$")
    );
    return pattern.match(projectId).hasMatch();
}

bool LauncherDownloadManager::isValidAssetName(const QString &assetName)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+-]{0,199}$")
    );
    return pattern.match(assetName).hasMatch()
        && assetName != QStringLiteral(".")
        && assetName != QStringLiteral("..");
}

bool LauncherDownloadManager::isValidHttpsUrl(const QString &value)
{
    const QUrl url(value);
    return url.isValid()
        && url.scheme() == QStringLiteral("https")
        && !url.host().isEmpty()
        && url.userInfo().isEmpty();
}
