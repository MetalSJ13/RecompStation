#include <QtTest>

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
    void readinessDescribesEachSituation();
    void preparesN64RecompLauncherOnce();
    void windowModeChangesOnlyThatSetting();
    void portableLauncherUsesItsInstallFolder();
    void doesNotCopyMismatchedRom();
    void preparesHarbourMastersPort();
    void harbourMastersSavesStayOutsideInstallWhenPreparingIsOff();
    void romFolderPointsTheLauncherAtItsDataFolder();
    void romFolderPassesThePlacedRomOnlyWhenItMatches();
    void discArgumentPassesTheGameFile();
    void backgroundReadsNeverReadOnTheCallersThread();

private:
    LaunchPreparer::Options options(const QTemporaryDir &root) const
    {
        LaunchPreparer::Options result;
        result.homeDirectory = QDir(root.path()).filePath(QStringLiteral("home"));
        result.dataHome = QDir(root.path()).filePath(QStringLiteral("home/.local/share"));
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

QTEST_GUILESS_MAIN(LaunchPreparerTest)
#include "LaunchPreparerTest.moc"
