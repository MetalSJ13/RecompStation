#include <QtTest>

#include "metadata/TheGamesDbAllowance.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>

class TheGamesDbAllowanceTest : public QObject
{
    Q_OBJECT

private:
    static QDateTime noon()
    {
        return QDateTime(QDate(2026, 11, 24), QTime(12, 0), QTimeZone::UTC);
    }

private slots:
    void unknownAllowanceAllowsAutomaticLookups()
    {
        TheGamesDbAllowance allowance;
        QCOMPARE(allowance.remaining(), -1);
        QVERIFY(allowance.automaticLookupsAllowed(noon()));
        QVERIFY(allowance.pauseReason(noon()).isEmpty());
    }

    void pausesAutomaticLookupsWhenFewRequestsAreLeft()
    {
        TheGamesDbAllowance allowance;
        QSignalSpy changed(&allowance, &TheGamesDbAllowance::changed);
        allowance.report(TheGamesDbAllowance::AutomaticReserve + 1, noon());
        QCOMPARE(changed.size(), 1);
        QVERIFY(allowance.automaticLookupsAllowed(noon()));

        allowance.report(TheGamesDbAllowance::AutomaticReserve, noon());
        QCOMPARE(allowance.remaining(), TheGamesDbAllowance::AutomaticReserve);
        QVERIFY(!allowance.automaticLookupsAllowed(noon().addSecs(3600)));
        QVERIFY(allowance.pauseReason(noon()).contains(QStringLiteral("50")));
    }

    void staleLowReportAllowsOneMoreCheck()
    {
        // Requests come back every day, so a day-old low figure may be out of date.
        TheGamesDbAllowance allowance;
        allowance.report(10, noon());
        QVERIFY(!allowance.automaticLookupsAllowed(noon().addSecs(23 * 3600)));
        QVERIFY(allowance.automaticLookupsAllowed(noon().addSecs(25 * 3600)));
    }

    void rejectionPausesForSomeHoursThenAllowsATry()
    {
        TheGamesDbAllowance allowance;
        allowance.report(500, noon());
        allowance.reportRejected(noon());
        QCOMPARE(allowance.remaining(), 0);
        QVERIFY(!allowance.automaticLookupsAllowed(noon().addSecs(3600)));
        const QDateTime afterPause =
            noon().addSecs(TheGamesDbAllowance::RejectionPauseHours * 3600 + 60);
        QVERIFY(allowance.automaticLookupsAllowed(afterPause));

        // A later successful response clears the pause.
        allowance.reportRejected(noon());
        allowance.report(120, noon().addSecs(60));
        QVERIFY(!allowance.pausedUntil().isValid());
        QVERIFY(allowance.automaticLookupsAllowed(noon().addSecs(120)));
    }

    void keepsStateAcrossRestarts()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path() + QStringLiteral("/metadata/allowance.ini");
        {
            TheGamesDbAllowance allowance(path);
            allowance.reportRejected(noon());
        }
        TheGamesDbAllowance reloaded(path);
        QCOMPARE(reloaded.remaining(), 0);
        QCOMPARE(reloaded.pausedUntil(),
                 noon().addSecs(TheGamesDbAllowance::RejectionPauseHours * 3600));
        QVERIFY(!reloaded.automaticLookupsAllowed(noon().addSecs(60)));

        reloaded.reset();
        TheGamesDbAllowance cleared(path);
        QCOMPARE(cleared.remaining(), -1);
        QVERIFY(cleared.automaticLookupsAllowed(noon()));
    }

    void ignoresNegativeReports()
    {
        TheGamesDbAllowance allowance;
        allowance.report(-1, noon());
        QCOMPARE(allowance.remaining(), -1);
        QVERIFY(!allowance.reportedAt().isValid());
    }
};

QTEST_GUILESS_MAIN(TheGamesDbAllowanceTest)
#include "TheGamesDbAllowanceTest.moc"
