#pragma once

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

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
    Q_INVOKABLE int fetchLatestRelease(const QString &owner,
                                       const QString &repository);

    static bool parseRepositoryResponse(const QByteArray &response,
                                        GitHubRepository *repository,
                                        QString *errorMessage = nullptr);
    static bool parseLatestReleaseResponse(const QByteArray &response,
                                           GitHubRelease *release,
                                           QString *errorMessage = nullptr);

signals:
    void repositoryLoaded(int requestId, const GitHubRepository &repository);
    void releaseLoaded(int requestId, const GitHubRelease &release);
    void requestFailed(int requestId, const QString &message);

private:
    QNetworkAccessManager m_network;
    int m_nextRequestId = 1;

    bool isValidRepositoryIdentifier(const QString &owner,
                                     const QString &repository) const;
    void setCommonHeaders(QNetworkRequest &request) const;
};