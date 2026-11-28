#pragma once

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QVariantMap>

class QNetworkRequest;

struct GitHubRepository
{
    QString name;
    QString fullName;
    QString owner;
    QString description;
    QString htmlUrl;
    QString defaultBranch;
    bool isPrivate = false;
};

struct GitHubReleaseAsset
{
    QString name;
    QString downloadUrl;
    QString contentType;
    qint64 size = 0;
    // Lowercase hex SHA-256 from the asset's "digest" ("sha256:…"), which
    // GitHub publishes for release assets; empty when absent.
    QString sha256;
};

struct GitHubRelease
{
    QString tagName;
    QString name;
    QString htmlUrl;
    QString publishedAt;
    // The release description as written on GitHub (Markdown), capped.
    QString body;
    QList<GitHubReleaseAsset> assets;
};

class GitHubManager : public QObject
{
    Q_OBJECT

public:
    explicit GitHubManager(QObject *parent = nullptr);

    Q_INVOKABLE int fetchRepository(const QString &owner,
                                    const QString &repository);
    // The newest published release. With includePrereleases, pre-releases
    // count too (projects that only publish pre-releases). A repository
    // without any full release falls back to its newest pre-release.
    // With assetPattern (the build's assetNameRegex), a newest release that
    // has no file for this system is passed over for the newest one that
    // does: some projects publish Linux and Windows in separate releases.
    Q_INVOKABLE int fetchLatestRelease(const QString &owner,
                                       const QString &repository,
                                       bool includePrereleases = false,
                                       const QString &assetPattern = QString());
    Q_INVOKABLE QVariantMap cachedLatestRelease(const QString &owner,
                                                const QString &repository) const;

    static QVariantMap releaseToVariantMap(const GitHubRelease &release);

    static bool parseRepositoryResponse(const QByteArray &response,
                                        GitHubRepository *repository,
                                        QString *errorMessage = nullptr);
    static bool parseLatestReleaseResponse(const QByteArray &response,
                                           GitHubRelease *release,
                                           QString *errorMessage = nullptr);
    // The newest non-draft release in a /releases list response. With
    // assetPattern, the newest one with a file whose name matches it;
    // with preferFullReleases, a matching full release comes before a
    // newer matching pre-release.
    static bool parseNewestReleaseFromList(const QByteArray &response,
                                           GitHubRelease *release,
                                           QString *errorMessage = nullptr,
                                           const QString &assetPattern = QString(),
                                           bool preferFullReleases = false);
    static bool releaseHasAsset(const GitHubRelease &release, const QString &assetPattern);

signals:
    void repositoryLoaded(int requestId, const GitHubRepository &repository);
    void releaseLoaded(int requestId, const GitHubRelease &release);
    void releaseDataLoaded(int requestId, const QVariantMap &release);
    void requestFailed(int requestId, const QString &message);

private:
    QNetworkAccessManager m_network;
    int m_nextRequestId = 1;

    // stage: "latest" (/releases/latest), "list" (newest in the list) or
    // "match" (the newest release in the list with a file for assetPattern).
    void requestRelease(int requestId, const QString &owner, const QString &repository,
                        const QString &stage, bool includePrereleases,
                        const QString &assetPattern,
                        const GitHubRelease &fallback = GitHubRelease());
    void finishRelease(int requestId, const QString &owner, const QString &repository,
                       const GitHubRelease &release);
    bool isValidRepositoryIdentifier(const QString &owner,
                                     const QString &repository) const;
    void setCommonHeaders(QNetworkRequest &request) const;
};