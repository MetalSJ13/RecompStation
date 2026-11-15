#pragma once

#include <QObject>

class GitHubManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesRepositoryResponse();
    void parsesLatestReleaseAndAssets();
    void convertsReleaseToQmlData();
    void rejectsInvalidRepositoryAndReleaseResponses();
    void rejectsMissingOutputDestinations();
    void rejectsInvalidRepositoryIdentifiersBeforeRequesting();
};
