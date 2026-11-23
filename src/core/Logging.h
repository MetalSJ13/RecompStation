#pragma once

#include <QString>
#include <QStringList>
#include <QLoggingCategory>
#include <QtLogging>

Q_DECLARE_LOGGING_CATEGORY(lcApplication)
Q_DECLARE_LOGGING_CATEGORY(lcUI)
Q_DECLARE_LOGGING_CATEGORY(lcSettings)
Q_DECLARE_LOGGING_CATEGORY(lcLibrary)
Q_DECLARE_LOGGING_CATEGORY(lcDatabase)
Q_DECLARE_LOGGING_CATEGORY(lcArtwork)
Q_DECLARE_LOGGING_CATEGORY(lcMetadata)
Q_DECLARE_LOGGING_CATEGORY(lcGitHub)
Q_DECLARE_LOGGING_CATEGORY(lcLauncher)
Q_DECLARE_LOGGING_CATEGORY(lcGameLaunch)

// The runtime log (logs/runtime.log) is what people attach to bug reports,
// so it is written to be read by someone who wasn't there:
//  - every session starts with a header block (version, system, display,
//    settings, library, launchers) and ends with a footer;
//  - one line per event: "2026-11-23 15:14:02.239 INFO  launch    message";
//  - the home folder is written as "~", so paths don't reveal user names;
//  - debug messages are off unless detailed logging is on (Settings ›
//    Application, or RECOMPSTATION_DEBUG=1); QT_LOGGING_RULES still wins.
namespace RecompLogging
{
bool initialize(const QString &logDirectory);
QString logFilePath();
QString logDirectory();

void setDetailedLogging(bool enabled);
bool detailedLogging();

// Writes lines exactly as given (after home-folder redaction), for the
// session header and footer and for launcher output excerpts.
void writeBlock(const QStringList &lines);

// "~/..." for paths under the home folder.
QString redactHome(const QString &text);
}
