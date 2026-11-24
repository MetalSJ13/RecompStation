#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

// Keeps track of how many TheGamesDB requests the key has left, so the app
// can stop making automatic requests before the allowance runs out.
//
// TheGamesDB reports the remaining allowance in every response. A public
// key's allowance is a rolling 30-day window: each day's requests come back
// 30 days later. When it is used up the API answers 403 and no longer says
// when requests return, so after a 403 automatic lookups wait a few hours.
//
// Manual actions (a search the user types, "Find info" on one game) are never
// blocked here; they also serve as the check that requests work again.
// State is saved to an INI file so a restart does not forget a 403.
class TheGamesDbAllowance : public QObject
{
    Q_OBJECT

public:
    // Automatic lookups stop when this many requests or fewer are left,
    // keeping the rest for the user's own searches.
    static constexpr int AutomaticReserve = 50;
    // A reported figure older than this is too stale to pause on: requests
    // come back every day, so one automatic request may check again.
    static constexpr int ReportLifetimeHours = 24;
    // How long automatic lookups wait after TheGamesDB answers 403.
    static constexpr int RejectionPauseHours = 6;

    explicit TheGamesDbAllowance(const QString &stateFilePath = QString(),
                                 QObject *parent = nullptr);

    // Requests left as last reported, or -1 when unknown.
    int remaining() const;
    QDateTime reportedAt() const;
    QDateTime pausedUntil() const;

    // Records the allowance a successful response reported.
    void report(int remaining, const QDateTime &now = QDateTime::currentDateTimeUtc());
    // Records a 403: the allowance is used up (or the key was refused).
    void reportRejected(const QDateTime &now = QDateTime::currentDateTimeUtc());
    // Forgets everything, for example when the API key changes.
    void reset();

    bool automaticLookupsAllowed(const QDateTime &now = QDateTime::currentDateTimeUtc()) const;
    // Why automatic lookups are paused, for the status line; empty when not.
    QString pauseReason(const QDateTime &now = QDateTime::currentDateTimeUtc()) const;

signals:
    void changed();

private:
    void load();
    void save() const;

    QString m_stateFilePath;
    int m_remaining = -1;
    QDateTime m_reportedAt;
    QDateTime m_pausedUntil;
};
