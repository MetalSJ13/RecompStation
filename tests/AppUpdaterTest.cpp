#include "core/AppUpdater.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <archive.h>
#include <archive_entry.h>

namespace {
bool writeText(const QString &path, const QByteArray &content, bool executable = false)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size())
        return false;
    file.close();
    if (executable)
        file.setPermissions(file.permissions() | QFile::ExeOwner);
    return true;
}

QByteArray readText(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// A portable RecompStation folder as the release zip unpacks it.
bool writePortableFolder(const QString &root, const QByteArray &marker)
{
    return writeText(QDir(root).filePath(QStringLiteral("RecompStation")), "#!/bin/sh\n", true)
        && writeText(QDir(root).filePath(QStringLiteral("usr/bin/RecompStationApp")), marker, true)
        && writeText(QDir(root).filePath(QStringLiteral("THIRD_PARTY_NOTICES.md")), "notices");
}

// The release zip: one top folder holding the portable files.
bool writeReleaseZip(const QString &zipPath, const QString &topFolder, const QByteArray &marker)
{
    struct archive *writer = archive_write_new();
    if (archive_write_set_format_zip(writer) != ARCHIVE_OK
        || archive_write_open_filename(writer, QFile::encodeName(zipPath).constData()) != ARCHIVE_OK)
        return false;
    const QList<QPair<QString, QPair<QByteArray, int>>> files{
        {topFolder + QStringLiteral("/RecompStation"), {QByteArray("#!/bin/sh\n"), 0755}},
        {topFolder + QStringLiteral("/usr/bin/RecompStationApp"), {marker, 0755}},
        {topFolder + QStringLiteral("/THIRD_PARTY_NOTICES.md"), {QByteArray("notices"), 0644}}};
    for (const auto &file : files) {
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, file.first.toUtf8().constData());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, file.second.second);
        archive_entry_set_size(entry, file.second.first.size());
        if (archive_write_header(writer, entry) != ARCHIVE_OK
            || archive_write_data(writer, file.second.first.constData(), file.second.first.size())
                   != file.second.first.size()) {
            archive_entry_free(entry);
            return false;
        }
        archive_entry_free(entry);
    }
    archive_write_close(writer);
    archive_write_free(writer);
    return true;
}

GitHubReleaseAsset asset(const QString &name, const QString &url = QString())
{
    GitHubReleaseAsset result;
    result.name = name;
    result.downloadUrl = url.isEmpty()
        ? QStringLiteral("https://github.com/MetalSJ13/RecompStation/releases/download/v0.2.0/") + name
        : url;
    result.size = 1000;
    result.sha256 = QString(64, QLatin1Char('a'));
    return result;
}
}

class AppUpdaterTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("RecompStationAppUpdaterTests"));
        QCoreApplication::setApplicationName(QStringLiteral("RecompStationAppUpdaterTests"));
        QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
        QSettings().clear();
    }

    void cleanup() { QSettings().clear(); }

    void comparesVersions()
    {
        QVERIFY(AppUpdater::compareVersions(QStringLiteral("v0.2.0"), QStringLiteral("0.1.0")) > 0);
        QVERIFY(AppUpdater::compareVersions(QStringLiteral("0.1.10"), QStringLiteral("0.1.9")) > 0);
        QCOMPARE(AppUpdater::compareVersions(QStringLiteral("0.2"), QStringLiteral("0.2.0")), 0);
        QVERIFY(AppUpdater::compareVersions(QStringLiteral("0.2.0-beta"), QStringLiteral("0.2.0")) < 0);
        QVERIFY(AppUpdater::compareVersions(QStringLiteral("1.0.0"), QStringLiteral("0.9.9")) > 0);
        QVERIFY(AppUpdater::compareVersions(QStringLiteral("0.1.0"), QStringLiteral("V0.1.1")) < 0);
    }

    void detectsTheKindOfInstall()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString appImage = QDir(root.path()).filePath(QStringLiteral("RecompStation-0.1.0-x86_64.AppImage"));
        QVERIFY(writeText(appImage, "elf", true));
        AppUpdater::Install install = AppUpdater::detectInstall(appImage, QStringLiteral("/tmp/.mount_x/usr/bin/RecompStationApp"));
        QCOMPARE(install.kind, QStringLiteral("appimage"));
        QCOMPARE(install.path, QFileInfo(appImage).canonicalFilePath());

        const QString portable = QDir(root.path()).filePath(QStringLiteral("RecompStation-0.1.0-linux-x86_64"));
        QVERIFY(writePortableFolder(portable, "old"));
        install = AppUpdater::detectInstall(QString(), QDir(portable).filePath(QStringLiteral("usr/bin/RecompStationApp")));
        QCOMPARE(install.kind, QStringLiteral("zip"));
        QCOMPARE(install.path, QFileInfo(portable).canonicalFilePath());

        // A missing AppImage path falls through; a build folder is "source".
        install = AppUpdater::detectInstall(QDir(root.path()).filePath(QStringLiteral("gone.AppImage")),
                                            QDir(portable).filePath(QStringLiteral("usr/bin/RecompStationApp")));
        QCOMPARE(install.kind, QStringLiteral("zip"));
        install = AppUpdater::detectInstall(QString(), QDir(root.path()).filePath(QStringLiteral("build/RecompStationApp")));
        QCOMPARE(install.kind, QStringLiteral("source"));
    }

    void picksTheRightDownload()
    {
        const QList<GitHubReleaseAsset> assets{
            asset(QStringLiteral("SHA256SUMS")),
            asset(QStringLiteral("RecompStation-0.2.0-linux-x86_64.zip")),
            asset(QStringLiteral("RecompStation-0.2.0-x86_64.AppImage"))};
        QCOMPARE(AppUpdater::selectAsset(assets, QStringLiteral("appimage")).name,
                 QStringLiteral("RecompStation-0.2.0-x86_64.AppImage"));
        QCOMPARE(AppUpdater::selectAsset(assets, QStringLiteral("zip")).name,
                 QStringLiteral("RecompStation-0.2.0-linux-x86_64.zip"));
        QVERIFY(AppUpdater::selectAsset(assets, QStringLiteral("source")).name.isEmpty());
        // Only from RecompStation's own releases.
        const QList<GitHubReleaseAsset> elsewhere{
            asset(QStringLiteral("RecompStation-0.2.0-x86_64.AppImage"),
                  QStringLiteral("https://example.com/RecompStation-0.2.0-x86_64.AppImage"))};
        QVERIFY(AppUpdater::selectAsset(elsewhere, QStringLiteral("appimage")).name.isEmpty());
    }

    void showsOnlyWhatChanged()
    {
        const QString body = QStringLiteral(
            "## Downloads (64-bit Linux, x86_64)\n\n| File | What it is |\n| --- | --- |\n| a | b |\n\n"
            "> [!IMPORTANT]\n> No game files.\n\n## Changes\n\n<!-- Write the changes here. -->\n"
            "- Faster library scans\n- Better cover matching\n");
        QCOMPARE(AppUpdater::changesSection(body),
                 QStringLiteral("- Faster library scans\n- Better cover matching"));
        QCOMPARE(AppUpdater::changesSection(QStringLiteral("Just a note.\r\n")), QStringLiteral("Just a note."));
    }

    void replacesTheAppImageKeepingABackup()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString current = QDir(root.path()).filePath(QStringLiteral("RecompStation.AppImage"));
        const QString download = QDir(root.path()).filePath(QStringLiteral(".RecompStation.AppImage.download"));
        QVERIFY(writeText(current, "old", true));
        QVERIFY(writeText(download, "new"));
        QString error;
        QVERIFY2(AppUpdater::replaceAppImage(current, download, &error), qPrintable(error));
        QCOMPARE(readText(current), QByteArray("new"));
        QVERIFY(QFileInfo(current).isExecutable());
        QCOMPARE(readText(AppUpdater::previousPath(current)), QByteArray("old"));
        QVERIFY(!QFileInfo::exists(download));

        // The next start of the new version removes the backup.
        AppUpdater updater(QDir(root.path()).filePath(QStringLiteral("work")));
        updater.setInstallForTesting({QStringLiteral("appimage"), current});
        QVERIFY(!updater.finishAtStartup());
        QVERIFY(!QFileInfo::exists(AppUpdater::previousPath(current)));
        QCOMPARE(readText(current), QByteArray("new"));
    }

    void stagesAndSwapsThePortableFolder()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString portable = QDir(root.path()).filePath(QStringLiteral("RecompStation-0.1.0-linux-x86_64"));
        QVERIFY(writePortableFolder(portable, "old"));
        const QString zip = QDir(root.path()).filePath(QStringLiteral("update.zip"));
        QVERIFY(writeReleaseZip(zip, QStringLiteral("RecompStation-0.2.0-linux-x86_64"), "new"));

        QString error;
        QVERIFY2(AppUpdater::stageZipUpdate(zip, portable, QStringLiteral("0.2.0"), &error), qPrintable(error));
        // Nothing changes until the swap.
        QCOMPARE(readText(QDir(portable).filePath(QStringLiteral("usr/bin/RecompStationApp"))), QByteArray("old"));
        QCOMPARE(AppUpdater::stagedVersion(portable), QStringLiteral("0.2.0"));
        QVERIFY(QFileInfo(QDir(AppUpdater::stagedPath(portable)).filePath(QStringLiteral("RecompStation"))).isExecutable());
        // No unpacking folders left behind.
        QCOMPARE(QDir(root.path()).entryList(QStringList{QStringLiteral(".*.unpack-*")},
                                             QDir::Dirs | QDir::Hidden).size(), 0);

        QVERIFY2(AppUpdater::applyStagedZip(portable, &error), qPrintable(error));
        QCOMPARE(readText(QDir(portable).filePath(QStringLiteral("usr/bin/RecompStationApp"))), QByteArray("new"));
        QCOMPARE(readText(QDir(AppUpdater::previousPath(portable)).filePath(QStringLiteral("usr/bin/RecompStationApp"))),
                 QByteArray("old"));
        QVERIFY(!QFileInfo::exists(AppUpdater::stagedPath(portable)));
        QVERIFY(!QFileInfo::exists(QDir(portable).filePath(QStringLiteral(".recompstation-update-version"))));
    }

    void refusesAZipThatIsNotRecompStation()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString portable = QDir(root.path()).filePath(QStringLiteral("RecompStation"));
        QVERIFY(writePortableFolder(portable, "old"));
        const QString zip = QDir(root.path()).filePath(QStringLiteral("other.zip"));
        struct archive *writer = archive_write_new();
        QVERIFY(archive_write_set_format_zip(writer) == ARCHIVE_OK);
        QVERIFY(archive_write_open_filename(writer, QFile::encodeName(zip).constData()) == ARCHIVE_OK);
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, "readme.txt");
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_entry_set_size(entry, 2);
        QVERIFY(archive_write_header(writer, entry) == ARCHIVE_OK);
        QCOMPARE(archive_write_data(writer, "hi", 2), la_ssize_t(2));
        archive_entry_free(entry);
        archive_write_close(writer);
        archive_write_free(writer);
        QString error;
        QVERIFY(!AppUpdater::stageZipUpdate(zip, portable, QStringLiteral("0.2.0"), &error));
        QVERIFY(!QFileInfo::exists(AppUpdater::stagedPath(portable)));
        QCOMPARE(readText(QDir(portable).filePath(QStringLiteral("usr/bin/RecompStationApp"))), QByteArray("old"));
    }

    void finishesAStagedUpdateAtStartup()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString portable = QDir(root.path()).filePath(QStringLiteral("RecompStation"));
        QVERIFY(writePortableFolder(portable, "old"));
        const QString zip = QDir(root.path()).filePath(QStringLiteral("update.zip"));
        QVERIFY(writeReleaseZip(zip, QStringLiteral("RecompStation-0.2.0-linux-x86_64"), "new"));
        QString error;
        QVERIFY(AppUpdater::stageZipUpdate(zip, portable, QStringLiteral("0.2.0"), &error));

        AppUpdater updater(QDir(root.path()).filePath(QStringLiteral("work")));
        updater.setInstallForTesting({QStringLiteral("zip"), portable});
        // The app quit before the swap (a crash): the next start puts the
        // update in place and asks to restart.
        QVERIFY(updater.finishAtStartup());
        QCOMPARE(readText(QDir(portable).filePath(QStringLiteral("usr/bin/RecompStationApp"))), QByteArray("new"));
        // The start of the new version clears the old folder.
        QVERIFY(!updater.finishAtStartup());
        QVERIFY(!QFileInfo::exists(AppUpdater::previousPath(portable)));

        // A staged copy that isn't newer is discarded, not applied.
        QVERIFY(AppUpdater::stageZipUpdate(zip, portable, QStringLiteral("0.1.0"), &error));
        QVERIFY(!updater.finishAtStartup());
        QVERIFY(!QFileInfo::exists(AppUpdater::stagedPath(portable)));
        QCOMPARE(readText(QDir(portable).filePath(QStringLiteral("usr/bin/RecompStationApp"))), QByteArray("new"));
    }

    void asksOncePerVersionAndRemembersSkips()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        AppUpdater updater(QDir(root.path()).filePath(QStringLiteral("work")));
        updater.setInstallForTesting({QStringLiteral("appimage"), QDir(root.path()).filePath(QStringLiteral("a.AppImage"))});
        GitHubRelease release;
        release.tagName = QStringLiteral("v0.2.0");
        release.htmlUrl = QStringLiteral("https://github.com/MetalSJ13/RecompStation/releases/tag/v0.2.0");
        release.body = QStringLiteral("## Changes\n- Faster scans\n");
        release.assets = {asset(QStringLiteral("RecompStation-0.2.0-x86_64.AppImage"))};
        updater.setReleaseForTesting(release);
        QVERIFY(updater.updateAvailable());
        QCOMPARE(updater.latestVersion(), QStringLiteral("0.2.0"));
        QCOMPARE(updater.releaseNotes(), QStringLiteral("- Faster scans"));
        QCOMPARE(updater.downloadSize(), qint64(1000));
        QVERIFY(updater.promptPending());

        updater.remindLater();
        QVERIFY(!updater.promptPending());

        QSignalSpy notices(&updater, &AppUpdater::notice);
        updater.skipLatestVersion();
        QCOMPARE(updater.skippedVersion(), QStringLiteral("0.2.0"));
        QCOMPARE(notices.count(), 1);
        QCOMPARE(notices.first().at(0).toString(), QStringLiteral("Skipped 0.2.0"));
        updater.undoSkip();
        QVERIFY(updater.skippedVersion().isEmpty());
        QVERIFY(updater.promptPending());

        // A skipped version is not offered by a new session; a newer one is.
        updater.skipLatestVersion();
        AppUpdater nextStart(QDir(root.path()).filePath(QStringLiteral("work")));
        nextStart.setReleaseForTesting(release);
        QVERIFY(!nextStart.promptPending());
        release.tagName = QStringLiteral("v0.2.1");
        nextStart.setReleaseForTesting(release);
        QVERIFY(nextStart.promptPending());

        // No newer version: nothing to offer.
        release.tagName = QStringLiteral("v0.1.0");
        nextStart.setReleaseForTesting(release);
        QVERIFY(!nextStart.updateAvailable());
        QVERIFY(!nextStart.promptPending());
    }
};

QTEST_GUILESS_MAIN(AppUpdaterTest)
#include "AppUpdaterTest.moc"
