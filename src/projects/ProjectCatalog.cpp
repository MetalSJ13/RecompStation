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
        const QString owner = object.value(QStringLiteral("repositoryOwner")).toString().trimmed();
        const QString repository = object.value(QStringLiteral("repositoryName")).toString().trimmed();
        const QJsonArray titles = object.value(QStringLiteral("gameTitles")).toArray();
        const QJsonObject architectures = object.value(QStringLiteral("architectures")).toObject();
        const QStringList supportedPlatforms = {QStringLiteral("N64"), QStringLiteral("PS1"),
                                                 QStringLiteral("GameCube"), QStringLiteral("Wii")};

        static const QRegularExpression idPattern(QStringLiteral("^[a-z0-9][a-z0-9-]*$"));
        if (!idPattern.match(id).hasMatch() || projectIds.contains(id)
            || name.isEmpty() || !supportedPlatforms.contains(platform)
            || !validRepositoryPart(owner) || !validRepositoryPart(repository)
            || titles.isEmpty() || architectures.isEmpty()) {
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

        QVariantMap project;
        project.insert(QStringLiteral("id"), id);
        project.insert(QStringLiteral("name"), name);
        project.insert(QStringLiteral("platform"), platform);
        project.insert(QStringLiteral("gameTitles"), gameTitles);
        project.insert(QStringLiteral("repositoryOwner"), owner);
        project.insert(QStringLiteral("repositoryName"), repository);
        project.insert(QStringLiteral("romRequirements"),
                       object.value(QStringLiteral("romRequirements")).toString());
        project.insert(QStringLiteral("architectures"), architectureRules);
        parsedProjects.append(project);
        projectIds.insert(id);
    }

    *projects = parsedProjects;
    if (errorMessage)
        errorMessage->clear();
    return true;
}
