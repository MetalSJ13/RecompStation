#include "Logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

#include <cstdio>
#include <cstdlib>

Q_LOGGING_CATEGORY(lcApplication, "recomp.app")
Q_LOGGING_CATEGORY(lcUI, "recomp.ui")
Q_LOGGING_CATEGORY(lcSettings, "recomp.settings")
Q_LOGGING_CATEGORY(lcLibrary, "recomp.library")
Q_LOGGING_CATEGORY(lcDatabase, "recomp.database")
Q_LOGGING_CATEGORY(lcArtwork, "recomp.artwork")
Q_LOGGING_CATEGORY(lcMetadata, "recomp.metadata")
Q_LOGGING_CATEGORY(lcGitHub, "recomp.github")
Q_LOGGING_CATEGORY(lcLauncher, "recomp.install")
Q_LOGGING_CATEGORY(lcGameLaunch, "recomp.launch")

namespace {
constexpr qint64 kMaximumLogSize = 5 * 1024 * 1024;
constexpr int kRotatedLogCount = 3;
QFile g_logFile;
QMutex g_logMutex;
QString g_homePath;
bool g_detailed = false;

void rotateLogs(const QString &path)
{
    if (!QFileInfo::exists(path) || QFileInfo(path).size() < kMaximumLogSize)
        return;

    QFile::remove(path + QStringLiteral(".%1").arg(kRotatedLogCount));
    for (int index = kRotatedLogCount - 1; index >= 1; --index) {
        const QString source = path + QStringLiteral(".%1").arg(index);
        const QString destination = path + QStringLiteral(".%1").arg(index + 1);
        if (QFile::exists(source))
            QFile::rename(source, destination);
    }
    QFile::rename(path, path + QStringLiteral(".1"));
}

// Writes one already formatted line to stderr and the log file. Caller
// holds g_logMutex.
void writeLine(const QByteArray &encoded)
{
    fprintf(stderr, "%s\n", encoded.constData());
    if (!g_logFile.isOpen())
        return;
    if (g_logFile.size() >= kMaximumLogSize) {
        const QString path = g_logFile.fileName();
        g_logFile.close();
        rotateLogs(path);
        g_logFile.setFileName(path);
        if (!g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            fprintf(stderr, "RecompStation: unable to reopen runtime log file: %s\n",
                    path.toUtf8().constData());
            return;
        }
    }
    g_logFile.write(encoded);
    g_logFile.write("\n");
    g_logFile.flush();
}

// "recomp.launch" -> "launch", "qml" stays, Qt's own -> "qt.<rest>".
QString shortCategory(const char *category)
{
    if (!category || !*category || qstrcmp(category, "default") == 0)
        return QStringLiteral("app");
    const QString name = QString::fromUtf8(category);
    if (name.startsWith(QStringLiteral("recomp.")))
        return name.mid(7);
    return name;
}

void messageHandler(QtMsgType type, const QMessageLogContext &context,
                    const QString &message)
{
    const char *level = "INFO ";
    switch (type) {
    case QtDebugMsg:    level = "DEBUG"; break;
    case QtInfoMsg:     level = "INFO "; break;
    case QtWarningMsg:  level = "WARN "; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg:    level = "FATAL"; break;
    }
    QString text = RecompLogging::redactHome(message);
    // QML warnings carry the full qrc URL of the file; the file name is enough.
    text.replace(QStringLiteral("qrc:/qt/qml/RecompStation/"), QString());

    const QString line = QStringLiteral("%1 %2 %3 %4")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
             QString::fromLatin1(level),
             shortCategory(context.category).leftJustified(9),
             text);

    {
        QMutexLocker locker(&g_logMutex);
        writeLine(line.toUtf8());
    }

    if (type == QtFatalMsg)
        abort();
}

void applyFilterRules()
{
    // Qt's own debug output is never wanted here, except the few lines on how
    // the window is drawn (graphics API, GPU, render loop): the first thing
    // to know when the interface stutters. Ours only when detailed.
    QLoggingCategory::setFilterRules(g_detailed
        ? QStringLiteral("*.debug=false\nqt.scenegraph.general.debug=true\nrecomp.*.debug=true\nqml.debug=true\njs.debug=true")
        : QStringLiteral("*.debug=false\nqt.scenegraph.general.debug=true"));
}
}

namespace RecompLogging
{
bool initialize(const QString &logDirectory)
{
    g_homePath = QDir::homePath();
    const QByteArray debugEnv = qgetenv("RECOMPSTATION_DEBUG");
    if (!debugEnv.isEmpty() && debugEnv != "0")
        g_detailed = true;
    applyFilterRules();

    if (!QDir().mkpath(logDirectory))
        return false;

    const QString path = QDir(logDirectory).filePath(QStringLiteral("runtime.log"));
    rotateLogs(path);
    g_logFile.setFileName(path);
    if (!g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return false;

    qInstallMessageHandler(messageHandler);
    return true;
}

QString logFilePath()
{
    return g_logFile.fileName();
}

QString logDirectory()
{
    const QString path = g_logFile.fileName();
    return path.isEmpty() ? QString() : QFileInfo(path).absolutePath();
}

void setDetailedLogging(bool enabled)
{
    if (g_detailed == enabled)
        return;
    g_detailed = enabled;
    applyFilterRules();
    qCInfo(lcApplication) << "Detailed logging" << (enabled ? "on" : "off");
}

bool detailedLogging()
{
    return g_detailed;
}

void writeBlock(const QStringList &lines)
{
    QMutexLocker locker(&g_logMutex);
    for (const QString &line : lines)
        writeLine(redactHome(line).toUtf8());
}

QString redactHome(const QString &text)
{
    const QString home = g_homePath.isEmpty() ? QDir::homePath() : g_homePath;
    if (home.isEmpty() || home == QStringLiteral("/"))
        return text;
    QString result = text;
    result.replace(QStringLiteral("file://") + home, QStringLiteral("file://~"));
    result.replace(home + QLatin1Char('/'), QStringLiteral("~/"));
    if (result.endsWith(home))
        result.replace(result.size() - home.size(), home.size(), QStringLiteral("~"));
    result.replace(home + QLatin1Char('"'), QStringLiteral("~\""));
    return result;
}
}
