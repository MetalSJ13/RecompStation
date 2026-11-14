#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>

class DatabaseManager;

class ArtworkManager : public QObject
{
    Q_OBJECT

public:
    explicit ArtworkManager(DatabaseManager *database,
                            const QString &artworkDirectory,
                            QObject *parent = nullptr);

    Q_INVOKABLE void downloadArtwork(int gameId, const QString &artworkUrl);

signals:
    void artworkSaved(int gameId, const QString &localPath);
    void artworkFailed(int gameId, const QString &message);

private:
    DatabaseManager *m_database = nullptr;
    QString m_artworkDirectory;
    QNetworkAccessManager m_network;
};
