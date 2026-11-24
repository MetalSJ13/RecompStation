#include <QtTest>

#include "GitHubManagerTest.h"
#include "github/GitHubManager.h"

void GitHubManagerTest::parsesRepositoryResponse()
{
    const QByteArray json = R"({
        "name": "n64-recomp",
        "full_name": "ExampleOrg/n64-recomp",
        "owner": {"login": "ExampleOrg"},
        "description": "A native recompilation project.",
        "html_url": "https://github.com/ExampleOrg/n64-recomp",
        "default_branch": "main",
        "private": false
    })";

    GitHubRepository repository;
    QString error;
    QVERIFY2(GitHubManager::parseRepositoryResponse(json, &repository, &error),
             qPrintable(error));

    QCOMPARE(repository.name, QStringLiteral("n64-recomp"));
    QCOMPARE(repository.fullName, QStringLiteral("ExampleOrg/n64-recomp"));
    QCOMPARE(repository.owner, QStringLiteral("ExampleOrg"));
    QCOMPARE(repository.description,
             QStringLiteral("A native recompilation project."));
    QCOMPARE(repository.defaultBranch, QStringLiteral("main"));
    QVERIFY(!repository.isPrivate);
}

void GitHubManagerTest::parsesLatestReleaseAndAssets()
{
    const QByteArray json = R"({
        "tag_name": "v1.2.0",
        "name": "Version 1.2.0",
        "html_url": "https://github.com/ExampleOrg/n64-recomp/releases/tag/v1.2.0",
        "published_at": "2026-10-01T12:00:00Z",
        "assets": [
            {
                "name": "n64-recomp-linux-x64.tar.gz",
                "browser_download_url": "https://github.com/ExampleOrg/n64-recomp/releases/download/v1.2.0/n64-recomp-linux-x64.tar.gz",
                "content_type": "application/gzip",
                "size": 123456
            },
            {
                "name": "checksums.txt",
                "browser_download_url": "https://github.com/ExampleOrg/n64-recomp/releases/download/v1.2.0/checksums.txt",
                "content_type": "text/plain",
                "size": 128
            }
        ]
    })";

    GitHubRelease release;
    QString error;
    QVERIFY2(GitHubManager::parseLatestReleaseResponse(json, &release, &error),
             qPrintable(error));

    QCOMPARE(release.tagName, QStringLiteral("v1.2.0"));
    QCOMPARE(release.name, QStringLiteral("Version 1.2.0"));
    QCOMPARE(release.assets.size(), 2);
    QCOMPARE(release.assets.first().name,
             QStringLiteral("n64-recomp-linux-x64.tar.gz"));
    QCOMPARE(release.assets.first().contentType,
             QStringLiteral("application/gzip"));
    QCOMPARE(release.assets.first().size, qint64(123456));
    QVERIFY(release.assets.first().downloadUrl.startsWith(
        QStringLiteral("https://")));
}

void GitHubManagerTest::picksNewestNonDraftReleaseFromList()
{
    const QByteArray json = R"([
        {"tag_name": "v0.10-draft", "draft": true, "prerelease": true,
         "html_url": "https://github.com/ExampleOrg/xbox-recomp/releases/tag/v0.10-draft", "assets": []},
        {"tag_name": "v0.9.68", "draft": false, "prerelease": true,
         "html_url": "https://github.com/ExampleOrg/xbox-recomp/releases/tag/v0.9.68",
         "assets": [{"name": "Game-x86_64.AppImage",
                     "browser_download_url": "https://github.com/ExampleOrg/xbox-recomp/releases/download/v0.9.68/Game-x86_64.AppImage",
                     "content_type": "application/octet-stream", "size": 1000}]},
        {"tag_name": "v0.9.67", "draft": false, "prerelease": true,
         "html_url": "https://github.com/ExampleOrg/xbox-recomp/releases/tag/v0.9.67", "assets": []}
    ])";

    GitHubRelease release;
    QString error;
    QVERIFY2(GitHubManager::parseNewestReleaseFromList(json, &release, &error), qPrintable(error));
    QCOMPARE(release.tagName, QStringLiteral("v0.9.68"));
    QCOMPARE(release.assets.size(), 1);

    QVERIFY(!GitHubManager::parseNewestReleaseFromList("[]", &release, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!GitHubManager::parseNewestReleaseFromList("{}", &release, &error));
}

void GitHubManagerTest::convertsReleaseToQmlData()
{
    GitHubRelease release;
    release.tagName = QStringLiteral("v1.2.3");
    release.name = QStringLiteral("Release 1.2.3");
    release.htmlUrl = QStringLiteral("https://github.com/example/project/releases/tag/v1.2.3");
    release.publishedAt = QStringLiteral("2026-11-01T12:00:00Z");
    release.assets.append({
        QStringLiteral("app-linux-x64.tar.gz"),
        QStringLiteral("https://github.com/example/project/releases/download/v1.2.3/app-linux-x64.tar.gz"),
        QStringLiteral("application/gzip"),
        987654
    });

    const QVariantMap result = GitHubManager::releaseToVariantMap(release);
    QCOMPARE(result.value(QStringLiteral("tagName")).toString(), QStringLiteral("v1.2.3"));
    const QVariantList assets = result.value(QStringLiteral("assets")).toList();
    QCOMPARE(assets.size(), 1);
    const QVariantMap asset = assets.first().toMap();
    QCOMPARE(asset.value(QStringLiteral("name")).toString(),
             QStringLiteral("app-linux-x64.tar.gz"));
    QCOMPARE(asset.value(QStringLiteral("size")).toLongLong(), qint64(987654));
}

void GitHubManagerTest::rejectsInvalidRepositoryAndReleaseResponses()
{
    GitHubRepository repository;
    GitHubRelease release;
    QString error;

    QVERIFY(!GitHubManager::parseRepositoryResponse(
        QByteArray("{invalid"), &repository, &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(!GitHubManager::parseRepositoryResponse(
        QByteArray(R"({"name":"missing-fields"})"), &repository, &error));
    QVERIFY(error.contains(QStringLiteral("required repository fields")));

    QVERIFY(!GitHubManager::parseLatestReleaseResponse(
        QByteArray(R"({"name":"No tag"})"), &release, &error));
    QVERIFY(error.contains(QStringLiteral("tag")));
}

void GitHubManagerTest::rejectsMissingOutputDestinations()
{
    QString error;
    QVERIFY(!GitHubManager::parseRepositoryResponse(
        QByteArray("{}"), nullptr, &error));
    QVERIFY(error.contains(QStringLiteral("destination")));

    QVERIFY(!GitHubManager::parseLatestReleaseResponse(
        QByteArray("{}"), nullptr, &error));
    QVERIFY(error.contains(QStringLiteral("destination")));
}

void GitHubManagerTest::rejectsInvalidRepositoryIdentifiersBeforeRequesting()
{
    GitHubManager manager;
    QSignalSpy failureSpy(&manager, &GitHubManager::requestFailed);
    QVERIFY(failureSpy.isValid());

    const int requestId = manager.fetchRepository(
        QStringLiteral("owner/name"), QStringLiteral("repo"));

    QCOMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.takeFirst().at(0).toInt(), requestId);
}

QTEST_GUILESS_MAIN(GitHubManagerTest)
