#pragma once

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

struct LaunchBoxPlatform
{
    QString name;
    QStringList alternateNames;
};

struct LaunchBoxFileAssociation
{
    QString platform;
    QString fileName;
    QString gameName;
};

struct LaunchBoxGameRecord
{
    int databaseId = 0;
    QString name;
    QString platform;
    QDate releaseDate;
    int releaseYear = 0;
    QString overview;
    QString maxPlayers;
    QString releaseType;
    QString cooperative;
    QString videoUrl;
    QString communityRating;
    QString wikipediaUrl;
    QString esrb;
    QString communityRatingCount;
    QString genres;
    QString developer;
    QString publisher;
};

class LaunchBoxXmlParser
{
public:
    static bool parsePlatforms(const QByteArray &xml,
                               QList<LaunchBoxPlatform> *platforms,
                               QString *errorMessage = nullptr);
    static bool parseFiles(const QByteArray &xml,
                           QList<LaunchBoxFileAssociation> *associations,
                           QString *errorMessage = nullptr);
    static bool parseMetadata(const QByteArray &xml,
                              QList<LaunchBoxGameRecord> *games,
                              QString *errorMessage = nullptr);

    // Streaming variants used by catalog indexing. Records are delivered one
    // at a time so large XML files never need to be materialized in memory.
    static bool parsePlatformsFile(
        const QString &path,
        const std::function<bool(const LaunchBoxPlatform &)> &consumer,
        QString *errorMessage = nullptr);
    static bool parseFilesFile(
        const QString &path,
        const std::function<bool(const LaunchBoxFileAssociation &)> &consumer,
        QString *errorMessage = nullptr);
    static bool parseMetadataFile(
        const QString &path,
        const std::function<bool(const LaunchBoxGameRecord &)> &consumer,
        QString *errorMessage = nullptr);
};
