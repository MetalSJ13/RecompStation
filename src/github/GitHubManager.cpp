#include "GitHubManager.h"
#include "../core/Logging.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>

namespace {
bool isHttpsUrl(const QString &value)
{
    const QUrl url(value);
    return url.isValid()
        && url.scheme() == QStringLiteral("https")
        && !url.host().isEmpty();
}

bool parseJsonObject(const QByteArray &response,
                     QJsonObject *object,
                     QString *errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid GitHub JSON response: %1")
                                .arg(parseError.errorString());
        }
        return false;
    }

    if (object)
        *object = document.object();
    return true;
}

QString apiErrorMessage(int statusCode, const QString &errorString,
                        const QString &resource)
{
    if (statusCode == 403) {
        return QStringLiteral("GitHub API rate limit or access restriction "
                              "while requesting %1 (HTTP 403).")
            .arg(resource);
    }
    if (statusCode == 404) {
        return QStringLiteral("GitHub could not find %1 (HTTP 404).")
            .arg(resource);
    }
    return QStringLiteral("GitHub request for %1 failed (HTTP %2): %3")
        .arg(resource)
        .arg(statusCode)
        .arg(errorString);
}
}

GitHubManager::GitHubManager(QObject *parent)
    : QObject(parent)
{
}

bool GitHubManager::isValidRepositoryIdentifier(
    const QString &owner, const QString &repository) const
{
    static const QRegularExpression segmentPattern(
        QStringLiteral("^[A-Za-z0-9_.-]+$"));
    return segmentPattern.match(owner).hasMatch()
        && segmentPattern.match(repository).hasMatch();
}

void GitHubManager::setCommonHeaders(QNetworkRequest &request) const
{
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader("User-Agent", "RecompStation");
    request.setTransferTimeout(15000);
}

int GitHubManager::fetchRepository(const QString &owner,
                                   const QString &repository)
{
    const int requestId = m_nextRequestId++;
    const QString cleanOwner = owner.trimmed();
    const QString cleanRepository = repository.trimmed();
    qCDebug(lcGitHub) << "Repository request:" << cleanOwner << "/" << cleanRepository;

    if (!isValidRepositoryIdentifier(cleanOwner, cleanRepository)) {
        emit requestFailed(requestId,
                           QStringLiteral("A valid GitHub owner and repository are required."));
        return requestId;
    }

    const QUrl url(QStringLiteral("https://api.github.com/repos/%1/%2")
                       .arg(cleanOwner, cleanRepository));
    QNetworkRequest request(url);
    setCommonHeaders(request);

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId,
                                                     cleanOwner, cleanRepository]() {
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            const QString message = apiErrorMessage(
                statusCode, reply->errorString(),
                QStringLiteral("repository %1/%2").arg(cleanOwner, cleanRepository));
            reply->deleteLater();
            qCWarning(lcGitHub) << "Repository request failed:" << message;
            emit requestFailed(requestId, message);
            return;
        }

        GitHubRepository repositoryInfo;
        QString error;
        const bool ok = parseRepositoryResponse(response, &repositoryInfo, &error);
        reply->deleteLater();
        if (!ok) {
            emit requestFailed(requestId, error);
            return;
        }
        qCInfo(lcGitHub) << "Repository data loaded:" << cleanOwner << "/" << cleanRepository;
        emit repositoryLoaded(requestId, repositoryInfo);
    });

    return requestId;
}

int GitHubManager::fetchLatestRelease(const QString &owner,
                                      const QString &repository,
                                      bool includePrereleases,
                                      const QString &assetPattern)
{
    const int requestId = m_nextRequestId++;
    const QString cleanOwner = owner.trimmed();
    const QString cleanRepository = repository.trimmed();
    qCDebug(lcGitHub) << "Latest release request:" << cleanOwner << "/" << cleanRepository
                      << "pre-releases:" << includePrereleases;

    if (!isValidRepositoryIdentifier(cleanOwner, cleanRepository)) {
        emit requestFailed(requestId,
                           QStringLiteral("A valid GitHub owner and repository are required."));
        return requestId;
    }

    requestRelease(requestId, cleanOwner, cleanRepository,
                   includePrereleases ? QStringLiteral("list") : QStringLiteral("latest"),
                   includePrereleases, assetPattern.trimmed());
    return requestId;
}

void GitHubManager::requestRelease(int requestId, const QString &cleanOwner,
                                   const QString &cleanRepository, const QString &stage,
                                   bool includePrereleases, const QString &assetPattern,
                                   const GitHubRelease &fallback)
{
    const bool fromList = stage != QStringLiteral("latest");
    const QUrl url(fromList
        ? QStringLiteral("https://api.github.com/repos/%1/%2/releases?per_page=%3")
              .arg(cleanOwner, cleanRepository,
                   stage == QStringLiteral("match") ? QStringLiteral("30") : QStringLiteral("10"))
        : QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
              .arg(cleanOwner, cleanRepository));
    QNetworkRequest request(url);
    setCommonHeaders(request);

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId, cleanOwner,
                                                     cleanRepository, stage, includePrereleases,
                                                     assetPattern, fallback]() {
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->readAll();
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            // No full release yet: GitHub answers 404 for /releases/latest
            // when a project has only published pre-releases.
            if (stage == QStringLiteral("latest") && statusCode == 404) {
                qCInfo(lcGitHub) << "No full release for" << cleanOwner << "/" << cleanRepository
                                 << "- checking pre-releases";
                requestRelease(requestId, cleanOwner, cleanRepository, QStringLiteral("list"),
                               includePrereleases, assetPattern);
                return;
            }
            // The newest release was found; only the search further back failed.
            if (stage == QStringLiteral("match")) {
                finishRelease(requestId, cleanOwner, cleanRepository, fallback);
                return;
            }
            const QString message = apiErrorMessage(
                statusCode, reply->errorString(),
                QStringLiteral("latest release for %1/%2")
                    .arg(cleanOwner, cleanRepository));
            qCWarning(lcGitHub) << "Release request failed:" << message;
            emit requestFailed(requestId, message);
            return;
        }

        GitHubRelease release;
        QString error;
        if (stage == QStringLiteral("match")) {
            // Keep the newest release (and its "no package" explanation)
            // when no recent release has a file for this system.
            if (!parseNewestReleaseFromList(response, &release, &error, assetPattern,
                                            !includePrereleases)) {
                qCInfo(lcGitHub) << "No recent release of" << cleanOwner << "/" << cleanRepository
                                 << "has a file for this system";
                release = fallback;
            } else {
                qCInfo(lcGitHub) << "Newest release" << fallback.tagName << "of" << cleanOwner
                                 << "/" << cleanRepository << "has no file for this system; using"
                                 << release.tagName;
            }
            finishRelease(requestId, cleanOwner, cleanRepository, release);
            return;
        }

        const bool ok = stage == QStringLiteral("list")
            ? parseNewestReleaseFromList(response, &release, &error)
            : parseLatestReleaseResponse(response, &release, &error);
        if (!ok) {
            emit requestFailed(requestId, error);
            return;
        }
        if (!assetPattern.isEmpty() && !releaseHasAsset(release, assetPattern)) {
            requestRelease(requestId, cleanOwner, cleanRepository, QStringLiteral("match"),
                           includePrereleases, assetPattern, release);
            return;
        }
        finishRelease(requestId, cleanOwner, cleanRepository, release);
    });
}

void GitHubManager::finishRelease(int requestId, const QString &cleanOwner,
                                  const QString &cleanRepository, const GitHubRelease &release)
{
    QSettings cache;
    cache.beginGroup(QStringLiteral("GitHubReleaseCache"));
    cache.setValue(cleanOwner + QLatin1Char('/') + cleanRepository,
                   QJsonDocument(QJsonObject{
                       {QStringLiteral("tagName"), release.tagName},
                       {QStringLiteral("name"), release.name},
                       {QStringLiteral("htmlUrl"), release.htmlUrl},
                       {QStringLiteral("publishedAt"), release.publishedAt},
                       {QStringLiteral("assets"), QJsonDocument::fromVariant(
                            releaseToVariantMap(release)).object().value(QStringLiteral("assets"))}
                   }).toJson(QJsonDocument::Compact));
    cache.endGroup();

    qCInfo(lcGitHub) << "Latest release loaded for" << cleanOwner << "/" << cleanRepository
                     << "tag:" << release.tagName;
    emit releaseLoaded(requestId, release);
    emit releaseDataLoaded(requestId, releaseToVariantMap(release));
}

bool GitHubManager::releaseHasAsset(const GitHubRelease &release, const QString &assetPattern)
{
    const QRegularExpression pattern(assetPattern, QRegularExpression::CaseInsensitiveOption);
    if (!pattern.isValid())
        return false;
    for (const GitHubReleaseAsset &asset : release.assets) {
        if (pattern.match(asset.name).hasMatch())
            return true;
    }
    return false;
}

bool GitHubManager::parseNewestReleaseFromList(const QByteArray &response,
                                               GitHubRelease *release,
                                               QString *errorMessage,
                                               const QString &assetPattern,
                                               bool preferFullReleases)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("GitHub returned an invalid release list.");
        return false;
    }
    // GitHub lists releases newest first.
    bool haveFallback = false;
    GitHubRelease prerelease;
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        if (object.isEmpty() || object.value(QStringLiteral("draft")).toBool())
            continue;
        GitHubRelease candidate;
        QString candidateError;
        if (!parseLatestReleaseResponse(QJsonDocument(object).toJson(QJsonDocument::Compact),
                                        &candidate, &candidateError))
            continue;
        if (!assetPattern.isEmpty() && !releaseHasAsset(candidate, assetPattern))
            continue;
        if (preferFullReleases && object.value(QStringLiteral("prerelease")).toBool()) {
            if (!haveFallback) {
                prerelease = candidate;
                haveFallback = true;
            }
            continue;
        }
        *release = candidate;
        return true;
    }
    if (haveFallback) {
        *release = prerelease;
        return true;
    }
    if (errorMessage)
        *errorMessage = assetPattern.isEmpty()
            ? QStringLiteral("This project has not published a release yet.")
            : QStringLiteral("No recent release has a file for this system.");
    return false;
}

QVariantMap GitHubManager::cachedLatestRelease(const QString &owner,
                                                            const QString &repository) const
{
    QSettings cache;
    cache.beginGroup(QStringLiteral("GitHubReleaseCache"));
    const QByteArray json = cache.value(owner.trimmed() + QLatin1Char('/')
                                        + repository.trimmed()).toByteArray();
    cache.endGroup();
    if (json.isEmpty())
        return {};

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    return parseError.error == QJsonParseError::NoError && document.isObject()
        ? document.object().toVariantMap() : QVariantMap{};
}

QVariantMap GitHubManager::releaseToVariantMap(const GitHubRelease &release)
{
    QVariantList assets;
    assets.reserve(release.assets.size());
    for (const GitHubReleaseAsset &asset : release.assets) {
        assets.append(QVariantMap{
            {QStringLiteral("name"), asset.name},
            {QStringLiteral("downloadUrl"), asset.downloadUrl},
            {QStringLiteral("contentType"), asset.contentType},
            {QStringLiteral("size"), asset.size},
            {QStringLiteral("sha256"), asset.sha256}
        });
    }

    return {
        {QStringLiteral("tagName"), release.tagName},
        {QStringLiteral("name"), release.name},
        {QStringLiteral("htmlUrl"), release.htmlUrl},
        {QStringLiteral("publishedAt"), release.publishedAt},
        {QStringLiteral("assets"), assets}
    };
}

bool GitHubManager::parseRepositoryResponse(
    const QByteArray &response,
    GitHubRepository *repository,
    QString *errorMessage)
{
    if (!repository) {
        if (errorMessage)
            *errorMessage = QStringLiteral("A repository destination is required.");
        return false;
    }

    QJsonObject root;
    if (!parseJsonObject(response, &root, errorMessage))
        return false;

    GitHubRepository parsed;
    parsed.name = root.value(QStringLiteral("name")).toString().trimmed();
    parsed.fullName = root.value(QStringLiteral("full_name")).toString().trimmed();
    parsed.owner = root.value(QStringLiteral("owner")).toObject()
                       .value(QStringLiteral("login")).toString().trimmed();
    parsed.description = root.value(QStringLiteral("description")).toString();
    parsed.htmlUrl = root.value(QStringLiteral("html_url")).toString().trimmed();
    parsed.defaultBranch =
        root.value(QStringLiteral("default_branch")).toString().trimmed();
    parsed.isPrivate = root.value(QStringLiteral("private")).toBool();

    if (parsed.name.isEmpty() || parsed.fullName.isEmpty()
        || parsed.owner.isEmpty() || !isHttpsUrl(parsed.htmlUrl)
        || parsed.defaultBranch.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "GitHub repository response is missing required repository fields.");
        }
        return false;
    }

    *repository = parsed;
    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool GitHubManager::parseLatestReleaseResponse(
    const QByteArray &response,
    GitHubRelease *release,
    QString *errorMessage)
{
    if (!release) {
        if (errorMessage)
            *errorMessage = QStringLiteral("A release destination is required.");
        return false;
    }

    QJsonObject root;
    if (!parseJsonObject(response, &root, errorMessage))
        return false;

    GitHubRelease parsed;
    parsed.tagName = root.value(QStringLiteral("tag_name")).toString().trimmed();
    parsed.name = root.value(QStringLiteral("name")).toString().trimmed();
    parsed.htmlUrl = root.value(QStringLiteral("html_url")).toString().trimmed();
    parsed.publishedAt = root.value(QStringLiteral("published_at")).toString();
    parsed.body = root.value(QStringLiteral("body")).toString().left(20000);

    if (parsed.tagName.isEmpty() || !isHttpsUrl(parsed.htmlUrl)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "GitHub release response is missing a tag or valid release URL.");
        }
        return false;
    }

    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &value : assets) {
        if (!value.isObject())
            continue;

        const QJsonObject assetObject = value.toObject();
        GitHubReleaseAsset asset;
        asset.name = assetObject.value(QStringLiteral("name")).toString().trimmed();
        asset.downloadUrl =
            assetObject.value(QStringLiteral("browser_download_url")).toString().trimmed();
        asset.contentType =
            assetObject.value(QStringLiteral("content_type")).toString().trimmed();
        asset.size = static_cast<qint64>(
            assetObject.value(QStringLiteral("size")).toDouble());
        const QString digest = assetObject.value(QStringLiteral("digest")).toString().trimmed().toLower();
        static const QRegularExpression sha256Digest(QStringLiteral("^sha256:([0-9a-f]{64})$"));
        const QRegularExpressionMatch match = sha256Digest.match(digest);
        if (match.hasMatch())
            asset.sha256 = match.captured(1);

        if (asset.name.isEmpty() || !isHttpsUrl(asset.downloadUrl))
            continue;
        parsed.assets.append(asset);
    }

    *release = parsed;
    if (errorMessage)
        errorMessage->clear();
    return true;
}