#include <QtTest>

#include "projects/ProjectCatalog.h"

class ProjectCatalogTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesValidCatalog();
    void acceptsZipPackageFormat();
    void parsesDevelopmentStatus();
    void rejectsUnknownDevelopmentStatus();
    void rejectsMalformedCatalog();
    void matchesGameByTitleAndPlatform();
    void selectsOnlyCompatibleLinuxAssets();
    void normalizesArchitectures();
    void filtersReleaseAssetsForCurrentArchitecture();
};

void ProjectCatalogTest::parsesValidCatalog()
{
    const QByteArray json = R"({
        "schemaVersion": 1,
        "projects": [{
            "id": "sample-project",
            "name": "Sample Recompiled",
            "developmentStatus": "playable",
            "platform": "N64",
            "gameTitles": ["Sample Game"],
            "repositoryOwner": "ExampleOrg",
            "repositoryName": "SampleProject",
            "romRequirements": "Specific ROM revision required.",
            "architectures": {
                "x86_64": {
                    "assetNameRegex": "^sample-linux-x64\\.tar\\.gz$",
                    "packageFormat": "tar.gz",
                    "executablePath": "bin/sample"
                }
            }
        }]
    })";

    QVariantList projects;
    QString error;
    QVERIFY2(ProjectCatalog::parseCatalog(json, &projects, &error), qPrintable(error));
    QCOMPARE(projects.size(), 1);
    const QVariantMap project = projects.first().toMap();
    QCOMPARE(project.value("id").toString(), QStringLiteral("sample-project"));
    QCOMPARE(project.value("repositoryOwner").toString(), QStringLiteral("ExampleOrg"));
    QCOMPARE(project.value("repositoryName").toString(), QStringLiteral("SampleProject"));
}


void ProjectCatalogTest::acceptsZipPackageFormat()
{
    const QByteArray json = R"({
        "schemaVersion": 1,
        "projects": [{
            "id": "zip-project",
            "name": "ZIP Recompiled",
            "developmentStatus": "beta",
            "platform": "N64",
            "gameTitles": ["ZIP Game"],
            "repositoryOwner": "ExampleOrg",
            "repositoryName": "ZipProject",
            "architectures": {
                "x86_64": {
                    "assetNameRegex": "^ZipProject-Linux-X64\\.zip$",
                    "packageFormat": "zip",
                    "executablePath": "ZipProject/ZipProject.AppImage"
                }
            }
        }]
    })";

    QVariantList projects;
    QString error;
    QVERIFY2(ProjectCatalog::parseCatalog(json, &projects, &error), qPrintable(error));
    QCOMPARE(projects.size(), 1);
    const QVariantMap rule = projects.first().toMap()
        .value(QStringLiteral("architectures")).toMap()
        .value(QStringLiteral("x86_64")).toMap();
    QCOMPARE(rule.value(QStringLiteral("packageFormat")).toString(),
             QStringLiteral("zip"));
}


void ProjectCatalogTest::parsesDevelopmentStatus()
{
    ProjectCatalog catalog;
    QVERIFY(catalog.isLoaded());
    const QVariantList matches = catalog.projectsForGame(
        QStringLiteral("Conker's Bad Fur Day"), QStringLiteral("N64"));
    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.first().toMap().value(QStringLiteral("developmentStatus")).toString(),
             QStringLiteral("beta"));

    const QVariantList snapMatches = catalog.projectsForGame(
        QStringLiteral("Pokemon Snap"), QStringLiteral("N64"));
    QCOMPARE(snapMatches.size(), 1);
    QCOMPARE(snapMatches.first().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("pokemon-snap-recompiled"));
    QCOMPARE(snapMatches.first().toMap().value(QStringLiteral("developmentStatus")).toString(),
             QStringLiteral("beta"));
}

void ProjectCatalogTest::rejectsUnknownDevelopmentStatus()
{
    const QByteArray json = R"({
        "schemaVersion": 1,
        "projects": [{
            "id": "bad-status", "name": "Bad Status", "platform": "N64",
            "developmentStatus": "experimental-ish",
            "gameTitles": ["Bad Status Game"],
            "repositoryOwner": "Example", "repositoryName": "BadStatus",
            "architectures": {"x86_64": {
                "assetNameRegex": "^bad$",
                "packageFormat": "tar.gz",
                "executablePath": "bad"
            }}
        }]
    })";
    QVariantList projects;
    QString error;
    QVERIFY(!ProjectCatalog::parseCatalog(json, &projects, &error));
    QVERIFY(error.contains(QStringLiteral("valid required fields")));
}

void ProjectCatalogTest::rejectsMalformedCatalog()
{
    QVariantList projects;
    QString error;
    QVERIFY(!ProjectCatalog::parseCatalog("{invalid", &projects, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!ProjectCatalog::parseCatalog(
        R"({"schemaVersion":2,"projects":[]})", &projects, &error));
    QVERIFY(error.contains(QStringLiteral("unsupported schema")));
    QVERIFY(!ProjectCatalog::parseCatalog(
        R"({"schemaVersion":1,"projects":[{"id":"../bad"}]})", &projects, &error));
    QVERIFY(!ProjectCatalog::parseCatalog(
        R"({"schemaVersion":1,"projects":[{
            "id":"unsafe-project","name":"Unsafe","platform":"N64",
            "gameTitles":["Unsafe"],"repositoryOwner":"Example","repositoryName":"Unsafe",
            "architectures":{"x86_64":{
                "assetNameRegex":"^unsafe$","packageFormat":"tar.gz",
                "executablePath":"../../outside"
            }}
        }]})", &projects, &error));
}

void ProjectCatalogTest::matchesGameByTitleAndPlatform()
{
    ProjectCatalog catalog;
    QVERIFY2(catalog.isLoaded(), qPrintable(catalog.errorString()));

    const QVariantList matches =
        catalog.projectsForGame(QStringLiteral("Banjo Kazooie"), QStringLiteral("N64"));
    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.first().toMap().value("id").toString(),
             QStringLiteral("banjo-kazooie-recompiled"));
    QVERIFY(catalog.projectsForGame(
        QStringLiteral("Banjo-Kazooie"), QStringLiteral("PS1")).isEmpty());
    QVERIFY(catalog.projectsForGame(
        QStringLiteral("Donkey Kong 64"), QStringLiteral("N64")).isEmpty());
}

void ProjectCatalogTest::selectsOnlyCompatibleLinuxAssets()
{
    ProjectCatalog catalog;
    QVERIFY(catalog.isAssetCompatible(
        QStringLiteral("banjo-kazooie-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("BanjoRecompiled-v1.0.2-Linux-X64.tar.gz")));
    QVERIFY(catalog.isAssetCompatible(
        QStringLiteral("banjo-kazooie-recompiled"), QStringLiteral("aarch64"),
        QStringLiteral("BanjoRecompiled-v1.0.2-Linux-ARM64.tar.gz")));
    QVERIFY(!catalog.isAssetCompatible(
        QStringLiteral("banjo-kazooie-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("BanjoRecompiled-v1.0.2-Windows.zip")));
    QVERIFY(!catalog.isAssetCompatible(
        QStringLiteral("banjo-kazooie-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("BanjoRecompiled-v1.0.2-Linux-Flatpak-X64.zip")));
    QVERIFY(!catalog.isAssetCompatible(
        QStringLiteral("banjo-kazooie-recompiled"), QStringLiteral("riscv64"),
        QStringLiteral("BanjoRecompiled-v1.0.2-Linux-X64.tar.gz")));
    QVERIFY(catalog.isAssetCompatible(
        QStringLiteral("conkers-bad-fur-day-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("ConkerRecomp-Linux-X64.tar.gz")));
    QVERIFY(!catalog.isAssetCompatible(
        QStringLiteral("conkers-bad-fur-day-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("ConkerRecomp-Windows-X64.zip")));
    QVERIFY(catalog.isAssetCompatible(
        QStringLiteral("diddy-kong-racing-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("DKR-R-1.0.15-Linux-x86_64.AppImage")));
    QVERIFY(catalog.isAssetCompatible(
        QStringLiteral("diddy-kong-racing-recompiled"), QStringLiteral("aarch64"),
        QStringLiteral("DKR-R-1.0.15-Linux-aarch64.AppImage")));

    // Conker's published archive has a versioned filename and a top-level
    // directory containing ConkerRecomp.
    QVERIFY(catalog.isAssetCompatible(
        QStringLiteral("conkers-bad-fur-day-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("ConkerRecompiled-V0.1.6-Linux.tar.gz")));
    QVERIFY(!catalog.isAssetCompatible(
        QStringLiteral("conkers-bad-fur-day-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("ConkerRecompiled-V0.1.6-Linux-Flatpak.tar.gz")));

    // Snap64 publishes separate x86_64 and ARM64 Linux archives, alongside
    // Flatpak assets that must never be offered by RecompStation.
    QVERIFY(catalog.isAssetCompatible(
        QStringLiteral("pokemon-snap-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("Snap64Recomp-1.1.7-linux-x86_64.tar.gz")));
    QVERIFY(catalog.isAssetCompatible(
        QStringLiteral("pokemon-snap-recompiled"), QStringLiteral("aarch64"),
        QStringLiteral("Snap64Recomp-1.1.7-linux-arm64.tar.gz")));
    QVERIFY(!catalog.isAssetCompatible(
        QStringLiteral("pokemon-snap-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("Snap64Recomp-1.1.7-linux-x86_64.flatpak")));
}

void ProjectCatalogTest::filtersReleaseAssetsForCurrentArchitecture()
{
    ProjectCatalog catalog;
    QVERIFY(catalog.isLoaded());

    const QVariantList assets = {
        QVariantMap{{"name", "BanjoRecompiled-v1.0.2-Linux-X64.tar.gz"},
                    {"downloadUrl", "https://example.test/linux.tar.gz"}},
        QVariantMap{{"name", "BanjoRecompiled-v1.0.2-Windows.zip"},
                    {"downloadUrl", "https://example.test/windows.zip"}},
        QVariantMap{{"name", "checksums.txt"},
                    {"downloadUrl", "https://example.test/checksums.txt"}}
    };

    const QVariantList compatible = catalog.compatibleAssets(
        QStringLiteral("banjo-kazooie-recompiled"), assets);
    const QString architecture = catalog.currentArchitecture();
    if (architecture == QStringLiteral("x86_64")) {
        QCOMPARE(compatible.size(), 1);
        QCOMPARE(compatible.first().toMap().value("name").toString(),
                 QStringLiteral("BanjoRecompiled-v1.0.2-Linux-X64.tar.gz"));
    } else if (architecture == QStringLiteral("aarch64")) {
        QVERIFY(compatible.isEmpty());
    } else {
        QVERIFY(compatible.isEmpty());
    }
}

void ProjectCatalogTest::normalizesArchitectures()
{
    QCOMPARE(ProjectCatalog::normalizeArchitecture(QStringLiteral("x86_64")),
             QStringLiteral("x86_64"));
    QCOMPARE(ProjectCatalog::normalizeArchitecture(QStringLiteral("amd64")),
             QStringLiteral("x86_64"));
    QCOMPARE(ProjectCatalog::normalizeArchitecture(QStringLiteral("arm64")),
             QStringLiteral("aarch64"));
    QVERIFY(ProjectCatalog::normalizeArchitecture(QStringLiteral("riscv64")).isEmpty());
}

QTEST_GUILESS_MAIN(ProjectCatalogTest)
#include "ProjectCatalogTest.moc"
