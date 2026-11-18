#include <QtTest>

#include "core/GameLaunchManager.h"
#include "core/LauncherDownloadManager.h"
#include "core/PlatformManager.h"

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
        + "\"\nsleep 1\n");
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
        + "\"\nsleep 1\n");
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

QTEST_GUILESS_MAIN(GameLaunchManagerTest)
#include "GameLaunchManagerTest.moc"
