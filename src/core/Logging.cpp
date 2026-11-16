#include "Logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

Q_LOGGING_CATEGORY(lcApplication, "recomp.application")
Q_LOGGING_CATEGORY(lcUI, "recomp.ui")
Q_LOGGING_CATEGORY(lcSettings, "recomp.settings")
Q_LOGGING_CATEGORY(lcLibrary, "recomp.library")
Q_LOGGING_CATEGORY(lcMetadata, "recomp.metadata")
Q_LOGGING_CATEGORY(lcGitHub, "recomp.github")
Q_LOGGING_CATEGORY(lcLauncher, "recomp.launcher")
Q_LOGGING_CATEGORY(lcGameLaunch, "recomp.gameLaunch")

namespace {
constexpr qint64 kMaximumLogSize = 5 * 1024 * 1024;
constexpr int kRotatedLogCount = 3;
QFile g_logFile;
QMutex g_logMutex;

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

void messageHandler(QtMsgType type, const QMessageLogContext &context,
                    const QString &message)
{
    static const char *levels[] = {"DEBUG", "INFO", "WARNING", "CRITICAL", "FATAL"};
    const int level = qBound(0, static_cast<int>(type), 4);
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const QString category = context.category
        ? QString::fromUtf8(context.category) : QStringLiteral("default");
    const QString line = QStringLiteral("%1 [%2] [%3] %4")
        .arg(timestamp, QString::fromLatin1(levels[level]), category, message);

    QMutexLocker locker(&g_logMutex);
    const QByteArray encoded = line.toUtf8();
    fprintf(stderr, "%s\n", encoded.constData());
    if (g_logFile.isOpen()) {
        g_logFile.write(encoded);
        g_logFile.write("\n");
        g_logFile.flush();
    }

    if (type == QtFatalMsg)
        abort();
}
}

namespace RecompLogging
{
bool initialize(const QString &logDirectory)
{
    if (!QDir().mkpath(logDirectory))
        return false;

    const QString path = QDir(logDirectory).filePath(QStringLiteral("runtime.log"));
    rotateLogs(path);
    g_logFile.setFileName(path);
    if (!g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return false;

    qInstallMessageHandler(messageHandler);
    qCInfo(lcApplication) << "Runtime logging initialized.";
    qCInfo(lcApplication) << "Log file:" << path;
    qCInfo(lcApplication) << "Log rotation: 5 MiB, retaining 3 archived files.";
    return true;
}

QString logFilePath()
{
    return g_logFile.fileName();
}
}
