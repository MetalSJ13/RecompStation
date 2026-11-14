#pragma once

#include <QDate>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

struct GameMetadata
{
    QString provider;
    int providerGameId = 0;
    QString title;
    QString overview;
    QDate releaseDate;
    int platformId = 0;
    QString rating;
    QString artworkUrl;
    QList<int> developerIds;
    QList<int> publisherIds;
    QList<int> genreIds;
};

Q_DECLARE_METATYPE(GameMetadata)
Q_DECLARE_METATYPE(QList<GameMetadata>)
