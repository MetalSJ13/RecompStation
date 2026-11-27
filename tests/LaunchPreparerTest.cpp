#include <QtTest>
#include <QCryptographicHash>

#include "core/LaunchPreparer.h"
#include "core/RomInspector.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {
// A small synthetic N64 image: big-endian magic, a header title and game code.
QByteArray makeRom(const QByteArray &title, char region, char revision = 0, char fill = 'x')
{
    QByteArray rom(0x2000, fill);
    rom[0] = char(0x80); rom[1] = char(0x37); rom[2] = char(0x12); rom[3] = char(0x40);
    QByteArray name = title.left(20);
    name.append(QByteArray(20 - name.size(), ' '));
    rom.replace(0x20, 20, name);
    rom.replace(0x3B, 3, QByteArray("NTE"));
    rom[0x3E] = region;
    rom[0x3F] = revision;
    return rom;
}

QByteArray byteSwapped(const QByteArray &rom)
{
    QByteArray out = rom;
    for (qsizetype i = 0; i + 1 < out.size(); i += 2)
        std::swap(out[i], out[i + 1]);
    return out;
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QVariantMap n64Project(const QString &hash)
{
    return QVariantMap{
        {QStringLiteral("id"), QStringLiteral("test-recomp")},
        {QStringLiteral("name"), QStringLiteral("Test: Recompiled")},
        {QStringLiteral("gameTitles"), QVariantList{QStringLiteral("Test Game")}},
        {QStringLiteral("launch"), QVariantMap{
            {QStringLiteral("method"), QStringLiteral("n64recomp")},
            {QStringLiteral("dataFolder"), QStringLiteral("TestRecompiled")},
            {QStringLiteral("romFileName"), QStringLiteral("test.n64.us.1.0.z64")},
            {QStringLiteral("romName"), QStringLiteral("TEST GAME")},
            {QStringLiteral("romXxh3"), hash},
            {QStringLiteral("romVersion"), QStringLiteral("North America 1.0")},
            {QStringLiteral("graphicsFile"), QStringLiteral("graphics.json")}}}
    };
}

// Pokémon Snap-like: ROM and saves in a data folder the launcher is pointed
// at with an environment variable.
QVariantMap snapProject(const QString &hash)
{
    return QVariantMap{
        {QStringLiteral("id"), QStringLiteral("test-snap")},
        {QStringLiteral("name"), QStringLiteral("Test Snap")},
        {QStringLiteral("gameTitles"), QVariantList{QStringLiteral("Test Game")}},
        {QStringLiteral("launch"), QVariantMap{
            {QStringLiteral("method"), QStringLiteral("romfolder")},
            {QStringLiteral("dataFolder"), QStringLiteral("TestSnap")},
            {QStringLiteral("romFileName"), QStringLiteral("testsnap.z64")},
            {QStringLiteral("romName"), QStringLiteral("TEST GAME")},
            {QStringLiteral("romXxh3"), hash},
            {QStringLiteral("romVersion"), QStringLiteral("North America")},
            {QStringLiteral("environment"), QVariantMap{{QStringLiteral("TEST_DATA_DIR"), QStringLiteral("{dataFolder}")}}},
            {QStringLiteral("dataFiles"), QStringList{QStringLiteral("saves"), QStringLiteral("mods.json")}},
            {QStringLiteral("savesSubfolder"), QStringLiteral("saves")}}}
    };
}

// Snowboard Kids-like: the placed ROM is passed as the first argument.
QVariantMap romArgumentProject(const QString &hash)
{
    return QVariantMap{
        {QStringLiteral("id"), QStringLiteral("test-rom-argument")},
        {QStringLiteral("name"), QStringLiteral("Test Kids")},
        {QStringLiteral("gameTitles"), QVariantList{QStringLiteral("Test Game")}},
        {QStringLiteral("launch"), QVariantMap{
            {QStringLiteral("method"), QStringLiteral("romfolder")},
            {QStringLiteral("dataFolder"), QStringLiteral("TestKids")},
            {QStringLiteral("romFileName"), QStringLiteral("rom/testkids.us.z64")},
            {QStringLiteral("romXxh3"), hash},
            {QStringLiteral("romVersion"), QStringLiteral("North America")},
            {QStringLiteral("arguments"), QStringList{QStringLiteral("{romPath}")}},
            {QStringLiteral("savesSubfolder"), QStringLiteral("userdata/saves")}}}
    };
}

// Luigi's Mansion-like: the game file itself is passed with a flag.
QVariantMap discProject()
{
    return QVariantMap{
        {QStringLiteral("id"), QStringLiteral("test-disc")},
        {QStringLiteral("name"), QStringLiteral("Test Mansion")},
        {QStringLiteral("gameTitles"), QVariantList{QStringLiteral("Test Game")}},
        {QStringLiteral("launch"), QVariantMap{
            {QStringLiteral("method"), QStringLiteral("discargument")},
            {QStringLiteral("arguments"), QStringList{QStringLiteral("--disc"), QStringLiteral("{gamePath}")}},
            {QStringLiteral("fileExtensions"), QStringList{QStringLiteral("iso"), QStringLiteral("rvz")}},
            {QStringLiteral("firstStartNote"), QStringLiteral("The first start takes a while.")}}}
    };
}

QVariantMap shipProject()
{
    return QVariantMap{
        {QStringLiteral("id"), QStringLiteral("test-ship")},
        {QStringLiteral("name"), QStringLiteral("Test Ship")},
        {QStringLiteral("gameTitles"), QVariantList{QStringLiteral("Test Game")}},
        {QStringLiteral("launch"), QVariantMap{
            {QStringLiteral("method"), QStringLiteral("harbourmasters")},
            {QStringLiteral("dataFolder"), QStringLiteral("testship")},
            {QStringLiteral("romFileName"), QStringLiteral("baserom.us.z64")},
            {QStringLiteral("romName"), QStringLiteral("TEST GAME")},
            {QStringLiteral("dataFiles"), QStringList{QStringLiteral("saves"), QStringLiteral("*.cfg.json")}}}}
    };
}
}

class LaunchPreparerTest : public QObject
{
    Q_OBJECT

private slots:
    void inspectsHeaderAndChecksumInAnyByteOrder();
    void fileArgumentPassesTheRom();
    void selectFileChoosesTheGameWithoutStartingIt();
    void selectFileWritesWindowsAndWinePaths();
    void readinessDescribesEachSituation();
    void preparesN64RecompLauncherOnce();
    void windowModeChangesOnlyThatSetting();
    void portableLauncherUsesItsInstallFolder();
    void doesNotCopyMismatchedRom();
    void preparesHarbourMastersPort();
    void harbourMastersSavesStayOutsideInstallWhenPreparingIsOff();
    void harbourMastersPortThatPicksItsOwnFile();
    void romFolderPointsTheLauncherAtItsDataFolder();
    void romFolderPassesThePlacedRomOnlyWhenItMatches();
    void romFolderInTheInstallFolderBySha1();
    void selectFileKeyValueInSdlFolders();
    void discArgumentPassesTheGameFile();
    void backgroundReadsNeverReadOnTheCallersThread();
    void windowsFolderRules();

private:
    LaunchPreparer::Options options(const QTemporaryDir &root) const
    {
        LaunchPreparer::Options result;
        result.homeDirectory = QDir(root.path()).filePath(QStringLiteral("home"));
        result.dataHome = QDir(root.path()).filePath(QStringLiteral("home/.local/share"));
        // Linux folder rules on every system; windowsFolderRules checks Windows'.
        result.system = QStringLiteral("linux");
        return result;
    }
};

void LaunchPreparerTest::inspectsHeaderAndChecksumInAnyByteOrder()
{
    const QByteArray rom = makeRom("TEST GAME", 'E', 1);
    const N64RomInfo bigEndian = RomInspector::inspectN64(rom);
    const N64RomInfo swapped = RomInspector::inspectN64(byteSwapped(rom));
    QVERIFY(bigEndian.valid);
    QVERIFY(swapped.valid);
    QCOMPARE(bigEndian.internalName, QStringLiteral("TEST GAME"));
    QCOMPARE(bigEndian.versionLabel(), QStringLiteral("North America 1.1"));
    QCOMPARE(swapped.xxh3, bigEndian.xxh3);
    QCOMPARE(bigEndian.xxh3Hex().size(), 16);
    QCOMPARE(RomInspector::canonicalN64(byteSwapped(rom)), rom);

    QVERIFY(!RomInspector::inspectN64(QByteArray(0x2000, 'z')).valid);
    QVERIFY(!RomInspector::inspectN64(QByteArray(16, 'z')).valid);
}

void LaunchPreparerTest::readinessDescribesEachSituation()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QByteArray rom = makeRom("TEST GAME", 'E');
    const QString hash = RomInspector::inspectN64(rom).xxh3Hex();
    const QVariantMap project = n64Project(hash);
    const LaunchPreparer::Options opts = options(root);
    LaunchPreparer preparer;

    QCOMPARE(preparer.readiness(project, QString(), QString(), opts).value("state").toString(),
             QStringLiteral("no_game"));

    const QString good = QDir(root.path()).filePath(QStringLiteral("good.v64"));
    QVERIFY(writeFile(good, byteSwapped(rom)));
    QVariantMap state = preparer.readiness(project, good, QString(), opts);
    QCOMPARE(state.value("state").toString(), QStringLiteral("ready"));
    QCOMPARE(state.value("tone").toString(), QStringLiteral("ok"));
    QCOMPARE(state.value("savesFolder").toString(),
             QDir(opts.homeDirectory).filePath(QStringLiteral(".config/TestRecompiled/saves")));

    const QString europe = QDir(root.path()).filePath(QStringLiteral("europe.z64"));
    QVERIFY(writeFile(europe, makeRom("TEST GAME", 'P')));
    state = preparer.readiness(project, europe, QString(), opts);
    QCOMPARE(state.value("state").toString(), QStringLiteral("wrong_version"));
    QCOMPARE(state.value("title").toString(), QStringLiteral("Your copy is Europe 1.0"));

    const QString modified = QDir(root.path()).filePath(QStringLiteral("modified.z64"));
    QVERIFY(writeFile(modified, makeRom("TEST GAME", 'E', 0, 'y')));
    QCOMPARE(preparer.readiness(project, modified, QString(), opts).value("title").toString(),
             QStringLiteral("Your copy doesn't match"));

    const QString other = QDir(root.path()).filePath(QStringLiteral("other.z64"));
    QVERIFY(writeFile(other, makeRom("OTHER GAME", 'E')));
    QCOMPARE(preparer.readiness(project, other, QString(), opts).value("state").toString(),
             QStringLiteral("wrong_game"));

    const QString notRom = QDir(root.path()).filePath(QStringLiteral("notes.z64"));
    QVERIFY(writeFile(notRom, QByteArray(0x2000, 'q')));
    QCOMPARE(preparer.readiness(project, notRom, QString(), opts).value("state").toString(),
             QStringLiteral("wrong_file"));

    LaunchPreparer::Options disabled = opts;
    disabled.prepareFiles = false;
    QCOMPARE(preparer.readiness(project, good, QString(), disabled).value("state").toString(),
             QStringLiteral("disabled"));

    QVariantMap manual = project;
    manual.insert(QStringLiteral("launch"), QVariantMap{{QStringLiteral("method"), QStringLiteral("manual")}});
    QCOMPARE(preparer.readiness(manual, good, QString(), opts).value("state").toString(),
             QStringLiteral("manual"));

    QVERIFY(writeFile(QDir(opts.homeDirectory).filePath(QStringLiteral(".config/TestRecompiled/test.n64.us.1.0.z64")), rom));
    QCOMPARE(preparer.readiness(project, europe, QString(), opts).value("state").toString(),
             QStringLiteral("has_copy"));
}

void LaunchPreparerTest::preparesN64RecompLauncherOnce()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QByteArray rom = makeRom("TEST GAME", 'E');
    const QVariantMap project = n64Project(RomInspector::inspectN64(rom).xxh3Hex());
    LaunchPreparer::Options opts = options(root);
    const QString game = QDir(root.path()).filePath(QStringLiteral("library/Test (USA).n64"));
    QVERIFY(writeFile(game, byteSwapped(rom)));
    const QByteArray original = readFile(game);

    LaunchPreparer preparer;
    LaunchPreparer::Plan plan = preparer.prepare(project, game, QString(), opts);
    QVERIFY(plan.ok);
    QVERIFY(plan.environment.isEmpty());
    QCOMPARE(plan.steps.size(), 3); // checked, placed, fullscreen from the next start
    QVERIFY(!plan.defaultFullscreenApplied);

    const QString folder = QDir(opts.homeDirectory).filePath(QStringLiteral(".config/TestRecompiled"));
    const QString stored = QDir(folder).filePath(QStringLiteral("test.n64.us.1.0.z64"));
    const QString graphics = QDir(folder).filePath(QStringLiteral("graphics.json"));
    QCOMPARE(readFile(stored), rom); // big-endian, as the launcher stores it
    QCOMPARE(readFile(game), original); // the library file is only read
    QVERIFY(!QFileInfo::exists(graphics)); // the launcher writes its own settings

    // The launcher's first start wrote its complete settings.
    QVERIFY(writeFile(stored, QByteArray("launcher's own copy")));
    QVERIFY(writeFile(graphics, QByteArray("{\"wm_option\":\"Windowed\",\"ds_option\":1}")));
    plan = preparer.prepare(project, game, QString(), opts);
    QCOMPARE(readFile(stored), QByteArray("launcher's own copy"));
    QJsonObject settings = QJsonDocument::fromJson(readFile(graphics)).object();
    QCOMPARE(settings.value("wm_option").toString(), QStringLiteral("Fullscreen"));
    QCOMPARE(settings.value("ds_option").toInt(), 1);
    QVERIFY(plan.defaultFullscreenApplied);
    QCOMPARE(plan.steps.size(), 2);

    // Set once: a later change inside the launcher is kept.
    opts.defaultFullscreenApplied = true;
    QVERIFY(writeFile(graphics, QByteArray("{\"wm_option\":\"Windowed\",\"ds_option\":1}")));
    plan = preparer.prepare(project, game, QString(), opts);
    settings = QJsonDocument::fromJson(readFile(graphics)).object();
    QCOMPARE(settings.value("wm_option").toString(), QStringLiteral("Windowed"));
    QCOMPARE(plan.steps.size(), 1);
}

void LaunchPreparerTest::windowModeChangesOnlyThatSetting()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = QDir(root.path()).filePath(QStringLiteral("graphics.json"));
    LaunchPreparer::Options opts;

    QVERIFY(LaunchPreparer::applyWindowMode(path, opts) == LaunchPreparer::WindowModeResult::Pending);
    opts.fullscreen = false;
    QVERIFY(LaunchPreparer::applyWindowMode(path, opts) == LaunchPreparer::WindowModeResult::Unchanged);
    opts.windowMode = QStringLiteral("windowed");
    QVERIFY(LaunchPreparer::applyWindowMode(path, opts) == LaunchPreparer::WindowModeResult::Pending);
    QVERIFY(!QFileInfo::exists(path));

    QVERIFY(writeFile(path, QByteArray("{\"wm_option\":\"Fullscreen\",\"ar_option\":\"Expand\"}")));
    QString summary;
    QVERIFY(LaunchPreparer::applyWindowMode(path, opts, &summary) == LaunchPreparer::WindowModeResult::Changed);
    const QJsonObject object = QJsonDocument::fromJson(readFile(path)).object();
    QCOMPARE(object.value("wm_option").toString(), QStringLiteral("Windowed"));
    QCOMPARE(object.value("ar_option").toString(), QStringLiteral("Expand"));
    QVERIFY(!summary.isEmpty());
    QVERIFY(LaunchPreparer::applyWindowMode(path, opts) == LaunchPreparer::WindowModeResult::Unchanged);

    QVERIFY(writeFile(path, QByteArray("not json")));
    opts.windowMode = QStringLiteral("fullscreen");
    QVERIFY(LaunchPreparer::applyWindowMode(path, opts) == LaunchPreparer::WindowModeResult::Unchanged);
    QCOMPARE(readFile(path), QByteArray("not json"));

    // A file with only wm_option (written by an earlier build) stops the
    // launcher from starting: it is removed so the launcher writes its own.
    QVERIFY(writeFile(path, QByteArray("{\"wm_option\":\"Fullscreen\"}")));
    QVERIFY(LaunchPreparer::applyWindowMode(path, opts) == LaunchPreparer::WindowModeResult::Pending);
    QVERIFY(!QFileInfo::exists(path));
}

void LaunchPreparerTest::portableLauncherUsesItsInstallFolder()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QByteArray rom = makeRom("TEST GAME", 'E');
    const QVariantMap project = n64Project(RomInspector::inspectN64(rom).xxh3Hex());
    const QString install = QDir(root.path()).filePath(QStringLiteral("launchers/test-recomp"));
    QVERIFY(writeFile(QDir(install).filePath(QStringLiteral("portable.txt")), QByteArray()));
    const QString game = QDir(root.path()).filePath(QStringLiteral("game.z64"));
    QVERIFY(writeFile(game, rom));

    LaunchPreparer preparer;
    preparer.prepare(project, game, install, options(root));
    QCOMPARE(readFile(QDir(install).filePath(QStringLiteral("test.n64.us.1.0.z64"))), rom);
}

void LaunchPreparerTest::doesNotCopyMismatchedRom()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QVariantMap project = n64Project(RomInspector::inspectN64(makeRom("TEST GAME", 'E')).xxh3Hex());
    const QString game = QDir(root.path()).filePath(QStringLiteral("europe.z64"));
    QVERIFY(writeFile(game, makeRom("TEST GAME", 'P')));
    const LaunchPreparer::Options opts = options(root);

    LaunchPreparer preparer;
    const LaunchPreparer::Plan plan = preparer.prepare(project, game, QString(), opts);
    QVERIFY(plan.ok);
    QVERIFY(!QFileInfo::exists(QDir(opts.homeDirectory).filePath(
        QStringLiteral(".config/TestRecompiled/test.n64.us.1.0.z64"))));
}

void LaunchPreparerTest::preparesHarbourMastersPort()
{
    if (!qEnvironmentVariableIsEmpty("SHIP_HOME"))
        QSKIP("SHIP_HOME is set in this environment.");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const LaunchPreparer::Options opts = options(root);
    const QString install = QDir(root.path()).filePath(QStringLiteral("launchers/test-ship"));
    QVERIFY(writeFile(QDir(install).filePath(QStringLiteral("saves/file1.sav")), QByteArray("save")));
    QVERIFY(writeFile(QDir(install).filePath(QStringLiteral("testship.cfg.json")), QByteArray("{}")));
    QVERIFY(writeFile(QDir(install).filePath(QStringLiteral("Port.AppImage")), QByteArray("binary")));
    const QByteArray rom = makeRom("TEST GAME", 'E');
    const QString game = QDir(root.path()).filePath(QStringLiteral("game.v64"));
    QVERIFY(writeFile(game, byteSwapped(rom)));

    LaunchPreparer preparer;
    QCOMPARE(preparer.readiness(shipProject(), game, install, opts).value("state").toString(),
             QStringLiteral("ready_extract"));

    const LaunchPreparer::Plan plan = preparer.prepare(shipProject(), game, install, opts);
    const QString folder = QDir(opts.dataHome).filePath(QStringLiteral("testship"));
    QCOMPARE(plan.environment.value(QStringLiteral("SHIP_HOME")), folder);
    QCOMPARE(readFile(QDir(folder).filePath(QStringLiteral("saves/file1.sav"))), QByteArray("save"));
    QVERIFY(QFileInfo::exists(QDir(folder).filePath(QStringLiteral("testship.cfg.json"))));
    QVERIFY(!QFileInfo::exists(QDir(folder).filePath(QStringLiteral("Port.AppImage"))));
    QCOMPARE(readFile(QDir(folder).filePath(QStringLiteral("baserom.us.z64"))), rom);

    // After extraction the ROM is not needed again.
    QVERIFY(QFile::remove(QDir(folder).filePath(QStringLiteral("baserom.us.z64"))));
    QVERIFY(writeFile(QDir(folder).filePath(QStringLiteral("game.o2r")), QByteArray("assets")));
    QCOMPARE(preparer.readiness(shipProject(), game, install, opts).value("state").toString(),
             QStringLiteral("has_copy"));
    preparer.prepare(shipProject(), game, install, opts);
    QVERIFY(!QFileInfo::exists(QDir(folder).filePath(QStringLiteral("baserom.us.z64"))));

    QVERIFY(preparer.readiness(shipProject(), QString(), install, opts).value("state").toString()
            == QStringLiteral("has_copy"));
}

// A port that only takes the game file from its own file picker: its folder
// is still set (saves stay outside the install folder), but no copy is placed.
void LaunchPreparerTest::harbourMastersPortThatPicksItsOwnFile()
{
    if (!qEnvironmentVariableIsEmpty("SHIP_HOME"))
        QSKIP("SHIP_HOME is set in this environment.");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const LaunchPreparer::Options opts = options(root);
    const QString install = QDir(root.path()).filePath(QStringLiteral("launchers/test-ship"));
    QVERIFY(writeFile(QDir(install).filePath(QStringLiteral("Port.AppImage")), QByteArray("binary")));
    const QString game = QDir(root.path()).filePath(QStringLiteral("game.z64"));
    QVERIFY(writeFile(game, makeRom("TEST GAME", 'E')));

    QVariantMap project = shipProject();
    QVariantMap recipe = project.value(QStringLiteral("launch")).toMap();
    recipe.insert(QStringLiteral("picksGameFile"), true);
    project.insert(QStringLiteral("launch"), recipe);

    LaunchPreparer preparer;
    QCOMPARE(preparer.readiness(project, game, install, opts).value("state").toString(),
             QStringLiteral("manual"));
    const LaunchPreparer::Plan plan = preparer.prepare(project, game, install, opts);
    const QString folder = QDir(opts.dataHome).filePath(QStringLiteral("testship"));
    QCOMPARE(plan.environment.value(QStringLiteral("SHIP_HOME")), folder);
    QVERIFY(!QFileInfo::exists(QDir(folder).filePath(QStringLiteral("baserom.us.z64"))));

    QVERIFY(writeFile(QDir(folder).filePath(QStringLiteral("game.o2r")), QByteArray("assets")));
    QCOMPARE(preparer.readiness(project, game, install, opts).value("state").toString(),
             QStringLiteral("has_copy"));
}

void LaunchPreparerTest::harbourMastersSavesStayOutsideInstallWhenPreparingIsOff()
{
    if (!qEnvironmentVariableIsEmpty("SHIP_HOME"))
        QSKIP("SHIP_HOME is set in this environment.");

    QTemporaryDir root;
    QVERIFY(root.isValid());
    LaunchPreparer::Options opts = options(root);
    opts.prepareFiles = false;
    const QString install = QDir(root.path()).filePath(QStringLiteral("launchers/test-ship"));
    QVERIFY(writeFile(QDir(install).filePath(QStringLiteral("saves/file1.sav")), QByteArray("save")));
    const QString game = QDir(root.path()).filePath(QStringLiteral("game.z64"));
    QVERIFY(writeFile(game, makeRom("TEST GAME", 'E')));

    LaunchPreparer preparer;
    const LaunchPreparer::Plan plan = preparer.prepare(shipProject(), game, install, opts);
    const QString folder = QDir(opts.dataHome).filePath(QStringLiteral("testship"));
    QCOMPARE(plan.environment.value(QStringLiteral("SHIP_HOME")), folder);
    QVERIFY(QFileInfo::exists(QDir(folder).filePath(QStringLiteral("saves/file1.sav"))));
    QVERIFY(!QFileInfo::exists(QDir(folder).filePath(QStringLiteral("baserom.us.z64"))));
}

void LaunchPreparerTest::romFolderPointsTheLauncherAtItsDataFolder()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QByteArray rom = makeRom("TEST GAME", 'E');
    const QVariantMap project = snapProject(RomInspector::inspectN64(rom).xxh3Hex());
    LaunchPreparer::Options opts = options(root);
    const QString install = QDir(root.path()).filePath(QStringLiteral("launchers/test-snap"));
    QVERIFY(writeFile(QDir(install).filePath(QStringLiteral("saves/slot1.sav")), QByteArray("save")));
    QVERIFY(writeFile(QDir(install).filePath(QStringLiteral("mods.json")), QByteArray("{}")));
    const QString game = QDir(root.path()).filePath(QStringLiteral("library/Test (USA).v64"));
    QVERIFY(writeFile(game, byteSwapped(rom)));

    LaunchPreparer preparer;
    const QString folder = QDir(opts.dataHome).filePath(QStringLiteral("TestSnap"));
    QCOMPARE(LaunchPreparer::savesFolder(project.value("launch").toMap(), install, opts),
             QDir(folder).filePath(QStringLiteral("saves")));
    QCOMPARE(preparer.readiness(project, game, install, opts).value("state").toString(),
             QStringLiteral("ready"));

    const LaunchPreparer::Plan plan = preparer.prepare(project, game, install, opts);
    QCOMPARE(plan.environment.value(QStringLiteral("TEST_DATA_DIR")), folder);
    QVERIFY(plan.arguments.isEmpty());
    // Saves made in the install folder came along; the ROM is big-endian.
    QCOMPARE(readFile(QDir(folder).filePath(QStringLiteral("saves/slot1.sav"))), QByteArray("save"));
    QVERIFY(QFileInfo::exists(QDir(folder).filePath(QStringLiteral("mods.json"))));
    QCOMPARE(readFile(QDir(folder).filePath(QStringLiteral("testsnap.z64"))), rom);
    QCOMPARE(preparer.readiness(project, game, install, opts).value("state").toString(),
             QStringLiteral("has_copy"));

    // With preparing off the launcher is still pointed outside its install.
    QTemporaryDir other;
    LaunchPreparer::Options off = options(other);
    off.prepareFiles = false;
    const LaunchPreparer::Plan offPlan = preparer.prepare(project, game, install, off);
    const QString offFolder = QDir(off.dataHome).filePath(QStringLiteral("TestSnap"));
    QCOMPARE(offPlan.environment.value(QStringLiteral("TEST_DATA_DIR")), offFolder);
    QVERIFY(!QFileInfo::exists(QDir(offFolder).filePath(QStringLiteral("testsnap.z64"))));
}

void LaunchPreparerTest::romFolderPassesThePlacedRomOnlyWhenItMatches()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QByteArray rom = makeRom("TEST GAME", 'E');
    LaunchPreparer::Options opts = options(root);
    const QString game = QDir(root.path()).filePath(QStringLiteral("library/Test Kids (USA).z64"));
    QVERIFY(writeFile(game, rom));
    LaunchPreparer preparer;

    // Another game: no argument, so the launcher asks as usual.
    const QVariantMap wrong = romArgumentProject(QStringLiteral("0123456789ABCDEF"));
    LaunchPreparer::Plan plan = preparer.prepare(wrong, game, QString(), opts);
    QVERIFY(plan.arguments.isEmpty());
    // No romName in the recipe: a mismatch reads as the wrong copy, not another game.
    QCOMPARE(preparer.readiness(wrong, game, QString(), opts).value("state").toString(),
             QStringLiteral("wrong_version"));

    const QVariantMap project = romArgumentProject(RomInspector::inspectN64(rom).xxh3Hex());
    plan = preparer.prepare(project, game, QString(), opts);
    const QString folder = QDir(opts.dataHome).filePath(QStringLiteral("TestKids"));
    const QString placed = QDir(folder).filePath(QStringLiteral("rom/testkids.us.z64"));
    QCOMPARE(readFile(placed), rom);
    QCOMPARE(plan.arguments, QStringList{placed});
    QCOMPARE(LaunchPreparer::savesFolder(project.value("launch").toMap(), QString(), opts),
             QDir(folder).filePath(QStringLiteral("userdata/saves")));
}

void LaunchPreparerTest::romFolderInTheInstallFolderBySha1()
{
    // goldeneye-pc-port-like: the ROM goes into data/ beside the program, and
    // the port publishes only a SHA-1 of the ROM.
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QByteArray rom = makeRom("TEST GAME", 'E');
    const QString sha1 = QString::fromLatin1(QCryptographicHash::hash(rom, QCryptographicHash::Sha1).toHex());
    QCOMPARE(RomInspector::inspectN64(rom).sha1, sha1);
    QVariantMap project = snapProject(QString());
    QVariantMap launch = project.value("launch").toMap();
    launch.remove(QStringLiteral("environment"));
    launch.remove(QStringLiteral("romXxh3"));
    launch.insert(QStringLiteral("dataFolder"), QString());
    launch.insert(QStringLiteral("installFolder"), true);
    launch.insert(QStringLiteral("romFileName"), QStringLiteral("data/test.ntsc-final.z64"));
    launch.insert(QStringLiteral("romSha1"), sha1);
    project.insert(QStringLiteral("launch"), launch);
    const QString install = QDir(root.path()).filePath(QStringLiteral("launchers/test"));
    QVERIFY(QDir().mkpath(install));
    const QString game = QDir(root.path()).filePath(QStringLiteral("library/Test (USA).z64"));
    QVERIFY(writeFile(game, rom));
    LaunchPreparer preparer;
    LaunchPreparer::Options opts = options(root);
    QCOMPARE(preparer.readiness(project, game, install, opts).value("state").toString(), QStringLiteral("ready"));
    preparer.prepare(project, game, install, opts);
    QCOMPARE(readFile(QDir(install).filePath(QStringLiteral("data/test.ntsc-final.z64"))), rom);

    // Another ROM is not copied.
    QVERIFY(QFile::remove(QDir(install).filePath(QStringLiteral("data/test.ntsc-final.z64"))));
    QVERIFY(writeFile(game, makeRom("TEST GAME", 'E', 1)));
    preparer.prepare(project, game, install, opts);
    QVERIFY(!QFileInfo::exists(QDir(install).filePath(QStringLiteral("data/test.ntsc-final.z64"))));
}

void LaunchPreparerTest::selectFileKeyValueInSdlFolders()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVariantMap project = discProject();
    project.insert(QStringLiteral("launch"), QVariantMap{
        {QStringLiteral("method"), QStringLiteral("selectfile")},
        {QStringLiteral("selectFiles"), QVariantList{
            QVariantMap{{QStringLiteral("folder"), QStringLiteral("userdata")}, {QStringLiteral("appName"), QStringLiteral("game")},
                        {QStringLiteral("appOrg"), QStringLiteral("org")},
                        {QStringLiteral("portableMarker"), QStringLiteral("portable.txt")},
                        {QStringLiteral("markerBesideAppImage"), true},
                        {QStringLiteral("file"), QStringLiteral("app.ini")},
                        {QStringLiteral("format"), QStringLiteral("keyvalue")}, {QStringLiteral("key"), QStringLiteral("rom_path")}}}}});
    const QString game = QDir(root.path()).filePath(QStringLiteral("library/Test.iso"));
    QVERIFY(writeFile(game, QByteArray("disc")));
    const QString install = QDir(root.path()).filePath(QStringLiteral("launchers/test"));
    QVERIFY(QDir().mkpath(install));
    LaunchPreparer preparer;

    // Linux: SDL leaves the organization out. Other lines stay.
    LaunchPreparer::Options opts = options(root);
    opts.executablePath = QDir(install).filePath(QStringLiteral("Test.AppImage"));
    const QString linuxFile = QDir(opts.dataHome).filePath(QStringLiteral("game/app.ini"));
    QVERIFY(writeFile(linuxFile, QByteArray("volume=5\nrom_path=/old.iso\n")));
    preparer.prepare(project, game, install, opts);
    QCOMPARE(readFile(linuxFile), QByteArray("volume=5\nrom_path=") + game.toUtf8() + QByteArray("\n"));

    // This launcher looks for portable.txt beside the AppImage file too.
    QVERIFY(writeFile(QDir(install).filePath(QStringLiteral("portable.txt")), QByteArray()));
    preparer.prepare(project, game, install, opts);
    QCOMPARE(readFile(QDir(install).filePath(QStringLiteral("app.ini"))), QByteArray("rom_path=") + game.toUtf8() + QByteArray("\n"));
    QVERIFY(QFile::remove(QDir(install).filePath(QStringLiteral("portable.txt"))));

    // Windows: %APPDATA%\org\game, with backslashes escaped.
    opts.system = QStringLiteral("windows");
    opts.appData = QDir(root.path()).filePath(QStringLiteral("AppData/Roaming"));
    opts.executablePath = QDir(install).filePath(QStringLiteral("Test.exe"));
    opts.winePaths = true;
    preparer.prepare(project, game, install, opts);
    const QString wine = QStringLiteral("Z:") + QString(game).replace(QLatin1Char('/'), QLatin1Char('\\'));
    QCOMPARE(readFile(QDir(opts.appData).filePath(QStringLiteral("org/game/app.ini"))),
             QByteArray("rom_path=") + QString(wine).replace(QStringLiteral("\\"), QStringLiteral("\\\\")).toUtf8() + QByteArray("\n"));
}

void LaunchPreparerTest::discArgumentPassesTheGameFile()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    LaunchPreparer::Options opts = options(root);
    const QString disc = QDir(root.path()).filePath(QStringLiteral("library/Test Mansion (USA).rvz"));
    QVERIFY(writeFile(disc, QByteArray("disc image")));
    LaunchPreparer preparer;

    const QVariantMap readiness = preparer.readiness(discProject(), disc, QString(), opts);
    QCOMPARE(readiness.value("state").toString(), QStringLiteral("ready"));
    QVERIFY(readiness.value("detail").toString().contains(QStringLiteral("first start")));
    LaunchPreparer::Plan plan = preparer.prepare(discProject(), disc, QString(), opts);
    QCOMPARE(plan.arguments, (QStringList{QStringLiteral("--disc"), disc}));
    QVERIFY(plan.environment.isEmpty());
    QCOMPARE(readFile(disc), QByteArray("disc image")); // nothing copied or changed

    // A format the launcher doesn't read, or preparing off: no arguments.
    const QString zip = QDir(root.path()).filePath(QStringLiteral("library/Test Mansion.zip"));
    QVERIFY(writeFile(zip, QByteArray("zip")));
    QCOMPARE(preparer.readiness(discProject(), zip, QString(), opts).value("state").toString(),
             QStringLiteral("wrong_file"));
    QVERIFY(preparer.prepare(discProject(), zip, QString(), opts).arguments.isEmpty());
    opts.prepareFiles = false;
    QVERIFY(preparer.prepare(discProject(), disc, QString(), opts).arguments.isEmpty());
}

void LaunchPreparerTest::fileArgumentPassesTheRom()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    LaunchPreparer::Options opts = options(root);
    QVariantMap project = discProject();
    project.insert(QStringLiteral("launch"), QVariantMap{
        {QStringLiteral("method"), QStringLiteral("fileargument")},
        {QStringLiteral("arguments"), QStringList{QStringLiteral("--rom"), QStringLiteral("{gamePath}")}},
        {QStringLiteral("fileExtensions"), QStringList{QStringLiteral("sfc"), QStringLiteral("smc")}}});
    const QString rom = QDir(root.path()).filePath(QStringLiteral("library/Test (Japan, USA) (En,Ja).sfc"));
    QVERIFY(writeFile(rom, QByteArray("rom")));
    LaunchPreparer preparer;
    QCOMPARE(preparer.readiness(project, rom, QString(), opts).value("state").toString(), QStringLiteral("ready"));
    QCOMPARE(preparer.prepare(project, rom, QString(), opts).arguments, (QStringList{QStringLiteral("--rom"), rom}));
    const QString other = QDir(root.path()).filePath(QStringLiteral("library/Test.zip"));
    QVERIFY(writeFile(other, QByteArray("zip")));
    const QVariantMap wrong = preparer.readiness(project, other, QString(), opts);
    QCOMPARE(wrong.value("state").toString(), QStringLiteral("wrong_file"));
    QVERIFY(wrong.value("detail").toString().contains(QStringLiteral("sfc, smc files")));
}

void LaunchPreparerTest::selectFileChoosesTheGameWithoutStartingIt()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    LaunchPreparer::Options opts = options(root);
    const QString install = QDir(root.path()).filePath(QStringLiteral("launchers/test"));
    QVERIFY(QDir().mkpath(install));
    opts.executablePath = QDir(install).filePath(QStringLiteral("Test.AppImage"));
    QVERIFY(writeFile(QDir(install).filePath(QStringLiteral("portable.txt")), QByteArray()));
    QVariantMap project = discProject();
    project.insert(QStringLiteral("launch"), QVariantMap{
        {QStringLiteral("method"), QStringLiteral("selectfile")},
        {QStringLiteral("arguments"), QStringList{QStringLiteral("--launcher")}},
        {QStringLiteral("fileExtensions"), QStringList{QStringLiteral("iso")}},
        {QStringLiteral("selectFiles"), QVariantList{
            QVariantMap{{QStringLiteral("folder"), QStringLiteral("launcher")}, {QStringLiteral("file"), QStringLiteral("rom.cfg")},
                        {QStringLiteral("format"), QStringLiteral("firstline")}},
            QVariantMap{{QStringLiteral("folder"), QStringLiteral("userdata")}, {QStringLiteral("appName"), QStringLiteral("test")},
                        {QStringLiteral("portableMarker"), QStringLiteral("portable.txt")},
                        {QStringLiteral("portableSubfolder"), QStringLiteral("user")},
                        {QStringLiteral("file"), QStringLiteral("launcher.cfg")},
                        {QStringLiteral("format"), QStringLiteral("quotedkey")}, {QStringLiteral("key"), QStringLiteral("disc")}},
            QVariantMap{{QStringLiteral("folder"), QStringLiteral("userdata")}, {QStringLiteral("appName"), QStringLiteral("test")},
                        {QStringLiteral("file"), QStringLiteral("library.txt")},
                        {QStringLiteral("format"), QStringLiteral("listline")}}}}});
    const QString disc = QDir(root.path()).filePath(QStringLiteral("library/Test Disc.iso"));
    QVERIFY(writeFile(disc, QByteArray("disc")));
    // The launcher's own settings stay as they were, except the disc line.
    const QString settingsFolder = QDir(opts.dataHome).filePath(QStringLiteral("test"));
    QVERIFY(writeFile(QDir(settingsFolder).filePath(QStringLiteral("launcher.cfg")),
                      QByteArray("vsync 1\ndisc \"/old.iso\"\nfullscreen 0\n")));
    QVERIFY(writeFile(QDir(settingsFolder).filePath(QStringLiteral("library.txt")), QByteArray("/other.iso\r\n")));

    LaunchPreparer preparer;
    const QVariantMap ready = preparer.readiness(project, disc, install, opts);
    QCOMPARE(ready.value("state").toString(), QStringLiteral("ready"));
    QVERIFY(ready.value("detail").toString().contains(QStringLiteral("already selected")));
    const LaunchPreparer::Plan plan = preparer.prepare(project, disc, install, opts);
    QCOMPARE(plan.arguments, QStringList{QStringLiteral("--launcher")}); // never the game itself
    QCOMPARE(readFile(QDir(install).filePath(QStringLiteral("rom.cfg"))), (disc + QLatin1Char('\n')).toUtf8());
    // An AppImage's portable marker isn't seen by the launcher: the SDL folder is used.
    QCOMPARE(readFile(QDir(settingsFolder).filePath(QStringLiteral("launcher.cfg"))),
             QByteArray("vsync 1\ndisc \"") + disc.toUtf8() + QByteArray("\"\nfullscreen 0\n"));
    QCOMPARE(readFile(QDir(settingsFolder).filePath(QStringLiteral("library.txt"))),
             QByteArray("/other.iso\r\n") + disc.toUtf8() + QByteArray("\n"));
    QCOMPARE(plan.steps.size(), 1);
    QCOMPARE(plan.steps.first().toMap().value("title").toString(), QStringLiteral("Selected your copy in the launcher"));

    // Starting again changes nothing: the disc is listed once.
    preparer.prepare(project, disc, install, opts);
    QCOMPARE(readFile(QDir(settingsFolder).filePath(QStringLiteral("library.txt"))),
             QByteArray("/other.iso\r\n") + disc.toUtf8() + QByteArray("\n"));

    // A portable copy (not an AppImage) keeps its settings beside the program.
    opts.executablePath = QDir(install).filePath(QStringLiteral("test"));
    preparer.prepare(project, disc, install, opts);
    QVERIFY(QFileInfo::exists(QDir(install).filePath(QStringLiteral("user/launcher.cfg"))));

    // Preparing off, or a file it doesn't read: nothing written, the launcher still opens.
    const QString other = QDir(root.path()).filePath(QStringLiteral("library/Test.zip"));
    QVERIFY(writeFile(other, QByteArray("zip")));
    QVERIFY(QFile::remove(QDir(install).filePath(QStringLiteral("rom.cfg"))));
    QCOMPARE(preparer.prepare(project, other, install, opts).arguments, QStringList{QStringLiteral("--launcher")});
    opts.prepareFiles = false;
    QVERIFY(preparer.prepare(project, disc, install, opts).steps.isEmpty());
    QVERIFY(!QFileInfo::exists(QDir(install).filePath(QStringLiteral("rom.cfg"))));
}

void LaunchPreparerTest::selectFileWritesWindowsAndWinePaths()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    LaunchPreparer::Options opts = options(root);
    opts.system = QStringLiteral("windows");
    opts.appData = QDir(root.path()).filePath(QStringLiteral("AppData/Roaming"));
    const QString install = QDir(root.path()).filePath(QStringLiteral("launchers/test"));
    QVERIFY(QDir().mkpath(install));
    opts.executablePath = QDir(install).filePath(QStringLiteral("Test.exe"));
    QVariantMap project = discProject();
    project.insert(QStringLiteral("launch"), QVariantMap{
        {QStringLiteral("method"), QStringLiteral("selectfile")},
        {QStringLiteral("selectFiles"), QVariantList{
            QVariantMap{{QStringLiteral("folder"), QStringLiteral("userdata")}, {QStringLiteral("appName"), QStringLiteral("test")},
                        {QStringLiteral("file"), QStringLiteral("last.txt")}, {QStringLiteral("format"), QStringLiteral("whole")}},
            QVariantMap{{QStringLiteral("folder"), QStringLiteral("launcher")}, {QStringLiteral("file"), QStringLiteral("launcher.cfg")},
                        {QStringLiteral("format"), QStringLiteral("quotedkey")}, {QStringLiteral("key"), QStringLiteral("disc")}}}}});
    const QString game = QDir(root.path()).filePath(QStringLiteral("library/Test.iso"));
    QVERIFY(writeFile(game, QByteArray("disc")));
    LaunchPreparer preparer;
    const QString last = QDir(opts.appData).filePath(QStringLiteral("test/last.txt"));
    preparer.prepare(project, game, install, opts);
    QCOMPARE(readFile(last), QDir::toNativeSeparators(game).toUtf8());

    // Through Wine: Wine's Z: drive, and file arguments the same way.
    opts.winePaths = true;
    preparer.prepare(project, game, install, opts);
    const QString wine = QStringLiteral("Z:") + QString(game).replace(QLatin1Char('/'), QLatin1Char('\\'));
    QCOMPARE(readFile(last), wine.toUtf8());
    // Backslashes escaped the way the launcher reads them back (std::quoted).
    QCOMPARE(readFile(QDir(install).filePath(QStringLiteral("launcher.cfg"))),
             QByteArray("disc \"") + QString(wine).replace(QStringLiteral("\\"), QStringLiteral("\\\\")).toUtf8()
                 + QByteArray("\"\n"));
    project.insert(QStringLiteral("launch"), QVariantMap{
        {QStringLiteral("method"), QStringLiteral("fileargument")},
        {QStringLiteral("arguments"), QStringList{QStringLiteral("{gamePath}")}}});
    QCOMPARE(preparer.prepare(project, game, install, opts).arguments, QStringList{wine});
}

void LaunchPreparerTest::backgroundReadsNeverReadOnTheCallersThread()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QByteArray rom = makeRom("TEST GAME", 'E');
    const QVariantMap project = n64Project(RomInspector::inspectN64(rom).xxh3Hex());
    LaunchPreparer::Options opts = options(root);
    const QString game = QDir(root.path()).filePath(QStringLiteral("library/Test (USA).z64"));
    QVERIFY(writeFile(game, rom));

    LaunchPreparer preparer;
    preparer.setBackgroundReads(true);
    QCOMPARE(preparer.readiness(project, game, QString(), opts).value("state").toString(),
             QStringLiteral("checking"));
    // Asked twice before the answer: listed once.
    preparer.readiness(project, game, QString(), opts);
    const QStringList pending = preparer.takePendingReads();
    QCOMPARE(pending, QStringList{game});
    QVERIFY(preparer.takePendingReads().isEmpty());

    // The caller reads it elsewhere and hands it back.
    preparer.storeInspection(game, RomInspector::inspectN64File(game));
    QCOMPARE(preparer.readiness(project, game, QString(), opts).value("state").toString(),
             QStringLiteral("ready"));
    QVERIFY(preparer.takePendingReads().isEmpty());

    // A changed file is read again.
    QVERIFY(writeFile(game, makeRom("OTHER GAME", 'E', 0, 'y')));
    QFile touched(game);
    QVERIFY(touched.open(QIODevice::ReadWrite));
    QVERIFY(touched.setFileTime(QDateTime::currentDateTime().addSecs(5), QFileDevice::FileModificationTime));
    touched.close();
    QCOMPARE(preparer.readiness(project, game, QString(), opts).value("state").toString(),
             QStringLiteral("checking"));
}


// On Windows: N64Recomp launchers and ROM-folder launchers use
// %LOCALAPPDATA%\<folder>; HarbourMasters launchers keep everything in their
// own folder (no SHIP_HOME), and get the copy there.
void LaunchPreparerTest::windowsFolderRules()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    LaunchPreparer::Options opts = options(root);
    opts.system = QStringLiteral("windows");
    opts.localAppData = QDir(root.path()).filePath(QStringLiteral("LocalAppData"));
    const QByteArray rom = makeRom("TEST GAME", 'E');
    const QString game = QDir(root.path()).filePath(QStringLiteral("library/Test (USA).z64"));
    QVERIFY(writeFile(game, rom));
    LaunchPreparer preparer;

    const QVariantMap n64 = n64Project(RomInspector::inspectN64(rom).xxh3Hex());
    QCOMPARE(LaunchPreparer::dataFolder(n64.value("launch").toMap(), QString(), opts),
             QDir(opts.localAppData).filePath(QStringLiteral("TestRecompiled")));
    QVERIFY(preparer.prepare(n64, game, QString(), opts).ok);
    QCOMPARE(readFile(QDir(opts.localAppData).filePath(QStringLiteral("TestRecompiled/test.n64.us.1.0.z64"))), rom);

    const QString program = QDir(root.path()).filePath(QStringLiteral("launchers/test-ship/Ship"));
    QVERIFY(writeFile(QDir(program).filePath(QStringLiteral("Ship.exe")), QByteArray("MZ")));
    const LaunchPreparer::Plan plan = preparer.prepare(shipProject(), game, program, opts);
    QVERIFY(plan.ok);
    QVERIFY(!plan.environment.contains(QStringLiteral("SHIP_HOME")));
    QCOMPARE(readFile(QDir(program).filePath(QStringLiteral("baserom.us.z64"))), rom);
    QCOMPARE(preparer.readiness(shipProject(), game, program, opts).value("dataFolder").toString(), program);
}

QTEST_GUILESS_MAIN(LaunchPreparerTest)
#include "LaunchPreparerTest.moc"
