#include <QtTest>

#include "core/PlatformManager.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

class PlatformManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initializeCreatesStagingDirectory();
    void providesProjectSpecificLauncherDirectory();
    void rejectsUnsafeProjectIds();
};

void PlatformManagerTest::initializeCreatesStagingDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PlatformManager manager(temporaryDirectory.path());
    QVERIFY(manager.initialize());

    const QString expected = QDir(temporaryDirectory.path())
                                 .filePath("downloads/staging");
    QCOMPARE(manager.stagingDirectory(), expected);
    QVERIFY(QFileInfo(manager.stagingDirectory()).isDir());
    QVERIFY(QFileInfo(QDir(temporaryDirectory.path())
                          .filePath("launchers")).isDir());

    // Existing per-platform game and launcher paths remain available.
    QVERIFY(QFileInfo(manager.platformDirectory("N64")).isDir());
    QVERIFY(QFileInfo(QDir(temporaryDirectory.path())
                          .filePath("launchers/N64")).isDir());
}

void PlatformManagerTest::providesProjectSpecificLauncherDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PlatformManager manager(temporaryDirectory.path());
    QVERIFY(manager.initialize());

    const QString expected = QDir(temporaryDirectory.path())
                                 .filePath("launchers/banjo-kazooie-recompiled");
    QCOMPARE(manager.launcherDirectory("banjo-kazooie-recompiled"), expected);
    QVERIFY(!QFileInfo::exists(expected));

    QVERIFY(manager.createLauncherDirectory("banjo-kazooie-recompiled"));
    QVERIFY(QFileInfo(expected).isDir());
}

void PlatformManagerTest::rejectsUnsafeProjectIds()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PlatformManager manager(temporaryDirectory.path());
    QVERIFY(manager.initialize());

    const QStringList invalidIds = {
        QString(),
        ".",
        "..",
        "../outside",
        "project/name",
        "project\\name",
        "Uppercase",
        "-leading-hyphen",
        "project name"
    };

    const QString outsidePath = QDir(temporaryDirectory.path())
                                    .filePath("outside");

    for (const QString &projectId : invalidIds) {
        QVERIFY2(manager.launcherDirectory(projectId).isEmpty(),
                 qPrintable(projectId));
        QVERIFY(!manager.createLauncherDirectory(projectId));
    }

    QVERIFY(!QFileInfo::exists(outsidePath));
}

QTEST_GUILESS_MAIN(PlatformManagerTest)
#include "PlatformManagerTest.moc"
