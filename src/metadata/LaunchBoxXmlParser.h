#pragma once

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QStringList>
#include <QList>

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
};
