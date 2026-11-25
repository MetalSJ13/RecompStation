#include "WineSupport.h"

#include "Logging.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

WineSupport::WineSupport(QObject *parent)
    : QObject(parent),
      // Decided when RecompStation runs, from the running kernel.
      m_supported(QSysInfo::kernelType() == QStringLiteral("linux")),
      m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        if (m_process) {
            qCInfo(lcApplication) << "Wine didn't answer within" << m_timeoutMs << "ms; treating it as unavailable.";
            m_process->kill();
        }
    });
}

WineSupport::~WineSupport()
{
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(500);
    }
}

bool WineSupport::isSupported() const { return m_supported; }
bool WineSupport::isAvailable() const { return m_available; }
bool WineSupport::isChecking() const { return m_checking; }
QString WineSupport::version() const { return m_version; }
QString WineSupport::program() const { return m_program; }

void WineSupport::setSearchPaths(const QStringList &paths) { m_searchPaths = paths; }
void WineSupport::setSupportedForTesting(bool supported) { m_supported = supported; }
void WineSupport::setTimeoutMs(int milliseconds) { m_timeoutMs = qMax(100, milliseconds); }

QString WineSupport::parseVersion(const QString &output)
{
    // Wine answers one line: "wine-10.0", "wine-10.19 (Staging)".
    static const QRegularExpression pattern(QStringLiteral("^wine-([0-9][^\\r\\n]*)"),
                                            QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = pattern.match(output.trimmed());
    return match.hasMatch() ? match.captured(1).trimmed().left(64) : QString();
}

void WineSupport::refresh()
{
    if (!m_supported || m_checking)
        return;

    QString found;
    for (const QString &name : {QStringLiteral("wine"), QStringLiteral("wine64")}) {
        found = QStandardPaths::findExecutable(name, m_searchPaths);
        if (!found.isEmpty())
            break;
    }
    if (found.isEmpty()) {
        finish(false, QString(), QString());
        return;
    }

    m_checking = true;
    emit changed();

    auto *process = new QProcess(this);
    m_process = process;
    // The player's environment as it is (WINEPREFIX included): nothing is
    // added or changed.
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::finished, this,
            [this, process, found](int exitCode, QProcess::ExitStatus status) {
        m_timer->stop();
        const QString output = QString::fromLocal8Bit(process->readAll());
        process->deleteLater();
        const QString version = status == QProcess::NormalExit && exitCode == 0
            ? parseVersion(output) : QString();
        finish(!version.isEmpty(), found, version);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, found](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return; // A crash or a kill ends in finished().
        m_timer->stop();
        process->deleteLater();
        finish(false, found, QString());
    });
    m_timer->start(m_timeoutMs);
    process->start(found, {QStringLiteral("--version")});
}

void WineSupport::finish(bool available, const QString &program, const QString &version)
{
    const bool wasChecking = m_checking;
    const bool differs = available != m_available || version != m_version || program != m_program;
    m_checking = false;
    m_available = available;
    m_version = available ? version : QString();
    m_program = available ? program : QString();
    if (differs || !m_loggedOnce) {
        m_loggedOnce = true;
        if (available)
            qCInfo(lcApplication).noquote() << "Wine found:" << m_version << "at" << m_program;
        else if (program.isEmpty())
            qCInfo(lcApplication) << "Wine not found on the program path.";
        else
            qCInfo(lcApplication).noquote() << "Wine at" << program << "didn't answer with a version.";
    }
    if (differs || wasChecking)
        emit changed();
    emit checked();
}
