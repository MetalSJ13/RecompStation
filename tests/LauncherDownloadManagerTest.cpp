#include <QtTest>

#include "core/LauncherDownloadManager.h"
#include "core/PlatformManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <archive.h>
#include <archive_entry.h>

class LauncherDownloadManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsInvalidProjectIdentifiers();
    void rejectsUnsafeAssetNames();
    void rejectsNonHttpsUrls();
    void rejectsOversizedAssets();
    void rejectsSymlinkedStagingRoot();
    void acceptsValidRequestAndReportsMissingNetworkDestination();
    void installsAndUninstallsManagedLauncher();
    void installsLauncherFromNestedArchiveDirectory();
    void installsZipLauncherWithCompanionFiles();
    void installsStandaloneAppImage();
};

void LauncherDownloadManagerTest::rejectsInvalidProjectIdentifiers()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());
    LauncherDownloadManager manager(&platformManager);
    QSignalSpy failureSpy(&manager, &LauncherDownloadManager::downloadFailed);
    QVERIFY(failureSpy.isValid());

    const int requestId = manager.downloadAsset(
        QStringLiteral("../outside"),
        QStringLiteral("launcher.tar.gz"),
        QStringLiteral("https://example.com/launcher.tar.gz")
    );

    QTRY_COMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.takeFirst().at(0).toInt(), requestId);
    QVERIFY(!QFileInfo::exists(
        QDir(temporaryDirectory.path()).filePath("outside")));
}

void LauncherDownloadManagerTest::rejectsUnsafeAssetNames()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());
    LauncherDownloadManager manager(&platformManager);
    QSignalSpy failureSpy(&manager, &LauncherDownloadManager::downloadFailed);

    const QStringList invalidNames = {
        QString(),
        ".",
        "..",
        "../launcher.tar.gz",
        "folder/launcher.tar.gz",
        "folder\\launcher.tar.gz",
        "launcher name.tar.gz"
    };

    for (const QString &name : invalidNames) {
        const int requestId = manager.downloadAsset(
            QStringLiteral("banjo-kazooie-recompiled"),
            name,
            QStringLiteral("https://example.com/launcher.tar.gz")
        );
        QTRY_VERIFY(failureSpy.count() > 0);
        QCOMPARE(failureSpy.takeFirst().at(0).toInt(), requestId);
    }
}

void LauncherDownloadManagerTest::rejectsNonHttpsUrls()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());
    LauncherDownloadManager manager(&platformManager);
    QSignalSpy failureSpy(&manager, &LauncherDownloadManager::downloadFailed);

    const int requestId = manager.downloadAsset(
        QStringLiteral("banjo-kazooie-recompiled"),
        QStringLiteral("launcher.tar.gz"),
        QStringLiteral("http://example.com/launcher.tar.gz")
    );

    QTRY_COMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.takeFirst().at(0).toInt(), requestId);
}

void LauncherDownloadManagerTest::rejectsOversizedAssets()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());
    LauncherDownloadManager manager(&platformManager);
    QSignalSpy failureSpy(&manager, &LauncherDownloadManager::downloadFailed);

    const int requestId = manager.downloadAsset(
        QStringLiteral("banjo-kazooie-recompiled"),
        QStringLiteral("launcher.tar.gz"),
        QStringLiteral("https://example.com/launcher.tar.gz"),
        9LL * 1024LL * 1024LL * 1024LL
    );

    QTRY_COMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.takeFirst().at(0).toInt(), requestId);
}

void LauncherDownloadManagerTest::rejectsSymlinkedStagingRoot()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString stagingPath = platformManager.stagingDirectory();
    QVERIFY(QDir(stagingPath).removeRecursively());

    const QString outsidePath = QDir(temporaryDirectory.path())
                                    .filePath("outside-staging");
    QVERIFY(QDir().mkpath(outsidePath));
    QVERIFY(QFile::link(outsidePath, stagingPath));

    LauncherDownloadManager manager(&platformManager);
    QSignalSpy failureSpy(&manager, &LauncherDownloadManager::downloadFailed);
    const int requestId = manager.downloadAsset(
        QStringLiteral("banjo-kazooie-recompiled"),
        QStringLiteral("launcher.tar.gz"),
        QStringLiteral("https://example.com/launcher.tar.gz")
    );

    QTRY_COMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.takeFirst().at(0).toInt(), requestId);
    QVERIFY(!QFileInfo::exists(
        QDir(outsidePath).filePath("banjo-kazooie-recompiled")));
}

void LauncherDownloadManagerTest::acceptsValidRequestAndReportsMissingNetworkDestination()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());
    LauncherDownloadManager manager(&platformManager);
    QSignalSpy failureSpy(&manager, &LauncherDownloadManager::downloadFailed);
    QVERIFY(failureSpy.isValid());

    const int requestId = manager.downloadAsset(
        QStringLiteral("banjo-kazooie-recompiled"),
        QStringLiteral("launcher.tar.gz"),
        QStringLiteral("https://127.0.0.1:1/launcher.tar.gz")
    );

    QTRY_COMPARE_WITH_TIMEOUT(failureSpy.count(), 1, 10000);
    QCOMPARE(failureSpy.takeFirst().at(0).toInt(), requestId);

    const QString projectStaging = QDir(platformManager.stagingDirectory())
                                       .filePath("banjo-kazooie-recompiled");
    QVERIFY(QFileInfo(projectStaging).isDir());
    QVERIFY(!QFileInfo::exists(
        QDir(projectStaging).filePath("launcher.tar.gz")));
}


void LauncherDownloadManagerTest::installsAndUninstallsManagedLauncher()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString projectId = QStringLiteral("sample-recomp-project");
    const QString assetName = QStringLiteral("SampleLauncher-Linux-X64.tar.gz");
    const QString stagingDirectory = QDir(platformManager.stagingDirectory())
                                         .filePath(projectId);
    QVERIFY(QDir().mkpath(stagingDirectory));
    const QString archivePath = QDir(stagingDirectory).filePath(assetName);

    struct archive *writer = archive_write_new();
    QVERIFY(writer != nullptr);
    QCOMPARE(archive_write_add_filter_gzip(writer), ARCHIVE_OK);
    QCOMPARE(archive_write_set_format_pax_restricted(writer), ARCHIVE_OK);
    const QByteArray archivePathBytes = QFile::encodeName(archivePath);
    QCOMPARE(archive_write_open_filename(writer, archivePathBytes.constData()), ARCHIVE_OK);

    const QByteArray executableContents("#!/bin/sh\necho RecompStation test launcher\n");
    struct archive_entry *entry = archive_entry_new();
    QVERIFY(entry != nullptr);
    archive_entry_set_pathname(entry, "SampleLauncher");
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0755);
    archive_entry_set_size(entry, executableContents.size());
    QCOMPARE(archive_write_header(writer, entry), ARCHIVE_OK);
    QCOMPARE(archive_write_data(writer, executableContents.constData(),
                               executableContents.size()),
             static_cast<la_ssize_t>(executableContents.size()));
    archive_entry_free(entry);
    QCOMPARE(archive_write_close(writer), ARCHIVE_OK);
    QCOMPARE(archive_write_free(writer), ARCHIVE_OK);
    QVERIFY(QFileInfo(archivePath).isFile());

    LauncherDownloadManager manager(&platformManager);
    QSignalSpy completedSpy(&manager, &LauncherDownloadManager::installationCompleted);
    QSignalSpy failedSpy(&manager, &LauncherDownloadManager::installationFailed);
    QVERIFY(completedSpy.isValid());
    QVERIFY(failedSpy.isValid());

    const int requestId = manager.installDownloadedAsset(
        projectId, assetName, QStringLiteral("tar.gz"), QStringLiteral("SampleLauncher"));
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 10000);
    QCOMPARE(completedSpy.takeFirst().at(0).toInt(), requestId);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(!QFileInfo::exists(archivePath));

    const QVariantList installed = manager.installedLaunchers();
    QCOMPARE(installed.size(), 1);
    const QVariantMap installation = installed.first().toMap();
    QCOMPARE(installation.value(QStringLiteral("projectId")).toString(), projectId);
    const QString executablePath =
        installation.value(QStringLiteral("executablePath")).toString();
    QVERIFY(QFileInfo(executablePath).isFile());
    QVERIFY(QFileInfo(executablePath).isExecutable());

    LauncherDownloadManager restartedManager(&platformManager);
    QCOMPARE(restartedManager.installedLaunchers().size(), 1);

    QFile replacementArchive(archivePath);
    QVERIFY(replacementArchive.open(QIODevice::WriteOnly));
    const QByteArray replacementContents("do not replace the existing launcher");
    QCOMPARE(replacementArchive.write(replacementContents), qint64(replacementContents.size()));
    replacementArchive.close();
    QSignalSpy replacementFailureSpy(
        &restartedManager, &LauncherDownloadManager::installationFailed);
    const int replacementRequest = restartedManager.installDownloadedAsset(
        projectId, assetName, QStringLiteral("tar.gz"), QStringLiteral("SampleLauncher"));
    QTRY_COMPARE(replacementFailureSpy.count(), 1);
    QCOMPARE(replacementFailureSpy.takeFirst().at(0).toInt(), replacementRequest);
    QVERIFY(QFileInfo(executablePath).isFile());
    QVERIFY(QFileInfo::exists(archivePath));

    const QString unmanagedDirectory = QDir(platformManager.dataDirectory())
                                           .filePath("launchers/unmanaged-project");
    QVERIFY(QDir().mkpath(unmanagedDirectory));
    const QString unmanagedFile = QDir(unmanagedDirectory).filePath("keep.txt");
    QFile unmanaged(unmanagedFile);
    QVERIFY(unmanaged.open(QIODevice::WriteOnly));
    QCOMPARE(unmanaged.write("not managed"), qint64(11));
    unmanaged.close();
    QVERIFY(!restartedManager.uninstallLauncher(QStringLiteral("unmanaged-project")));
    QVERIFY(QFileInfo::exists(unmanagedFile));

    const QString unrelatedFile = QDir(temporaryDirectory.path()).filePath("keep-this-file.txt");
    QFile unrelated(unrelatedFile);
    QVERIFY(unrelated.open(QIODevice::WriteOnly));
    QCOMPARE(unrelated.write("keep"), qint64(4));
    unrelated.close();

    QVERIFY(!manager.uninstallLauncher(QStringLiteral("../outside")));
    QVERIFY(restartedManager.uninstallLauncher(projectId));
    QVERIFY(restartedManager.installedLaunchers().isEmpty());
    QVERIFY(QFileInfo::exists(unrelatedFile));
}


void LauncherDownloadManagerTest::installsLauncherFromNestedArchiveDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString projectId = QStringLiteral("nested-sample-project");
    const QString assetName = QStringLiteral("NestedLauncher-Linux.tar.gz");
    const QString stagingDirectory = QDir(platformManager.stagingDirectory()).filePath(projectId);
    QVERIFY(QDir().mkpath(stagingDirectory));
    const QString archivePath = QDir(stagingDirectory).filePath(assetName);

    struct archive *writer = archive_write_new();
    QVERIFY(writer != nullptr);
    QCOMPARE(archive_write_add_filter_gzip(writer), ARCHIVE_OK);
    QCOMPARE(archive_write_set_format_pax_restricted(writer), ARCHIVE_OK);
    const QByteArray archivePathBytes = QFile::encodeName(archivePath);
    QCOMPARE(archive_write_open_filename(writer, archivePathBytes.constData()), ARCHIVE_OK);

    struct archive_entry *directory = archive_entry_new();
    QVERIFY(directory != nullptr);
    archive_entry_set_pathname(directory, "NestedLauncher/");
    archive_entry_set_filetype(directory, AE_IFDIR);
    archive_entry_set_perm(directory, 0755);
    archive_entry_set_size(directory, 0);
    QCOMPARE(archive_write_header(writer, directory), ARCHIVE_OK);
    archive_entry_free(directory);

    const QByteArray executableContents("#!/bin/sh\\necho nested launcher\\n");
    struct archive_entry *executable = archive_entry_new();
    QVERIFY(executable != nullptr);
    archive_entry_set_pathname(executable, "NestedLauncher/ConkerRecomp");
    archive_entry_set_filetype(executable, AE_IFREG);
    archive_entry_set_perm(executable, 0644);
    archive_entry_set_size(executable, executableContents.size());
    QCOMPARE(archive_write_header(writer, executable), ARCHIVE_OK);
    QCOMPARE(archive_write_data(writer, executableContents.constData(), executableContents.size()),
             static_cast<la_ssize_t>(executableContents.size()));
    archive_entry_free(executable);
    QCOMPARE(archive_write_close(writer), ARCHIVE_OK);
    QCOMPARE(archive_write_free(writer), ARCHIVE_OK);

    LauncherDownloadManager manager(&platformManager);
    QSignalSpy completedSpy(&manager, &LauncherDownloadManager::installationCompleted);
    QSignalSpy failedSpy(&manager, &LauncherDownloadManager::installationFailed);
    const int requestId = manager.installDownloadedAsset(
        projectId, assetName, QStringLiteral("tar.gz"), QStringLiteral("ConkerRecomp"));
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 10000);
    QCOMPARE(completedSpy.takeFirst().at(0).toInt(), requestId);
    QCOMPARE(failedSpy.count(), 0);

    const QString expectedExecutable = QDir(platformManager.dataDirectory()).filePath(
        "launchers/" + projectId + "/NestedLauncher/ConkerRecomp");
    QVERIFY(QFileInfo(expectedExecutable).isExecutable());

    const QVariantList installed = manager.installedLaunchers();
    QCOMPARE(installed.size(), 1);
    QCOMPARE(installed.first().toMap().value(QStringLiteral("executablePath")).toString(),
             expectedExecutable);
}

void LauncherDownloadManagerTest::installsZipLauncherWithCompanionFiles()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString projectId = QStringLiteral("zip-sample-project");
    const QString assetName = QStringLiteral("SampleProject-Linux-X64.zip");
    const QString stagingDirectory = QDir(platformManager.stagingDirectory()).filePath(projectId);
    QVERIFY(QDir().mkpath(stagingDirectory));
    const QString archivePath = QDir(stagingDirectory).filePath(assetName);

    struct archive *writer = archive_write_new();
    QVERIFY(writer != nullptr);
    QCOMPARE(archive_write_set_format_zip(writer), ARCHIVE_OK);
    const QByteArray archivePathBytes = QFile::encodeName(archivePath);
    QCOMPARE(archive_write_open_filename(writer, archivePathBytes.constData()), ARCHIVE_OK);

    const QList<QPair<QByteArray, QByteArray>> files = {
        {QByteArray("SampleProject.AppImage"), QByteArray("appimage-binary")},
        {QByteArray("resources/data.bin"), QByteArray("companion-data")}
    };
    for (const auto &file : files) {
        struct archive_entry *entry = archive_entry_new();
        QVERIFY(entry != nullptr);
        archive_entry_set_pathname(entry, file.first.constData());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0755);
        archive_entry_set_size(entry, file.second.size());
        QCOMPARE(archive_write_header(writer, entry), ARCHIVE_OK);
        QCOMPARE(archive_write_data(writer, file.second.constData(), file.second.size()),
                 static_cast<la_ssize_t>(file.second.size()));
        archive_entry_free(entry);
    }
    QCOMPARE(archive_write_close(writer), ARCHIVE_OK);
    QCOMPARE(archive_write_free(writer), ARCHIVE_OK);

    LauncherDownloadManager manager(&platformManager);
    QSignalSpy completedSpy(&manager, &LauncherDownloadManager::installationCompleted);
    QSignalSpy failedSpy(&manager, &LauncherDownloadManager::installationFailed);
    const int requestId = manager.installDownloadedAsset(
        projectId, assetName, QStringLiteral("zip"),
        QStringLiteral("SampleProject.AppImage"));
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 10000);
    QCOMPARE(completedSpy.takeFirst().at(0).toInt(), requestId);
    QCOMPARE(failedSpy.count(), 0);

    const QString installRoot = QDir(platformManager.dataDirectory())
                                    .filePath("launchers/" + projectId);
    QVERIFY(QFileInfo(QDir(installRoot).filePath("SampleProject.AppImage")).isExecutable());
    QFile companion(QDir(installRoot).filePath("resources/data.bin"));
    QVERIFY(companion.open(QIODevice::ReadOnly));
    QCOMPARE(companion.readAll(), QByteArray("companion-data"));
}

void LauncherDownloadManagerTest::installsStandaloneAppImage()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString projectId = QStringLiteral("standalone-appimage-project");
    const QString assetName = QStringLiteral("StandaloneLauncher-x86_64.AppImage");
    const QString stagingDirectory = QDir(platformManager.stagingDirectory()).filePath(projectId);
    QVERIFY(QDir().mkpath(stagingDirectory));
    const QString appImagePath = QDir(stagingDirectory).filePath(assetName);
    QFile appImage(appImagePath);
    QVERIFY(appImage.open(QIODevice::WriteOnly));
    const QByteArray content("standalone-appimage-content");
    QCOMPARE(appImage.write(content), qint64(content.size()));
    appImage.close();

    LauncherDownloadManager manager(&platformManager);
    QSignalSpy completedSpy(&manager, &LauncherDownloadManager::installationCompleted);
    QSignalSpy failedSpy(&manager, &LauncherDownloadManager::installationFailed);
    const int requestId = manager.installDownloadedAsset(
        projectId, assetName, QStringLiteral("appimage"), assetName);
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 10000);
    QCOMPARE(completedSpy.takeFirst().at(0).toInt(), requestId);
    QCOMPARE(failedSpy.count(), 0);

    const QString installedPath = QDir(platformManager.dataDirectory())
        .filePath("launchers/" + projectId + "/" + assetName);
    QVERIFY(QFileInfo(installedPath).isExecutable());
    QFile installed(installedPath);
    QVERIFY(installed.open(QIODevice::ReadOnly));
    QCOMPARE(installed.readAll(), content);
}

QTEST_GUILESS_MAIN(LauncherDownloadManagerTest)
#include "LauncherDownloadManagerTest.moc"
