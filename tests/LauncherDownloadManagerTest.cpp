#include <QtTest>

#include "core/LauncherDownloadManager.h"
#include "core/PlatformManager.h"

#include <QDir>
#include <QDirIterator>
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
    void keepsSavesMadeInsideTheInstallAcrossReinstall();
    void cleansUpLeftoverDownloadsAtStartup();
    void recordsReleaseAndUpdatesKeepingSaves();
    void failedUpdateKeepsThePreviousVersion();
    void refusesToReplaceFoldersItDidNotMake();
    void recoversInterruptedUpdates();
    void hashesFiles();
    void installsLauncherFromNestedArchiveDirectory();
    void installsZipLauncherWithCompanionFiles();
    void installsStandaloneAppImage();
    void installsWindowsBuildAndKeepsItsFilesApart();
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


namespace {
// A tar.gz launcher package: an executable plus the given files.
bool writeLauncherPackage(const QString &path, const QMap<QString, QByteArray> &files)
{
    struct archive *writer = archive_write_new();
    if (!writer || archive_write_add_filter_gzip(writer) != ARCHIVE_OK
        || archive_write_set_format_pax_restricted(writer) != ARCHIVE_OK
        || archive_write_open_filename(writer, QFile::encodeName(path).constData()) != ARCHIVE_OK)
        return false;
    QMap<QString, QByteArray> all = files;
    all.insert(QStringLiteral("SampleLauncher"), QByteArray("#!/bin/sh\necho test\n"));
    for (auto it = all.cbegin(); it != all.cend(); ++it) {
        struct archive_entry *entry = archive_entry_new();
        const QByteArray name = it.key().toUtf8();
        archive_entry_set_pathname(entry, name.constData());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, it.key() == QStringLiteral("SampleLauncher") ? 0755 : 0644);
        archive_entry_set_size(entry, it.value().size());
        const bool ok = archive_write_header(writer, entry) == ARCHIVE_OK
            && archive_write_data(writer, it.value().constData(), it.value().size()) == it.value().size();
        archive_entry_free(entry);
        if (!ok)
            return false;
    }
    return archive_write_close(writer) == ARCHIVE_OK && archive_write_free(writer) == ARCHIVE_OK;
}

bool writeText(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

QByteArray readText(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

void LauncherDownloadManagerTest::keepsSavesMadeInsideTheInstallAcrossReinstall()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString projectId = QStringLiteral("portable-sample-project");
    const QString assetName = QStringLiteral("Portable-Linux-X64.tar.gz");
    const QString staging = QDir(platformManager.stagingDirectory()).filePath(projectId);
    QVERIFY(QDir().mkpath(staging));
    const QString archivePath = QDir(staging).filePath(assetName);
    const QMap<QString, QByteArray> package{{QStringLiteral("config.json"), QByteArray("{\"v\":1}")}};

    LauncherDownloadManager manager(&platformManager);
    QSignalSpy completed(&manager, &LauncherDownloadManager::installationCompleted);
    QVERIFY(writeLauncherPackage(archivePath, package));
    manager.installDownloadedAsset(projectId, assetName, QStringLiteral("tar.gz"), QStringLiteral("SampleLauncher"));
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 10000);

    const QString install = QDir(platformManager.dataDirectory()).filePath(QStringLiteral("launchers/") + projectId);
    // The launcher ran in portable mode: saves and a settings change in its folder.
    QVERIFY(writeText(QDir(install).filePath(QStringLiteral("portable.txt")), QByteArray()));
    QVERIFY(writeText(QDir(install).filePath(QStringLiteral("saves/slot1.sav")), QByteArray("save one")));
    QVERIFY(writeText(QDir(install).filePath(QStringLiteral("config.json")), QByteArray("{\"v\":2}")));

    QVERIFY(manager.uninstallLauncher(projectId));
    QVERIFY(!QFileInfo::exists(install));
    const QString kept = manager.keptFilesDirectory(projectId);
    QCOMPARE(readText(QDir(kept).filePath(QStringLiteral("saves/slot1.sav"))), QByteArray("save one"));
    QVERIFY(QFileInfo::exists(QDir(kept).filePath(QStringLiteral("portable.txt"))));
    // Files from the package itself go with the install, edited or not.
    QVERIFY(!QFileInfo::exists(QDir(kept).filePath(QStringLiteral("config.json"))));
    QVERIFY(!QFileInfo::exists(QDir(kept).filePath(QStringLiteral("SampleLauncher"))));

    // Installing again (an update) puts them back.
    QVERIFY(writeLauncherPackage(archivePath, package));
    manager.installDownloadedAsset(projectId, assetName, QStringLiteral("tar.gz"), QStringLiteral("SampleLauncher"));
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 2, 10000);
    QCOMPARE(readText(QDir(install).filePath(QStringLiteral("saves/slot1.sav"))), QByteArray("save one"));
    QVERIFY(QFileInfo::exists(QDir(install).filePath(QStringLiteral("portable.txt"))));
    QCOMPARE(readText(QDir(install).filePath(QStringLiteral("config.json"))), QByteArray("{\"v\":1}"));
    QVERIFY(!QFileInfo::exists(kept));
}

namespace {
QStringList previousFolders(const QString &launchers)
{
    return QDir(launchers).entryList(QStringList{QStringLiteral(".*.previous-*")},
                                     QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
}
}

void LauncherDownloadManagerTest::recordsReleaseAndUpdatesKeepingSaves()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString projectId = QStringLiteral("update-sample-project");
    const QString assetName = QStringLiteral("Update-Linux-X64.tar.gz");
    const QString staging = QDir(platformManager.stagingDirectory()).filePath(projectId);
    QVERIFY(QDir().mkpath(staging));
    const QString archivePath = QDir(staging).filePath(assetName);

    LauncherDownloadManager manager(&platformManager);
    QSignalSpy completed(&manager, &LauncherDownloadManager::installationCompleted);
    QSignalSpy failed(&manager, &LauncherDownloadManager::installationFailed);
    QVERIFY(writeLauncherPackage(archivePath, {{QStringLiteral("config.json"), QByteArray("{\"v\":1}")},
                                               {QStringLiteral("old-only.dat"), QByteArray("old")}}));
    manager.installDownloadedAsset(projectId, assetName, QStringLiteral("tar.gz"),
                                   QStringLiteral("SampleLauncher"), QStringLiteral("v1.0.0"));
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 10000);

    auto entry = [&]() {
        for (const QVariant &value : manager.installedLaunchers()) {
            if (value.toMap().value(QStringLiteral("projectId")).toString() == projectId)
                return value.toMap();
        }
        return QVariantMap();
    };
    QCOMPARE(entry().value(QStringLiteral("version")).toString(), QStringLiteral("v1.0.0"));
    QCOMPARE(entry().value(QStringLiteral("assetName")).toString(), assetName);
    // Installed from a file the manager didn't download: nothing to verify.
    QCOMPARE(entry().value(QStringLiteral("checksumVerified")).toBool(), false);

    const QString install = QDir(platformManager.dataDirectory()).filePath(QStringLiteral("launchers/") + projectId);
    QVERIFY(writeText(QDir(install).filePath(QStringLiteral("saves/slot1.sav")), QByteArray("save one")));
    QVERIFY(writeText(QDir(install).filePath(QStringLiteral("portable.txt")), QByteArray()));
    // The player made a file the new version now ships: the new one wins and
    // the player's copy is kept aside.
    QVERIFY(writeText(QDir(install).filePath(QStringLiteral("shaders.cache")), QByteArray("player cache")));

    // The update: unpacked beside the install, swapped in, saves carried over.
    QVERIFY(writeLauncherPackage(archivePath, {{QStringLiteral("config.json"), QByteArray("{\"v\":2}")},
                                               {QStringLiteral("shaders.cache"), QByteArray("shipped")}}));
    manager.installDownloadedAsset(projectId, assetName, QStringLiteral("tar.gz"),
                                   QStringLiteral("SampleLauncher"), QStringLiteral("v2.0.0"), true);
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 2, 10000);
    QCOMPARE(failed.count(), 0);

    QCOMPARE(entry().value(QStringLiteral("version")).toString(), QStringLiteral("v2.0.0"));
    QCOMPARE(readText(QDir(install).filePath(QStringLiteral("config.json"))), QByteArray("{\"v\":2}"));
    QCOMPARE(readText(QDir(install).filePath(QStringLiteral("saves/slot1.sav"))), QByteArray("save one"));
    QVERIFY(QFileInfo::exists(QDir(install).filePath(QStringLiteral("portable.txt"))));
    QCOMPARE(readText(QDir(install).filePath(QStringLiteral("shaders.cache"))), QByteArray("shipped"));
    QCOMPARE(readText(QDir(manager.keptFilesDirectory(projectId)).filePath(QStringLiteral("shaders.cache"))),
             QByteArray("player cache"));
    // Files only the old package had are gone with it.
    QVERIFY(!QFileInfo::exists(QDir(install).filePath(QStringLiteral("old-only.dat"))));
    QVERIFY(previousFolders(QFileInfo(install).absolutePath()).isEmpty());
    QVERIFY(!QFileInfo::exists(archivePath));

    // The saves still count as the player's: uninstall sets them aside.
    QVERIFY(manager.uninstallLauncher(projectId));
    QCOMPARE(readText(QDir(manager.keptFilesDirectory(projectId)).filePath(QStringLiteral("saves/slot1.sav"))),
             QByteArray("save one"));
}

void LauncherDownloadManagerTest::failedUpdateKeepsThePreviousVersion()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString projectId = QStringLiteral("failed-update-project");
    const QString assetName = QStringLiteral("Failed-Linux-X64.tar.gz");
    const QString staging = QDir(platformManager.stagingDirectory()).filePath(projectId);
    QVERIFY(QDir().mkpath(staging));
    const QString archivePath = QDir(staging).filePath(assetName);

    LauncherDownloadManager manager(&platformManager);
    QSignalSpy completed(&manager, &LauncherDownloadManager::installationCompleted);
    QSignalSpy failed(&manager, &LauncherDownloadManager::installationFailed);
    QVERIFY(writeLauncherPackage(archivePath, {{QStringLiteral("config.json"), QByteArray("v1")}}));
    manager.installDownloadedAsset(projectId, assetName, QStringLiteral("tar.gz"),
                                   QStringLiteral("SampleLauncher"), QStringLiteral("v1"));
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 10000);
    const QString install = QDir(platformManager.dataDirectory()).filePath(QStringLiteral("launchers/") + projectId);
    QVERIFY(writeText(QDir(install).filePath(QStringLiteral("saves/slot1.sav")), QByteArray("save one")));

    // A broken download: not an archive at all.
    QVERIFY(writeText(archivePath, QByteArray("this is not a tar.gz")));
    manager.installDownloadedAsset(projectId, assetName, QStringLiteral("tar.gz"),
                                   QStringLiteral("SampleLauncher"), QStringLiteral("v2"), true);
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 10000);

    // A package that lacks the launcher the catalog names.
    struct archive *writer = archive_write_new();
    QVERIFY(archive_write_add_filter_gzip(writer) == ARCHIVE_OK);
    QVERIFY(archive_write_set_format_pax_restricted(writer) == ARCHIVE_OK);
    QVERIFY(archive_write_open_filename(writer, QFile::encodeName(archivePath).constData()) == ARCHIVE_OK);
    struct archive_entry *item = archive_entry_new();
    archive_entry_set_pathname(item, "readme.txt");
    archive_entry_set_filetype(item, AE_IFREG);
    archive_entry_set_perm(item, 0644);
    archive_entry_set_size(item, 2);
    QVERIFY(archive_write_header(writer, item) == ARCHIVE_OK);
    QCOMPARE(archive_write_data(writer, "hi", 2), la_ssize_t(2));
    archive_entry_free(item);
    QVERIFY(archive_write_close(writer) == ARCHIVE_OK);
    archive_write_free(writer);
    manager.installDownloadedAsset(projectId, assetName, QStringLiteral("tar.gz"),
                                   QStringLiteral("SampleLauncher"), QStringLiteral("v2"), true);
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 2, 10000);

    QCOMPARE(completed.count(), 1);
    QCOMPARE(readText(QDir(install).filePath(QStringLiteral("config.json"))), QByteArray("v1"));
    QCOMPARE(readText(QDir(install).filePath(QStringLiteral("saves/slot1.sav"))), QByteArray("save one"));
    QVERIFY(QFileInfo(QDir(install).filePath(QStringLiteral("SampleLauncher"))).isFile());
    QVERIFY(previousFolders(QFileInfo(install).absolutePath()).isEmpty());
    for (const QVariant &value : manager.installedLaunchers()) {
        if (value.toMap().value(QStringLiteral("projectId")).toString() == projectId)
            QCOMPARE(value.toMap().value(QStringLiteral("version")).toString(), QStringLiteral("v1"));
    }
}

void LauncherDownloadManagerTest::refusesToReplaceFoldersItDidNotMake()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString projectId = QStringLiteral("unmanaged-project");
    const QString assetName = QStringLiteral("Sample-Linux-X64.tar.gz");
    const QString staging = QDir(platformManager.stagingDirectory()).filePath(projectId);
    QVERIFY(QDir().mkpath(staging));
    const QString archivePath = QDir(staging).filePath(assetName);
    // A folder RecompStation didn't make: never replaced, even when asked.
    const QString install = QDir(platformManager.dataDirectory()).filePath(QStringLiteral("launchers/") + projectId);
    QVERIFY(writeText(QDir(install).filePath(QStringLiteral("mine.txt")), QByteArray("mine")));

    LauncherDownloadManager manager(&platformManager);
    QSignalSpy failed(&manager, &LauncherDownloadManager::installationFailed);
    QVERIFY(writeLauncherPackage(archivePath, {}));
    manager.installDownloadedAsset(projectId, assetName, QStringLiteral("tar.gz"),
                                   QStringLiteral("SampleLauncher"), QStringLiteral("v1"), true);
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 10000);
    QCOMPARE(readText(QDir(install).filePath(QStringLiteral("mine.txt"))), QByteArray("mine"));
    QVERIFY(!QFileInfo::exists(QDir(install).filePath(QStringLiteral("SampleLauncher"))));
}

void LauncherDownloadManagerTest::recoversInterruptedUpdates()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());
    const QString launchers = QDir(platformManager.dataDirectory()).filePath(QStringLiteral("launchers"));
    QVERIFY(QDir().mkpath(launchers));

    // Cut short after the old version stepped aside: it comes back.
    const QString asideOnly = QDir(launchers).filePath(QStringLiteral(".alpha-project.previous-1111"));
    QVERIFY(writeText(QDir(asideOnly).filePath(QStringLiteral("SampleLauncher")), QByteArray("old")));
    QVERIFY(writeText(QDir(asideOnly).filePath(QStringLiteral(".recompstation-files")), QByteArray("SampleLauncher\n")));
    QVERIFY(writeText(QDir(asideOnly).filePath(QStringLiteral("saves/a.sav")), QByteArray("alpha save")));

    // Cut short after the new version was in place: saves finish moving.
    const QString betaNew = QDir(launchers).filePath(QStringLiteral("beta-project"));
    QVERIFY(writeText(QDir(betaNew).filePath(QStringLiteral("SampleLauncher")), QByteArray("new")));
    const QString betaOld = QDir(launchers).filePath(QStringLiteral(".beta-project.previous-2222"));
    QVERIFY(writeText(QDir(betaOld).filePath(QStringLiteral("SampleLauncher")), QByteArray("old")));
    QVERIFY(writeText(QDir(betaOld).filePath(QStringLiteral(".recompstation-files")), QByteArray("SampleLauncher\n")));
    QVERIFY(writeText(QDir(betaOld).filePath(QStringLiteral("saves/b.sav")), QByteArray("beta save")));

    // Not ours: a hidden folder whose name isn't a project id.
    const QString stray = QDir(launchers).filePath(QStringLiteral(".Not A Project.previous-3"));
    QVERIFY(writeText(QDir(stray).filePath(QStringLiteral("x")), QByteArray("x")));

    LauncherDownloadManager manager(&platformManager);
    QCOMPARE(manager.recoverInterruptedUpdates(), 2);
    const QString alpha = QDir(launchers).filePath(QStringLiteral("alpha-project"));
    QCOMPARE(readText(QDir(alpha).filePath(QStringLiteral("SampleLauncher"))), QByteArray("old"));
    QCOMPARE(readText(QDir(alpha).filePath(QStringLiteral("saves/a.sav"))), QByteArray("alpha save"));
    QCOMPARE(readText(QDir(betaNew).filePath(QStringLiteral("SampleLauncher"))), QByteArray("new"));
    QCOMPARE(readText(QDir(betaNew).filePath(QStringLiteral("saves/b.sav"))), QByteArray("beta save"));
    QVERIFY(!QFileInfo::exists(asideOnly));
    QVERIFY(!QFileInfo::exists(betaOld));
    QVERIFY(QFileInfo::exists(stray));
    QCOMPARE(manager.recoverInterruptedUpdates(), 0);
}

void LauncherDownloadManagerTest::hashesFiles()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = QDir(temporaryDirectory.path()).filePath(QStringLiteral("file"));
    QVERIFY(writeText(path, QByteArray("test")));
    QCOMPARE(LauncherDownloadManager::sha256OfFile(path),
             QStringLiteral("9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"));
    QVERIFY(LauncherDownloadManager::sha256OfFile(path + QStringLiteral(".missing")).isEmpty());
}

void LauncherDownloadManagerTest::cleansUpLeftoverDownloadsAtStartup()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());
    const QString staging = platformManager.stagingDirectory();
    const QString project = QDir(staging).filePath(QStringLiteral("sample-project"));

    // What an interrupted session leaves: an archive never installed, a
    // half-written download and a half-extracted folder.
    QVERIFY(writeText(QDir(project).filePath(QStringLiteral("Sample-Linux.tar.gz")), QByteArray(4096, 'a')));
    QVERIFY(writeText(QDir(project).filePath(QStringLiteral("Other.zip.Ab12Cd")), QByteArray("partial")));
    const QString extracting = QDir(project).filePath(QStringLiteral("Sample-Linux.tar.gz.extracting-1234"));
    QVERIFY(writeText(QDir(extracting).filePath(QStringLiteral("bin/Sample")), QByteArray("exe")));
    // A link inside points outside: the link goes, what it points at stays.
    const QString outside = QDir(temporaryDirectory.path()).filePath(QStringLiteral("keep-me.txt"));
    QVERIFY(writeText(outside, QByteArray("keep")));
    QVERIFY(QFile::link(outside, QDir(extracting).filePath(QStringLiteral("link-out"))));
    QVERIFY(QFile::link(outside, QDir(project).filePath(QStringLiteral("link-out"))));
    // Not ours to guess at: a stray file and a folder whose name isn't a project id.
    QVERIFY(writeText(QDir(staging).filePath(QStringLiteral("stray.txt")), QByteArray("stray")));
    QVERIFY(writeText(QDir(staging).filePath(QStringLiteral("Not A Project/file")), QByteArray("x")));

    LauncherDownloadManager manager(&platformManager);
    QCOMPARE(manager.cleanUpStaging(), 4);
    QVERIFY(!QFileInfo::exists(project));
    QCOMPARE(readText(outside), QByteArray("keep"));
    QVERIFY(QFileInfo::exists(QDir(staging).filePath(QStringLiteral("stray.txt"))));
    QVERIFY(QFileInfo::exists(QDir(staging).filePath(QStringLiteral("Not A Project/file"))));
    QCOMPARE(manager.cleanUpStaging(), 0);
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


// A Windows build: the program is picked from the zip, the build is
// recorded, and files the launcher made are kept apart from the Linux
// launcher's, so switching builds never mixes saves.
void LauncherDownloadManagerTest::installsWindowsBuildAndKeepsItsFilesApart()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    PlatformManager platformManager(temporaryDirectory.path());
    QVERIFY(platformManager.initialize());

    const QString projectId = QStringLiteral("windows-sample-project");
    const QString assetName = QStringLiteral("Sample-Windows.zip");
    const QString stagingDirectory = QDir(platformManager.stagingDirectory()).filePath(projectId);
    QVERIFY(QDir().mkpath(stagingDirectory));
    const QString archivePath = QDir(stagingDirectory).filePath(assetName);
    struct archive *writer = archive_write_new();
    QCOMPARE(archive_write_set_format_zip(writer), ARCHIVE_OK);
    QCOMPARE(archive_write_open_filename(writer, QFile::encodeName(archivePath).constData()), ARCHIVE_OK);
    for (const auto &file : QList<QPair<QByteArray, QByteArray>>{
             {"Sample.exe", "MZ"}, {"crashpad_handler.exe", "MZ"}, {"data/assets.bin", "data"}}) {
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, file.first.constData());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_entry_set_size(entry, file.second.size());
        QCOMPARE(archive_write_header(writer, entry), ARCHIVE_OK);
        archive_write_data(writer, file.second.constData(), file.second.size());
        archive_entry_free(entry);
    }
    QCOMPARE(archive_write_close(writer), ARCHIVE_OK);
    QCOMPARE(archive_write_free(writer), ARCHIVE_OK);

    LauncherDownloadManager manager(&platformManager);
    QSignalSpy completedSpy(&manager, &LauncherDownloadManager::installationCompleted);
    QSignalSpy failedSpy(&manager, &LauncherDownloadManager::installationFailed);

    // A Linux build must name its program.
    manager.installDownloadedAsset(projectId, assetName, QStringLiteral("zip"), QString(),
                                   QStringLiteral("v1"), false, QStringLiteral("linux-x86_64"));
    QTRY_COMPARE(failedSpy.count(), 1);
    failedSpy.clear();

    manager.installDownloadedAsset(projectId, assetName, QStringLiteral("zip"), QString(),
                                   QStringLiteral("v1"), false, QStringLiteral("windows-x86_64"));
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 10000);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(completedSpy.takeFirst().at(2).toString().endsWith(QStringLiteral("/Sample.exe")));

    const QVariantList installed = manager.installedLaunchers();
    QCOMPARE(installed.size(), 1);
    QCOMPARE(installed.first().toMap().value(QStringLiteral("build")).toString(), QStringLiteral("windows-x86_64"));

    // A file the Windows launcher made is set aside in its own folder.
    const QString installRoot = QDir(platformManager.dataDirectory()).filePath("launchers/" + projectId);
    QVERIFY(writeText(QDir(installRoot).filePath(QStringLiteral("portable-save.dat")), "save"));
    QVERIFY(manager.uninstallLauncher(projectId));
    const QString windowsKept = manager.keptFilesDirectory(projectId, QStringLiteral("windows-x86_64"));
    const QString linuxKept = manager.keptFilesDirectory(projectId, QStringLiteral("linux-x86_64"));
    QVERIFY(windowsKept != linuxKept);
    QCOMPARE(readText(QDir(windowsKept).filePath(QStringLiteral("portable-save.dat"))), QByteArray("save"));
    QVERIFY(!QFileInfo::exists(QDir(linuxKept).filePath(QStringLiteral("portable-save.dat"))));
}

QTEST_GUILESS_MAIN(LauncherDownloadManagerTest)
#include "LauncherDownloadManagerTest.moc"
