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
};

struct GitHubRelease
{
    QString tagName;
    QString name;
    QString htmlUrl;
    QString publishedAt;
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
    Q_INVOKABLE int fetchLatestRelease(const QString &owner,
                                       const QString &repository,
                                       bool includePrereleases = false);
    Q_INVOKABLE QVariantMap cachedLatestRelease(const QString &owner,
                                                const QString &repository) const;

    static QVariantMap releaseToVariantMap(const GitHubRelease &release);

    static bool parseRepositoryResponse(const QByteArray &response,
                                        GitHubRepository *repository,
                                        QString *errorMessage = nullptr);
    static bool parseLatestReleaseResponse(const QByteArray &response,
                                           GitHubRelease *release,
                                           QString *errorMessage = nullptr);
    // The newest non-draft release in a /releases list response.
    static bool parseNewestReleaseFromList(const QByteArray &response,
                                           GitHubRelease *release,
                                           QString *errorMessage = nullptr);

signals:
    void repositoryLoaded(int requestId, const GitHubRepository &repository);
    void releaseLoaded(int requestId, const GitHubRelease &release);
    void releaseDataLoaded(int requestId, const QVariantMap &release);
    void requestFailed(int requestId, const QString &message);

private:
    QNetworkAccessManager m_network;
    int m_nextRequestId = 1;

    void requestRelease(int requestId, const QString &owner, const QString &repository,
                        bool fromList);
    bool isValidRepositoryIdentifier(const QString &owner,
                                     const QString &repository) const;
    void setCommonHeaders(QNetworkRequest &request) const;
};