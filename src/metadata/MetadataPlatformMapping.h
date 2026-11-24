#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>
#include <QByteArray>

struct TheGamesDbPlatform
{
    int id = 0;
    QString name;
    QString alias;
};

class MetadataPlatformMapping
{
public:
    static bool parsePlatformCatalog(const QByteArray &response,
                                     QList<TheGamesDbPlatform> *platforms,
                                     QString *errorMessage = nullptr)
    {
        if (!platforms) {
            if (errorMessage)
                *errorMessage = QStringLiteral("A platform destination is required.");
            return false;
        }

        QJsonParseError jsonError;
        const QJsonDocument document =
            QJsonDocument::fromJson(response, &jsonError);
        if (jsonError.error != QJsonParseError::NoError || !document.isObject()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Invalid TheGamesDB platform JSON: %1")
                                    .arg(jsonError.errorString());
            return false;
        }

        const QJsonObject data =
            document.object().value(QStringLiteral("data")).toObject();
        const QJsonValue value = data.value(QStringLiteral("platforms"));
        if (!value.isObject()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("TheGamesDB response is missing data.platforms.");
            return false;
        }

        QList<TheGamesDbPlatform> parsed;
        const QJsonObject entries = value.toObject();
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (!it.value().isObject())
                continue;

            const QJsonObject object = it.value().toObject();
            TheGamesDbPlatform platform;
            platform.id = object.value(QStringLiteral("id")).toInt(it.key().toInt());
            platform.name = object.value(QStringLiteral("name")).toString().trimmed();
            platform.alias = object.value(QStringLiteral("alias")).toString().trimmed();
            if (platform.id > 0 && !platform.name.isEmpty())
                parsed.append(platform);
        }

        if (parsed.isEmpty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("TheGamesDB returned an empty platform catalog.");
            return false;
        }

        *platforms = parsed;
        if (errorMessage)
            errorMessage->clear();
        return true;
    }

    static int platformIdForInternalName(
        const QString &internalName,
        const QList<TheGamesDbPlatform> &platforms)
    {
        QStringList accepted;
        const QString key = internalName.trimmed().toLower();
        if (key == QStringLiteral("n64"))
            accepted = {QStringLiteral("nintendo64"), QStringLiteral("n64")};
        else if (key == QStringLiteral("ps1"))
            accepted = {QStringLiteral("playstation"), QStringLiteral("sonyplaystation"),
                        QStringLiteral("playstation1"), QStringLiteral("ps1")};
        else if (key == QStringLiteral("gamecube"))
            accepted = {QStringLiteral("gamecube"), QStringLiteral("nintendogamecube")};
        else if (key == QStringLiteral("wii"))
            accepted = {QStringLiteral("wii"), QStringLiteral("nintendowii")};
        else if (key == QStringLiteral("xbox"))
            accepted = {QStringLiteral("xbox"), QStringLiteral("microsoftxbox"),
                        QStringLiteral("originalxbox")};
        else if (key == QStringLiteral("xbox 360"))
            accepted = {QStringLiteral("xbox360"), QStringLiteral("microsoftxbox360")};
        else
            return 0;

        for (const TheGamesDbPlatform &platform : platforms) {
            const QString name = normalized(platform.name);
            const QString alias = normalized(platform.alias);
            if (accepted.contains(name) || accepted.contains(alias))
                return platform.id;
        }
        return 0;
    }

private:
    static QString normalized(const QString &value)
    {
        QString result;
        result.reserve(value.size());
        for (const QChar character : value) {
            if (character.isLetterOrNumber())
                result.append(character.toLower());
        }
        return result;
    }
};
