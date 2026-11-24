#include <QtTest>

#include "core/UiSoundGate.h"

class UiSoundGateTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsUnknownSounds()
    {
        UiSoundGate gate;
        QVERIFY(!gate.isKnown(QStringLiteral("explosion")));
        QVERIFY(!gate.allow(QStringLiteral("explosion"), 0));
    }

    void limitsHowFastAMoveRepeats()
    {
        UiSoundGate gate;
        QVERIFY(gate.allow(QStringLiteral("move"), 1000));
        QVERIFY(!gate.allow(QStringLiteral("move"), 1030));
        QVERIFY(gate.allow(QStringLiteral("move"), 1050));
    }

    void holdingAgainstTheEndRepeatsSlowly()
    {
        UiSoundGate gate;
        QVERIFY(gate.allow(QStringLiteral("edge"), 0));
        QVERIFY(!gate.allow(QStringLiteral("edge"), 100));
        QVERIFY(gate.allow(QStringLiteral("edge"), 160));
    }

    void anImportantSoundSilencesAQuieterOneRightAfterIt()
    {
        UiSoundGate gate;
        QVERIFY(gate.allow(QStringLiteral("select"), 0));
        // Focus lands on the Game Details page as it opens: no Move on top.
        QVERIFY(!gate.allow(QStringLiteral("move"), 20));
        QVERIFY(gate.allow(QStringLiteral("move"), UiSoundGate::PriorityWindowMs + 1));
    }

    void aMoreImportantSoundAlwaysPlays()
    {
        UiSoundGate gate;
        QVERIFY(gate.allow(QStringLiteral("move"), 0));
        QVERIFY(gate.allow(QStringLiteral("select"), 10));
        QVERIFY(gate.allow(QStringLiteral("launch"), 20));
    }

    void differentSoundsOfTheSameRankBothPlay()
    {
        UiSoundGate gate;
        QVERIFY(gate.allow(QStringLiteral("menu"), 0));
        QVERIFY(gate.allow(QStringLiteral("back"), 10));
    }
};

QTEST_GUILESS_MAIN(UiSoundGateTest)
#include "UiSoundGateTest.moc"
