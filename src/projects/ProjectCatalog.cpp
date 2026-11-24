#include "ProjectCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QResource>
#include <QSet>
#include <QStringList>
#include <QSysInfo>

namespace {
QString normalizedArchitecture(const QString &architecture)
{
    const QString value = architecture.trimmed().toLower();
    if (value == QStringLiteral("x86_64")
        || value == QStringLiteral("amd64")
        || value == QStringLiteral("x64")) {
        return QStringLiteral("x86_64");
    }
    if (value == QStringLiteral("aarch64")
        || value == QStringLiteral("arm64")) {
        return QStringLiteral("aarch64");
    }
    return {};
}

bool validRepositoryPart(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_.-]+$"));
    return pattern.match(value).hasMatch() && value != QStringLiteral(".")
        && value != QStringLiteral("..");
}

// A single file or folder name: no separators, no leading dot, no "..".
bool validFileName(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9 _.'+-]*$"));
    return value.size() <= 96 && pattern.match(value).hasMatch()
        && !value.contains(QStringLiteral(".."));
}

// The optional "launch" recipe: how RecompStation hands the player's game
// file to this launcher. Returns false when the object is present but invalid.
bool parseLaunchRecipe(const QJsonObject &object, QVariantMap *recipe)
{
    const QString method = object.value(QStringLiteral("method")).toString();
    if (method == QStringLiteral("manual")) {
        *recipe = QVariantMap{{QStringLiteral("method"), method}};
        return true;
    }
    if (method != QStringLiteral("n64recomp") && method != QStringLiteral("harbourmasters"))
        return false;

    const QString dataFolder = object.value(QStringLiteral("dataFolder")).toString();
    const QString romFileName = object.value(QStringLiteral("romFileName")).toString();
    const QString romName = object.value(QStringLiteral("romName")).toString().trimmed();
    const QString romXxh3 = object.value(QStringLiteral("romXxh3")).toString().toUpper();
    const QString romVersion = object.value(QStringLiteral("romVersion")).toString().trimmed();
    const QString graphicsFile = object.value(QStringLiteral("graphicsFile")).toString();
    static const QRegularExpression hashPattern(QStringLiteral("^[0-9A-F]{16}$"));
    if (!validFileName(dataFolder) || !validFileName(romFileName)
        || !romFileName.endsWith(QStringLiteral(".z64")) || romName.isEmpty() || romName.size() > 20
        || (!romXxh3.isEmpty() && !hashPattern.match(romXxh3).hasMatch())
        || (!graphicsFile.isEmpty()
            && (!validFileName(graphicsFile) || !graphicsFile.endsWith(QStringLiteral(".json"))))) {
        return false;
    }
    if (method == QStringLiteral("n64recomp") && romXxh3.isEmpty())
        return false;

    QStringList dataFiles;
    for (const QJsonValue &value : object.value(QStringLiteral("dataFiles")).toArray()) {
        QString name = value.toString();
        // "*" is allowed as a simple wildcard, nothing else.
        if (!validFileName(QString(name).replace(QLatin1Char('*'), QLatin1Char('x'))))
            return false;
        dataFiles.append(name);
    }

    *recipe = QVariantMap{
        {QStringLiteral("method"), method},
        {QStringLiteral("dataFolder"), dataFolder},
        {QStringLiteral("romFileName"), romFileName},
        {QStringLiteral("romName"), romName},
        {QStringLiteral("romXxh3"), romXxh3},
        {QStringLiteral("romVersion"), romVersion},
        {QStringLiteral("graphicsFile"), graphicsFile},
        {QStringLiteral("dataFiles"), dataFiles}
    };
    return true;
}
}

ProjectCatalog::ProjectCatalog(QObject *parent)
    : QObject(parent)
{
    // Explicit registration also ensures the resource object is retained by
    // linkers when this class is used from a separate test executable.
    Q_INIT_RESOURCE(projects);

    QFile file(QStringLiteral(":/data/projects.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        m_errorString = QStringLiteral("The bundled project catalog could not be opened.");
        return;
    }

    QString error;
    if (!parseCatalog(file.readAll(), &m_projects, &error)) {
        m_errorString = error;
        return;
    }
    m_loaded = true;
}

bool ProjectCatalog::isLoaded() const
{
    return m_loaded;
}

QString ProjectCatalog::errorString() const
{
    return m_errorString;
}

QString ProjectCatalog::currentArchitecture() const
{
    return normalizeArchitecture(QSysInfo::currentCpuArchitecture());
}

QVariantList ProjectCatalog::projects() const
{
    return m_projects;
}

QStringList ProjectCatalog::supportedPlatforms() const
{
    QStringList platforms;
    for (const QVariant &entry : m_projects) {
        const QString platform = entry.toMap().value(QStringLiteral("platform")).toString();
        if (!platform.isEmpty() && !platforms.contains(platform))
            platforms.append(platform);
    }
    return platforms;
}

QVariantMap ProjectCatalog::project(const QString &projectId) const
{
    for (const QVariant &entry : m_projects) {
        const QVariantMap item = entry.toMap();
        if (item.value(QStringLiteral("id")).toString() == projectId)
            return item;
    }
    return {};
}

QString ProjectCatalog::normalizeGameTitle(const QString &title)
{
    QString normalized;
    normalized.reserve(title.size());
    for (const QChar character : title) {
        if (character.isLetterOrNumber())
            normalized.append(character.toCaseFolded());
    }
    return normalized;
}

QString ProjectCatalog::normalizeArchitecture(const QString &architecture)
{
    return normalizedArchitecture(architecture);
}

QVariantList ProjectCatalog::projectsForGame(const QString &metadataTitle,
                                             const QString &platform) const
{
    QVariantList matches;
    if (!m_loaded || metadataTitle.trimmed().isEmpty() || platform.trimmed().isEmpty())
        return matches;

    const QString normalizedTitle = normalizeGameTitle(metadataTitle);
    for (const QVariant &entry : m_projects) {
        const QVariantMap item = entry.toMap();
        if (item.value(QStringLiteral("platform")).toString() != platform)
            continue;

        const QVariantList aliases = item.value(QStringLiteral("gameTitles")).toList();
        for (const QVariant &alias : aliases) {
            if (normalizeGameTitle(alias.toString()) == normalizedTitle) {
                matches.append(item);
                break;
            }
        }
    }
    return matches;
}

QVariantList ProjectCatalog::compatibleAssets(const QString &projectId,
                                               const QVariantList &assets) const
{
    QVariantList compatible;
    for (const QVariant &entry : assets) {
        const QVariantMap asset = entry.toMap();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (isAssetCompatible(projectId, currentArchitecture(), name))
            compatible.append(asset);
    }
    return compatible;
}

bool ProjectCatalog::isAssetCompatible(const QString &projectId,
                                       const QString &architecture,
                                       const QString &assetName) const
{
    const QVariantMap selectedProject = project(projectId);
    if (selectedProject.isEmpty() || assetName.trimmed().isEmpty())
        return false;

    const QString normalizedArch = normalizeArchitecture(architecture);
    if (normalizedArch.isEmpty())
        return false;

    const QVariantMap architectures =
        selectedProject.value(QStringLiteral("architectures")).toMap();
    const QVariantMap rules = architectures.value(normalizedArch).toMap();
    if (rules.isEmpty())
        return false;

    const QRegularExpression pattern(
        rules.value(QStringLiteral("assetNameRegex")).toString(),
        QRegularExpression::CaseInsensitiveOption);
    return pattern.isValid() && pattern.match(assetName).hasMatch();
}

bool ProjectCatalog::parseCatalog(const QByteArray &json,
                                  QVariantList *projects,
                                  QString *errorMessage)
{
    auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    if (!projects)
        return fail(QStringLiteral("A project catalog output destination is required."));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(QStringLiteral("The project catalog contains invalid JSON."));

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt() != 1
        || !root.value(QStringLiteral("projects")).isArray()) {
        return fail(QStringLiteral("The project catalog has an unsupported schema."));
    }

    QVariantList parsedProjects;
    QSet<QString> projectIds;
    const QJsonArray entries = root.value(QStringLiteral("projects")).toArray();
    for (const QJsonValue &value : entries) {
        if (!value.isObject())
            return fail(QStringLiteral("Every project catalog entry must be an object."));

        const QJsonObject object = value.toObject();
        const QString id = object.value(QStringLiteral("id")).toString().trimmed();
        const QString name = object.value(QStringLiteral("name")).toString().trimmed();
        const QString platform = object.value(QStringLiteral("platform")).toString().trimmed();
        const QString developmentStatus =
            object.value(QStringLiteral("developmentStatus")).toString().trimmed();
        const QString owner = object.value(QStringLiteral("repositoryOwner")).toString().trimmed();
        const QString repository = object.value(QStringLiteral("repositoryName")).toString().trimmed();
        const QJsonArray titles = object.value(QStringLiteral("gameTitles")).toArray();
        const QJsonObject architectures = object.value(QStringLiteral("architectures")).toObject();
        static const QRegularExpression idPattern(QStringLiteral("^[a-z0-9][a-z0-9-]*$"));
        static const QRegularExpression platformPattern(
            QStringLiteral("^[A-Za-z0-9][A-Za-z0-9 _-]*$"));
        if (!idPattern.match(id).hasMatch() || projectIds.contains(id)
            || name.isEmpty() || !platformPattern.match(platform).hasMatch()
            || !validRepositoryPart(owner) || !validRepositoryPart(repository)
            || titles.isEmpty() || architectures.isEmpty()
            || !QStringList{QStringLiteral("playable"), QStringLiteral("beta"),
                            QStringLiteral("alpha"), QStringLiteral("in_progress"),
                            QStringLiteral("abandoned")}.contains(developmentStatus)) {
            return fail(QStringLiteral("A project catalog entry is missing valid required fields."));
        }

        QVariantList gameTitles;
        for (const QJsonValue &titleValue : titles) {
            const QString title = titleValue.toString().trimmed();
            if (title.isEmpty())
                return fail(QStringLiteral("Project game-title aliases cannot be empty."));
            gameTitles.append(title);
        }

        QVariantMap architectureRules;
        for (auto it = architectures.begin(); it != architectures.end(); ++it) {
            const QString architecture = normalizedArchitecture(it.key());
            const QJsonObject rule = it.value().toObject();
            const QString regexText = rule.value(QStringLiteral("assetNameRegex")).toString();
            const QString packageFormat = rule.value(QStringLiteral("packageFormat")).toString();
            const QString executablePath = rule.value(QStringLiteral("executablePath")).toString();
            const QStringList executablePathParts = executablePath.split(QStringLiteral("/"), Qt::SkipEmptyParts);
            const bool unsafeExecutablePath = executablePath.startsWith(QStringLiteral("/"))
                || executablePathParts.contains(QStringLiteral(".."));
            const QRegularExpression regex(regexText, QRegularExpression::CaseInsensitiveOption);
            if (architecture.isEmpty() || regexText.isEmpty() || !regex.isValid()
                || executablePath.trimmed().isEmpty() || unsafeExecutablePath
                || (packageFormat != QStringLiteral("tar.gz")
                    && packageFormat != QStringLiteral("tar.xz")
                    && packageFormat != QStringLiteral("zip")
                    && packageFormat != QStringLiteral("appimage")
                    && packageFormat != QStringLiteral("executable"))) {
                return fail(QStringLiteral("A project contains an invalid Linux asset rule."));
            }

            architectureRules.insert(architecture, QVariantMap{
                {QStringLiteral("assetNameRegex"), regexText},
                {QStringLiteral("packageFormat"), packageFormat},
                {QStringLiteral("executablePath"), executablePath}
            });
        }

        QVariantMap launchRecipe{{QStringLiteral("method"), QStringLiteral("manual")}};
        if (object.contains(QStringLiteral("launch"))
            && (!object.value(QStringLiteral("launch")).isObject()
                || !parseLaunchRecipe(object.value(QStringLiteral("launch")).toObject(), &launchRecipe))) {
            return fail(QStringLiteral("A project contains an invalid launch recipe."));
        }

        QVariantMap project;
        project.insert(QStringLiteral("id"), id);
        project.insert(QStringLiteral("name"), name);
        project.insert(QStringLiteral("platform"), platform);
        project.insert(QStringLiteral("developmentStatus"), developmentStatus);
        project.insert(QStringLiteral("gameTitles"), gameTitles);
        project.insert(QStringLiteral("repositoryOwner"), owner);
        project.insert(QStringLiteral("repositoryName"), repository);
        project.insert(QStringLiteral("romRequirements"),
                       object.value(QStringLiteral("romRequirements")).toString());
        project.insert(QStringLiteral("architectures"), architectureRules);
        project.insert(QStringLiteral("launch"), launchRecipe);
        // Projects that publish only pre-releases.
        project.insert(QStringLiteral("includePrereleases"),
                       object.value(QStringLiteral("includePrereleases")).toBool(false));
        parsedProjects.append(project);
        projectIds.insert(id);
    }

    *projects = parsedProjects;
    if (errorMessage)
        errorMessage->clear();
    return true;
}
