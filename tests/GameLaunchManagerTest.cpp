#include <QtTest>

#include "core/GameLaunchManager.h"
#include "core/LauncherDownloadManager.h"
#include "core/PlatformManager.h"
#include "core/SettingsManager.h"
#include "core/WineSupport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

class GameLaunchManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void launchesManagedExecutableAndTracksProcess();
    void launchesShellScriptRelativeToInstallationDirectory();
    void startsWindowsLauncherThroughWine();
};

void GameLaunchManagerTest::launchesManagedExecutableAndTracksProcess()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString projectId = QStringLiteral("test-launcher-project");
    const QString installDirectory = QDir(platformManager.dataDirectory())
                                         .filePath("launchers/" + projectId);
    QVERIFY(QDir().mkpath(installDirectory));

    QFile marker(QDir(installDirectory).filePath(".recompstation-managed"));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    QCOMPARE(marker.write("RecompStation managed launcher installation v1\n"), qint64(47));
    marker.close();

    QFile manifest(QDir(installDirectory).filePath(".recompstation-install"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    QCOMPARE(manifest.write("launcher.sh\n"), qint64(12));
    manifest.close();

    const QString executablePath = QDir(installDirectory).filePath("launcher.sh");
    QFile executable(executablePath);
    QVERIFY(executable.open(QIODevice::WriteOnly));
    const QString invokedPathFile = QDir(installDirectory).filePath("invoked-path.txt");
    const QByteArray script(
        "#!/bin/sh\nprintf '%s' \"$0\" > \"" + invokedPathFile.toUtf8()
        + "\"\nsleep 0.3\n");
    QCOMPARE(executable.write(script), qint64(script.size()));
    executable.close();
    QVERIFY(executable.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                                      | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                      | QFileDevice::ExeOther));

    LauncherDownloadManager downloadManager(&platformManager);
    GameLaunchManager manager(&downloadManager);
    QSignalSpy startedSpy(&manager, &GameLaunchManager::launchStarted);
    QSignalSpy finishedSpy(&manager, &GameLaunchManager::processFinished);
    QVERIFY(startedSpy.isValid());
    QVERIFY(finishedSpy.isValid());

    QVERIFY(manager.launchProject(projectId));
    QTRY_COMPARE_WITH_TIMEOUT(startedSpy.count(), 1, 5000);
    QVERIFY(manager.isRunning(projectId));
    QVERIFY(!manager.launchProject(projectId));

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    QVERIFY(!manager.isRunning(projectId));
    QCOMPARE(finishedSpy.first().at(1).toInt(), 0);
    QCOMPARE(finishedSpy.first().at(2).toBool(), false);

    QFile invokedPath(invokedPathFile);
    QVERIFY(invokedPath.open(QIODevice::ReadOnly));
    QCOMPARE(invokedPath.readAll(), QByteArray("./launcher.sh"));
}

void GameLaunchManagerTest::launchesShellScriptRelativeToInstallationDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString projectId = QStringLiteral("test-shell-script-project");
    const QString installDirectory = QDir(platformManager.dataDirectory())
                                         .filePath("launchers/" + projectId);
    QVERIFY(QDir().mkpath(installDirectory));

    QFile marker(QDir(installDirectory).filePath(".recompstation-managed"));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    QCOMPARE(marker.write("RecompStation managed launcher installation v1\n"), qint64(47));
    marker.close();

    QFile manifest(QDir(installDirectory).filePath(".recompstation-install"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    QCOMPARE(manifest.write("run.sh\n"), qint64(7));
    manifest.close();

    const QString scriptPath = QDir(installDirectory).filePath("run.sh");
    const QString invokedPathFile = QDir(installDirectory).filePath("invoked-path.txt");
    QFile scriptFile(scriptPath);
    QVERIFY(scriptFile.open(QIODevice::WriteOnly));
    const QByteArray script(
        "#!/bin/sh\nprintf '%s' \"$0\" > \"" + invokedPathFile.toUtf8()
        + "\"\nsleep 0.3\n");
    QCOMPARE(scriptFile.write(script), qint64(script.size()));
    scriptFile.close();
    QVERIFY(scriptFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                                      | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                      | QFileDevice::ExeOther));

    LauncherDownloadManager downloadManager(&platformManager);
    GameLaunchManager manager(&downloadManager);
    QSignalSpy startedSpy(&manager, &GameLaunchManager::launchStarted);
    QSignalSpy finishedSpy(&manager, &GameLaunchManager::processFinished);
    QVERIFY(startedSpy.isValid());
    QVERIFY(finishedSpy.isValid());

    QVERIFY(manager.launchProject(projectId));
    QTRY_COMPARE_WITH_TIMEOUT(startedSpy.count(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    QCOMPARE(finishedSpy.first().at(1).toInt(), 0);

    QFile invokedPath(invokedPathFile);
    QVERIFY(invokedPath.open(QIODevice::ReadOnly));
    QCOMPARE(invokedPath.readAll(), QByteArray("./run.sh"));
}


// A Windows launcher starts as "wine <program>.exe" from the program's own
// folder, and only with the Wine switch on and Wine found.
void GameLaunchManagerTest::startsWindowsLauncherThroughWine()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString projectId = QStringLiteral("test-windows-project");
    const QString installDirectory = QDir(platformManager.dataDirectory()).filePath("launchers/" + projectId);
    QVERIFY(QDir().mkpath(installDirectory + QStringLiteral("/bin")));
    auto write = [](const QString &path, const QByteArray &data) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    };
    QVERIFY(write(installDirectory + "/.recompstation-managed", "RecompStation managed launcher installation v1\n"));
    QVERIFY(write(installDirectory + "/.recompstation-install", "bin/Game.exe\n"));
    QVERIFY(write(installDirectory + "/.recompstation-release", R"({"tag":"v1","build":"windows-x86_64"})"));
    QVERIFY(write(installDirectory + "/bin/Game.exe", "MZ"));

    // A fake Wine: answers --version, and otherwise records how it was run.
    QTemporaryDir wineDirectory;
    const QString record = QDir(wineDirectory.path()).filePath("ran.txt");
    const QString winePath = QDir(wineDirectory.path()).filePath("wine");
    QVERIFY(write(winePath, "#!/bin/sh\nif [ \"$1\" = --version ]; then echo wine-10.0; exit 0; fi\n"
                            "printf '%s\\n%s\\n' \"$(pwd -P)\" \"$1\" > \"" + record.toUtf8() + "\"\n"));
    QVERIFY(QFile::setPermissions(winePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

    SettingsManager settings(QDir(temporaryDirectory.path()).filePath("settings.ini"));
    WineSupport wine;
    wine.setSupportedForTesting(true);
    wine.setSearchPaths({wineDirectory.path()});
    QSignalSpy checked(&wine, &WineSupport::checked);
    wine.refresh();
    QVERIFY(checked.wait(5000));
    QVERIFY(wine.isAvailable());

    LauncherDownloadManager downloadManager(&platformManager);
    GameLaunchManager manager(&downloadManager);
    manager.setSettingsManager(&settings);
    manager.setWineSupport(&wine);
    QSignalSpy failedSpy(&manager, &GameLaunchManager::launchFailed);
    QSignalSpy finishedSpy(&manager, &GameLaunchManager::processFinished);

    // The switch is off by default: not started, and the reason is given.
    QVERIFY(!manager.launchProject(projectId));
    QCOMPARE(failedSpy.count(), 1);
    QVERIFY(failedSpy.first().at(1).toString().contains(QStringLiteral("Windows launchers through Wine")));

    settings.setWindowsLaunchersThroughWine(true);
    QVERIFY(manager.launchProject(projectId));
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
    QFile ran(record);
    QVERIFY(ran.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = ran.readAll().split('\n');
    QVERIFY(lines.size() >= 2);
    QCOMPARE(QString::fromUtf8(lines.at(0)), QFileInfo(installDirectory + "/bin").canonicalFilePath());
    QVERIFY(QString::fromUtf8(lines.at(1)).endsWith(QStringLiteral("/bin/Game.exe")));
}

QTEST_GUILESS_MAIN(GameLaunchManagerTest)
#include "GameLaunchManagerTest.moc"
