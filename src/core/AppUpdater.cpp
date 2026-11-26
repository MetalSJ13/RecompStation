#include "AppUpdater.h"

#include "ArchiveExtractor.h"
#include "Logging.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QSysInfo>
#include <QTimer>
#include <QUuid>

namespace {
const QString kOwner = QStringLiteral("MetalSJ13");
const QString kRepository = QStringLiteral("RecompStation");
const QString kDownloadPrefix =
    QStringLiteral("https://github.com/MetalSJ13/RecompStation/releases/download/");
const QString kVersionFile = QStringLiteral(".recompstation-update-version");
constexpr qint64 kMaximumDownload = 1024LL * 1024 * 1024;   // 1 GiB
constexpr qint64 kCheckIntervalSeconds = 20 * 60 * 60;      // about once a day

QString stripV(QString version)
{
    version = version.trimmed();
    if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        version.remove(0, 1);
    return version;
}

bool isPortableFolder(const QString &root)
{
    const QDir dir(root);
    return QFileInfo(dir.filePath(QStringLiteral("RecompStation"))).isFile()
        && QFileInfo(dir.filePath(QStringLiteral("usr/bin/RecompStationApp"))).isFile();
}
}

AppUpdater::AppUpdater(const QString &workDirectory, QObject *parent)
    : QObject(parent),
      m_install(detectInstall(qEnvironmentVariable("APPIMAGE"),
                              QCoreApplication::applicationFilePath())),
      m_workDirectory(workDirectory)
{
    m_network.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    connect(&m_github, &GitHubManager::releaseLoaded, this,
            [this](int requestId, const GitHubRelease &release) {
        if (requestId != m_releaseRequest)
            return;
        m_releaseRequest = -1;
        QSettings settings;
        settings.setValue(QStringLiteral("updates/lastCheck"),
                          QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        applyRelease(release, true);
        rememberRelease(release);
        qCInfo(lcApplication) << "Update check:" << currentVersion() << "installed,"
                              << m_latestVersion << "latest";
        setState(updateAvailable() ? QStringLiteral("available") : QStringLiteral("upToDate"));
        m_userInitiated = false;
        updatePrompt();
    });
    connect(&m_github, &GitHubManager::requestFailed, this,
            [this](int requestId, const QString &message) {
        if (requestId != m_releaseRequest)
            return;
        m_releaseRequest = -1;
        qCWarning(lcApplication) << "Update check failed:" << message;
        setState(QStringLiteral("checkFailed"),
                 QStringLiteral("Couldn't check for updates. Try again later."));
        m_userInitiated = false;
    });
}

QString AppUpdater::currentVersion() const
{
    return QCoreApplication::applicationVersion();
}

bool AppUpdater::updateAvailable() const
{
    return !m_latestVersion.isEmpty() && compareVersions(m_latestVersion, currentVersion()) > 0;
}

QString AppUpdater::lastChecked() const
{
    const QDateTime when = QDateTime::fromString(
        QSettings().value(QStringLiteral("updates/lastCheck")).toString(), Qt::ISODate).toLocalTime();
    if (!when.isValid())
        return {};
    const QDate today = QDate::currentDate();
    const QString time = when.time().toString(QStringLiteral("HH:mm"));
    if (when.date() == today)
        return QStringLiteral("today at %1").arg(time);
    if (when.date() == today.addDays(-1))
        return QStringLiteral("yesterday at %1").arg(time);
    return when.date().toString(QStringLiteral("d MMM yyyy"));
}

bool AppUpdater::checkAutomatically() const
{
    return QSettings().value(QStringLiteral("updates/checkAutomatically"), true).toBool();
}

void AppUpdater::setCheckAutomatically(bool enabled)
{
    if (enabled == checkAutomatically())
        return;
    QSettings().setValue(QStringLiteral("updates/checkAutomatically"), enabled);
    emit settingsChanged();
}

QString AppUpdater::skippedVersion() const
{
    return QSettings().value(QStringLiteral("updates/skippedVersion")).toString();
}

void AppUpdater::checkAtStartup()
{
    // A newer version found earlier ("Remind me later") is offered again
    // without asking GitHub.
    if (restoreRememberedRelease() && updateAvailable()) {
        setState(QStringLiteral("available"));
        updatePrompt();
    }
    if (!checkAutomatically())
        return;
    const QDateTime last = QDateTime::fromString(
        QSettings().value(QStringLiteral("updates/lastCheck")).toString(), Qt::ISODate);
    if (last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < kCheckIntervalSeconds)
        return;
    // Let the library load first.
    QTimer::singleShot(8000, this, [this]() { requestRelease(); });
}

void AppUpdater::checkNow()
{
    m_userInitiated = true;
    requestRelease();
}

void AppUpdater::requestRelease()
{
    if (m_releaseRequest >= 0 || m_state == QStringLiteral("downloading")
        || m_state == QStringLiteral("verifying") || m_state == QStringLiteral("ready"))
        return;
    setState(QStringLiteral("checking"));
    m_releaseRequest = m_github.fetchLatestRelease(kOwner, kRepository);
}

void AppUpdater::applyRelease(const GitHubRelease &release, bool)
{
    m_latestVersion = stripV(release.tagName);
    m_releaseNotes = changesSection(release.body);
    m_releaseUrl = release.htmlUrl;
    const QDateTime published = QDateTime::fromString(release.publishedAt, Qt::ISODate);
    m_releaseDate = published.isValid() ? published.toLocalTime().date().toString(QStringLiteral("d MMM yyyy"))
                                        : QString();
    m_asset = selectAsset(release.assets, m_install.kind);
    emit releaseChanged();
}

void AppUpdater::rememberRelease(const GitHubRelease &release) const
{
    QJsonArray assets;
    for (const GitHubReleaseAsset &asset : release.assets) {
        assets.append(QJsonObject{{QStringLiteral("name"), asset.name},
                                  {QStringLiteral("url"), asset.downloadUrl},
                                  {QStringLiteral("size"), double(asset.size)},
                                  {QStringLiteral("sha256"), asset.sha256}});
    }
    const QJsonObject saved{{QStringLiteral("tag"), release.tagName},
                            {QStringLiteral("body"), release.body},
                            {QStringLiteral("url"), release.htmlUrl},
                            {QStringLiteral("published"), release.publishedAt},
                            {QStringLiteral("assets"), assets}};
    QSettings().setValue(QStringLiteral("updates/release"),
                         QString::fromUtf8(QJsonDocument(saved).toJson(QJsonDocument::Compact)));
}

bool AppUpdater::restoreRememberedRelease()
{
    const QJsonObject saved = QJsonDocument::fromJson(
        QSettings().value(QStringLiteral("updates/release")).toString().toUtf8()).object();
    if (saved.isEmpty())
        return false;
    GitHubRelease release;
    release.tagName = saved.value(QStringLiteral("tag")).toString();
    release.body = saved.value(QStringLiteral("body")).toString().left(20000);
    release.htmlUrl = saved.value(QStringLiteral("url")).toString();
    release.publishedAt = saved.value(QStringLiteral("published")).toString();
    for (const QJsonValue &value : saved.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject object = value.toObject();
        GitHubReleaseAsset asset;
        asset.name = object.value(QStringLiteral("name")).toString();
        asset.downloadUrl = object.value(QStringLiteral("url")).toString();
        asset.size = qint64(object.value(QStringLiteral("size")).toDouble());
        asset.sha256 = object.value(QStringLiteral("sha256")).toString();
        release.assets.append(asset);
    }
    if (release.tagName.isEmpty())
        return false;
    applyRelease(release, false);
    return true;
}

void AppUpdater::updatePrompt()
{
    const bool busy = m_state == QStringLiteral("downloading") || m_state == QStringLiteral("verifying")
        || m_state == QStringLiteral("ready");
    setPromptPending(updateAvailable() && m_latestVersion != skippedVersion()
                     && !m_promptAnswered && !busy);
}

void AppUpdater::setPromptPending(bool pending)
{
    if (m_promptPending == pending)
        return;
    m_promptPending = pending;
    emit promptPendingChanged();
}

void AppUpdater::setState(const QString &state, const QString &error)
{
    if (m_state == state && m_errorMessage == error)
        return;
    m_state = state;
    m_errorMessage = error;
    emit stateChanged();
}

void AppUpdater::remindLater()
{
    m_promptAnswered = true;
    setPromptPending(false);
}

void AppUpdater::skipLatestVersion()
{
    if (m_latestVersion.isEmpty())
        return;
    QSettings settings;
    m_undoSkipValue = settings.value(QStringLiteral("updates/skippedVersion")).toString();
    settings.setValue(QStringLiteral("updates/skippedVersion"), m_latestVersion);
    m_promptAnswered = true;
    setPromptPending(false);
    emit settingsChanged();
    emit notice(QStringLiteral("Skipped %1").arg(m_latestVersion), true);
}

void AppUpdater::undoSkip()
{
    QSettings settings;
    if (m_undoSkipValue.isEmpty())
        settings.remove(QStringLiteral("updates/skippedVersion"));
    else
        settings.setValue(QStringLiteral("updates/skippedVersion"), m_undoSkipValue);
    m_promptAnswered = false;
    emit settingsChanged();
    updatePrompt();
}

void AppUpdater::clearSkippedVersion()
{
    QSettings().remove(QStringLiteral("updates/skippedVersion"));
    emit settingsChanged();
}

void AppUpdater::startUpdate()
{
    if (!canInstall() || !updateAvailable() || m_reply
        || m_state == QStringLiteral("ready"))
        return;
    m_promptAnswered = true;
    setPromptPending(false);
    if (m_asset.downloadUrl.isEmpty()) {
        failUpdate(QStringLiteral("This version has no download for this kind of install."));
        return;
    }
    if (m_asset.sha256.isEmpty()) {
        failUpdate(QStringLiteral("This version couldn't be checked, so it wasn't installed."));
        return;
    }

    if (m_install.kind == QStringLiteral("appimage")) {
        // Downloaded next to the AppImage, so it can replace it in one step.
        const QFileInfo current(m_install.path);
        if (!QFileInfo(current.absolutePath()).isWritable()) {
            failUpdate(QStringLiteral("RecompStation can't write to the folder it's in."));
            return;
        }
        m_downloadPath = QDir(current.absolutePath()).filePath(
            QLatin1Char('.') + current.fileName() + QStringLiteral(".download"));
    } else {
        QDir().mkpath(m_workDirectory);
        m_downloadPath = QDir(m_workDirectory).filePath(QStringLiteral("RecompStation-update.zip"));
    }

    delete m_file;
    m_file = new QSaveFile(m_downloadPath, this);
    if (!m_file->open(QIODevice::WriteOnly)) {
        failUpdate(QStringLiteral("Couldn't save the download."));
        return;
    }
    m_hash.reset();
    m_bytesReceived = 0;
    m_bytesTotal = m_asset.size;
    emit progressChanged();
    setState(QStringLiteral("downloading"));

    QNetworkRequest request{QUrl(m_asset.downloadUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("RecompStation/%1").arg(currentVersion()));
    m_reply = m_network.get(request);
    qCInfo(lcApplication) << "Downloading update" << m_latestVersion << m_asset.name;
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (!m_reply || !m_file)
            return;
        const QByteArray data = m_reply->readAll();
        if (m_bytesReceived + data.size() > qMax(m_asset.size, qint64(0)) + 1
            || m_bytesReceived + data.size() > kMaximumDownload) {
            failUpdate(QStringLiteral("The download was damaged, so it wasn't used."));
            return;
        }
        if (m_file->write(data) != data.size()) {
            failUpdate(QStringLiteral("Couldn't save the download."));
            return;
        }
        m_hash.addData(data);
        m_bytesReceived += data.size();
        emit progressChanged();
    });
    connect(m_reply, &QNetworkReply::finished, this, [this]() { finishDownload(); });
}

void AppUpdater::finishDownload()
{
    QNetworkReply *reply = m_reply;
    if (!reply)
        return;
    m_reply = nullptr;
    reply->deleteLater();
    if (!m_file)
        return;
    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() != QNetworkReply::OperationCanceledError)
            failUpdate(QStringLiteral("The download didn't finish. Check your connection and try again."));
        return;
    }
    const QByteArray rest = reply->readAll();
    if (!rest.isEmpty()) {
        m_file->write(rest);
        m_hash.addData(rest);
        m_bytesReceived += rest.size();
    }
    setState(QStringLiteral("verifying"));
    const QString actual = QString::fromLatin1(m_hash.result().toHex());
    if (m_bytesReceived != m_asset.size || actual != m_asset.sha256.toLower()) {
        qCWarning(lcApplication) << "Update download rejected: size" << m_bytesReceived
                                 << "of" << m_asset.size << "sha256" << actual;
        failUpdate(QStringLiteral("The download was damaged, so it wasn't used."));
        return;
    }
    if (!m_file->commit()) {
        delete m_file;
        m_file = nullptr;
        failUpdate(QStringLiteral("Couldn't save the download."));
        return;
    }
    delete m_file;
    m_file = nullptr;

    QString error;
    bool installed = false;
    if (m_install.kind == QStringLiteral("appimage")) {
        installed = replaceAppImage(m_install.path, m_downloadPath, &error);
    } else {
        installed = stageZipUpdate(m_downloadPath, m_install.path, m_latestVersion, &error);
        QFile::remove(m_downloadPath);
    }
    if (!installed) {
        QFile::remove(m_downloadPath);
        qCWarning(lcApplication) << "Update could not be put in place:" << error;
        setState(QStringLiteral("failed"), error);
        return;
    }
    qCInfo(lcApplication) << "Update" << m_latestVersion << "is ready; it starts with the next restart.";
    setState(QStringLiteral("ready"));
}

void AppUpdater::failUpdate(const QString &message)
{
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->abort();
        reply->deleteLater();
    }
    if (m_file) {
        m_file->cancelWriting();
        delete m_file;
        m_file = nullptr;
    }
    qCWarning(lcApplication) << "Update failed:" << message;
    setState(QStringLiteral("failed"), message);
}

void AppUpdater::cancelUpdate()
{
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->abort();
        reply->deleteLater();
    }
    if (m_file) {
        m_file->cancelWriting();
        delete m_file;
        m_file = nullptr;
    }
    setState(updateAvailable() ? QStringLiteral("available") : QStringLiteral("idle"));
}

void AppUpdater::restartNow()
{
    if (m_state == QStringLiteral("ready"))
        emit restartRequested();
}

bool AppUpdater::applyStagedUpdate()
{
    if (m_install.kind != QStringLiteral("zip") || !QFileInfo::exists(stagedPath(m_install.path)))
        return false;
    QString error;
    if (!applyStagedZip(m_install.path, &error)) {
        qCWarning(lcApplication) << "Could not put the staged update in place:" << error;
        return false;
    }
    qCInfo(lcApplication) << "Staged update put in place.";
    return true;
}

bool AppUpdater::finishAtStartup()
{
    if (m_install.kind == QStringLiteral("appimage")) {
        const QString previous = previousPath(m_install.path);
        if (QFileInfo(previous).isFile() && !QFileInfo(previous).isSymLink()) {
            QFile::remove(previous);
            qCInfo(lcApplication) << "Removed the previous AppImage after an update.";
        }
        const QFileInfo current(m_install.path);
        QFile::remove(QDir(current.absolutePath()).filePath(
            QLatin1Char('.') + current.fileName() + QStringLiteral(".download")));
        return false;
    }
    if (m_install.kind != QStringLiteral("zip"))
        return false;

    const QString staged = stagedPath(m_install.path);
    if (QFileInfo(staged).isDir()) {
        if (compareVersions(stagedVersion(m_install.path), currentVersion()) > 0) {
            QString error;
            if (applyStagedZip(m_install.path, &error)) {
                qCInfo(lcApplication) << "Put a staged update in place at startup; restarting into it.";
                return true;
            }
            qCWarning(lcApplication) << "Could not put the staged update in place:" << error;
        }
        QDir(staged).removeRecursively();
    }
    const QString previous = previousPath(m_install.path);
    if (QFileInfo(previous).isDir() && !QFileInfo(previous).isSymLink() && isPortableFolder(previous)) {
        QDir(previous).removeRecursively();
        qCInfo(lcApplication) << "Removed the previous version's folder after an update.";
    }
    // Unpacking folders an interrupted update left behind.
    const QFileInfo root(m_install.path);
    QDirIterator leftovers(root.absolutePath(),
                           QStringList{QLatin1Char('.') + root.fileName() + QStringLiteral(".unpack-*")},
                           QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    while (leftovers.hasNext()) {
        const QFileInfo leftover(leftovers.next());
        if (!leftover.isSymLink())
            QDir(leftover.filePath()).removeRecursively();
    }
    return false;
}

AppUpdater::Install AppUpdater::detectInstall(const QString &appImageEnvironment,
                                              const QString &applicationFilePath)
{
#ifdef Q_OS_WIN
    // Updating the Windows version isn't built yet: it points to the website.
    Q_UNUSED(appImageEnvironment);
    return {QStringLiteral("windows"), QFileInfo(applicationFilePath).absolutePath()};
#endif
    if (!appImageEnvironment.isEmpty()) {
        const QFileInfo appImage(appImageEnvironment);
        if (appImage.isFile() && !appImage.isSymLink())
            return {QStringLiteral("appimage"), appImage.canonicalFilePath()};
    }
    const QFileInfo program(applicationFilePath);
    const QString binDirectory = program.absolutePath();
    if (binDirectory.endsWith(QStringLiteral("/usr/bin"))) {
        const QString root = QDir(binDirectory + QStringLiteral("/../..")).canonicalPath();
        if (!root.isEmpty() && isPortableFolder(root)
            && QFileInfo(QDir(root).filePath(QStringLiteral("THIRD_PARTY_NOTICES.md"))).isFile())
            return {QStringLiteral("zip"), root};
    }
    return {QStringLiteral("source"), QString()};
}

int AppUpdater::compareVersions(const QString &left, const QString &right)
{
    auto split = [](const QString &version, QList<int> *numbers, bool *preRelease) {
        const QString clean = stripV(version);
        const QString core = clean.section(QLatin1Char('-'), 0, 0);
        *preRelease = clean.contains(QLatin1Char('-'));
        for (const QString &part : core.split(QLatin1Char('.')))
            numbers->append(part.toInt());
    };
    QList<int> a, b;
    bool aPre = false, bPre = false;
    split(left, &a, &aPre);
    split(right, &b, &bPre);
    for (int i = 0; i < qMax(a.size(), b.size()); ++i) {
        const int x = i < a.size() ? a.at(i) : 0;
        const int y = i < b.size() ? b.at(i) : 0;
        if (x != y)
            return x < y ? -1 : 1;
    }
    if (aPre != bPre)
        return aPre ? -1 : 1;
    return 0;
}

GitHubReleaseAsset AppUpdater::selectAsset(const QList<GitHubReleaseAsset> &assets,
                                           const QString &installKind)
{
    static const QRegularExpression appImage(
        QStringLiteral("^RecompStation-[0-9][0-9A-Za-z.]*-x86_64\\.AppImage$"));
    static const QRegularExpression zip(
        QStringLiteral("^RecompStation-[0-9][0-9A-Za-z.]*-linux-x86_64\\.zip$"));
    if (QSysInfo::currentCpuArchitecture() != QStringLiteral("x86_64"))
        return {};
    const QRegularExpression *pattern = installKind == QStringLiteral("appimage") ? &appImage
        : installKind == QStringLiteral("zip") ? &zip : nullptr;
    if (!pattern)
        return {};
    for (const GitHubReleaseAsset &asset : assets) {
        if (pattern->match(asset.name).hasMatch() && asset.downloadUrl.startsWith(kDownloadPrefix)
            && asset.size > 0 && asset.size <= kMaximumDownload)
            return asset;
    }
    return {};
}

QString AppUpdater::changesSection(const QString &releaseBody)
{
    QString body = releaseBody;
    body.remove(QRegularExpression(QStringLiteral("<!--.*?-->"),
                                   QRegularExpression::DotMatchesEverythingOption));
    body.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    // Release notes carry a downloads table and a notice above "## Changes";
    // the app shows only what changed.
    static const QRegularExpression changesHeading(
        QStringLiteral("^#{1,3}\\s*(?:Changes|What'?s new)\\s*$"),
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch heading = changesHeading.match(body);
    if (heading.hasMatch()) {
        body = body.mid(heading.capturedEnd());
        static const QRegularExpression nextHeading(QStringLiteral("^#{1,2}\\s"),
                                                    QRegularExpression::MultilineOption);
        const QRegularExpressionMatch next = nextHeading.match(body);
        if (next.hasMatch())
            body = body.left(next.capturedStart());
    }
    body = body.trimmed();
    if (body.size() > 3000)
        body = body.left(3000).trimmed() + QStringLiteral("…");
    return body;
}

QString AppUpdater::previousPath(const QString &path)
{
    const QFileInfo info(path);
    return QDir(info.absolutePath()).filePath(QLatin1Char('.') + info.fileName() + QStringLiteral(".previous"));
}

QString AppUpdater::stagedPath(const QString &portableRoot)
{
    const QFileInfo info(portableRoot);
    return QDir(info.absolutePath()).filePath(QLatin1Char('.') + info.fileName() + QStringLiteral(".update"));
}

QString AppUpdater::stagedVersion(const QString &portableRoot)
{
    QFile file(QDir(stagedPath(portableRoot)).filePath(kVersionFile));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 64)
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

bool AppUpdater::replaceAppImage(const QString &currentPath, const QString &newFile, QString *error)
{
    QFile::setPermissions(newFile, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                   | QFile::ReadGroup | QFile::ExeGroup
                                   | QFile::ReadOther | QFile::ExeOther);
    const QString previous = previousPath(currentPath);
    QFile::remove(previous);
    // The running copy keeps working after its file is renamed: it was
    // opened when it started.
    if (!QFile::rename(currentPath, previous)) {
        *error = QStringLiteral("Couldn't replace the current version.");
        return false;
    }
    if (!QFile::rename(newFile, currentPath)) {
        QFile::rename(previous, currentPath);
        *error = QStringLiteral("Couldn't replace the current version.");
        return false;
    }
    return true;
}

bool AppUpdater::stageZipUpdate(const QString &zipPath, const QString &portableRoot,
                                const QString &version, QString *error)
{
    const QFileInfo root(portableRoot);
    if (!QFileInfo(root.absolutePath()).isWritable()) {
        *error = QStringLiteral("RecompStation can't write to the folder it's in.");
        return false;
    }
    const QString staged = stagedPath(portableRoot);
    if (QFileInfo::exists(staged))
        QDir(staged).removeRecursively();
    const QString unpack = QDir(root.absolutePath()).filePath(
        QLatin1Char('.') + root.fileName() + QStringLiteral(".unpack-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const ArchiveExtractor::Result result = ArchiveExtractor::extractArchive(
        zipPath, unpack, QStringLiteral("usr/bin/RecompStationApp"));
    if (!result.success) {
        qCWarning(lcApplication) << "Update could not be unpacked:" << result.error;
        *error = QStringLiteral("The download couldn't be unpacked.");
        return false;
    }
    const QString newRoot = QDir(QFileInfo(result.executablePath).absolutePath()
                                 + QStringLiteral("/../..")).canonicalPath();
    const QString canonicalUnpack = QFileInfo(unpack).canonicalFilePath();
    if (newRoot.isEmpty() || !(newRoot == canonicalUnpack || newRoot.startsWith(canonicalUnpack + QLatin1Char('/')))
        || !isPortableFolder(newRoot)) {
        QDir(unpack).removeRecursively();
        *error = QStringLiteral("The download isn't a RecompStation package.");
        return false;
    }
    QFile versionFile(QDir(newRoot).filePath(kVersionFile));
    if (!versionFile.open(QIODevice::WriteOnly) || versionFile.write(version.toUtf8()) < 0) {
        QDir(unpack).removeRecursively();
        *error = QStringLiteral("Couldn't prepare the update.");
        return false;
    }
    versionFile.close();
    if (!QDir().rename(newRoot, staged)) {
        QDir(unpack).removeRecursively();
        *error = QStringLiteral("Couldn't prepare the update.");
        return false;
    }
    QDir(unpack).removeRecursively();
    return true;
}

bool AppUpdater::applyStagedZip(const QString &portableRoot, QString *error)
{
    const QString staged = stagedPath(portableRoot);
    if (!QFileInfo(staged).isDir() || !isPortableFolder(staged)) {
        *error = QStringLiteral("No update is waiting.");
        return false;
    }
    const QString previous = previousPath(portableRoot);
    if (QFileInfo::exists(previous))
        QDir(previous).removeRecursively();
    if (!QDir().rename(portableRoot, previous)) {
        *error = QStringLiteral("Couldn't move the current version aside.");
        return false;
    }
    if (!QDir().rename(staged, portableRoot)) {
        QDir().rename(previous, portableRoot);
        *error = QStringLiteral("Couldn't put the new version in place.");
        return false;
    }
    QFile::remove(QDir(portableRoot).filePath(kVersionFile));
    return true;
}
