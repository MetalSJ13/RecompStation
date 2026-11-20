#include "LaunchBoxXmlParser.h"

#include <QSet>
#include <QXmlStreamReader>

namespace {

QString readText(QXmlStreamReader &reader)
{
    return reader.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
}

QString normalizedKey(const QString &value)
{
    QString key;
    key.reserve(value.size());
    for (const QChar character : value) {
        if (character.isLetterOrNumber())
            key.append(character.toCaseFolded());
    }
    return key;
}

bool beginDocument(const QByteArray &xml, QXmlStreamReader *reader,
                   QString *errorMessage)
{
    if (!reader) {
        if (errorMessage)
            *errorMessage = QStringLiteral("An XML reader is required.");
        return false;
    }
    reader->addData(xml);
    if (!reader->readNextStartElement()
        || reader->name() != QStringLiteral("LaunchBox")) {
        if (errorMessage) {
            *errorMessage = reader->hasError()
                ? reader->errorString()
                : QStringLiteral("Expected a LaunchBox root element.");
        }
        return false;
    }
    return true;
}

bool finishDocument(QXmlStreamReader &reader, const QString &catalogName,
                    int recordCount, QString *errorMessage)
{
    while (!reader.atEnd())
        reader.readNext();
    if (reader.hasError()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid %1 XML: %2")
                                .arg(catalogName, reader.errorString());
        return false;
    }
    if (recordCount == 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("%1 XML contains no usable records.")
                                .arg(catalogName);
        return false;
    }
    if (errorMessage)
        errorMessage->clear();
    return true;
}

void appendUnique(QStringList *values, const QString &value)
{
    if (!values || value.isEmpty())
        return;
    const QString key = normalizedKey(value);
    for (const QString &existing : *values) {
        if (normalizedKey(existing) == key)
            return;
    }
    values->append(value);
}

void parsePlatformRecord(QXmlStreamReader &reader, LaunchBoxPlatform *platform)
{
    while (reader.readNextStartElement()) {
        const QString field = reader.name().toString();
        const QString value = readText(reader);
        if (field == QStringLiteral("Name")) {
            platform->name = value;
        } else if (field == QStringLiteral("Alternate")
                   || field == QStringLiteral("PlatformAlternateName")) {
            appendUnique(&platform->alternateNames, value);
        }
    }
}

void parseFileRecord(QXmlStreamReader &reader, LaunchBoxFileAssociation *file)
{
    while (reader.readNextStartElement()) {
        const QString field = reader.name().toString();
        const QString value = readText(reader);
        if (field == QStringLiteral("Platform"))
            file->platform = value;
        else if (field == QStringLiteral("FileName"))
            file->fileName = value;
        else if (field == QStringLiteral("GameName"))
            file->gameName = value;
    }
}

void parseGameRecord(QXmlStreamReader &reader, LaunchBoxGameRecord *game)
{
    while (reader.readNextStartElement()) {
        const QString field = reader.name().toString();
        const QString value = readText(reader);
        if (field == QStringLiteral("DatabaseID")) {
            bool ok = false;
            const int id = value.toInt(&ok);
            if (ok && id > 0)
                game->databaseId = id;
        } else if (field == QStringLiteral("Name")) {
            game->name = value;
        } else if (field == QStringLiteral("Platform")) {
            game->platform = value;
        } else if (field == QStringLiteral("ReleaseDate")) {
            game->releaseDate = QDate::fromString(value, Qt::ISODate);
        } else if (field == QStringLiteral("ReleaseYear")) {
            bool ok = false;
            const int year = value.toInt(&ok);
            if (ok && year > 0)
                game->releaseYear = year;
        } else if (field == QStringLiteral("Overview")) {
            game->overview = value;
        } else if (field == QStringLiteral("MaxPlayers")) {
            game->maxPlayers = value;
        } else if (field == QStringLiteral("ReleaseType")) {
            game->releaseType = value;
        } else if (field == QStringLiteral("Cooperative")) {
            game->cooperative = value;
        } else if (field == QStringLiteral("VideoURL")) {
            game->videoUrl = value;
        } else if (field == QStringLiteral("CommunityRating")) {
            game->communityRating = value;
        } else if (field == QStringLiteral("WikipediaURL")) {
            game->wikipediaUrl = value;
        } else if (field == QStringLiteral("ESRB")) {
            game->esrb = value;
        } else if (field == QStringLiteral("CommunityRatingCount")) {
            game->communityRatingCount = value;
        } else if (field == QStringLiteral("Genres")) {
            game->genres = value;
        } else if (field == QStringLiteral("Developer")) {
            game->developer = value;
        } else if (field == QStringLiteral("Publisher")) {
            game->publisher = value;
        }
    }
}

} // namespace

bool LaunchBoxXmlParser::parsePlatforms(const QByteArray &xml,
                                        QList<LaunchBoxPlatform> *platforms,
                                        QString *errorMessage)
{
    if (!platforms) {
        if (errorMessage)
            *errorMessage = QStringLiteral("A platform destination is required.");
        return false;
    }
    QXmlStreamReader reader;
    if (!beginDocument(xml, &reader, errorMessage))
        return false;

    QList<LaunchBoxPlatform> parsed;
    QSet<QString> seenNames;
    while (reader.readNextStartElement()) {
        if (reader.name() != QStringLiteral("Platform")) {
            reader.skipCurrentElement();
            continue;
        }
        LaunchBoxPlatform platform;
        parsePlatformRecord(reader, &platform);
        const QString key = normalizedKey(platform.name);
        if (key.isEmpty() || seenNames.contains(key))
            continue;
        seenNames.insert(key);
        parsed.append(platform);
    }

    if (!finishDocument(reader, QStringLiteral("Platforms"), parsed.size(),
                        errorMessage))
        return false;
    *platforms = parsed;
    return true;
}

bool LaunchBoxXmlParser::parseFiles(
    const QByteArray &xml,
    QList<LaunchBoxFileAssociation> *associations,
    QString *errorMessage)
{
    if (!associations) {
        if (errorMessage)
            *errorMessage = QStringLiteral("A file-association destination is required.");
        return false;
    }
    QXmlStreamReader reader;
    if (!beginDocument(xml, &reader, errorMessage))
        return false;

    QList<LaunchBoxFileAssociation> parsed;
    QSet<QString> exactRecords;
    while (reader.readNextStartElement()) {
        if (reader.name() != QStringLiteral("File")) {
            reader.skipCurrentElement();
            continue;
        }
        LaunchBoxFileAssociation file;
        parseFileRecord(reader, &file);
        if (file.platform.isEmpty() || file.fileName.isEmpty()
            || file.gameName.isEmpty()) {
            continue;
        }
        const QString key = normalizedKey(file.platform) + QLatin1Char('|')
            + normalizedKey(file.fileName) + QLatin1Char('|')
            + normalizedKey(file.gameName);
        if (exactRecords.contains(key))
            continue;
        exactRecords.insert(key);
        parsed.append(file);
    }

    if (!finishDocument(reader, QStringLiteral("Files"), parsed.size(),
                        errorMessage))
        return false;
    *associations = parsed;
    return true;
}

bool LaunchBoxXmlParser::parseMetadata(const QByteArray &xml,
                                       QList<LaunchBoxGameRecord> *games,
                                       QString *errorMessage)
{
    if (!games) {
        if (errorMessage)
            *errorMessage = QStringLiteral("A game-metadata destination is required.");
        return false;
    }
    QXmlStreamReader reader;
    if (!beginDocument(xml, &reader, errorMessage))
        return false;

    QList<LaunchBoxGameRecord> parsed;
    QSet<int> seenIds;
    while (reader.readNextStartElement()) {
        if (reader.name() != QStringLiteral("Game")) {
            reader.skipCurrentElement();
            continue;
        }
        LaunchBoxGameRecord game;
        parseGameRecord(reader, &game);
        if (game.databaseId <= 0 || game.name.isEmpty()
            || game.platform.isEmpty() || seenIds.contains(game.databaseId)) {
            continue;
        }
        seenIds.insert(game.databaseId);
        parsed.append(game);
    }

    if (!finishDocument(reader, QStringLiteral("Metadata"), parsed.size(),
                        errorMessage))
        return false;
    *games = parsed;
    return true;
}
