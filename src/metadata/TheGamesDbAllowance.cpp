#include "TheGamesDbAllowance.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

TheGamesDbAllowance::TheGamesDbAllowance(const QString &stateFilePath, QObject *parent)
    : QObject(parent),
      m_stateFilePath(stateFilePath)
{
    load();
}

int TheGamesDbAllowance::remaining() const { return m_remaining; }
QDateTime TheGamesDbAllowance::reportedAt() const { return m_reportedAt; }
QDateTime TheGamesDbAllowance::pausedUntil() const { return m_pausedUntil; }

void TheGamesDbAllowance::report(int remaining, const QDateTime &now)
{
    if (remaining < 0)
        return;
    m_remaining = remaining;
    m_reportedAt = now.toUTC();
    // A successful response means requests work again.
    m_pausedUntil = QDateTime();
    save();
    emit changed();
}

void TheGamesDbAllowance::reportRejected(const QDateTime &now)
{
    m_remaining = 0;
    m_reportedAt = now.toUTC();
    m_pausedUntil = now.toUTC().addSecs(RejectionPauseHours * 3600);
    save();
    emit changed();
}

void TheGamesDbAllowance::reset()
{
    m_remaining = -1;
    m_reportedAt = QDateTime();
    m_pausedUntil = QDateTime();
    save();
    emit changed();
}

bool TheGamesDbAllowance::automaticLookupsAllowed(const QDateTime &now) const
{
    return pauseReason(now).isEmpty();
}

QString TheGamesDbAllowance::pauseReason(const QDateTime &now) const
{
    const QDateTime current = now.toUTC();
    if (m_pausedUntil.isValid() && current < m_pausedUntil) {
        return QStringLiteral("TheGamesDB has no requests left right now; "
                              "automatic lookups will try again later.");
    }
    if (m_pausedUntil.isValid())
        return {};  // The pause after a 403 is over: allow one try.
    const bool fresh = m_reportedAt.isValid()
        && m_reportedAt.secsTo(current) < qint64(ReportLifetimeHours) * 3600;
    if (fresh && m_remaining >= 0 && m_remaining <= AutomaticReserve) {
        return QStringLiteral("Only %1 TheGamesDB requests left; automatic lookups "
                              "are paused to keep them for your own searches.")
            .arg(m_remaining);
    }
    return {};
}

void TheGamesDbAllowance::load()
{
    if (m_stateFilePath.isEmpty() || !QFileInfo::exists(m_stateFilePath))
        return;
    QSettings state(m_stateFilePath, QSettings::IniFormat);
    m_remaining = state.value(QStringLiteral("remaining"), -1).toInt();
    m_reportedAt = QDateTime::fromString(
        state.value(QStringLiteral("reportedAt")).toString(), Qt::ISODate);
    m_pausedUntil = QDateTime::fromString(
        state.value(QStringLiteral("pausedUntil")).toString(), Qt::ISODate);
}

void TheGamesDbAllowance::save() const
{
    if (m_stateFilePath.isEmpty())
        return;
    QDir().mkpath(QFileInfo(m_stateFilePath).absolutePath());
    QSettings state(m_stateFilePath, QSettings::IniFormat);
    state.setValue(QStringLiteral("remaining"), m_remaining);
    state.setValue(QStringLiteral("reportedAt"),
                   m_reportedAt.isValid() ? m_reportedAt.toString(Qt::ISODate) : QString());
    state.setValue(QStringLiteral("pausedUntil"),
                   m_pausedUntil.isValid() ? m_pausedUntil.toString(Qt::ISODate) : QString());
    state.sync();
}
