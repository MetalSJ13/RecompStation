#include "TheGamesDbClient.h"
#include "../core/Logging.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include "MetadataPlatformMapping.h"
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

QString frontBoxartUrl(const QJsonObject &root, int gameId, int *pixelCount)
{
    const QJsonObject boxart =
        root.value(QStringLiteral("include")).toObject()
            .value(QStringLiteral("boxart")).toObject();
    const QJsonObject baseUrls =
        boxart.value(QStringLiteral("base_url")).toObject();
    QString baseUrl;
    for (const QString &size : {QStringLiteral("original"), QStringLiteral("large"),
                                QStringLiteral("medium"), QStringLiteral("thumb")}) {
        baseUrl = baseUrls.value(size).toString();
        if (!baseUrl.isEmpty())
            break;
    }
    if (baseUrl.isEmpty())
        return {};

    const QJsonArray images =
        boxart.value(QStringLiteral("data")).toObject()
            .value(QString::number(gameId)).toArray();
    QString frontFilename;
    QString fallbackFilename;
    QString frontResolution;
    QString fallbackResolution;
    for (const QJsonValue &entry : images) {
        if (!entry.isObject())
            continue;
        const QJsonObject image = entry.toObject();
        const QString filename =
            image.value(QStringLiteral("filename")).toString().trimmed();
        if (filename.isEmpty())
            continue;
        if (fallbackFilename.isEmpty()) {
            fallbackFilename = filename;
            fallbackResolution = image.value(QStringLiteral("resolution")).toString();
        }
        if (image.value(QStringLiteral("side")).toString().compare(
                QStringLiteral("front"), Qt::CaseInsensitive) == 0) {
            frontFilename = filename;
            frontResolution = image.value(QStringLiteral("resolution")).toString();
            break;
        }
    }

    const bool hasFront = !frontFilename.isEmpty();
    const QString filename = hasFront ? frontFilename : fallbackFilename;
    const QString resolution = hasFront ? frontResolution : fallbackResolution;
    if (pixelCount) {
        *pixelCount = 0;
        const QStringList dimensions = resolution.split(QLatin1Char('x'));
        if (dimensions.size() == 2) {
            bool widthOk = false;
            bool heightOk = false;
            const int width = dimensions.at(0).toInt(&widthOk);
            const int height = dimensions.at(1).toInt(&heightOk);
            if (widthOk && heightOk && width > 0 && height > 0)
                *pixelCount = width * height;
        }
    }
    if (filename.isEmpty())
        return {};
    if (!baseUrl.endsWith(QLatin1Char('/')))
        baseUrl.append(QLatin1Char('/'));
    const QUrl resolved = QUrl(baseUrl).resolved(QUrl(filename));
    return resolved.scheme() == QStringLiteral("https") ? resolved.toString() : QString();
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

void TheGamesDbClient::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey.trimmed();
}

int TheGamesDbClient::remainingAllowance(const QByteArray &response)
{
    const QJsonDocument document = QJsonDocument::fromJson(response);
    if (!document.isObject())
        return -1;
    const QJsonObject root = document.object();
    if (!root.contains(QStringLiteral("remaining_monthly_allowance")))
        return -1;
    return root.value(QStringLiteral("remaining_monthly_allowance")).toInt()
        + root.value(QStringLiteral("extra_allowance")).toInt();
}

void TheGamesDbClient::fetchPlatforms()
{
    if (!isConfigured()) {
        emit platformsFailed(QStringLiteral("No TheGamesDB API key. Add one in Settings > Metadata."));
        return;
    }

    QUrl url(QStringLiteral("https://api.thegamesdb.net/v1/Platforms"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("apikey"), m_apiKey);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            const QString message = QStringLiteral("TheGamesDB platform request failed (HTTP %1): %2")
                                        .arg(statusCode).arg(reply->errorString());
            reply->deleteLater();
            emit platformsFailed(message);
            return;
        }

        QList<TheGamesDbPlatform> platforms;
        QString error;
        const QByteArray body = reply->readAll();
        const int allowance = remainingAllowance(body);
        if (allowance >= 0)
            emit allowanceReported(allowance);
        const bool ok = parsePlatformResponse(body, &platforms, &error);
        reply->deleteLater();
        if (!ok) {
            emit platformsFailed(error);
            return;
        }
        emit platformsLoaded(platforms);
    });
}

bool TheGamesDbClient::parsePlatformResponse(
    const QByteArray &response,
    QList<TheGamesDbPlatform> *platforms,
    QString *errorMessage)
{
    return MetadataPlatformMapping::parsePlatformCatalog(
        response, platforms, errorMessage);
}

void TheGamesDbClient::searchGames(const QString &searchTerm, int platformId)
{
    const int requestId = m_nextRequestId++;
    const QString term = searchTerm.trimmed();
    qCInfo(lcMetadata) << "TheGamesDB search started. Term:" << term
                       << "platform ID:" << platformId;

    if (!isConfigured()) {
        emit requestFailed(requestId,
                           QStringLiteral("No TheGamesDB API key. Add one in Settings > Metadata."));
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
                       QStringLiteral("overview,publishers,genres,platform,developers,rating"));
    query.addQueryItem(QStringLiteral("include"),
                       QStringLiteral("boxart,platform"));

    if (platformId > 0)
        query.addQueryItem(QStringLiteral("filter[platform]"),
                           QString::number(platformId));

    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
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
            qCWarning(lcMetadata) << "TheGamesDB search failed:" << message;
            emit requestFailed(requestId, message);
            return;
        }

        QList<GameMetadata> results;
        QString parseError;
        const QByteArray body = reply->readAll();
        const int allowance = remainingAllowance(body);
        if (allowance >= 0)
            emit allowanceReported(allowance);
        if (!parseSearchResponse(body, &results, &parseError)) {
            reply->deleteLater();
            qCWarning(lcMetadata) << "TheGamesDB response could not be parsed:" << parseError;
            emit requestFailed(requestId, parseError);
            return;
        }

        reply->deleteLater();
        qCInfo(lcMetadata) << "TheGamesDB search completed. Results:" << results.size();
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
        metadata.artworkUrl = frontBoxartUrl(
            root, metadata.providerGameId, &metadata.artworkPixelCount);

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
