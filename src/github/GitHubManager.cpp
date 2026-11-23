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
                                      const QString &repository)
{
    const int requestId = m_nextRequestId++;
    const QString cleanOwner = owner.trimmed();
    const QString cleanRepository = repository.trimmed();
    qCDebug(lcGitHub) << "Latest release request:" << cleanOwner << "/" << cleanRepository;

    if (!isValidRepositoryIdentifier(cleanOwner, cleanRepository)) {
        emit requestFailed(requestId,
                           QStringLiteral("A valid GitHub owner and repository are required."));
        return requestId;
    }

    const QUrl url(QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
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
                QStringLiteral("latest release for %1/%2")
                    .arg(cleanOwner, cleanRepository));
            reply->deleteLater();
            qCWarning(lcGitHub) << "Release request failed:" << message;
            emit requestFailed(requestId, message);
            return;
        }

        GitHubRelease release;
        QString error;
        const bool ok = parseLatestReleaseResponse(response, &release, &error);
        reply->deleteLater();
        if (!ok) {
            emit requestFailed(requestId, error);
            return;
        }
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
    });

    return requestId;
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
            {QStringLiteral("size"), asset.size}
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

        if (asset.name.isEmpty() || !isHttpsUrl(asset.downloadUrl))
            continue;
        parsed.assets.append(asset);
    }

    *release = parsed;
    if (errorMessage)
        errorMessage->clear();
    return true;
}