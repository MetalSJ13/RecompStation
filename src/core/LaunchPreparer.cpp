#include "LaunchPreparer.h"

#include "Logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {
QString homeDirectory(const LaunchPreparer::Options &options)
{
    return options.homeDirectory.isEmpty() ? QDir::homePath() : options.homeDirectory;
}

QString dataHome(const LaunchPreparer::Options &options)
{
    if (!options.dataHome.isEmpty())
        return options.dataHome;
    const QString xdg = qEnvironmentVariable("XDG_DATA_HOME");
    if (!xdg.isEmpty() && QDir::isAbsolutePath(xdg))
        return xdg;
    return QDir(homeDirectory(options)).filePath(QStringLiteral(".local/share"));
}

QString shortPath(const QString &path, const LaunchPreparer::Options &options)
{
    const QString home = homeDirectory(options);
    if (path == home)
        return QStringLiteral("~");
    if (path.startsWith(home + QLatin1Char('/')))
        return QStringLiteral("~") + path.mid(home.size());
    return path;
}

QVariantMap step(const QString &title, const QString &detail = {}, bool warning = false)
{
    return QVariantMap{{QStringLiteral("title"), title}, {QStringLiteral("detail"), detail},
                       {QStringLiteral("warning"), warning}};
}

bool folderHasArchive(const QString &folder)
{
    return !QDir(folder).entryList({QStringLiteral("*.o2r")}, QDir::Files).isEmpty();
}

bool writeFileOnce(const QString &path, const QByteArray &data, QString *error)
{
    if (QFileInfo::exists(path)) {
        *error = QStringLiteral("A file is already there.");
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = file.errorString();
        return false;
    }
    if (file.write(data) != data.size() || !file.commit()) {
        *error = file.errorString();
        return false;
    }
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    return true;
}

// Copies a file or folder without following links, and without replacing
// anything at the destination. Returns how many files were copied.
int copyWithoutReplacing(const QString &source, const QString &destination)
{
    const QFileInfo info(source);
    if (info.isSymLink())
        return 0;
    if (info.isFile()) {
        if (QFileInfo::exists(destination))
            return 0;
        return QFile::copy(source, destination) ? 1 : 0;
    }
    if (!info.isDir() || !QDir().mkpath(destination))
        return 0;
    int copied = 0;
    const QFileInfoList entries = QDir(source).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &entry : entries)
        copied += copyWithoutReplacing(entry.filePath(), QDir(destination).filePath(entry.fileName()));
    return copied;
}

// Fills {dataFolder}, {romPath} and {gamePath} in recipe strings.
QString fillIn(QString text, const QHash<QString, QString> &values)
{
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        text.replace(QLatin1Char('{') + it.key() + QLatin1Char('}'), it.value());
    return text;
}

QStringList filledArguments(const QVariantMap &recipe, const QHash<QString, QString> &values)
{
    QStringList arguments;
    for (const QVariant &argument : recipe.value(QStringLiteral("arguments")).toList())
        arguments << fillIn(argument.toString(), values);
    return arguments;
}

bool usesRomChecksum(const QString &method)
{
    return method == QStringLiteral("n64recomp") || method == QStringLiteral("romfolder");
}

QString gameName(const QVariantMap &project)
{
    const QVariantList titles = project.value(QStringLiteral("gameTitles")).toList();
    return titles.isEmpty() ? project.value(QStringLiteral("name")).toString()
                            : titles.first().toString();
}
}

QString LaunchPreparer::dataFolder(const QVariantMap &recipe,
                                   const QString &installDirectory,
                                   const Options &options)
{
    const QString method = recipe.value(QStringLiteral("method")).toString();
    const QString folder = recipe.value(QStringLiteral("dataFolder")).toString();
    if (method == QStringLiteral("n64recomp")) {
        // A launcher in portable mode keeps everything next to its executable
        // (it checks its working directory, which is the install folder).
        if (!installDirectory.isEmpty()
            && QFileInfo::exists(QDir(installDirectory).filePath(QStringLiteral("portable.txt"))))
            return installDirectory;
        return QDir(homeDirectory(options)).filePath(QStringLiteral(".config/") + folder);
    }
    if (method == QStringLiteral("romfolder"))
        return QDir(dataHome(options)).filePath(folder);
    if (method == QStringLiteral("harbourmasters")) {
        const QString shipHome = qEnvironmentVariable("SHIP_HOME");
        if (!shipHome.isEmpty() && QDir::isAbsolutePath(shipHome))
            return shipHome;
        return QDir(dataHome(options)).filePath(folder);
    }
    return {};
}

QString LaunchPreparer::savesFolder(const QVariantMap &recipe,
                                    const QString &installDirectory,
                                    const Options &options)
{
    const QString folder = dataFolder(recipe, installDirectory, options);
    if (folder.isEmpty())
        return {};
    const QString method = recipe.value(QStringLiteral("method")).toString();
    if (method == QStringLiteral("n64recomp"))
        return QDir(folder).filePath(QStringLiteral("saves"));
    if (method == QStringLiteral("romfolder"))
        return QDir(folder).filePath(recipe.value(QStringLiteral("savesSubfolder"), QStringLiteral("saves")).toString());
    return folder;
}

bool LaunchPreparer::isInspected(const QString &path) const
{
    const QFileInfo info(path);
    auto it = m_cache.constFind(path);
    return it != m_cache.constEnd() && it->size == info.size() && it->modified == info.lastModified();
}

QStringList LaunchPreparer::takePendingReads()
{
    QStringList pending;
    pending.swap(m_pendingReads);
    return pending;
}

void LaunchPreparer::storeInspection(const QString &path, const N64RomInfo &info)
{
    const QFileInfo file(path);
    CacheEntry entry;
    entry.size = file.size();
    entry.modified = file.lastModified();
    entry.info = info;
    if (m_cache.size() > 64)
        m_cache.clear();
    m_cache.insert(path, entry);
}

N64RomInfo LaunchPreparer::inspect(const QString &path)
{
    const QFileInfo info(path);
    auto it = m_cache.constFind(path);
    if (it != m_cache.constEnd() && it->size == info.size() && it->modified == info.lastModified())
        return it->info;

    CacheEntry entry;
    entry.size = info.size();
    entry.modified = info.lastModified();
    entry.info = RomInspector::inspectN64File(path);
    if (m_cache.size() > 64)
        m_cache.clear();
    m_cache.insert(path, entry);
    return entry.info;
}

QVariantMap LaunchPreparer::readiness(const QVariantMap &project,
                                      const QString &gamePath,
                                      const QString &installDirectory,
                                      const Options &options)
{
    const QVariantMap recipe = project.value(QStringLiteral("launch")).toMap();
    const QString method = recipe.value(QStringLiteral("method"), QStringLiteral("manual")).toString();
    const QString name = project.value(QStringLiteral("name")).toString();
    const QString romVersion = recipe.value(QStringLiteral("romVersion")).toString();
    const bool hasGame = !gamePath.isEmpty() && QFileInfo(gamePath).isFile();

    QVariantMap result{
        {QStringLiteral("method"), method},
        {QStringLiteral("gameFile"), hasGame ? gamePath : QString()},
        {QStringLiteral("dataFolder"), dataFolder(recipe, installDirectory, options)},
        {QStringLiteral("savesFolder"), savesFolder(recipe, installDirectory, options)}
    };
    auto set = [&result](const QString &state, const QString &tone,
                         const QString &title, const QString &detail) {
        result.insert(QStringLiteral("state"), state);
        result.insert(QStringLiteral("tone"), tone);
        result.insert(QStringLiteral("title"), title);
        result.insert(QStringLiteral("detail"), detail);
        return result;
    };
    const auto noGame = [&]() {
        return set(QStringLiteral("no_game"), QStringLiteral("warn"),
                   QStringLiteral("Add your copy of %1").arg(gameName(project)),
                   QStringLiteral("RecompStation doesn't supply games. Add the file from your own copy."));
    };

    if (method == QStringLiteral("manual")) {
        if (!hasGame)
            return noGame();
        return set(QStringLiteral("manual"), QStringLiteral("info"),
                   QStringLiteral("Pick the file once in the launcher"),
                   QStringLiteral("%1 asks for your game file the first time it starts. Show file opens its folder.").arg(name));
    }
    if (!options.prepareFiles) {
        return set(QStringLiteral("disabled"), QStringLiteral("info"),
                   QStringLiteral("The launcher asks for the file itself"),
                   QStringLiteral("Preparing games for launchers is off in Settings › Library."));
    }

    const QString folder = result.value(QStringLiteral("dataFolder")).toString();
    const QString romName = recipe.value(QStringLiteral("romName")).toString();

    if (method == QStringLiteral("discargument") || method == QStringLiteral("fileargument")) {
        if (!hasGame)
            return noGame();
        const QStringList extensions = recipe.value(QStringLiteral("fileExtensions")).toStringList();
        const QString suffix = QFileInfo(gamePath).suffix().toLower();
        if (!extensions.isEmpty() && !extensions.contains(suffix)) {
            return set(QStringLiteral("wrong_file"), QStringLiteral("warn"),
                       QStringLiteral("%1 can't read this file").arg(name),
                       (method == QStringLiteral("discargument") ? QStringLiteral("It reads %1 disc images.")
                                                                    : QStringLiteral("It reads %1 files."))
                           .arg(extensions.join(QStringLiteral(", "))));
        }
        const QString note = recipe.value(QStringLiteral("firstStartNote")).toString();
        return set(QStringLiteral("ready"), QStringLiteral("ok"), QStringLiteral("Ready to play"),
                   QStringLiteral("Play starts %1 with your copy.").arg(name)
                       + (note.isEmpty() ? QString() : QLatin1Char(' ') + note));
    }

    if (usesRomChecksum(method)) {
        if (QFileInfo::exists(QDir(folder).filePath(recipe.value(QStringLiteral("romFileName")).toString()))) {
            return set(QStringLiteral("has_copy"), QStringLiteral("ok"), QStringLiteral("Ready to play"),
                       QStringLiteral("The launcher already has a copy from an earlier start. RecompStation leaves it as it is."));
        }
        if (!hasGame)
            return noGame();
        if (m_backgroundReads && !isInspected(gamePath)) {
            if (!m_pendingReads.contains(gamePath))
                m_pendingReads.append(gamePath);
            return set(QStringLiteral("checking"), QStringLiteral("info"),
                       QStringLiteral("Checking your copy…"),
                       QStringLiteral("Reading the game file once to see if it's the version %1 needs.").arg(name));
        }
        const N64RomInfo info = inspect(gamePath);
        if (!info.valid) {
            return set(QStringLiteral("wrong_file"), QStringLiteral("warn"),
                       QStringLiteral("This file isn't an N64 ROM"),
                       info.error + QStringLiteral(" Choose another file."));
        }
        if (info.xxh3Hex() == recipe.value(QStringLiteral("romXxh3")).toString()) {
            return set(QStringLiteral("ready"), QStringLiteral("ok"), QStringLiteral("Ready to play"),
                       QStringLiteral("Your copy is %1, the version this launcher needs. Play opens straight to the game.").arg(romVersion));
        }
        // No name in the recipe: the launcher reads it from the ROM, so a
        // mismatch is most likely the same game in another version.
        if (romName.isEmpty() || info.internalName.compare(romName, Qt::CaseInsensitive) == 0) {
            if (info.versionLabel() == romVersion || romVersion.isEmpty()) {
                return set(QStringLiteral("wrong_version"), QStringLiteral("warn"),
                           QStringLiteral("Your copy doesn't match"),
                           QStringLiteral("It differs from the copy %1 needs. It may be modified or a bad dump, so the launcher won't start with it.").arg(name));
            }
            return set(QStringLiteral("wrong_version"), QStringLiteral("warn"),
                       QStringLiteral("Your copy is %1").arg(info.versionLabel()),
                       QStringLiteral("%1 needs %2. It won't start with this copy.").arg(name, romVersion));
        }
        return set(QStringLiteral("wrong_game"), QStringLiteral("warn"),
                   QStringLiteral("This file is a different game"),
                   QStringLiteral("It is %1 (%2). %3 needs %4.")
                       .arg(info.internalName.isEmpty() ? QStringLiteral("untitled") : info.internalName,
                            info.regionName(), name, gameName(project)));
    }

    // harbourmasters
    if (folderHasArchive(folder)) {
        return set(QStringLiteral("has_copy"), QStringLiteral("ok"), QStringLiteral("Ready to play"),
                   QStringLiteral("Game files were already extracted from your copy."));
    }
    if (!hasGame)
        return noGame();
    // Some HarbourMasters launchers only take the game file from their own
    // file picker: nothing is placed for them, only their folder is set.
    if (recipe.value(QStringLiteral("picksGameFile")).toBool()) {
        return set(QStringLiteral("manual"), QStringLiteral("info"),
                   QStringLiteral("Pick the file once in the launcher"),
                   QStringLiteral("%1 asks for your game file the first time it starts and extracts its game files from it. Show file opens its folder.").arg(name));
    }
    if (m_backgroundReads && !isInspected(gamePath)) {
        if (!m_pendingReads.contains(gamePath))
            m_pendingReads.append(gamePath);
        return set(QStringLiteral("checking"), QStringLiteral("info"),
                   QStringLiteral("Checking your copy…"),
                   QStringLiteral("Reading the game file once."));
    }
    const N64RomInfo info = inspect(gamePath);
    if (!info.valid) {
        return set(QStringLiteral("wrong_file"), QStringLiteral("warn"),
                   QStringLiteral("This file isn't an N64 ROM"),
                   info.error + QStringLiteral(" Choose another file."));
    }
    if (info.internalName.compare(romName, Qt::CaseInsensitive) != 0) {
        return set(QStringLiteral("wrong_game"), QStringLiteral("warn"),
                   QStringLiteral("This file is a different game"),
                   QStringLiteral("It is %1. %2 needs %3.")
                       .arg(info.internalName.isEmpty() ? QStringLiteral("untitled") : info.internalName,
                            name, gameName(project)));
    }
    return set(QStringLiteral("ready_extract"), QStringLiteral("ok"),
               QStringLiteral("Ready: one question on first start"),
               QStringLiteral("%1 asks once to extract game files from your copy. After that, Play opens straight to the game.").arg(name));
}

LaunchPreparer::WindowModeResult LaunchPreparer::applyWindowMode(const QString &graphicsPath,
                                                                const Options &options,
                                                                QString *summary)
{
    if (!QFileInfo::exists(graphicsPath)) {
        // Never create the file: launchers expect every option in it (a
        // missing number makes RecompFrontend launchers abort while loading).
        // They write a complete one on their first start; set it from then.
        const bool wanted = options.windowMode != QStringLiteral("default")
            || (options.fullscreen && !options.defaultFullscreenApplied);
        return wanted ? WindowModeResult::Pending : WindowModeResult::Unchanged;
    }

    QFile file(graphicsPath);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        return WindowModeResult::Unchanged;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return WindowModeResult::Unchanged;
    QJsonObject object = document.object();

    // An earlier RecompStation build wrote a file with only this key, which
    // stops the launcher from starting. Remove it so the launcher writes its own.
    if (object.size() == 1 && object.contains(QStringLiteral("wm_option"))) {
        QFile::remove(graphicsPath);
        qCInfo(lcGameLaunch) << "Removed an incomplete launcher graphics file:" << graphicsPath;
        return applyWindowMode(graphicsPath, options, summary);
    }

    QString value;
    if (options.windowMode == QStringLiteral("fullscreen"))
        value = QStringLiteral("Fullscreen");
    else if (options.windowMode == QStringLiteral("windowed"))
        value = QStringLiteral("Windowed");
    else if (options.fullscreen && !options.defaultFullscreenApplied)
        value = QStringLiteral("Fullscreen");
    else
        return WindowModeResult::Unchanged;

    if (object.value(QStringLiteral("wm_option")).toString() == value)
        return options.windowMode == QStringLiteral("default") ? WindowModeResult::DefaultApplied
                                                               : WindowModeResult::Unchanged;
    object.insert(QStringLiteral("wm_option"), value);

    QSaveFile output(graphicsPath);
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit())
        return WindowModeResult::Unchanged;
    if (summary) {
        *summary = options.windowMode == QStringLiteral("default")
            ? QStringLiteral("Set to fullscreen")
            : (value == QStringLiteral("Fullscreen") ? QStringLiteral("Fullscreen, as chosen for this game")
                                                     : QStringLiteral("Windowed, as chosen for this game"));
    }
    return options.windowMode == QStringLiteral("default") ? WindowModeResult::DefaultApplied
                                                           : WindowModeResult::Changed;
}

LaunchPreparer::Plan LaunchPreparer::prepare(const QVariantMap &project,
                                             const QString &gamePath,
                                             const QString &installDirectory,
                                             const Options &options)
{
    Plan plan;
    plan.ok = true;
    const QVariantMap recipe = project.value(QStringLiteral("launch")).toMap();
    const QString method = recipe.value(QStringLiteral("method"), QStringLiteral("manual")).toString();
    if (method == QStringLiteral("manual"))
        return plan;
    if (method == QStringLiteral("discargument") || method == QStringLiteral("fileargument")) {
        const QStringList extensions = recipe.value(QStringLiteral("fileExtensions")).toStringList();
        const QFileInfo game(gamePath);
        if (!options.prepareFiles || gamePath.isEmpty() || !game.isFile()
            || (!extensions.isEmpty() && !extensions.contains(game.suffix().toLower())))
            return plan;
        plan.arguments = filledArguments(recipe, {{QStringLiteral("gamePath"), game.absoluteFilePath()}});
        plan.steps.append(step(QStringLiteral("Starting with your copy"), game.fileName()));
        return plan;
    }
    // Pointing a launcher at a folder outside its install folder is about
    // keeping saves, so it happens even when preparing files is off.
    const bool keepsSavesOutside = method == QStringLiteral("harbourmasters")
        || (method == QStringLiteral("romfolder")
            && !recipe.value(QStringLiteral("environment")).toMap().isEmpty());
    if (!options.prepareFiles && !keepsSavesOutside)
        return plan;

    const QString folder = dataFolder(recipe, installDirectory, options);
    if (folder.isEmpty())
        return plan;
    const bool newFolder = !QFileInfo::exists(folder) || QDir(folder).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    if (!QDir().mkpath(folder)) {
        qCWarning(lcGameLaunch) << "Could not create the launcher data folder" << folder;
        return plan;
    }

    const QString romFileName = recipe.value(QStringLiteral("romFileName")).toString();
    const QString romPath = QDir(folder).filePath(romFileName);
    const QString romName = recipe.value(QStringLiteral("romName")).toString();

    if (method == QStringLiteral("harbourmasters") || method == QStringLiteral("romfolder")) {
        if (method == QStringLiteral("harbourmasters")) {
            plan.environment.insert(QStringLiteral("SHIP_HOME"), folder);
        } else {
            const QVariantMap environment = recipe.value(QStringLiteral("environment")).toMap();
            for (auto it = environment.cbegin(); it != environment.cend(); ++it)
                plan.environment.insert(it.key(), fillIn(it.value().toString(), {{QStringLiteral("dataFolder"), folder}}));
        }
        // Earlier starts kept saves and settings in the install folder. Copy
        // them over the first time, so nothing is lost when switching.
        if (newFolder && !installDirectory.isEmpty()) {
            int copied = 0;
            const QStringList patterns = recipe.value(QStringLiteral("dataFiles")).toStringList();
            for (const QString &pattern : patterns) {
                const QFileInfoList matches = QDir(installDirectory).entryInfoList(
                    {pattern}, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
                for (const QFileInfo &match : matches)
                    copied += copyWithoutReplacing(match.filePath(), QDir(folder).filePath(match.fileName()));
            }
            if (copied > 0) {
                plan.steps.append(step(QStringLiteral("Copied your saves and settings"),
                                       QStringLiteral("To %1, outside the launcher's install folder").arg(shortPath(folder, options))));
            }
        }
        if (method == QStringLiteral("harbourmasters")
            && (!options.prepareFiles || folderHasArchive(folder) || QFileInfo::exists(romPath)
                || gamePath.isEmpty() || recipe.value(QStringLiteral("picksGameFile")).toBool()))
            return plan;
        if (!options.prepareFiles)
            return plan;
        if (method == QStringLiteral("romfolder") && QFileInfo::exists(romPath))
            plan.steps.append(step(QStringLiteral("The launcher already has your copy"), shortPath(romPath, options)));
    } else if (QFileInfo::exists(romPath)) {
        plan.steps.append(step(QStringLiteral("The launcher already has your copy"), shortPath(romPath, options)));
    }

    if (!QFileInfo::exists(romPath) && !gamePath.isEmpty()) {
        QFile source(gamePath);
        const QFileInfo sourceInfo(gamePath);
        if (sourceInfo.isFile() && sourceInfo.size() <= RomInspector::MaximumN64RomSize
            && source.open(QIODevice::ReadOnly)) {
            QString error;
            const QByteArray canonical = RomInspector::canonicalN64(source.readAll(), &error);
            const N64RomInfo info = RomInspector::inspectN64(canonical);
            const bool matches = usesRomChecksum(method)
                ? info.valid && info.xxh3Hex() == recipe.value(QStringLiteral("romXxh3")).toString()
                : info.valid && info.internalName.compare(romName, Qt::CaseInsensitive) == 0;
            if (matches) {
                plan.steps.append(step(QStringLiteral("Checked your copy"),
                                       sourceInfo.fileName() + QStringLiteral(" · ")
                                           + (usesRomChecksum(method)
                                                  ? recipe.value(QStringLiteral("romVersion")).toString()
                                                  : info.versionLabel())));
                QDir().mkpath(QFileInfo(romPath).absolutePath());
                if (writeFileOnce(romPath, canonical, &error)) {
                    plan.steps.append(step(QStringLiteral("Gave it to the launcher"), shortPath(romPath, options)));
                    qCInfo(lcGameLaunch) << "Placed the game file for the launcher at" << romPath;
                } else {
                    qCWarning(lcGameLaunch) << "Could not place the game file at" << romPath << error;
                    plan.steps.append(step(QStringLiteral("Couldn't give your copy to the launcher"),
                                           QStringLiteral("It will ask for the file instead"), true));
                }
            } else {
                plan.steps.append(step(QStringLiteral("Your copy isn't the one this launcher needs"),
                                       QStringLiteral("It will ask for a file instead"), true));
            }
        }
    }

    // Launchers that take the ROM on the command line get it only once it is
    // in place; otherwise they ask for a file as usual.
    if (method == QStringLiteral("romfolder") && QFileInfo::exists(romPath))
        plan.arguments = filledArguments(recipe, {{QStringLiteral("romPath"), romPath},
                                                  {QStringLiteral("dataFolder"), folder}});

    const QString graphicsFile = recipe.value(QStringLiteral("graphicsFile")).toString();
    if (method == QStringLiteral("n64recomp") && !graphicsFile.isEmpty()) {
        QString summary;
        switch (applyWindowMode(QDir(folder).filePath(graphicsFile), options, &summary)) {
        case WindowModeResult::DefaultApplied:
            plan.defaultFullscreenApplied = true;
            if (!summary.isEmpty())
                plan.steps.append(step(summary, QStringLiteral("Change it any time in the launcher's own settings")));
            break;
        case WindowModeResult::Changed:
            plan.steps.append(step(summary, QStringLiteral("Change it any time on the game's page")));
            break;
        case WindowModeResult::Pending:
            plan.steps.append(step(options.windowMode == QStringLiteral("windowed")
                                       ? QStringLiteral("Windowed from the next start")
                                       : QStringLiteral("Fullscreen from the next start"),
                                   QStringLiteral("The launcher creates its settings the first time it starts")));
            break;
        case WindowModeResult::Unchanged:
            break;
        }
    }
    return plan;
}
