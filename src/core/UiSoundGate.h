#pragma once

#include <QHash>
#include <QString>

// Decides whether an interface sound may play right now, so sounds never
// pile up:
// - the same sound is not repeated faster than its minimum gap (holding a
//   direction plays Move at most every 45 ms, others every 90 ms);
// - a quieter, more frequent sound is skipped right after a more important
//   one (focus landing on a game as Game Details opens plays only Open).
// Times are milliseconds from any monotonic clock.
class UiSoundGate
{
public:
    static int rank(const QString &sound);
    static int minimumGapMs(const QString &sound);
    // How long a sound keeps lower-ranked sounds quiet.
    static constexpr int PriorityWindowMs = 90;

    bool isKnown(const QString &sound) const;
    // Returns true and records the sound when it may play.
    bool allow(const QString &sound, qint64 nowMs);

private:
    QHash<QString, qint64> m_lastPlayed;
    int m_lastRank = -1;
    qint64 m_lastRankedAt = -1;
};
