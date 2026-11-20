#include "LaunchBoxXmlParser.h"

#include <QSet>
#include <QFile>
#include <QXmlStreamReader>
#include <atomic>

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

void parseGameImageRecord(QXmlStreamReader &reader, LaunchBoxGameImageRecord *image)
{
    while (reader.readNextStartElement()) {
        const QString field = reader.name().toString();
        const QString value = readText(reader);
        if (field == QStringLiteral("DatabaseID")) {
            bool ok = false;
            const int id = value.toInt(&ok);
            if (ok && id > 0)
                image->databaseId = id;
        } else if (field == QStringLiteral("FileName")) {
            image->fileName = value;
        } else if (field == QStringLiteral("Type")) {
            image->type = value;
        } else if (field == QStringLiteral("Region")) {
            image->region = value;
        } else if (field == QStringLiteral("CRC32")) {
            image->crc32 = value;
        }
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

bool LaunchBoxXmlParser::parseGameImages(
    const QByteArray &xml, QList<LaunchBoxGameImageRecord> *images,
    QString *errorMessage)
{
    if (!images) {
        if (errorMessage)
            *errorMessage = QStringLiteral("A game-image destination is required.");
        return false;
    }
    QXmlStreamReader reader;
    if (!beginDocument(xml, &reader, errorMessage))
        return false;

    QList<LaunchBoxGameImageRecord> parsed;
    while (reader.readNextStartElement()) {
        if (reader.name() != QStringLiteral("GameImage")) {
            reader.skipCurrentElement();
            continue;
        }
        LaunchBoxGameImageRecord image;
        parseGameImageRecord(reader, &image);
        if (image.databaseId <= 0 || image.fileName.isEmpty()
            || image.type.isEmpty())
            continue;
        parsed.append(image);
    }

    if (!finishDocument(reader, QStringLiteral("Game images"), parsed.size(),
                        errorMessage))
        return false;
    *images = parsed;
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


namespace {

template <typename Record, typename ParseRecord, typename IsUsable, typename Consumer>
bool streamCatalogFile(const QString &path, const QString &catalogName,
                       ParseRecord parseRecord, IsUsable isUsable,
                       const Consumer &consumer, QString *errorMessage,
                       const std::atomic_bool *cancelled,
                       bool requireUsableRecords = true)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not open %1 XML: %2")
                                .arg(catalogName, file.errorString());
        return false;
    }

    QXmlStreamReader reader(&file);
    if (!reader.readNextStartElement()
        || reader.name() != QStringLiteral("LaunchBox")) {
        if (errorMessage) {
            *errorMessage = reader.hasError()
                ? reader.errorString()
                : QStringLiteral("Expected a LaunchBox root element in %1 XML.")
                      .arg(catalogName);
        }
        return false;
    }

    int recordCount = 0;
    while (reader.readNextStartElement()) {
        if (cancelled && cancelled->load()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("LaunchBox catalog update cancelled.");
            return false;
        }
        if (reader.name() != Record::elementName()) {
            reader.skipCurrentElement();
            continue;
        }
        typename Record::value_type record;
        parseRecord(reader, &record);
        if (!isUsable(record))
            continue;
        if (!consumer(record)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("The %1 record consumer rejected a record.")
                                    .arg(catalogName);
            return false;
        }
        ++recordCount;
    }

    while (!reader.atEnd())
        reader.readNext();
    if (reader.hasError()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid %1 XML: %2")
                                .arg(catalogName, reader.errorString());
        return false;
    }
    if (requireUsableRecords && recordCount == 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("%1 XML contains no usable records.")
                                .arg(catalogName);
        return false;
    }
    if (errorMessage)
        errorMessage->clear();
    return true;
}

struct PlatformRecordTag {
    using value_type = LaunchBoxPlatform;
    static QString elementName() { return QStringLiteral("Platform"); }
};
struct FileRecordTag {
    using value_type = LaunchBoxFileAssociation;
    static QString elementName() { return QStringLiteral("File"); }
};
struct GameRecordTag {
    using value_type = LaunchBoxGameRecord;
    static QString elementName() { return QStringLiteral("Game"); }
};
struct GameImageRecordTag {
    using value_type = LaunchBoxGameImageRecord;
    static QString elementName() { return QStringLiteral("GameImage"); }
};

} // namespace

bool LaunchBoxXmlParser::parsePlatformsFile(
    const QString &path,
    const std::function<bool(const LaunchBoxPlatform &)> &consumer,
    QString *errorMessage, const std::atomic_bool *cancelled)
{
    return streamCatalogFile<PlatformRecordTag>(
        path, QStringLiteral("Platforms"),
        [](QXmlStreamReader &reader, LaunchBoxPlatform *record) {
            parsePlatformRecord(reader, record);
        },
        [](const LaunchBoxPlatform &record) {
            return !normalizedKey(record.name).isEmpty();
        }, consumer, errorMessage, cancelled);
}

bool LaunchBoxXmlParser::parseFilesFile(
    const QString &path,
    const std::function<bool(const LaunchBoxFileAssociation &)> &consumer,
    QString *errorMessage, const std::atomic_bool *cancelled)
{
    return streamCatalogFile<FileRecordTag>(
        path, QStringLiteral("Files"),
        [](QXmlStreamReader &reader, LaunchBoxFileAssociation *record) {
            parseFileRecord(reader, record);
        },
        [](const LaunchBoxFileAssociation &record) {
            return !record.platform.isEmpty() && !record.fileName.isEmpty()
                && !record.gameName.isEmpty();
        }, consumer, errorMessage, cancelled);
}

bool LaunchBoxXmlParser::parseMetadataFile(
    const QString &path,
    const std::function<bool(const LaunchBoxGameRecord &)> &consumer,
    QString *errorMessage, const std::atomic_bool *cancelled)
{
    return streamCatalogFile<GameRecordTag>(
        path, QStringLiteral("Metadata"),
        [](QXmlStreamReader &reader, LaunchBoxGameRecord *record) {
            parseGameRecord(reader, record);
        },
        [](const LaunchBoxGameRecord &record) {
            return record.databaseId > 0 && !record.name.isEmpty()
                && !record.platform.isEmpty();
        }, consumer, errorMessage, cancelled);
}

bool LaunchBoxXmlParser::parseGameImagesFile(
    const QString &path,
    const std::function<bool(const LaunchBoxGameImageRecord &)> &consumer,
    QString *errorMessage, const std::atomic_bool *cancelled)
{
    return streamCatalogFile<GameImageRecordTag>(
        path, QStringLiteral("Game images"),
        [](QXmlStreamReader &reader, LaunchBoxGameImageRecord *record) {
            parseGameImageRecord(reader, record);
        },
        [](const LaunchBoxGameImageRecord &record) {
            return record.databaseId > 0 && !record.fileName.isEmpty()
                && !record.type.isEmpty();
        }, consumer, errorMessage, cancelled, false);
}
