#pragma once

#include <QDate>
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
    QList<int> developerIds;
    QList<int> publisherIds;
    QList<int> genreIds;
};

Q_DECLARE_METATYPE(GameMetadata)
Q_DECLARE_METATYPE(QList<GameMetadata>)
