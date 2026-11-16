#pragma once

#include <QString>
#include <QLoggingCategory>
#include <QtLogging>

Q_DECLARE_LOGGING_CATEGORY(lcApplication)
Q_DECLARE_LOGGING_CATEGORY(lcUI)
Q_DECLARE_LOGGING_CATEGORY(lcSettings)
Q_DECLARE_LOGGING_CATEGORY(lcLibrary)
Q_DECLARE_LOGGING_CATEGORY(lcMetadata)
Q_DECLARE_LOGGING_CATEGORY(lcGitHub)
Q_DECLARE_LOGGING_CATEGORY(lcLauncher)
Q_DECLARE_LOGGING_CATEGORY(lcGameLaunch)

namespace RecompLogging
{
bool initialize(const QString &logDirectory);
QString logFilePath();
}
