#include "LauncherDownloadManager.h"

#include "PlatformManager.h"
#include "Logging.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QFile>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QSet>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <QVariantMap>
#include <QUuid>
#include <QUrl>

namespace {
// Files RecompStation writes into every install.
const QStringList kInstallMarkers{QStringLiteral(".recompstation-managed"),
                                  QStringLiteral(".recompstation-install"),
                                  QStringLiteral(".recompstation-files"),
                                  QStringLiteral(".recompstation-release")};
// Where an update keeps the old version while the two are swapped.
const QString kPreviousPrefix = QStringLiteral(".previous-");

// Every file in a folder as relative paths, without following links.
QStringList relativeFiles(const QString &root)
{
    QStringList files;
    QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().isSymLink())
            continue;
        const QString relative = QDir(root).relativeFilePath(it.filePath());
        if (!kInstallMarkers.contains(relative))
            files.append(relative);
    }
    return files;
}

// Moves one file, replacing what is at the destination. Both sit under
// RecompStation's data folder, so a rename is enough.
bool moveReplacing(const QString &source, const QString &destination)
{
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()))
        return false;
    if (QFileInfo::exists(destination) && !QFile::remove(destination))
        return false;
    return QFile::rename(source, destination);
}

void removeEmptyFolders(const QString &root)
{
    QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    QStringList folders;
    while (it.hasNext())
        folders.prepend(it.next());
    for (const QString &folder : std::as_const(folders))
        QDir().rmdir(folder);
    QDir().rmdir(root);
}
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
    QString expectedSha256;
    QCryptographicHash hash{QCryptographicHash::Sha256};
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
    qint64 expectedSize,
    const QString &expectedSha256
)
{
    const int requestId = m_nextRequestId++;
    qCInfo(lcLauncher) << "Launcher asset download requested:" << projectId << assetName;

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
        qCWarning(lcLauncher) << "Download rejected:" << validationError;
        QTimer::singleShot(0, this, [this, requestId, validationError]() {
            emit downloadFailed(requestId, validationError);
        });
        return requestId;
    }

    const QString stagingDirectory = m_platformManager->stagingDirectory();
    const QString canonicalDataRoot =
        QFileInfo(m_platformManager->dataDirectory()).canonicalFilePath();
    const QString canonicalStagingRoot =
        QFileInfo(stagingDirectory).canonicalFilePath();
    const QString expectedStagingRoot =
        QDir(canonicalDataRoot).filePath("downloads/staging");
    if (canonicalDataRoot.isEmpty() || canonicalStagingRoot.isEmpty()
        || canonicalStagingRoot != expectedStagingRoot) {
        QTimer::singleShot(0, this, [this, requestId]() {
            emit downloadFailed(
                requestId,
                QStringLiteral("The download staging directory must not resolve through a symlink.")
            );
        });
        return requestId;
    }

    const QString projectStagingPath =
        QDir(stagingDirectory).filePath(projectId);
    if (!QDir().mkpath(projectStagingPath)) {
        QTimer::singleShot(0, this, [this, requestId]() {
            emit downloadFailed(
                requestId,
                QStringLiteral("Could not create the launcher download staging directory.")
            );
        });
        return requestId;
    }

    const QString canonicalProjectStaging =
        QFileInfo(projectStagingPath).canonicalFilePath();
    const QString expectedProjectStaging =
        QDir(canonicalStagingRoot).filePath(projectId);
    if (canonicalProjectStaging.isEmpty()
        || canonicalProjectStaging != expectedProjectStaging) {
        QTimer::singleShot(0, this, [this, requestId]() {
            emit downloadFailed(
                requestId,
                QStringLiteral("The project staging directory resolves outside the download staging area.")
            );
        });
        return requestId;
    }

    const QString targetPath = QDir(canonicalProjectStaging).filePath(assetName);
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
    // Only a well-formed digest counts; anything else means "not published".
    static const QRegularExpression sha256Pattern(QStringLiteral("^[0-9a-f]{64}$"));
    const QString digest = expectedSha256.trimmed().toLower();
    download->expectedSha256 = sha256Pattern.match(digest).hasMatch() ? digest : QString();
    download->file.setFileName(targetPath);

    if (!download->file.open(QIODevice::WriteOnly)) {
        const QString message = QStringLiteral("Could not create the temporary download file: %1")
                                    .arg(download->file.errorString());
        delete download;
        QTimer::singleShot(0, this, [this, requestId, message]() {
            qCWarning(lcLauncher) << "Download failed (request" << requestId << "):" << message;
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

    qCInfo(lcLauncher) << "Starting HTTPS download for" << projectId << assetName;
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

    download->hash.addData(data);
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

    const QString actualSha256 = QString::fromLatin1(download->hash.result().toHex());
    if (!download->expectedSha256.isEmpty() && actualSha256 != download->expectedSha256) {
        qCWarning(lcLauncher) << "Checksum mismatch for" << download->projectId << download->assetName
                              << "expected" << download->expectedSha256 << "got" << actualSha256;
        reply->deleteLater();
        download->reply = nullptr;
        failDownload(requestId, QStringLiteral(
            "The download doesn't match the checksum the project published, so it wasn't kept. "
            "It may have been damaged on the way; try again."));
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
    const qint64 bytesReceived = download->bytesReceived;
    m_checksums.insert(filePath, Checksum{actualSha256, !download->expectedSha256.isEmpty()});
    m_activeTargets.remove(download->targetPath);
    m_downloads.remove(requestId);
    reply->deleteLater();
    delete download;

    qCInfo(lcLauncher) << "Download completed:" << projectId << assetName
                       << "bytes:" << bytesReceived
                       << (m_checksums.value(filePath).verified ? "checksum verified" : "no published checksum");
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

    qCWarning(lcLauncher) << "Download failed (request" << requestId << "):" << message;
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


int LauncherDownloadManager::installDownloadedAsset(
    const QString &projectId,
    const QString &assetName,
    const QString &packageFormat,
    const QString &expectedExecutable,
    const QString &releaseTag,
    bool replaceExisting,
    const QString &build
)
{
    const int requestId = m_nextRequestId++;
    qCInfo(lcLauncher) << "Launcher installation requested:" << projectId
                       << assetName << "format:" << packageFormat;
    auto reject = [this, requestId](const QString &message) {
        QTimer::singleShot(0, this, [this, requestId, message]() {
            emit installationFailed(requestId, message);
        });
    };

    if (!m_platformManager || !isValidProjectId(projectId)
        || !isValidAssetName(assetName)) {
        reject(QStringLiteral("The project identifier or release asset filename is invalid."));
        return requestId;
    }
    if (packageFormat != QStringLiteral("tar.gz")
        && packageFormat != QStringLiteral("tar.xz")
        && packageFormat != QStringLiteral("zip")
        && packageFormat != QStringLiteral("appimage")) {
        reject(QStringLiteral("This package format is not supported by the automatic installer."));
        return requestId;
    }
    if (packageFormat == QStringLiteral("appimage")
        && QFileInfo(expectedExecutable).fileName() != expectedExecutable) {
        reject(QStringLiteral("A standalone AppImage must use its filename as the executable path."));
        return requestId;
    }
    const bool windowsBuild = build.startsWith(QStringLiteral("windows-"));
    if (expectedExecutable.isEmpty()
            ? !(windowsBuild && packageFormat == QStringLiteral("zip"))
            : !isSafeRelativePath(expectedExecutable)) {
        reject(QStringLiteral("The expected launcher executable path is invalid."));
        return requestId;
    }
    if (!build.isEmpty() && build != QStringLiteral("linux-x86_64") && build != QStringLiteral("linux-aarch64")
        && build != QStringLiteral("windows-x86_64")) {
        reject(QStringLiteral("The launcher build is invalid."));
        return requestId;
    }
    if (m_activeInstallProjects.contains(projectId)) {
        reject(QStringLiteral("An installation for this project is already in progress."));
        return requestId;
    }

    const QString dataRoot = QFileInfo(m_platformManager->dataDirectory()).canonicalFilePath();
    const QString stagingRoot = QFileInfo(m_platformManager->stagingDirectory()).canonicalFilePath();
    if (dataRoot.isEmpty() || stagingRoot != QDir(dataRoot).filePath("downloads/staging")) {
        reject(QStringLiteral("The application or staging directory resolves through an unexpected filesystem path."));
        return requestId;
    }

    const QString projectStaging = QDir(stagingRoot).filePath(projectId);
    const QString canonicalProjectStaging = QFileInfo(projectStaging).canonicalFilePath();
    if (canonicalProjectStaging != projectStaging) {
        reject(QStringLiteral("The project staging directory is missing or resolves through a symlink."));
        return requestId;
    }

    const QString archivePath = QDir(canonicalProjectStaging).filePath(assetName);
    const QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.isFile() || archiveInfo.isSymLink() || !archiveInfo.isReadable()) {
        reject(QStringLiteral("The downloaded archive is missing, unreadable, or is a symbolic link."));
        return requestId;
    }

    const QString launcherRoot = QDir(dataRoot).filePath("launchers");
    const QString canonicalLauncherRoot = QFileInfo(launcherRoot).canonicalFilePath();
    if (canonicalLauncherRoot != launcherRoot) {
        reject(QStringLiteral("The launcher installation directory resolves through an unexpected filesystem path."));
        return requestId;
    }

    const QString destination = QDir(canonicalLauncherRoot).filePath(projectId);
    const QFileInfo destinationInfo(destination);
    if (destinationInfo.isSymLink()) {
        reject(QStringLiteral("The launcher's folder is a link; refusing to touch it."));
        return requestId;
    }
    if (destinationInfo.exists()) {
        bool managed = false;
        for (const QVariant &entry : installedLaunchers()) {
            if (entry.toMap().value(QStringLiteral("projectId")).toString() == projectId)
                managed = true;
        }
        if (!replaceExisting || !managed) {
            reject(QStringLiteral("A launcher is already installed for this project. Existing installations will not be overwritten."));
            return requestId;
        }
    }
    if (releaseTag.size() > 100 || releaseTag.contains(QLatin1Char('\n'))) {
        reject(QStringLiteral("The release name is invalid."));
        return requestId;
    }
    const Checksum checksum = m_checksums.value(archivePath);

    const QString extractionPath = QDir(canonicalProjectStaging)
        .filePath(assetName + ".extracting-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QFileInfo extractionInfo(extractionPath);
    if (extractionInfo.exists() || extractionInfo.isSymLink()) {
        reject(QStringLiteral("A temporary extraction directory already exists; refusing to overwrite it."));
        return requestId;
    }

    m_activeInstallProjects.insert(projectId);
    auto *watcher = new QFutureWatcher<ArchiveExtractor::Result>(this);
    m_installations.insert(requestId, watcher);
    connect(watcher, &QFutureWatcher<ArchiveExtractor::Result>::finished,
            this, [this, watcher, requestId, projectId, archivePath, assetName,
                   extractionPath, destination, releaseTag, checksum, replaceExisting, build]() {
        const ArchiveExtractor::Result result = watcher->result();
        m_installations.remove(requestId);
        m_activeInstallProjects.remove(projectId);
        watcher->deleteLater();

        if (!result.success) {
            qCWarning(lcLauncher) << "Launcher extraction failed for" << projectId << ":" << result.error;
            emit installationFailed(requestId, result.error);
            return;
        }

        const QString executableRelativePath =
            QDir(extractionPath).relativeFilePath(result.executablePath);
        if (!isSafeRelativePath(executableRelativePath)) {
            QDir(extractionPath).removeRecursively();
            emit installationFailed(requestId, QStringLiteral("The extracted launcher path failed final validation."));
            return;
        }

        const QString markerPath = QDir(extractionPath).filePath(".recompstation-managed");
        const QString manifestPath = QDir(extractionPath).filePath(".recompstation-install");
        QSaveFile marker(markerPath);
        QSaveFile manifest(manifestPath);
        const QByteArray markerContent("RecompStation managed launcher installation v1\n");
        const QByteArray manifestContent = executableRelativePath.toUtf8() + "\n";
        // Which release this is, for Game Details and update checks.
        {
            const QJsonObject release{
                {QStringLiteral("tag"), releaseTag},
                {QStringLiteral("asset"), assetName},
                {QStringLiteral("sha256"), checksum.sha256},
                {QStringLiteral("checksumVerified"), checksum.verified},
                {QStringLiteral("build"), build},
                {QStringLiteral("installedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
            QSaveFile releaseFile(QDir(extractionPath).filePath(QStringLiteral(".recompstation-release")));
            const QByteArray content = QJsonDocument(release).toJson(QJsonDocument::Indented);
            if (!releaseFile.open(QIODevice::WriteOnly) || releaseFile.write(content) != content.size()
                || !releaseFile.commit()) {
                QDir(extractionPath).removeRecursively();
                emit installationFailed(requestId, QStringLiteral("Could not record the launcher's release."));
                return;
            }
        }
        // What the package itself contains: anything else found here later
        // (saves, settings, portable mode) is the player's, and uninstall
        // sets it aside instead of deleting it.
        {
            QSaveFile fileList(QDir(extractionPath).filePath(QStringLiteral(".recompstation-files")));
            const QByteArray listContent = relativeFiles(extractionPath).join(QLatin1Char('\n')).toUtf8() + "\n";
            if (!fileList.open(QIODevice::WriteOnly) || fileList.write(listContent) != listContent.size()
                || !fileList.commit()) {
                QDir(extractionPath).removeRecursively();
                emit installationFailed(requestId, QStringLiteral("Could not record the launcher's files."));
                return;
            }
        }
        if (!marker.open(QIODevice::WriteOnly)
            || marker.write(markerContent) != markerContent.size()
            || !marker.commit()
            || !manifest.open(QIODevice::WriteOnly)
            || manifest.write(manifestContent) != manifestContent.size()
            || !manifest.commit()) {
            QDir(extractionPath).removeRecursively();
            emit installationFailed(requestId, QStringLiteral("Could not write the launcher ownership marker and installation manifest."));
            return;
        }

        if (replaceExisting && QFileInfo::exists(destination)) {
            QString error;
            if (!replaceInstallation(projectId, extractionPath, destination, executableRelativePath, &error)) {
                QDir(extractionPath).removeRecursively();
                qCWarning(lcLauncher) << "Update of" << projectId << "failed; the previous version stays:" << error;
                emit installationFailed(requestId, error);
                return;
            }
            QFile::remove(archivePath);
            m_checksums.remove(archivePath);
            qCInfo(lcLauncher) << "Launcher updated:" << projectId << (releaseTag.isEmpty() ? QString() : releaseTag);
            emit installationCompleted(requestId, projectId, QDir(destination).filePath(executableRelativePath));
            return;
        }

        const QFileInfo finalDestinationInfo(destination);
        if (finalDestinationInfo.exists() || finalDestinationInfo.isSymLink()
            || !QDir().rename(extractionPath, destination)) {
            QDir(extractionPath).removeRecursively();
            emit installationFailed(requestId, QStringLiteral("Could not safely move the extracted launcher into its permanent installation directory. Existing files were left untouched."));
            return;
        }

        const QString executablePath = QDir(destination).filePath(executableRelativePath);
        const QFileInfo installedExecutable(executablePath);
        const QString canonicalExecutable = installedExecutable.canonicalFilePath();
        if (!installedExecutable.isFile() || installedExecutable.isSymLink()
            || !canonicalExecutable.startsWith(destination + QDir::separator())) {
            QDir(destination).removeRecursively();
            emit installationFailed(requestId, QStringLiteral("The installed launcher executable failed its final validation."));
            return;
        }

        QFile::remove(archivePath);
        m_checksums.remove(archivePath);
        const int restored = restoreKeptFiles(projectId, destination);
        if (restored > 0)
            qCInfo(lcLauncher) << "Put back" << restored << "kept file(s) for" << projectId;
        qCInfo(lcLauncher) << "Launcher installed:" << projectId;
        emit installationCompleted(requestId, projectId, executablePath);
    });

    watcher->setFuture(QtConcurrent::run(
        [archivePath, extractionPath, expectedExecutable, packageFormat]() {
            if (packageFormat == QStringLiteral("appimage")) {
                return ArchiveExtractor::stageAppImage(
                    archivePath, extractionPath, expectedExecutable);
            }
            return ArchiveExtractor::extractArchive(
                archivePath, extractionPath, expectedExecutable);
        }));
    return requestId;
}

bool LauncherDownloadManager::isSafeRelativePath(const QString &path)
{
    if (path.trimmed().isEmpty() || path.startsWith('/')
        || path.contains('\\') || path.contains(':'))
        return false;

    const QStringList parts = path.split('/', Qt::KeepEmptyParts);
    if (parts.isEmpty())
        return false;
    for (const QString &part : parts) {
        if (part.isEmpty() || part == QStringLiteral(".")
            || part == QStringLiteral(".."))
            return false;
    }
    return true;
}


QVariantList LauncherDownloadManager::installedLaunchers() const
{
    QVariantList installed;
    if (!m_platformManager)
        return installed;

    const QString dataRoot = QFileInfo(m_platformManager->dataDirectory()).canonicalFilePath();
    if (dataRoot.isEmpty())
        return installed;

    const QString launcherRoot = QDir(dataRoot).filePath("launchers");
    if (QFileInfo(launcherRoot).canonicalFilePath() != launcherRoot)
        return installed;

    const QStringList directories = QDir(launcherRoot).entryList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    const QByteArray expectedMarker("RecompStation managed launcher installation v1\n");

    for (const QString &projectId : directories) {
        if (!isValidProjectId(projectId))
            continue;

        const QString directory = QDir(launcherRoot).filePath(projectId);
        const QFileInfo directoryInfo(directory);
        if (directoryInfo.isSymLink()
            || directoryInfo.canonicalFilePath() != directory)
            continue;

        const QString markerPath = QDir(directory).filePath(".recompstation-managed");
        const QString manifestPath = QDir(directory).filePath(".recompstation-install");
        const QFileInfo markerInfo(markerPath);
        const QFileInfo manifestInfo(manifestPath);
        if (!markerInfo.isFile() || markerInfo.isSymLink()
            || !manifestInfo.isFile() || manifestInfo.isSymLink())
            continue;

        QFile marker(markerPath);
        QFile manifest(manifestPath);
        if (!marker.open(QIODevice::ReadOnly)
            || marker.readAll() != expectedMarker
            || !manifest.open(QIODevice::ReadOnly))
            continue;

        const QString executableRelativePath =
            QString::fromUtf8(manifest.readAll()).trimmed();
        if (!isSafeRelativePath(executableRelativePath))
            continue;

        const QString executablePath = QDir(directory).filePath(executableRelativePath);
        const QFileInfo executableInfo(executablePath);
        const QString canonicalExecutable = executableInfo.canonicalFilePath();
        if (!executableInfo.isFile() || executableInfo.isSymLink()
            || !canonicalExecutable.startsWith(directory + QDir::separator()))
            continue;

        QVariantMap entry{
            {QStringLiteral("projectId"), projectId},
            {QStringLiteral("directory"), directory},
            {QStringLiteral("executablePath"), executablePath},
            {QStringLiteral("version"), QString()},
            {QStringLiteral("checksumVerified"), false}
        };
        // Installs from before release tracking have no record: version "".
        QFile releaseFile(QDir(directory).filePath(QStringLiteral(".recompstation-release")));
        if (!QFileInfo(releaseFile.fileName()).isSymLink() && releaseFile.size() < 4096
            && releaseFile.open(QIODevice::ReadOnly)) {
            const QJsonObject release = QJsonDocument::fromJson(releaseFile.readAll()).object();
            entry.insert(QStringLiteral("version"), release.value(QStringLiteral("tag")).toString().left(100));
            entry.insert(QStringLiteral("assetName"), release.value(QStringLiteral("asset")).toString().left(200));
            entry.insert(QStringLiteral("checksumVerified"), release.value(QStringLiteral("checksumVerified")).toBool());
            entry.insert(QStringLiteral("installedAt"), release.value(QStringLiteral("installedAt")).toString().left(40));
            entry.insert(QStringLiteral("build"), release.value(QStringLiteral("build")).toString().left(40));
        }
        installed.append(entry);
    }

    return installed;
}

int LauncherDownloadManager::cleanUpStaging()
{
    if (!m_platformManager || !m_activeTargets.isEmpty() || !m_activeInstallProjects.isEmpty())
        return 0;
    const QString dataRoot = QFileInfo(m_platformManager->dataDirectory()).canonicalFilePath();
    const QString stagingRoot = QFileInfo(m_platformManager->stagingDirectory()).canonicalFilePath();
    if (dataRoot.isEmpty() || stagingRoot.isEmpty()
        || stagingRoot != QDir(dataRoot).filePath(QStringLiteral("downloads/staging")))
        return -1;

    int removed = 0;
    qint64 bytes = 0;
    const QFileInfoList projects = QDir(stagingRoot).entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &project : projects) {
        // Only the folders downloads create; anything else (a link, a stray
        // file) is left alone rather than guessed at.
        if (project.isSymLink() || !project.isDir() || !isValidProjectId(project.fileName()))
            continue;
        const QFileInfoList entries = QDir(project.filePath()).entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &entry : entries) {
            bool ok = false;
            if (entry.isSymLink() || !entry.isDir()) {
                bytes += entry.isSymLink() ? 0 : entry.size();
                ok = QFile::remove(entry.filePath());
            } else {
                QDirIterator it(entry.filePath(), QDir::Files | QDir::Hidden | QDir::System,
                                QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    it.next();
                    if (!it.fileInfo().isSymLink())
                        bytes += it.fileInfo().size();
                }
                // Links inside are removed as links; their targets are kept.
                ok = QDir(entry.filePath()).removeRecursively();
            }
            if (ok)
                ++removed;
            else
                qCWarning(lcLauncher) << "Could not remove a leftover download:"
                                      << RecompLogging::redactHome(entry.filePath());
        }
        QDir().rmdir(project.filePath());
    }
    if (removed > 0)
        qCInfo(lcLauncher) << "Removed" << removed << "leftover download(s) from earlier sessions,"
                           << QString::number(bytes / (1024.0 * 1024.0), 'f', 1) << "MB";
    return removed;
}

QString LauncherDownloadManager::sha256OfFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

QString LauncherDownloadManager::launcherRoot() const
{
    if (!m_platformManager)
        return {};
    const QString dataRoot = QFileInfo(m_platformManager->dataDirectory()).canonicalFilePath();
    if (dataRoot.isEmpty())
        return {};
    const QString root = QDir(dataRoot).filePath(QStringLiteral("launchers"));
    return QFileInfo(root).canonicalFilePath() == root ? root : QString();
}

QStringList LauncherDownloadManager::playerFiles(const QString &installDirectory)
{
    QSet<QString> packaged;
    bool hasList = false;
    QFile list(QDir(installDirectory).filePath(QStringLiteral(".recompstation-files")));
    if (list.open(QIODevice::ReadOnly)) {
        hasList = true;
        for (const QByteArray &line : list.readAll().split('\n')) {
            if (!line.isEmpty())
                packaged.insert(QString::fromUtf8(line));
        }
    }
    // Installs made before the file list existed: the usual places
    // launchers write to.
    static const QStringList fallback{QStringLiteral("saves"), QStringLiteral("portable"),
                                      QStringLiteral("portable.txt"), QStringLiteral("userdata"),
                                      QStringLiteral("mods"), QStringLiteral("mod_config"),
                                      QStringLiteral("mods.json")};
    QStringList files;
    for (const QString &relative : relativeFiles(installDirectory)) {
        if (hasList ? !packaged.contains(relative)
                    : fallback.contains(relative.section(QLatin1Char('/'), 0, 0)))
            files.append(relative);
    }
    return files;
}

bool LauncherDownloadManager::replaceInstallation(const QString &projectId, const QString &newDirectory,
                                                  const QString &destination,
                                                  const QString &executableRelative, QString *error)
{
    const QString root = launcherRoot();
    if (root.isEmpty() || QFileInfo(destination).absolutePath() != root) {
        *error = QStringLiteral("The launcher folder isn't where it should be.");
        return false;
    }
    const QString previous = QDir(root).filePath(QLatin1Char('.') + projectId + kPreviousPrefix
                                                 + QUuid::createUuid().toString(QUuid::WithoutBraces));
    // 1. The installed version steps aside (one rename, so it stays whole).
    if (!QDir().rename(destination, previous)) {
        *error = QStringLiteral("Couldn't move the installed version aside, so nothing was changed.");
        return false;
    }
    auto rollBack = [&]() {
        QDir(destination).removeRecursively();
        QDir().rename(previous, destination);
    };
    // 2. The new version takes its place.
    if (!QDir().rename(newDirectory, destination)) {
        rollBack();
        *error = QStringLiteral("Couldn't put the new version in place; the previous version was kept.");
        return false;
    }
    const QFileInfo executable(QDir(destination).filePath(executableRelative));
    if (!executable.isFile() || executable.isSymLink()
        || !executable.canonicalFilePath().startsWith(destination + QDir::separator())) {
        rollBack();
        *error = QStringLiteral("The new version's launcher failed its final check; the previous version was kept.");
        return false;
    }
    // 3. The player's files move across. If the new package ships a file of
    // the same name, the new one wins and the old one is kept aside.
    for (const QString &relative : playerFiles(previous)) {
        const QString source = QDir(previous).filePath(relative);
        const QString target = QDir(destination).filePath(relative);
        const QString moveTo = QFileInfo::exists(target)
            ? QDir(keptFilesDirectory(projectId, installedBuild(destination))).filePath(relative) : target;
        if (!moveReplacing(source, moveTo)) {
            rollBack();
            *error = QStringLiteral("Couldn't carry your saves over to the new version; the previous version was kept.");
            return false;
        }
    }
    // 4. Only now is the old version removed.
    if (!QDir(previous).removeRecursively())
        qCWarning(lcLauncher) << "Could not remove the previous version of" << projectId
                              << "; it is removed at the next start.";
    return true;
}

int LauncherDownloadManager::recoverInterruptedUpdates()
{
    const QString root = launcherRoot();
    if (root.isEmpty() || !m_activeInstallProjects.isEmpty())
        return 0;
    int recovered = 0;
    const QFileInfoList entries = QDir(root).entryInfoList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        const QString name = entry.fileName();
        const int at = name.indexOf(kPreviousPrefix);
        if (!name.startsWith(QLatin1Char('.')) || at <= 1 || entry.isSymLink())
            continue;
        const QString projectId = name.mid(1, at - 1);
        if (!isValidProjectId(projectId))
            continue;
        const QString destination = QDir(root).filePath(projectId);
        if (!QFileInfo::exists(destination)) {
            // Interrupted before the new version was in place: put the old back.
            if (QDir().rename(entry.filePath(), destination)) {
                qCInfo(lcLauncher) << "Restored" << projectId << "after an interrupted update.";
                ++recovered;
            }
            continue;
        }
        // The new version is in place: finish moving the player's files.
        for (const QString &relative : playerFiles(entry.filePath())) {
            const QString target = QDir(destination).filePath(relative);
            moveReplacing(QDir(entry.filePath()).filePath(relative),
                          QFileInfo::exists(target) ? QDir(keptFilesDirectory(projectId, installedBuild(destination))).filePath(relative) : target);
        }
        QDir(entry.filePath()).removeRecursively();
        qCInfo(lcLauncher) << "Finished an interrupted update of" << projectId;
        ++recovered;
    }
    return recovered;
}

QString LauncherDownloadManager::keptFilesDirectory(const QString &projectId, const QString &build) const
{
    if (!m_platformManager || !isValidProjectId(projectId))
        return {};
    const QString dataRoot = QFileInfo(m_platformManager->dataDirectory()).canonicalFilePath();
    // Linux launchers keep the folder name they always had.
    const QString name = build.startsWith(QStringLiteral("windows-"))
        ? projectId + QStringLiteral(".windows") : projectId;
    return dataRoot.isEmpty() ? QString()
                              : QDir(dataRoot).filePath(QStringLiteral("launcher-data/") + name);
}

QString LauncherDownloadManager::installedBuild(const QString &installDirectory)
{
    QFile releaseFile(QDir(installDirectory).filePath(QStringLiteral(".recompstation-release")));
    if (QFileInfo(releaseFile.fileName()).isSymLink() || releaseFile.size() >= 4096
        || !releaseFile.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(releaseFile.readAll()).object()
        .value(QStringLiteral("build")).toString().left(40);
}

int LauncherDownloadManager::setAsideUserFiles(const QString &projectId, const QString &installDirectory)
{
    const QString keepRoot = keptFilesDirectory(projectId, installedBuild(installDirectory));
    if (keepRoot.isEmpty())
        return -1;
    int moved = 0;
    for (const QString &relative : playerFiles(installDirectory)) {
        if (!moveReplacing(QDir(installDirectory).filePath(relative), QDir(keepRoot).filePath(relative)))
            return -1;
        ++moved;
    }
    return moved;
}

int LauncherDownloadManager::restoreKeptFiles(const QString &projectId, const QString &installDirectory)
{
    const QString keepRoot = keptFilesDirectory(projectId, installedBuild(installDirectory));
    const QFileInfo keepInfo(keepRoot);
    if (keepRoot.isEmpty() || !keepInfo.isDir() || keepInfo.isSymLink())
        return 0;
    int restored = 0;
    for (const QString &relative : relativeFiles(keepRoot)) {
        const QString destination = QDir(installDirectory).filePath(relative);
        // A file the new version ships itself wins; the old copy stays kept.
        if (QFileInfo::exists(destination) || QFileInfo(destination).isSymLink())
            continue;
        if (moveReplacing(QDir(keepRoot).filePath(relative), destination))
            ++restored;
    }
    removeEmptyFolders(keepRoot);
    return restored;
}

bool LauncherDownloadManager::uninstallLauncher(const QString &projectId)
{
    if (!m_platformManager || !isValidProjectId(projectId)
        || m_activeInstallProjects.contains(projectId))
        return false;

    const QString dataRoot = QFileInfo(m_platformManager->dataDirectory()).canonicalFilePath();
    if (dataRoot.isEmpty())
        return false;

    const QString launcherRoot = QDir(dataRoot).filePath("launchers");
    if (QFileInfo(launcherRoot).canonicalFilePath() != launcherRoot)
        return false;

    const QString target = QDir(launcherRoot).filePath(projectId);
    const QFileInfo targetInfo(target);
    if (!targetInfo.isDir() || targetInfo.isSymLink()
        || targetInfo.canonicalFilePath() != target)
        return false;

    bool isManagedInstallation = false;
    for (const QVariant &entry : installedLaunchers()) {
        if (entry.toMap().value(QStringLiteral("projectId")).toString() == projectId) {
            isManagedInstallation = true;
            break;
        }
    }
    if (!isManagedInstallation)
        return false;

    const QString build = installedBuild(target);
    const int kept = setAsideUserFiles(projectId, target);
    if (kept < 0) {
        qCWarning(lcLauncher) << "Uninstall stopped: could not set aside the player's files for" << projectId;
        return false;
    }
    if (kept > 0)
        qCInfo(lcLauncher) << "Set aside" << kept << "file(s) the launcher created, in"
                           << RecompLogging::redactHome(keptFilesDirectory(projectId, build));
    return QDir(target).removeRecursively();
}
