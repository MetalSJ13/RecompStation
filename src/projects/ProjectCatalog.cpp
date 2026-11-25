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

// A relative path of one or two names ("rom/game.z64"), each a valid name.
bool validRelativePath(const QString &value, int maximumParts = 2)
{
    const QStringList parts = value.split(QLatin1Char('/'));
    if (parts.isEmpty() || parts.size() > maximumParts)
        return false;
    for (const QString &part : parts) {
        if (!validFileName(part))
            return false;
    }
    return true;
}

// Launcher arguments: a flag ("--disc") or a placeholder RecompStation fills
// in with a path it chose ({romPath}, {gamePath}). Nothing else, so a catalog
// entry can't make a launcher run anything it wasn't built to.
bool parseArguments(const QJsonValue &value, const QStringList &placeholders, QStringList *arguments)
{
    static const QRegularExpression flag(QStringLiteral("^--?[a-z][a-z0-9-]{0,40}$"));
    if (!value.isUndefined() && !value.isArray())
        return false;
    for (const QJsonValue &entry : value.toArray()) {
        const QString argument = entry.toString();
        if (!placeholders.contains(argument) && !flag.match(argument).hasMatch())
            return false;
        arguments->append(argument);
    }
    return arguments->size() <= 8;
}

// Environment for the launcher: only folder settings it reads, set to the
// folder RecompStation prepared. Variables that change how programs load or
// where the system looks for things are refused.
bool parseEnvironment(const QJsonValue &value, QVariantMap *environment)
{
    static const QRegularExpression name(QStringLiteral("^[A-Z][A-Z0-9_]{1,40}$"));
    if (!value.isUndefined() && !value.isObject())
        return false;
    const QJsonObject object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
        const QString key = it.key();
        if (!name.match(key).hasMatch() || key.startsWith(QStringLiteral("LD_"))
            || key.startsWith(QStringLiteral("XDG_")) || key.startsWith(QStringLiteral("QT_"))
            || key.startsWith(QStringLiteral("SDL_"))
            || QStringList{QStringLiteral("PATH"), QStringLiteral("HOME"), QStringLiteral("SHELL"),
                           QStringLiteral("USER"), QStringLiteral("DISPLAY"),
                           QStringLiteral("WAYLAND_DISPLAY")}.contains(key)
            || it.value().toString() != QStringLiteral("{dataFolder}"))
            return false;
        environment->insert(key, it.value().toString());
    }
    return environment->size() <= 4;
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
    // discargument (disc images) and fileargument (cartridge ROMs) work the
    // same way: the game file itself is passed as an argument.
    if (method == QStringLiteral("discargument") || method == QStringLiteral("fileargument")) {
        QStringList arguments;
        if (!parseArguments(object.value(QStringLiteral("arguments")), {QStringLiteral("{gamePath}")}, &arguments)
            || !arguments.contains(QStringLiteral("{gamePath}")))
            return false;
        static const QRegularExpression extension(QStringLiteral("^[a-z0-9]{1,5}$"));
        QStringList extensions;
        for (const QJsonValue &entry : object.value(QStringLiteral("fileExtensions")).toArray()) {
            const QString value = entry.toString().toLower();
            if (!extension.match(value).hasMatch())
                return false;
            extensions.append(value);
        }
        const QString note = object.value(QStringLiteral("firstStartNote")).toString().trimmed();
        if (note.size() > 200)
            return false;
        *recipe = QVariantMap{
            {QStringLiteral("method"), method},
            {QStringLiteral("arguments"), arguments},
            {QStringLiteral("fileExtensions"), extensions},
            {QStringLiteral("firstStartNote"), note}
        };
        return true;
    }
    if (method != QStringLiteral("n64recomp") && method != QStringLiteral("harbourmasters")
        && method != QStringLiteral("romfolder"))
        return false;
    const bool romFolder = method == QStringLiteral("romfolder");

    const QString dataFolder = object.value(QStringLiteral("dataFolder")).toString();
    const QString romFileName = object.value(QStringLiteral("romFileName")).toString();
    const QString romName = object.value(QStringLiteral("romName")).toString().trimmed();
    const QString romXxh3 = object.value(QStringLiteral("romXxh3")).toString().toUpper();
    const QString romVersion = object.value(QStringLiteral("romVersion")).toString().trimmed();
    const QString graphicsFile = object.value(QStringLiteral("graphicsFile")).toString();
    static const QRegularExpression hashPattern(QStringLiteral("^[0-9A-F]{16}$"));
    // romfolder launchers may keep the ROM one folder down ("rom/game.z64"),
    // and may read the name from the ROM themselves (no romName).
    if (!validFileName(dataFolder)
        || !(romFolder ? validRelativePath(romFileName) : validFileName(romFileName))
        || !romFileName.endsWith(QStringLiteral(".z64"))
        || (romName.isEmpty() && !romFolder) || romName.size() > 20
        || (!romXxh3.isEmpty() && !hashPattern.match(romXxh3).hasMatch())
        || (!graphicsFile.isEmpty()
            && (!validFileName(graphicsFile) || !graphicsFile.endsWith(QStringLiteral(".json"))))) {
        return false;
    }
    if ((method == QStringLiteral("n64recomp") || romFolder) && romXxh3.isEmpty())
        return false;

    QStringList arguments;
    QVariantMap environment;
    const QString savesSubfolder = object.value(QStringLiteral("savesSubfolder")).toString();
    if (romFolder) {
        if (!parseArguments(object.value(QStringLiteral("arguments")), {QStringLiteral("{romPath}")}, &arguments)
            || !parseEnvironment(object.value(QStringLiteral("environment")), &environment)
            || (!savesSubfolder.isEmpty() && !validRelativePath(savesSubfolder)))
            return false;
    } else if (object.contains(QStringLiteral("arguments")) || object.contains(QStringLiteral("environment"))) {
        return false;
    }

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
    if (romFolder) {
        recipe->insert(QStringLiteral("arguments"), arguments);
        recipe->insert(QStringLiteral("environment"), environment);
        if (!savesSubfolder.isEmpty())
            recipe->insert(QStringLiteral("savesSubfolder"), savesSubfolder);
    }
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

bool ProjectCatalog::loadFromJson(const QByteArray &json)
{
    QVariantList parsed;
    QString error;
    if (!parseCatalog(json, &parsed, &error)) {
        m_errorString = error;
        return false;
    }
    m_projects = parsed;
    m_errorString.clear();
    m_loaded = true;
    return true;
}

QString ProjectCatalog::systemForKernelType(const QString &kernelType)
{
    const QString value = kernelType.trimmed().toLower();
    if (value == QStringLiteral("linux"))
        return QStringLiteral("linux");
    if (value == QStringLiteral("winnt"))
        return QStringLiteral("windows");
    return {};
}

QString ProjectCatalog::currentSystem() const
{
    // Asked of the running kernel, not decided when compiling.
    return systemForKernelType(QSysInfo::kernelType());
}

QString ProjectCatalog::currentBuild() const
{
    const QString system = currentSystem();
    const QString architecture = currentArchitecture();
    if (system.isEmpty() || architecture.isEmpty())
        return {};
    return system + QLatin1Char('-') + architecture;
}

QString ProjectCatalog::resolveBuild(const QString &build) const
{
    if (build.trimmed().isEmpty())
        return currentBuild();
    const QString value = build.trimmed().toLower();
    const int dash = value.indexOf(QLatin1Char('-'));
    if (dash < 0) {
        const QString architecture = normalizeArchitecture(value);
        const QString system = currentSystem();
        return architecture.isEmpty() || system.isEmpty()
            ? QString() : system + QLatin1Char('-') + architecture;
    }
    const QString system = value.left(dash);
    const QString architecture = normalizeArchitecture(value.mid(dash + 1));
    if ((system != QStringLiteral("linux") && system != QStringLiteral("windows"))
        || architecture.isEmpty())
        return {};
    return system + QLatin1Char('-') + architecture;
}

QStringList ProjectCatalog::availableBuilds(const QString &projectId) const
{
    QStringList keys = project(projectId).value(QStringLiteral("builds")).toMap().keys();
    keys.sort();
    return keys;
}

QVariantMap ProjectCatalog::buildRules(const QString &projectId, const QString &build) const
{
    const QString key = resolveBuild(build);
    if (key.isEmpty())
        return {};
    return project(projectId).value(QStringLiteral("builds")).toMap().value(key).toMap();
}

QString ProjectCatalog::buildLabel(const QString &build) const
{
    const QString key = resolveBuild(build);
    const int dash = key.indexOf(QLatin1Char('-'));
    if (dash < 0)
        return QStringLiteral("this system");
    const QString system = key.left(dash) == QStringLiteral("windows")
        ? QStringLiteral("Windows") : QStringLiteral("Linux");
    const QString architecture = key.mid(dash + 1);
    return architecture == QStringLiteral("x86_64")
        ? QStringLiteral("64-bit %1 (x86_64)").arg(system)
        : QStringLiteral("%1 on %2").arg(system, architecture);
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

QString ProjectCatalog::explainNoCompatibleAsset(const QString &projectId,
                                                const QVariantList &assets,
                                                const QString &build) const
{
    const QString key = resolveBuild(build);
    const bool forWindows = key.startsWith(QStringLiteral("windows-"));
    const QString system = buildLabel(key);
    const QVariantMap rules = buildRules(projectId, key);
    if (rules.isEmpty())
        return QStringLiteral("This project has no package for %1 in RecompStation's catalog.").arg(system);
    if (assets.isEmpty())
        return QStringLiteral("The latest release has no downloads attached; the project may only publish its source code.");

    // What the release offers instead, from the file names.
    bool windows = false, mac = false, linuxArm = false, linuxOther = false, flatpak = false, android = false;
    static const QRegularExpression windowsName(QStringLiteral("(win(dows)?(32|64)?[^a-z]|\\.exe$|\\.msi$|msvc)"),
                                                QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression macName(QStringLiteral("(mac|darwin|osx|\\.dmg$|apple)"),
                                            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression armName(QStringLiteral("(arm64|aarch64|armhf)"),
                                            QRegularExpression::CaseInsensitiveOption);
    for (const QVariant &entry : assets) {
        const QString name = entry.toMap().value(QStringLiteral("name")).toString();
        const QString lower = name.toLower();
        if (lower.endsWith(QStringLiteral(".flatpak")) || lower.contains(QStringLiteral("flatpak")))
            flatpak = true;
        else if (lower.endsWith(QStringLiteral(".apk")) || lower.contains(QStringLiteral("android")))
            android = true;
        // macOS first: "darwin" contains "win".
        else if (macName.match(name).hasMatch())
            mac = true;
        else if (windowsName.match(name + QLatin1Char(' ')).hasMatch())
            windows = true;
        else if (lower.contains(QStringLiteral("linux")) || lower.endsWith(QStringLiteral(".appimage")))
            (armName.match(name).hasMatch() ? linuxArm : linuxOther) = true;
    }
    if (forWindows) {
        if (windows)
            return QStringLiteral("The latest release's Windows package isn't in a form RecompStation recognises; "
                                  "the project may have renamed its files, or only offer an installer.");
        if (linuxOther || linuxArm || flatpak)
            return QStringLiteral("The latest release has no Windows package; it offers Linux only.");
        return QStringLiteral("The latest release has no package for %1.").arg(system);
    }
    if (flatpak)
        return QStringLiteral("The latest release offers Linux only as a Flatpak, which RecompStation can't install yet.");
    if (linuxArm && !linuxOther)
        return QStringLiteral("The latest release has a Linux package only for ARM computers, not for %1.").arg(system);
    if (linuxOther)
        return QStringLiteral("The latest release's Linux package isn't in a form RecompStation recognises; "
                              "the project may have renamed its files.");
    QStringList offered;
    if (windows) offered << QStringLiteral("Windows");
    if (mac) offered << QStringLiteral("macOS");
    if (android) offered << QStringLiteral("Android");
    if (!offered.isEmpty())
        return QStringLiteral("The latest release has no Linux package; it offers %1 only.")
            .arg(offered.join(QStringLiteral(" and ")));
    return QStringLiteral("The latest release has no package for %1.").arg(system);
}

QVariantList ProjectCatalog::compatibleAssets(const QString &projectId,
                                               const QVariantList &assets,
                                               const QString &build) const
{
    const QString key = resolveBuild(build);
    QVariantList compatible;
    if (key.isEmpty())
        return compatible;
    for (const QVariant &entry : assets) {
        const QVariantMap asset = entry.toMap();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (isAssetCompatible(projectId, key, name))
            compatible.append(asset);
    }
    return compatible;
}

bool ProjectCatalog::isAssetCompatible(const QString &projectId,
                                       const QString &build,
                                       const QString &assetName) const
{
    if (assetName.trimmed().isEmpty())
        return false;
    const QVariantMap rules = buildRules(projectId, build);
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
        // "builds" keyed "<system>-<architecture>"; the older "architectures"
        // (keyed by architecture alone) means Linux builds.
        const bool legacyBuilds = !object.contains(QStringLiteral("builds"));
        const QJsonObject buildObjects = legacyBuilds
            ? object.value(QStringLiteral("architectures")).toObject()
            : object.value(QStringLiteral("builds")).toObject();
        static const QRegularExpression idPattern(QStringLiteral("^[a-z0-9][a-z0-9-]*$"));
        static const QRegularExpression platformPattern(
            QStringLiteral("^[A-Za-z0-9][A-Za-z0-9 _-]*$"));
        if (!idPattern.match(id).hasMatch() || projectIds.contains(id)
            || name.isEmpty() || !platformPattern.match(platform).hasMatch()
            || !validRepositoryPart(owner) || !validRepositoryPart(repository)
            || titles.isEmpty() || buildObjects.isEmpty()
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

        QVariantMap buildRulesByKey;
        for (auto it = buildObjects.begin(); it != buildObjects.end(); ++it) {
            QString system = QStringLiteral("linux");
            QString architecture;
            if (legacyBuilds) {
                architecture = normalizedArchitecture(it.key());
            } else {
                const QString key = it.key().trimmed().toLower();
                const int dash = key.indexOf(QLatin1Char('-'));
                system = dash > 0 ? key.left(dash) : QString();
                architecture = dash > 0 ? normalizedArchitecture(key.mid(dash + 1)) : QString();
                if (system != QStringLiteral("linux") && system != QStringLiteral("windows"))
                    architecture.clear();
            }
            const QJsonObject rule = it.value().toObject();
            const QString regexText = rule.value(QStringLiteral("assetNameRegex")).toString();
            const QString packageFormat = rule.value(QStringLiteral("packageFormat")).toString();
            const QString executablePath = rule.value(QStringLiteral("executablePath")).toString();
            const QStringList executablePathParts = executablePath.split(QStringLiteral("/"), Qt::SkipEmptyParts);
            const bool unsafeExecutablePath = executablePath.startsWith(QStringLiteral("/"))
                || executablePath.contains(QLatin1Char('\\'))
                || executablePathParts.contains(QStringLiteral(".."));
            const QRegularExpression regex(regexText, QRegularExpression::CaseInsensitiveOption);
            // Windows builds: portable downloads only (a .zip, or a single
            // .exe that runs from its folder); installers are never run.
            const bool windowsBuild = system == QStringLiteral("windows");
            const bool formatAllowed = windowsBuild
                ? (packageFormat == QStringLiteral("zip") || packageFormat == QStringLiteral("executable"))
                    && executablePath.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)
                : (packageFormat == QStringLiteral("tar.gz")
                    || packageFormat == QStringLiteral("tar.xz")
                    || packageFormat == QStringLiteral("zip")
                    || packageFormat == QStringLiteral("appimage")
                    || packageFormat == QStringLiteral("executable"));
            if (architecture.isEmpty() || regexText.isEmpty() || !regex.isValid()
                || executablePath.trimmed().isEmpty() || unsafeExecutablePath || !formatAllowed) {
                return fail(QStringLiteral("A project contains an invalid build rule."));
            }

            buildRulesByKey.insert(system + QLatin1Char('-') + architecture, QVariantMap{
                {QStringLiteral("system"), system},
                {QStringLiteral("architecture"), architecture},
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
        // The short name Game Details shows ("Banjo-Tooie (USA) ROM").
        project.insert(QStringLiteral("romFile"),
                       object.value(QStringLiteral("romFile")).toString().left(120));
        project.insert(QStringLiteral("builds"), buildRulesByKey);
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
