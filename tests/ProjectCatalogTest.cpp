#include <QtTest>

#include "projects/ProjectCatalog.h"

class ProjectCatalogTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesValidCatalog();
    void acceptsZipPackageFormat();
    void parsesDevelopmentStatus();
    void derivesSupportedPlatformsFromProjects();
    void rejectsUnknownDevelopmentStatus();
    void rejectsMalformedCatalog();
    void matchesGameByTitleAndPlatform();
    void matchesNewN64ProjectsAndMultiplePorts();
    void matchesTenNewProjectsAndPlatforms();
    void selectsOnlyPublishedLinuxPackagesForTenNewProjects();
    void selectsOnlyPublishedLinuxPackagesForNewProjects();
    void selectsOnlyCompatibleLinuxAssets();
    void normalizesArchitectures();
    void explainsMissingCompatibleAssets();
    void filtersReleaseAssetsForCurrentArchitecture();
    void parsesLaunchRecipes();
    void bundledLaunchRecipesAreComplete();
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

void ProjectCatalogTest::derivesSupportedPlatformsFromProjects()
{
    ProjectCatalog catalog;
    QVERIFY(catalog.isLoaded());
    QCOMPARE(catalog.supportedPlatforms(), (QStringList{QStringLiteral("N64"), QStringLiteral("GameCube"),
                                                        QStringLiteral("PS1"), QStringLiteral("Xbox"),
                                                        QStringLiteral("Xbox 360"), QStringLiteral("SNES")}));
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
    const QVariantList donkeyKongMatches = catalog.projectsForGame(
        QStringLiteral("Donkey Kong 64"), QStringLiteral("N64"));
    QCOMPARE(donkeyKongMatches.size(), 1);
    QCOMPARE(donkeyKongMatches.first().toMap().value("id").toString(),
             QStringLiteral("donkey-kong-64-recompiled"));

    const QVariantList marioMatches = catalog.projectsForGame(
        QStringLiteral("Super Mario 64"), QStringLiteral("N64"));
    QCOMPARE(marioMatches.size(), 2);
    QStringList marioProjectIds;
    for (const QVariant &entry : marioMatches)
        marioProjectIds.append(entry.toMap().value("id").toString());
    QVERIFY(marioProjectIds.contains(QStringLiteral("super-mario-64-recompiled")));
    QVERIFY(marioProjectIds.contains(QStringLiteral("ghostship-super-mario-64")));

    const QVariantList perfectDarkMatches = catalog.projectsForGame(
        QStringLiteral("Perfect Dark"), QStringLiteral("N64"));
    QCOMPARE(perfectDarkMatches.size(), 1);
    QCOMPARE(perfectDarkMatches.first().toMap().value("id").toString(),
             QStringLiteral("perfect-dark-recompiled"));
}

void ProjectCatalogTest::matchesNewN64ProjectsAndMultiplePorts()
{
    ProjectCatalog catalog;
    QVERIFY(catalog.isLoaded());

    const QList<QPair<QString, QString>> expected = {
        {QStringLiteral("Star Fox 64"), QStringLiteral("starfox-64-recompiled")},
        {QStringLiteral("Donkey Kong 64"), QStringLiteral("donkey-kong-64-recompiled")},
        {QStringLiteral("Mario Kart 64"), QStringLiteral("mario-kart-64-recompiled")},
        {QStringLiteral("Mario Party"), QStringLiteral("mario-party-recompiled")},
        {QStringLiteral("Mario Party 2"), QStringLiteral("mario-party-2-recompiled")},
        {QStringLiteral("Mario Party 3"), QStringLiteral("mario-party-3-recompiled")},
        {QStringLiteral("Super Smash Bros."), QStringLiteral("super-smash-bros-recompiled")},
        {QStringLiteral("Banjo-Tooie"), QStringLiteral("banjo-tooie-recompiled")},
        {QStringLiteral("Paper Mario"), QStringLiteral("paper-mario-64-recompiled")}
    };

    for (const auto &item : expected) {
        const QVariantList matches = catalog.projectsForGame(item.first, QStringLiteral("N64"));
        QVERIFY2(matches.size() == 1, qPrintable(item.first));
        QCOMPARE(matches.first().toMap().value(QStringLiteral("id")).toString(), item.second);
    }
}

void ProjectCatalogTest::matchesTenNewProjectsAndPlatforms()
{
    ProjectCatalog catalog;
    QVERIFY(catalog.isLoaded());
    const QList<QPair<QString, QString>> expected = {
        {QStringLiteral("Yoshi's Story"), QStringLiteral("yoshis-story-dinotale")},
        {QStringLiteral("Mega Man 64"), QStringLiteral("mega-man-64-recompiled")},
        {QStringLiteral("F-Zero X"), QStringLiteral("f-zero-x-recompiled")},
        {QStringLiteral("Mario Golf"), QStringLiteral("mario-golf-64-recompiled")},
        {QStringLiteral("The Legend of Zelda: Majora's Mask"), QStringLiteral("zelda64recomp-majoras-mask")},
        {QStringLiteral("The Legend of Zelda: Ocarina of Time"), QStringLiteral("ship-of-harkinian-ocarina-of-time")},
        {QStringLiteral("Pikmin"), QStringLiteral("open-nectar-pikmin")},
        {QStringLiteral("Pikmin 2"), QStringLiteral("open-nectar-pikmin")},
        {QStringLiteral("Super Mario Strikers"), QStringLiteral("super-mario-strikers-recompiled")},
        {QStringLiteral("The Legend of Zelda: Twilight Princess"), QStringLiteral("twilight-princess-dusklight")},
        {QStringLiteral("Luigi's Mansion"), QStringLiteral("luigis-mansion-recompiled")}
    };
    for (const auto &item : expected) {
        const QString platform = (item.second == QStringLiteral("open-nectar-pikmin")
            || item.second == QStringLiteral("super-mario-strikers-recompiled")
            || item.second == QStringLiteral("twilight-princess-dusklight")
            || item.second == QStringLiteral("luigis-mansion-recompiled"))
            ? QStringLiteral("GameCube") : QStringLiteral("N64");
        const QVariantList matches = catalog.projectsForGame(item.first, platform);
        QVERIFY2(matches.size() == 1, qPrintable(item.first));
        QCOMPARE(matches.first().toMap().value(QStringLiteral("id")).toString(), item.second);
    }
    QVERIFY(catalog.projectsForGame(QStringLiteral("The Legend of Zelda: Ocarina of Time"),
                                    QStringLiteral("GameCube")).isEmpty());
}

void ProjectCatalogTest::selectsOnlyPublishedLinuxPackagesForTenNewProjects()
{
    ProjectCatalog catalog;
    const auto accepts = [&catalog](const QString &id, const QString &arch,
                                    const QString &asset) {
        return catalog.isAssetCompatible(id, arch, asset);
    };
    QVERIFY(accepts("yoshis-story-dinotale", "x86_64", "DinoTale-0.1.8-linux.tar.gz"));
    QVERIFY(!accepts("yoshis-story-dinotale", "x86_64", "DinoTale-0.1.8-windows.zip"));
    QVERIFY(accepts("mega-man-64-recompiled", "x86_64", "MegaMan64Recompiled-v0.9.1-Linux-X64.zip"));
    QVERIFY(accepts("mega-man-64-recompiled", "aarch64", "MegaMan64Recompiled-v0.9.1-Linux-ARM64.zip"));
    QVERIFY(!accepts("mega-man-64-recompiled", "x86_64", "MegaMan64Recompiled-v0.9.1-Flatpak-X64.zip"));
    QVERIFY(accepts("f-zero-x-recompiled", "x86_64", "FZeroXRecompiled-v0.1.16-linux-x64.tar.gz"));
    QVERIFY(accepts("mario-golf-64-recompiled", "x86_64", "MarioGolfRecompiled-v0.1.74-linux-x64.tar.gz"));
    QVERIFY(accepts("zelda64recomp-majoras-mask", "x86_64", "Zelda64Recompiled-v1.2.2-Linux-X64.zip"));
    QVERIFY(accepts("zelda64recomp-majoras-mask", "aarch64", "Zelda64Recompiled-v1.2.2-Linux-ARM64.zip"));
    QVERIFY(!accepts("zelda64recomp-majoras-mask", "x86_64", "Zelda64Recompiled-v1.2.2-Linux-Flatpak-X64.zip"));
    QVERIFY(accepts("ship-of-harkinian-ocarina-of-time", "x86_64", "SoH-Dewey-Bravo.appimage"));
    QVERIFY(accepts("open-nectar-pikmin", "x86_64", "Open_Nectar-x86_64.AppImage"));
    QVERIFY(accepts("super-mario-strikers-recompiled", "x86_64", "strikers-linux-x86_64.AppImage"));
    QVERIFY(accepts("super-mario-strikers-recompiled", "aarch64", "strikers-linux-arm64.AppImage"));
    QVERIFY(accepts("twilight-princess-dusklight", "x86_64", "Dusklight-v2.0.3-linux-x86_64.AppImage"));
    QVERIFY(accepts("twilight-princess-dusklight", "aarch64", "Dusklight-v2.0.3-linux-arm64.AppImage"));
    QVERIFY(accepts("luigis-mansion-recompiled", "x86_64", "LuigisMansionRecompiled-1.1.6-linux-x86_64.tar.gz"));
    QVERIFY(!accepts("luigis-mansion-recompiled", "x86_64", "LuigisMansionRecompiled-1.1.6-windows-x64.zip"));
    const QVariantMap luigisMansion = catalog.project(QStringLiteral("luigis-mansion-recompiled"));
    const QVariantMap luigisMansionRule = luigisMansion.value(QStringLiteral("architectures"))
        .toMap().value(QStringLiteral("x86_64")).toMap();
    QCOMPARE(luigisMansionRule.value(QStringLiteral("executablePath")).toString(),
             QStringLiteral("LuigisMansionRecompiled.sh"));
}

void ProjectCatalogTest::selectsOnlyPublishedLinuxPackagesForNewProjects()
{
    ProjectCatalog catalog;
    const auto accepts = [&catalog](const QString &id, const QString &arch,
                                    const QString &asset) {
        return catalog.isAssetCompatible(id, arch, asset);
    };

    QVERIFY(accepts("starfox-64-recompiled", "x86_64",
        "Starfox64Recompiled-v1.0.3-Linux-X64-Release.zip"));
    QVERIFY(accepts("starfox-64-recompiled", "aarch64",
        "Starfox64Recompiled-v1.0.3-Linux-ARM64-Release.zip"));
    QVERIFY(!accepts("starfox-64-recompiled", "x86_64",
        "Starfox64Recompiled-v1.0.3-Flatpak-X64-Release.zip"));
    QVERIFY(accepts("donkey-kong-64-recompiled", "x86_64",
        "DK64Recompiled-Linux-X64-Release-1-0-3.zip"));
    QVERIFY(accepts("donkey-kong-64-recompiled", "aarch64",
        "DK64Recompiled-Linux-ARM64-Release-1-0-3.zip"));
    QVERIFY(accepts("mario-kart-64-recompiled", "x86_64",
        "MarioKart64Recompiled-v0.9.2-Linux-X64-Release.zip"));
    QVERIFY(accepts("mario-kart-64-recompiled", "aarch64",
        "MarioKart64Recompiled-v0.9.2-Linux-ARM64-Release.zip"));

    QVERIFY(accepts("mario-party-recompiled", "x86_64",
        "MarioPartyRecompiled-v0.2.14-linux-x64.tar.gz"));
    QVERIFY(accepts("mario-party-2-recompiled", "x86_64",
        "MarioParty2Recompiled-v0.2.17-linux-x64.tar.gz"));
    QVERIFY(accepts("mario-party-3-recompiled", "x86_64",
        "MarioParty3Recompiled-v0.2.26-linux-x64.tar.gz"));
    QVERIFY(accepts("super-smash-bros-recompiled", "x86_64",
        "SmashBrosRecompiled-v0.1.14-linux-x64.tar.gz"));
    QVERIFY(accepts("ghostship-super-mario-64", "x86_64",
        "Nautilus-Alfa-Linux.zip"));
    QVERIFY(accepts("banjo-tooie-recompiled", "x86_64",
        "BanjoTooieRecompiled-linux-x64.tar.gz"));
    const QVariantMap banjoTooie = catalog.project(QStringLiteral("banjo-tooie-recompiled"));
    const QVariantMap banjoTooieRule = banjoTooie.value(QStringLiteral("architectures"))
        .toMap().value(QStringLiteral("x86_64")).toMap();
    QCOMPARE(banjoTooieRule.value(QStringLiteral("executablePath")).toString(),
             QStringLiteral("run.sh"));
    QVERIFY(accepts("paper-mario-64-recompiled", "x86_64",
        "Paperboat-Mulberry-Delta-Linux.zip"));

    QVERIFY(!accepts("mario-party-recompiled", "x86_64",
        "MarioPartyRecompiled-v0.2.14-windows-x64.zip"));
    QVERIFY(!accepts("super-smash-bros-recompiled", "x86_64",
        "SmashBrosRecompiled-v0.1.14-windows-x64.zip"));
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

    QVERIFY(catalog.isAssetCompatible(
        QStringLiteral("super-mario-64-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("SuperMario64Recompiled-v0.1.13-linux-x64.tar.gz")));
    QVERIFY(!catalog.isAssetCompatible(
        QStringLiteral("super-mario-64-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("SuperMario64Recompiled-v0.1.13-windows-x64.zip")));
    QVERIFY(catalog.isAssetCompatible(
        QStringLiteral("perfect-dark-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("PerfectDarkRecompiled-v0.1.14-linux-x64.tar.gz")));
    QVERIFY(!catalog.isAssetCompatible(
        QStringLiteral("perfect-dark-recompiled"), QStringLiteral("x86_64"),
        QStringLiteral("PerfectDarkRecompiled-v0.1.14-windows-x64.zip")));
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

void ProjectCatalogTest::explainsMissingCompatibleAssets()
{
    ProjectCatalog catalog;
    QVERIFY(catalog.isLoaded());
    if (catalog.currentArchitecture() != QStringLiteral("x86_64"))
        QSKIP("The bundled catalog's packages are described for x86_64.");
    const QString project = QStringLiteral("banjo-kazooie-recompiled");
    auto names = [](const QStringList &list) {
        QVariantList assets;
        for (const QString &name : list)
            assets.append(QVariantMap{{"name", name}, {"downloadUrl", "https://example.test/" + name}});
        return assets;
    };
    auto explain = [&](const QStringList &list) { return catalog.explainNoCompatibleAsset(project, names(list)); };

    QVERIFY(explain({}).contains(QStringLiteral("no downloads")));
    QVERIFY(explain({"Game-Windows-x64.zip", "Game-macOS.dmg"}).contains(QStringLiteral("Windows and macOS only")));
    QVERIFY(explain({"Game-darwin-universal.zip"}).contains(QStringLiteral("macOS only")));
    QVERIFY(!explain({"Game-darwin-universal.zip"}).contains(QStringLiteral("Windows")));
    QVERIFY(explain({"Game-Linux-aarch64.tar.gz"}).contains(QStringLiteral("only for ARM")));
    QVERIFY(explain({"Game.flatpak", "Game-Windows.zip"}).contains(QStringLiteral("Flatpak")));
    QVERIFY(explain({"checksums.txt"}).contains(QStringLiteral("x86_64")));
    QVERIFY(catalog.explainNoCompatibleAsset(QStringLiteral("not-a-project"), names({"a.zip"}))
                .contains(QStringLiteral("catalog")));
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

namespace {
QByteArray catalogWithLaunch(const QByteArray &launch)
{
    return QByteArray(R"({"schemaVersion": 1, "projects": [{
        "id": "sample-project", "name": "Sample Recompiled", "developmentStatus": "playable",
        "platform": "N64", "gameTitles": ["Sample Game"],
        "repositoryOwner": "ExampleOrg", "repositoryName": "SampleProject",
        "architectures": {"x86_64": {"assetNameRegex": "^sample\\.tar\\.gz$",
                                      "packageFormat": "tar.gz", "executablePath": "sample"}})")
        + (launch.isEmpty() ? QByteArray() : QByteArray(", \"launch\": ") + launch)
        + QByteArray("}]}");
}
}

void ProjectCatalogTest::parsesLaunchRecipes()
{
    QVariantList projects;
    QString error;
    QVERIFY2(ProjectCatalog::parseCatalog(catalogWithLaunch({}), &projects, &error), qPrintable(error));
    QCOMPARE(projects.first().toMap().value("launch").toMap().value("method").toString(),
             QStringLiteral("manual"));

    const QByteArray valid = R"({"method": "n64recomp", "dataFolder": "SampleRecompiled",
        "romFileName": "sample.n64.us.1.0.z64", "romName": "SAMPLE", "romXxh3": "1b67585d56e07f8c",
        "romVersion": "North America 1.0", "graphicsFile": "graphics.json"})";
    QVERIFY2(ProjectCatalog::parseCatalog(catalogWithLaunch(valid), &projects, &error), qPrintable(error));
    const QVariantMap launch = projects.first().toMap().value("launch").toMap();
    QCOMPARE(launch.value("dataFolder").toString(), QStringLiteral("SampleRecompiled"));
    QCOMPARE(launch.value("romXxh3").toString(), QStringLiteral("1B67585D56E07F8C"));

    const QList<QByteArray> invalid = {
        R"({"method": "script"})",
        R"({"method": "n64recomp", "dataFolder": "../evil", "romFileName": "a.z64", "romName": "A", "romXxh3": "1B67585D56E07F8C"})",
        R"({"method": "n64recomp", "dataFolder": ".config", "romFileName": "a.z64", "romName": "A", "romXxh3": "1B67585D56E07F8C"})",
        R"({"method": "n64recomp", "dataFolder": "Ok", "romFileName": "sub/a.z64", "romName": "A", "romXxh3": "1B67585D56E07F8C"})",
        R"({"method": "n64recomp", "dataFolder": "Ok", "romFileName": "a.exe", "romName": "A", "romXxh3": "1B67585D56E07F8C"})",
        R"({"method": "n64recomp", "dataFolder": "Ok", "romFileName": "a.z64", "romName": "A"})",
        R"({"method": "n64recomp", "dataFolder": "Ok", "romFileName": "a.z64", "romName": "A", "romXxh3": "XYZ"})",
        R"({"method": "harbourmasters", "dataFolder": "ok", "romFileName": "a.z64", "romName": "A", "dataFiles": ["../saves"]})",
        R"("n64recomp")",
        // New recipe kinds: nothing beyond flags, placeholders and folder settings.
        R"({"method": "romfolder", "dataFolder": "Ok", "romFileName": "a/b/c.z64", "romXxh3": "1B67585D56E07F8C"})",
        R"({"method": "romfolder", "dataFolder": "Ok", "romFileName": "a.z64"})",
        R"({"method": "romfolder", "dataFolder": "Ok", "romFileName": "a.z64", "romXxh3": "1B67585D56E07F8C", "environment": {"LD_PRELOAD": "{dataFolder}"}})",
        R"({"method": "romfolder", "dataFolder": "Ok", "romFileName": "a.z64", "romXxh3": "1B67585D56E07F8C", "environment": {"PATH": "{dataFolder}"}})",
        R"({"method": "romfolder", "dataFolder": "Ok", "romFileName": "a.z64", "romXxh3": "1B67585D56E07F8C", "environment": {"GAME_DIR": "/tmp"}})",
        R"({"method": "romfolder", "dataFolder": "Ok", "romFileName": "a.z64", "romXxh3": "1B67585D56E07F8C", "arguments": ["; rm -rf ~"]})",
        R"({"method": "romfolder", "dataFolder": "Ok", "romFileName": "a.z64", "romXxh3": "1B67585D56E07F8C", "arguments": ["{gamePath}"]})",
        R"({"method": "romfolder", "dataFolder": "Ok", "romFileName": "a.z64", "romXxh3": "1B67585D56E07F8C", "savesSubfolder": "../x"})",
        R"({"method": "n64recomp", "dataFolder": "Ok", "romFileName": "a.z64", "romName": "A", "romXxh3": "1B67585D56E07F8C", "arguments": ["--x"]})",
        R"({"method": "discargument", "arguments": ["--disc"]})",
        R"({"method": "discargument", "arguments": ["--disc", "{gamePath}", "--exec=/bin/sh"]})",
        R"({"method": "discargument", "arguments": ["--disc", "{gamePath}"], "fileExtensions": ["is o"]})"
    };
    for (const QByteArray &launchJson : invalid)
        QVERIFY2(!ProjectCatalog::parseCatalog(catalogWithLaunch(launchJson), &projects, &error),
                 launchJson.constData());

    const QByteArray romFolder = R"({"method": "romfolder", "dataFolder": "Sample",
        "romFileName": "rom/sample.z64", "romXxh3": "1b67585d56e07f8c", "romVersion": "North America",
        "arguments": ["{romPath}"], "environment": {"SAMPLE_DATA_DIR": "{dataFolder}"},
        "dataFiles": ["saves"], "savesSubfolder": "userdata/saves"})";
    QVERIFY2(ProjectCatalog::parseCatalog(catalogWithLaunch(romFolder), &projects, &error), qPrintable(error));
    const QVariantMap folderRecipe = projects.first().toMap().value("launch").toMap();
    QCOMPARE(folderRecipe.value("arguments").toStringList(), QStringList{QStringLiteral("{romPath}")});
    QCOMPARE(folderRecipe.value("environment").toMap().value("SAMPLE_DATA_DIR").toString(),
             QStringLiteral("{dataFolder}"));
    QCOMPARE(folderRecipe.value("savesSubfolder").toString(), QStringLiteral("userdata/saves"));

    const QByteArray disc = R"({"method": "discargument", "arguments": ["--disc", "{gamePath}"],
        "fileExtensions": ["ISO", "rvz"], "firstStartNote": "Takes a while."})";
    QVERIFY2(ProjectCatalog::parseCatalog(catalogWithLaunch(disc), &projects, &error), qPrintable(error));
    const QVariantMap discRecipe = projects.first().toMap().value("launch").toMap();
    QCOMPARE(discRecipe.value("fileExtensions").toStringList(),
             (QStringList{QStringLiteral("iso"), QStringLiteral("rvz")}));
}

void ProjectCatalogTest::bundledLaunchRecipesAreComplete()
{
    ProjectCatalog catalog;
    QVERIFY2(catalog.isLoaded(), qPrintable(catalog.errorString()));
    int prepared = 0;
    for (const QVariant &entry : catalog.projects()) {
        const QVariantMap launch = entry.toMap().value("launch").toMap();
        const QString method = launch.value("method").toString();
        QVERIFY(!method.isEmpty());
        // Every project names the game file it needs, briefly.
        const QString romFile = entry.toMap().value("romFile").toString();
        QVERIFY2(!romFile.isEmpty() && romFile.size() <= 60,
                 qPrintable(entry.toMap().value("id").toString()));
        if (method == QStringLiteral("n64recomp")) {
            QVERIFY(!launch.value("romXxh3").toString().isEmpty());
            QVERIFY(!launch.value("romVersion").toString().isEmpty());
            ++prepared;
        }
    }
    QVERIFY(prepared >= 17);
    QCOMPARE(catalog.project(QStringLiteral("pokemon-snap-recompiled"))
                 .value("launch").toMap().value("method").toString(), QStringLiteral("romfolder"));
    QCOMPARE(catalog.project(QStringLiteral("snowboard-kids-recompiled"))
                 .value("launch").toMap().value("method").toString(), QStringLiteral("romfolder"));
    QCOMPARE(catalog.project(QStringLiteral("luigis-mansion-recompiled"))
                 .value("launch").toMap().value("method").toString(), QStringLiteral("discargument"));
    for (const QString &id : {QStringLiteral("super-metroid-recompiled"), QStringLiteral("mega-man-x-recompiled")}) {
        const QVariantMap launch = catalog.project(id).value("launch").toMap();
        QCOMPARE(launch.value("method").toString(), QStringLiteral("fileargument"));
        QCOMPARE(launch.value("arguments").toStringList(), (QStringList{QStringLiteral("--rom"), QStringLiteral("{gamePath}")}));
    }
    QCOMPARE(catalog.project(QStringLiteral("banjo-kazooie-recompiled"))
                 .value("launch").toMap().value("romFileName").toString(),
             QStringLiteral("bk.n64.us.1.0.z64"));
}

QTEST_GUILESS_MAIN(ProjectCatalogTest)
#include "ProjectCatalogTest.moc"
