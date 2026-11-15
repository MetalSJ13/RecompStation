#include <QtTest>

#include "github/GitHubManager.h"

class GitHubManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesRepositoryResponse()
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

    void parsesLatestReleaseAndAssets()
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

    void rejectsInvalidRepositoryAndReleaseResponses()
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

    void rejectsMissingOutputDestinations()
    {
        QString error;
        QVERIFY(!GitHubManager::parseRepositoryResponse(
            QByteArray("{}"), nullptr, &error));
        QVERIFY(error.contains(QStringLiteral("destination")));

        QVERIFY(!GitHubManager::parseLatestReleaseResponse(
            QByteArray("{}"), nullptr, &error));
        QVERIFY(error.contains(QStringLiteral("destination")));
    }

    void rejectsInvalidRepositoryIdentifiersBeforeRequesting()
    {
        GitHubManager manager;
        QSignalSpy failureSpy(&manager, &GitHubManager::requestFailed);
        QVERIFY(failureSpy.isValid());

        const int requestId = manager.fetchRepository(
            QStringLiteral("owner/name"), QStringLiteral("repo"));

        QCOMPARE(failureSpy.count(), 1);
        QCOMPARE(failureSpy.takeFirst().at(0).toInt(), requestId);
    }
};

QTEST_GUILESS_MAIN(GitHubManagerTest)
#include "GitHubManagerTest.moc"