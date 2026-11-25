#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

class QProcess;
class QTimer;

// Whether Windows launchers can run through Wine on this computer.
//
// Players install Wine themselves with their package manager; RecompStation
// never installs it and never asks a package manager. It only looks for
// `wine` (or `wine64` on older setups) on the regular program path and runs
// `wine --version` with a short time limit. Wine is used exactly as the
// system has it set up. Only the Linux version offers this.
class WineSupport : public QObject
{
    Q_OBJECT
    // True on Linux: the Settings row exists only there.
    Q_PROPERTY(bool supported READ isSupported CONSTANT)
    // Wine was found and answered.
    Q_PROPERTY(bool available READ isAvailable NOTIFY changed)
    Q_PROPERTY(bool checking READ isChecking NOTIFY changed)
    // "10.0", "10.19 (Staging)"; empty when not available.
    Q_PROPERTY(QString version READ version NOTIFY changed)
    Q_PROPERTY(QString program READ program NOTIFY changed)

public:
    explicit WineSupport(QObject *parent = nullptr);
    ~WineSupport() override;

    bool isSupported() const;
    bool isAvailable() const;
    bool isChecking() const;
    QString version() const;
    QString program() const;

    // Looks for Wine again (startup, opening Settings › Library, and before
    // a Windows launcher is installed or started). Does nothing while a
    // check is running.
    Q_INVOKABLE void refresh();

    // "wine-10.0 (Staging)" -> "10.0 (Staging)"; empty if it isn't Wine's answer.
    static QString parseVersion(const QString &output);

    // For tests: where to look instead of the program path, whether this
    // system offers Wine, and how long to wait for an answer.
    void setSearchPaths(const QStringList &paths);
    void setSupportedForTesting(bool supported);
    void setTimeoutMs(int milliseconds);

signals:
    void changed();
    // A check has finished, with or without Wine.
    void checked();

private:
    void finish(bool available, const QString &program, const QString &version);

    bool m_supported = false;
    bool m_available = false;
    bool m_checking = false;
    bool m_loggedOnce = false;
    QString m_version;
    QString m_program;
    QStringList m_searchPaths;
    int m_timeoutMs = 5000;
    QPointer<QProcess> m_process;
    QTimer *m_timer = nullptr;
};
