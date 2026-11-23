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
    const LaunchPreparer::Options opts = options(root);
    const QString game = QDir(root.path()).filePath(QStringLiteral("library/Test (USA).v64"));
    QVERIFY(writeFile(game, byteSwapped(rom)));
    const QByteArray original = readFile(game);

    LaunchPreparer preparer;
    LaunchPreparer::Plan plan = preparer.prepare(project, game, QString(), opts);
    QVERIFY(plan.ok);
    QVERIFY(plan.environment.isEmpty());
    QCOMPARE(plan.steps.size(), 3);

    const QString folder = QDir(opts.homeDirectory).filePath(QStringLiteral(".config/TestRecompiled"));
    const QString stored = QDir(folder).filePath(QStringLiteral("test.n64.us.1.0.z64"));
    QCOMPARE(readFile(stored), rom); // big-endian, as the launcher stores it
    QCOMPARE(readFile(game), original); // the library file is only read

    const QJsonObject graphics = QJsonDocument::fromJson(
        readFile(QDir(folder).filePath(QStringLiteral("graphics.json")))).object();
    QCOMPARE(graphics.value("wm_option").toString(), QStringLiteral("Fullscreen"));
    QCOMPARE(graphics.size(), 1);

    // Second start: nothing is replaced.
    QVERIFY(writeFile(stored, QByteArray("launcher's own copy")));
    QVERIFY(writeFile(QDir(folder).filePath(QStringLiteral("graphics.json")),
                      QByteArray("{\"wm_option\":\"Windowed\",\"res_option\":\"Original\"}")));
    plan = preparer.prepare(project, game, QString(), opts);
    QCOMPARE(readFile(stored), QByteArray("launcher's own copy"));
    QCOMPARE(QJsonDocument::fromJson(readFile(QDir(folder).filePath(QStringLiteral("graphics.json"))))
                 .object().value("wm_option").toString(),
             QStringLiteral("Windowed"));
    QCOMPARE(plan.steps.size(), 1);
}

void LaunchPreparerTest::windowModeChangesOnlyThatSetting()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString path = QDir(root.path()).filePath(QStringLiteral("graphics.json"));
    LaunchPreparer::Options opts;

    opts.fullscreen = false;
    QVERIFY(!LaunchPreparer::applyWindowMode(path, opts));
    QVERIFY(!QFileInfo::exists(path));

    QVERIFY(writeFile(path, QByteArray("{\"wm_option\":\"Fullscreen\",\"ar_option\":\"Expand\"}")));
    opts.windowMode = QStringLiteral("windowed");
    QString summary;
    QVERIFY(LaunchPreparer::applyWindowMode(path, opts, &summary));
    const QJsonObject object = QJsonDocument::fromJson(readFile(path)).object();
    QCOMPARE(object.value("wm_option").toString(), QStringLiteral("Windowed"));
    QCOMPARE(object.value("ar_option").toString(), QStringLiteral("Expand"));
    QVERIFY(!summary.isEmpty());
    QVERIFY(!LaunchPreparer::applyWindowMode(path, opts)); // already windowed

    QVERIFY(writeFile(path, QByteArray("not json")));
    opts.windowMode = QStringLiteral("fullscreen");
    QVERIFY(!LaunchPreparer::applyWindowMode(path, opts));
    QCOMPARE(readFile(path), QByteArray("not json"));
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

QTEST_GUILESS_MAIN(LaunchPreparerTest)
#include "LaunchPreparerTest.moc"
