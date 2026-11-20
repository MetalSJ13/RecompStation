#include "LaunchBoxCatalogManager.h"

#include "LaunchBoxArchiveExtractor.h"
#include "LaunchBoxCatalogDatabase.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QTimer>
#include <QUuid>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

namespace {
const QUrl MetadataArchiveUrl(QStringLiteral("https://gamesdb.launchbox-app.com/Metadata.zip"));
constexpr qint64 MaximumArchiveBytes = 200LL * 1024LL * 1024LL;
}

LaunchBoxCatalogManager::LaunchBoxCatalogManager(
    const QString &dataDirectory, QObject *parent)
    : QObject(parent),
      m_dataDirectory(QDir(dataDirectory).absolutePath()),
      m_catalogDirectory(QDir(m_dataDirectory).filePath(QStringLiteral("metadata/launchbox"))),
      m_catalogPath(QDir(m_catalogDirectory).filePath(QStringLiteral("catalog.sqlite")))
{
    connect(&m_buildWatcher, &QFutureWatcher<LaunchBoxCatalogBuildResult>::finished,
            this, &LaunchBoxCatalogManager::finishBackgroundBuild);
}

LaunchBoxCatalogManager::~LaunchBoxCatalogManager()
{
    if (m_cancelFlag)
        m_cancelFlag->store(true);
    if (m_reply)
        m_reply->abort();
    if (m_buildWatcher.isRunning())
        m_buildWatcher.waitForFinished();
    m_downloadFile.cancelWriting();
    cleanupStaging();
}

bool LaunchBoxCatalogManager::busy() const { return m_busy; }
QString LaunchBoxCatalogManager::catalogPath() const { return m_catalogPath; }

void LaunchBoxCatalogManager::setBusy(bool value)
{
    if (m_busy == value)
        return;
    m_busy = value;
    emit busyChanged();
}

int LaunchBoxCatalogManager::updateCatalog()
{
    if (m_busy)
        return -1;
    if (m_dataDirectory.isEmpty() || !QDir().mkpath(m_catalogDirectory)) {
        QTimer::singleShot(0, this, [this]() {
            emit updateFailed(QStringLiteral("Could not create the LaunchBox catalog directory."));
        });
        return -1;
    }

    ++m_requestId;
    m_cancelRequested = false;
    m_cancelFlag = std::make_shared<std::atomic_bool>(false);
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString staging = QDir(m_catalogDirectory).filePath(QStringLiteral(".update-") + token);
    if (!QDir().mkdir(staging)) {
        QTimer::singleShot(0, this, [this]() {
            emit updateFailed(QStringLiteral("Could not create a fresh LaunchBox update staging directory."));
        });
        return -1;
    }
    m_archivePath = QDir(staging).filePath(QStringLiteral("Metadata.zip"));
    m_extractPath = QDir(staging).filePath(QStringLiteral("extracted"));
    m_temporaryDatabasePath = QDir(staging).filePath(QStringLiteral("catalog.sqlite"));
    m_backupDatabasePath = m_catalogPath + QStringLiteral(".previous-") + token;

    m_downloadFile.setFileName(m_archivePath);
    if (!m_downloadFile.open(QIODevice::WriteOnly)) {
        cleanupStaging();
        QTimer::singleShot(0, this, [this]() {
            emit updateFailed(QStringLiteral("Could not create the staged LaunchBox archive."));
        });
        return -1;
    }

    setBusy(true);
    emit updateProgress(QStringLiteral("Downloading LaunchBox metadata"), 0);

    QNetworkRequest request(MetadataArchiveUrl);
    request.setTransferTimeout(60000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", "RecompStation");
    m_reply = m_network.get(request);
    connect(m_reply, &QIODevice::readyRead, this,
            &LaunchBoxCatalogManager::receiveDownloadData);
    connect(m_reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        emit downloadProgress(received, total);
        const int percent = total > 0
            ? qBound(0, static_cast<int>((received * 100) / total), 100) : 0;
        emit updateProgress(QStringLiteral("Downloading LaunchBox metadata"), percent);
    });
    connect(m_reply, &QNetworkReply::finished, this,
            &LaunchBoxCatalogManager::finishDownload);
    return m_requestId;
}

void LaunchBoxCatalogManager::cancelUpdate()
{
    if (!m_busy)
        return;
    m_cancelRequested = true;
    if (m_cancelFlag)
        m_cancelFlag->store(true);
    if (m_reply)
        m_reply->abort();
}

void LaunchBoxCatalogManager::receiveDownloadData()
{
    if (!m_reply || !m_downloadFile.isOpen())
        return;
    const QByteArray bytes = m_reply->readAll();
    if (bytes.isEmpty())
        return;
    if (m_downloadFile.size() > MaximumArchiveBytes - bytes.size()) {
        m_reply->abort();
        failUpdate(QStringLiteral("The LaunchBox archive exceeds the 200 MB download limit."));
        return;
    }
    if (m_downloadFile.write(bytes) != bytes.size()) {
        const QString error = m_downloadFile.errorString();
        m_reply->abort();
        failUpdate(QStringLiteral("Could not write the LaunchBox archive: %1").arg(error));
    }
}

void LaunchBoxCatalogManager::finishDownload()
{
    if (!m_reply)
        return;
    receiveDownloadData();
    if (!m_reply)
        return;

    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QUrl finalUrl = reply->url();
    const auto networkError = reply->error();
    const QString networkMessage = reply->errorString();
    reply->deleteLater();

    if (m_cancelRequested) {
        failUpdate(QStringLiteral("LaunchBox catalog update cancelled."));
        return;
    }
    if (networkError != QNetworkReply::NoError || status < 200 || status >= 300
        || finalUrl.scheme() != QStringLiteral("https")) {
        failUpdate(QStringLiteral("LaunchBox metadata download failed: %1").arg(networkMessage));
        return;
    }
    if (m_downloadFile.size() <= 0 || !m_downloadFile.commit()) {
        failUpdate(QStringLiteral("The downloaded LaunchBox archive is empty or could not be finalized."));
        return;
    }
    startBackgroundBuild();
}

void LaunchBoxCatalogManager::startBackgroundBuild()
{
    const QString archivePath = m_archivePath;
    const QString extractPath = m_extractPath;
    const QString databasePath = m_temporaryDatabasePath;
    const auto cancelFlag = m_cancelFlag;
    emit updateProgress(QStringLiteral("Extracting LaunchBox metadata"), 0);

    m_buildWatcher.setFuture(QtConcurrent::run(
        [this, archivePath, extractPath, databasePath, cancelFlag]() {
        LaunchBoxCatalogBuildResult result;
        const auto extracted = LaunchBoxArchiveExtractor::extractMetadataArchive(
            archivePath, extractPath, cancelFlag.get(),
            [this](qint64 received, qint64) {
                QMetaObject::invokeMethod(this, [this, received]() {
                    emit updateProgress(QStringLiteral("Extracting LaunchBox metadata"),
                                        qBound(0, static_cast<int>(received * 100 / (1024LL * 1024LL * 550LL)), 99));
                }, Qt::QueuedConnection);
            });
        if (!extracted.success) {
            result.error = extracted.error;
            return result;
        }
        if (cancelFlag->load()) {
            result.error = QStringLiteral("LaunchBox catalog update cancelled.");
            return result;
        }

        LaunchBoxCatalogDatabase catalog;
        if (!catalog.open(databasePath)) {
            result.error = catalog.lastError();
            return result;
        }
        const bool indexed = catalog.rebuildFromXmlFiles(
            extracted.files.platformsXmlPath, extracted.files.filesXmlPath,
            extracted.files.metadataXmlPath, cancelFlag.get(),
            [this](const QString &stage, int percent) {
                QMetaObject::invokeMethod(this, [this, stage, percent]() {
                    emit updateProgress(stage, percent);
                }, Qt::QueuedConnection);
            });
        if (!indexed) {
            result.error = catalog.lastError();
            return result;
        }
        result.platformCount = catalog.platformCount();
        result.fileCount = catalog.fileAssociationCount();
        result.gameCount = catalog.gameCount();
        result.success = true;
        return result;
    }));
}

void LaunchBoxCatalogManager::finishBackgroundBuild()
{
    const LaunchBoxCatalogBuildResult result = m_buildWatcher.result();
    if (m_cancelRequested || (m_cancelFlag && m_cancelFlag->load())) {
        failUpdate(QStringLiteral("LaunchBox catalog update cancelled."));
        return;
    }
    if (!result.success) {
        failUpdate(result.error.isEmpty()
            ? QStringLiteral("Could not build the LaunchBox catalog.") : result.error);
        return;
    }

    QString installError;
    if (!validateAndInstallCatalog(&installError)) {
        failUpdate(installError);
        return;
    }

    cleanupStaging();
    m_cancelFlag.reset();
    setBusy(false);
    emit updateProgress(QStringLiteral("LaunchBox catalog ready"), 100);
    emit updateCompleted(result.platformCount, result.fileCount, result.gameCount);
}

bool LaunchBoxCatalogManager::validateAndInstallCatalog(QString *errorMessage)
{
    QFileInfo stagedInfo(m_temporaryDatabasePath);
    if (!stagedInfo.isFile() || stagedInfo.size() <= 0) {
        *errorMessage = QStringLiteral("The rebuilt LaunchBox catalog database is missing or empty.");
        return false;
    }

    LaunchBoxCatalogDatabase validation;
    if (!validation.open(m_temporaryDatabasePath)) {
        *errorMessage = QStringLiteral("The rebuilt LaunchBox catalog failed validation: %1")
                            .arg(validation.lastError());
        return false;
    }
    if (validation.platformCount() <= 0 || validation.fileAssociationCount() <= 0
        || validation.gameCount() <= 0 || validation.indexedAt().isEmpty()) {
        *errorMessage = QStringLiteral("The rebuilt LaunchBox catalog is incomplete.");
        validation.close();
        return false;
    }
    validation.close();

    const bool hadPrevious = QFileInfo::exists(m_catalogPath);
    if (hadPrevious && !QFile::rename(m_catalogPath, m_backupDatabasePath)) {
        *errorMessage = QStringLiteral("Could not preserve the existing LaunchBox catalog before updating it.");
        return false;
    }
    if (!QFile::rename(m_temporaryDatabasePath, m_catalogPath)) {
        if (hadPrevious && !QFile::rename(m_backupDatabasePath, m_catalogPath))
            *errorMessage = QStringLiteral("Catalog activation failed and automatic restoration failed; backup remains at %1").arg(m_backupDatabasePath);
        else
            *errorMessage = QStringLiteral("Could not activate the new LaunchBox catalog; the previous catalog was restored.");
        return false;
    }
    if (hadPrevious)
        QFile::remove(m_backupDatabasePath);
    return true;
}

void LaunchBoxCatalogManager::failUpdate(const QString &message)
{
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_cancelFlag)
        m_cancelFlag->store(true);
    if (m_buildWatcher.isRunning())
        return; // The worker completion handler performs final cleanup.
    m_downloadFile.cancelWriting();
    cleanupStaging();
    m_cancelFlag.reset();
    const bool cancelled = m_cancelRequested;
    m_cancelRequested = false;
    setBusy(false);
    if (cancelled)
        emit updateCancelled();
    else
        emit updateFailed(message);
}

void LaunchBoxCatalogManager::cleanupStaging()
{
    if (m_archivePath.isEmpty())
        return;
    const QString staging = QFileInfo(m_archivePath).absolutePath();
    if (QFileInfo(staging).fileName().startsWith(QStringLiteral(".update-")))
        QDir(staging).removeRecursively();
    m_archivePath.clear();
    m_extractPath.clear();
    m_temporaryDatabasePath.clear();
    m_backupDatabasePath.clear();
}
