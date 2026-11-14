#include "TheGamesDbClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace {
QList<int> integerList(const QJsonValue &value)
{
    QList<int> values;
    if (!value.isArray())
        return values;

    for (const QJsonValue &entry : value.toArray()) {
        if (entry.isDouble())
            values.append(entry.toInt());
    }
    return values;
}

QList<QJsonObject> gameObjects(const QJsonValue &value)
{
    QList<QJsonObject> objects;
    if (value.isArray()) {
        for (const QJsonValue &entry : value.toArray()) {
            if (entry.isObject())
                objects.append(entry.toObject());
        }
    } else if (value.isObject()) {
        const QJsonObject keyedGames = value.toObject();
        for (auto it = keyedGames.begin(); it != keyedGames.end(); ++it) {
            if (it.value().isObject())
                objects.append(it.value().toObject());
        }
    }
    return objects;
}
}

TheGamesDbClient::TheGamesDbClient(const QString &apiKey, QObject *parent)
    : MetadataProvider(parent),
      m_apiKey(apiKey.trimmed())
{
    qRegisterMetaType<QList<GameMetadata>>();
}

QString TheGamesDbClient::providerId() const
{
    return QStringLiteral("thegamesdb");
}

bool TheGamesDbClient::isConfigured() const
{
    return !m_apiKey.isEmpty();
}

void TheGamesDbClient::searchGames(const QString &searchTerm, int platformId)
{
    const int requestId = m_nextRequestId++;
    const QString term = searchTerm.trimmed();

    if (!isConfigured()) {
        emit requestFailed(requestId,
                           QStringLiteral("TheGamesDB API key is not configured. "
                                          "Set RECOMPSTATION_TGDB_API_KEY."));
        return;
    }

    if (term.isEmpty()) {
        emit requestFailed(requestId, QStringLiteral("Enter a game title to search."));
        return;
    }

    QUrl url(QStringLiteral("https://api.thegamesdb.net/v1.1/Games/ByGameName"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("apikey"), m_apiKey);
    query.addQueryItem(QStringLiteral("name"), term);
    query.addQueryItem(QStringLiteral("fields"),
                       QStringLiteral("overview,publishers,genres,platform"));
    query.addQueryItem(QStringLiteral("include"),
                       QStringLiteral("boxart,platform"));

    if (platformId > 0)
        query.addQueryItem(QStringLiteral("filter[platform]"),
                           QString::number(platformId));

    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::KnownHeaders::AcceptHeader,
                      QStringLiteral("application/json"));
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, requestId]() {
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError) {
            const QString message = statusCode == 403
                ? QStringLiteral("TheGamesDB rejected the API key or the request "
                                 "has reached its rate limit (HTTP 403).")
                : QStringLiteral("TheGamesDB request failed (HTTP %1): %2")
                      .arg(statusCode)
                      .arg(reply->errorString());
            reply->deleteLater();
            emit requestFailed(requestId, message);
            return;
        }

        QList<GameMetadata> results;
        QString parseError;
        if (!parseSearchResponse(reply->readAll(), &results, &parseError)) {
            reply->deleteLater();
            emit requestFailed(requestId, parseError);
            return;
        }

        reply->deleteLater();
        emit searchCompleted(requestId, results);
    });
}

bool TheGamesDbClient::parseSearchResponse(const QByteArray &response,
                                           QList<GameMetadata> *results,
                                           QString *errorMessage)
{
    if (!results) {
        if (errorMessage)
            *errorMessage = QStringLiteral("A results destination is required.");
        return false;
    }

    QJsonParseError jsonError;
    const QJsonDocument document =
        QJsonDocument::fromJson(response, &jsonError);

    if (jsonError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid TheGamesDB JSON response: %1")
                                .arg(jsonError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QJsonValue gamesValue = data.value(QStringLiteral("games"));
    if (!gamesValue.isArray() && !gamesValue.isObject()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("TheGamesDB response is missing data.games.");
        return false;
    }

    QList<GameMetadata> parsedResults;
    const QList<QJsonObject> games = gameObjects(gamesValue);
    parsedResults.reserve(games.size());

    for (const QJsonObject &game : games) {
        GameMetadata metadata;
        metadata.provider = QStringLiteral("thegamesdb");
        metadata.providerGameId = game.value(QStringLiteral("id")).toInt();
        metadata.title = game.value(QStringLiteral("game_title")).toString().trimmed();
        metadata.overview = game.value(QStringLiteral("overview")).toString();
        metadata.rating = game.value(QStringLiteral("rating")).toString();
        metadata.platformId = game.value(QStringLiteral("platform")).toInt();
        metadata.developerIds =
            integerList(game.value(QStringLiteral("developers")));
        metadata.publisherIds =
            integerList(game.value(QStringLiteral("publishers")));
        metadata.genreIds =
            integerList(game.value(QStringLiteral("genres")));

        const QString releaseDate =
            game.value(QStringLiteral("release_date")).toString();
        if (!releaseDate.isEmpty())
            metadata.releaseDate = QDate::fromString(releaseDate, Qt::ISODate);

        if (!metadata.title.isEmpty())
            parsedResults.append(metadata);
    }

    *results = parsedResults;
    if (errorMessage)
        errorMessage->clear();
    return true;
}
