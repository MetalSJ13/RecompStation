#include <QtTest>

#include "core/PlatformManager.h"

#include <QDir>
#include <QFile>
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
    void migratesLegacySettingsIntoApplicationConfig();
    void missingConfigRequiresFirstRunSetup();
    void rejectsInvalidLibraryLocations();
    void listsLibraryDirectoriesAndCreatesFoldersSafely();
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
    QVERIFY(manager.platforms().contains(QStringLiteral("Xbox")));
    QVERIFY(QFileInfo(QDir(temporaryDirectory.path())
                          .filePath("launchers/Xbox")).isDir());
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
    QSettings settings(reloaded.settingsFilePath(), QSettings::IniFormat);
    QVERIFY(settings.contains(QStringLiteral("library/root")));
    QVERIFY(QFileInfo(reloaded.settingsFilePath()).isFile());
}


void PlatformManagerTest::migratesLegacySettingsIntoApplicationConfig()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString libraryRoot = QDir(temporaryDirectory.path()).filePath("LegacyROMs");
    QVERIFY(QDir().mkpath(libraryRoot));
    QSettings legacySettings;
    legacySettings.setValue(QStringLiteral("library/root"), libraryRoot);
    legacySettings.setValue(QStringLiteral("appearance/theme"),
                            QStringLiteral("electric-blue"));
    legacySettings.sync();

    PlatformManager manager(QDir(temporaryDirectory.path()).filePath("app-data"));
    QVERIFY(manager.initialize(QStringList{QStringLiteral("N64")}));
    QCOMPARE(manager.libraryDirectory(), QDir(libraryRoot).absolutePath());

    QSettings migrated(manager.settingsFilePath(), QSettings::IniFormat);
    QCOMPARE(migrated.value(QStringLiteral("appearance/theme")).toString(),
             QStringLiteral("electric-blue"));
    QCOMPARE(migrated.value(QStringLiteral("library/root")).toString(),
             QDir(libraryRoot).absolutePath());
    legacySettings.sync();
    QVERIFY(legacySettings.allKeys().isEmpty());
}

void PlatformManagerTest::missingConfigRequiresFirstRunSetup()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString appData = QDir(temporaryDirectory.path()).filePath("app-data");
    const QString libraryRoot = QDir(temporaryDirectory.path()).filePath("ROMs");
    QVERIFY(QDir().mkpath(libraryRoot));

    QString settingsPath;
    {
        PlatformManager manager(appData);
        QVERIFY(manager.initialize(QStringList{QStringLiteral("N64")}));
        settingsPath = manager.settingsFilePath();
        QVERIFY(manager.setLibraryDirectory(QUrl::fromLocalFile(libraryRoot)));
    }

    QVERIFY(QFile::remove(settingsPath));
    PlatformManager freshManager(appData);
    QVERIFY(freshManager.initialize(QStringList{QStringLiteral("N64")}));
    QVERIFY(!freshManager.libraryConfigured());
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

void PlatformManagerTest::listsLibraryDirectoriesAndCreatesFoldersSafely()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString root = QDir(temporaryDirectory.path())
                             .filePath(QStringLiteral("Game Library"));
    const QString existing = QDir(root).filePath(QStringLiteral("Existing Games"));
    QVERIFY(QDir().mkpath(existing));

    const QString appData = QDir(temporaryDirectory.path())
                                .filePath(QStringLiteral("app-data"));
    PlatformManager manager(appData);
    QVERIFY(manager.initialize());

    QCOMPARE(manager.homeDirectory(), QDir::homePath());
    QCOMPARE(manager.parentDirectory(existing), QDir(root).absolutePath());

    const QVariantList entries = manager.libraryDirectories(root);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Existing Games"));
    QCOMPARE(entries.first().toMap().value(QStringLiteral("path")).toString(),
             QFileInfo(existing).absoluteFilePath());

    QVERIFY(manager.createLibraryFolder(root, QStringLiteral("New Library")));
    QVERIFY(QFileInfo(QDir(root).filePath(QStringLiteral("New Library"))).isDir());
    QVERIFY(!manager.createLibraryFolder(root, QStringLiteral("New Library")));
    QVERIFY(!manager.createLibraryFolder(root, QStringLiteral("../outside")));
    QVERIFY(!manager.createLibraryFolder(root, QStringLiteral("nested/name")));
    QVERIFY(!manager.createLibraryFolder(root, QStringLiteral("")));
    QVERIFY(!QFileInfo::exists(QDir(temporaryDirectory.path()).filePath("outside")));
}

QTEST_GUILESS_MAIN(PlatformManagerTest)
#include "PlatformManagerTest.moc"
