#include "UiSoundGate.h"

namespace {
// Higher numbers win. Move and Edge are heard most and matter least.
const QHash<QString, int> &ranks()
{
    static const QHash<QString, int> table{
        {QStringLiteral("move"), 0},
        {QStringLiteral("edge"), 1},
        {QStringLiteral("toggle-on"), 2},
        {QStringLiteral("toggle-off"), 2},
        {QStringLiteral("switch"), 3},
        {QStringLiteral("menu"), 3},
        {QStringLiteral("back"), 3},
        {QStringLiteral("error"), 3},
        {QStringLiteral("select"), 4},
        {QStringLiteral("launch"), 5},
    };
    return table;
}
}

int UiSoundGate::rank(const QString &sound)
{
    return ranks().value(sound, -1);
}

int UiSoundGate::minimumGapMs(const QString &sound)
{
    if (sound == QStringLiteral("move"))
        return 45;
    // Pushing against the end of a list repeats quickly while held.
    if (sound == QStringLiteral("edge"))
        return 150;
    return 90;
}

bool UiSoundGate::isKnown(const QString &sound) const
{
    return ranks().contains(sound);
}

bool UiSoundGate::allow(const QString &sound, qint64 nowMs)
{
    const int soundRank = rank(sound);
    if (soundRank < 0)
        return false;

    const auto last = m_lastPlayed.constFind(sound);
    if (last != m_lastPlayed.constEnd() && nowMs - last.value() < minimumGapMs(sound))
        return false;

    if (m_lastRankedAt >= 0 && soundRank < m_lastRank
            && nowMs - m_lastRankedAt < PriorityWindowMs)
        return false;

    m_lastPlayed.insert(sound, nowMs);
    m_lastRank = soundRank;
    m_lastRankedAt = nowMs;
    return true;
}
