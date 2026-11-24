#include "LauncherDownloadManager.h"

#include "PlatformManager.h"
#include "Logging.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QFile>

#include <QDir>
#include <QDirIterator>
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
                                  QStringLiteral(".recompstation-files")};

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
    const qint64 bytesReceived = download->bytesReceived;
    m_activeTargets.remove(download->targetPath);
    m_downloads.remove(requestId);
    reply->deleteLater();
    delete download;

    qCInfo(lcLauncher) << "Download completed:" << projectId << assetName
                       << "bytes:" << bytesReceived;
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
    const QString &expectedExecutable
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
    if (!isSafeRelativePath(expectedExecutable)) {
        reject(QStringLiteral("The expected launcher executable path is invalid."));
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
    if (destinationInfo.exists() || destinationInfo.isSymLink()) {
        reject(QStringLiteral("A launcher is already installed for this project. Existing installations will not be overwritten."));
        return requestId;
    }

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
            this, [this, watcher, requestId, projectId, archivePath,
                   extractionPath, destination]() {
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

        installed.append(QVariantMap{
            {QStringLiteral("projectId"), projectId},
            {QStringLiteral("directory"), directory},
            {QStringLiteral("executablePath"), executablePath}
        });
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

QString LauncherDownloadManager::keptFilesDirectory(const QString &projectId) const
{
    if (!m_platformManager || !isValidProjectId(projectId))
        return {};
    const QString dataRoot = QFileInfo(m_platformManager->dataDirectory()).canonicalFilePath();
    return dataRoot.isEmpty() ? QString()
                              : QDir(dataRoot).filePath(QStringLiteral("launcher-data/") + projectId);
}

int LauncherDownloadManager::setAsideUserFiles(const QString &projectId, const QString &installDirectory)
{
    const QString keepRoot = keptFilesDirectory(projectId);
    if (keepRoot.isEmpty())
        return -1;

    // Installs made before the file list existed: keep the usual places
    // launchers write to.
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
    static const QStringList fallback{QStringLiteral("saves"), QStringLiteral("portable"),
                                      QStringLiteral("portable.txt"), QStringLiteral("userdata"),
                                      QStringLiteral("mods"), QStringLiteral("mod_config"),
                                      QStringLiteral("mods.json")};

    int moved = 0;
    for (const QString &relative : relativeFiles(installDirectory)) {
        const bool players = hasList ? !packaged.contains(relative)
                                     : fallback.contains(relative.section(QLatin1Char('/'), 0, 0));
        if (!players)
            continue;
        if (!moveReplacing(QDir(installDirectory).filePath(relative), QDir(keepRoot).filePath(relative)))
            return -1;
        ++moved;
    }
    return moved;
}

int LauncherDownloadManager::restoreKeptFiles(const QString &projectId, const QString &installDirectory)
{
    const QString keepRoot = keptFilesDirectory(projectId);
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

    const int kept = setAsideUserFiles(projectId, target);
    if (kept < 0) {
        qCWarning(lcLauncher) << "Uninstall stopped: could not set aside the player's files for" << projectId;
        return false;
    }
    if (kept > 0)
        qCInfo(lcLauncher) << "Set aside" << kept << "file(s) the launcher created, in"
                           << RecompLogging::redactHome(keptFilesDirectory(projectId));
    return QDir(target).removeRecursively();
}
