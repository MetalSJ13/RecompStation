#include <QtTest>

#include "core/PlatformManager.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QSettings>
#include <QUrl>

class PlatformManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void initializeCreatesStagingDirectory();
    void providesProjectSpecificLauncherDirectory();
    void rejectsUnsafeProjectIds();
    void configuresOneLibraryRootAndCreatesOnlySupportedFolders();
    void persistsLibraryRootAcrossRestarts();
    void rejectsInvalidLibraryLocations();
};


void PlatformManagerTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("RecompStationPlatformManagerTests"));
    QCoreApplication::setApplicationName(QStringLiteral("PlatformManagerTests"));
}

void PlatformManagerTest::init()
{
    QSettings().clear();
}

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

    // ROM folders are not created until the user chooses a library root.
    QVERIFY(manager.platformDirectory("N64").isEmpty());
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


void PlatformManagerTest::configuresOneLibraryRootAndCreatesOnlySupportedFolders()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString libraryRoot = QDir(temporaryDirectory.path()).filePath("My Games");
    QVERIFY(QDir().mkpath(libraryRoot));

    PlatformManager manager(temporaryDirectory.path());
    QVERIFY(manager.initialize(QStringList{QStringLiteral("N64")}));
    QVERIFY(!manager.libraryConfigured());
    QVERIFY(manager.setLibraryDirectory(QUrl::fromLocalFile(libraryRoot)));

    QCOMPARE(manager.libraryDirectory(), QDir(libraryRoot).absolutePath());
    QVERIFY(manager.libraryConfigured());
    QVERIFY(QFileInfo(QDir(libraryRoot).filePath("N64")).isDir());
    QVERIFY(!QFileInfo::exists(QDir(libraryRoot).filePath("PS1")));
    QCOMPARE(manager.platformDirectory("N64"), QDir(libraryRoot).filePath("N64"));
    QVERIFY(manager.platformDirectory("PS1").isEmpty());
}

void PlatformManagerTest::persistsLibraryRootAcrossRestarts()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString libraryRoot = QDir(temporaryDirectory.path()).filePath("ROMs");
    QVERIFY(QDir().mkpath(libraryRoot));

    {
        PlatformManager manager(temporaryDirectory.path());
        QVERIFY(manager.initialize(QStringList{QStringLiteral("N64"), QStringLiteral("Wii")}));
        QVERIFY(manager.setLibraryDirectory(QUrl::fromLocalFile(libraryRoot)));
    }

    PlatformManager reloaded(temporaryDirectory.path());
    QVERIFY(reloaded.initialize(QStringList{QStringLiteral("N64")}));
    QCOMPARE(reloaded.libraryDirectory(), QDir(libraryRoot).absolutePath());
    QVERIFY(QFileInfo(QDir(libraryRoot).filePath("N64")).isDir());
    QVERIFY(QFileInfo(QDir(libraryRoot).filePath("Wii")).isDir());
    QVERIFY(QSettings().contains(QStringLiteral("library/root")));
}

void PlatformManagerTest::rejectsInvalidLibraryLocations()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    PlatformManager manager(temporaryDirectory.path());
    QVERIFY(manager.initialize(QStringList{QStringLiteral("N64")}));

    QVERIFY(!manager.setLibraryDirectory(QUrl(QStringLiteral("https://example.com/roms"))));
    QVERIFY(!manager.setLibraryDirectory(
        QUrl::fromLocalFile(QDir(temporaryDirectory.path()).filePath("missing"))));
    QVERIFY(!manager.libraryConfigured());
}

QTEST_GUILESS_MAIN(PlatformManagerTest)
#include "PlatformManagerTest.moc"
