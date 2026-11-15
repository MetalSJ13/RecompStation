#include <QtTest>

#include "core/LauncherDownloadManager.h"
#include "core/PlatformManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

class LauncherDownloadManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsInvalidProjectIdentifiers();
    void rejectsUnsafeAssetNames();
    void rejectsNonHttpsUrls();
    void rejectsOversizedAssets();
    void acceptsValidRequestAndReportsMissingNetworkDestination();
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

QTEST_GUILESS_MAIN(LauncherDownloadManagerTest)
#include "LauncherDownloadManagerTest.moc"
