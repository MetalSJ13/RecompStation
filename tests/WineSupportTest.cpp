#include <QtTest>

#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/WineSupport.h"

class WineSupportTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesWineVersions();
    void findsWineThatAnswers();
    void fallsBackToWine64();
    void missingWineIsUnavailable();
    void wineThatFailsIsUnavailable();
    void wineThatHangsTimesOut();
    void doesNothingWhereUnsupported();
    void noticesWineBeingRemoved();

private:
    // A fake program in dir that runs a shell script body.
    static void writeProgram(const QString &dir, const QString &name, const QByteArray &body);
    static bool waitForCheck(WineSupport &wine, int timeoutMs = 5000);
};

void WineSupportTest::writeProgram(const QString &dir, const QString &name, const QByteArray &body)
{
    QFile file(dir + QLatin1Char('/') + name);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\n" + body + "\n");
    file.close();
    QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
}

bool WineSupportTest::waitForCheck(WineSupport &wine, int timeoutMs)
{
    QSignalSpy spy(&wine, &WineSupport::checked);
    wine.refresh();
    return !spy.isEmpty() || spy.wait(timeoutMs);
}

void WineSupportTest::parsesWineVersions()
{
    QCOMPARE(WineSupport::parseVersion(QStringLiteral("wine-10.0\n")), QStringLiteral("10.0"));
    QCOMPARE(WineSupport::parseVersion(QStringLiteral("wine-10.19 (Staging)")), QStringLiteral("10.19 (Staging)"));
    QCOMPARE(WineSupport::parseVersion(QStringLiteral("wine-9.0 (Ubuntu 9.0~repack-4build3)")),
             QStringLiteral("9.0 (Ubuntu 9.0~repack-4build3)"));
    QVERIFY(WineSupport::parseVersion(QStringLiteral("command not found")).isEmpty());
    QVERIFY(WineSupport::parseVersion(QString()).isEmpty());
    QVERIFY(WineSupport::parseVersion(QStringLiteral("wine-")).isEmpty());
}

void WineSupportTest::findsWineThatAnswers()
{
    QTemporaryDir dir;
    writeProgram(dir.path(), QStringLiteral("wine"), "echo 'wine-10.19 (Staging)'");
    WineSupport wine;
    wine.setSupportedForTesting(true);
    wine.setSearchPaths({dir.path()});
    QVERIFY(waitForCheck(wine));
    QVERIFY(wine.isAvailable());
    QVERIFY(!wine.isChecking());
    QCOMPARE(wine.version(), QStringLiteral("10.19 (Staging)"));
    QCOMPARE(wine.program(), dir.path() + QStringLiteral("/wine"));
}

void WineSupportTest::fallsBackToWine64()
{
    QTemporaryDir dir;
    writeProgram(dir.path(), QStringLiteral("wine64"), "echo wine-8.0");
    WineSupport wine;
    wine.setSupportedForTesting(true);
    wine.setSearchPaths({dir.path()});
    QVERIFY(waitForCheck(wine));
    QVERIFY(wine.isAvailable());
    QCOMPARE(wine.version(), QStringLiteral("8.0"));
    QVERIFY(wine.program().endsWith(QStringLiteral("/wine64")));
}

void WineSupportTest::missingWineIsUnavailable()
{
    QTemporaryDir dir;
    WineSupport wine;
    wine.setSupportedForTesting(true);
    wine.setSearchPaths({dir.path()});
    QVERIFY(waitForCheck(wine));
    QVERIFY(!wine.isAvailable());
    QVERIFY(wine.version().isEmpty());
}

void WineSupportTest::wineThatFailsIsUnavailable()
{
    QTemporaryDir dir;
    writeProgram(dir.path(), QStringLiteral("wine"), "echo 'broken' >&2; exit 1");
    WineSupport wine;
    wine.setSupportedForTesting(true);
    wine.setSearchPaths({dir.path()});
    QVERIFY(waitForCheck(wine));
    QVERIFY(!wine.isAvailable());

    // Answers, but not with a Wine version.
    writeProgram(dir.path(), QStringLiteral("wine"), "echo hello");
    QVERIFY(waitForCheck(wine));
    QVERIFY(!wine.isAvailable());
}

void WineSupportTest::wineThatHangsTimesOut()
{
    QTemporaryDir dir;
    writeProgram(dir.path(), QStringLiteral("wine"), "exec sleep 30");
    WineSupport wine;
    wine.setSupportedForTesting(true);
    wine.setSearchPaths({dir.path()});
    wine.setTimeoutMs(300);
    QElapsedTimer timer;
    timer.start();
    QVERIFY(waitForCheck(wine, 5000));
    QVERIFY(timer.elapsed() < 5000);
    QVERIFY(!wine.isAvailable());
    QVERIFY(!wine.isChecking());
}

void WineSupportTest::doesNothingWhereUnsupported()
{
    QTemporaryDir dir;
    writeProgram(dir.path(), QStringLiteral("wine"), "echo wine-10.0");
    WineSupport wine;
    wine.setSupportedForTesting(false);
    wine.setSearchPaths({dir.path()});
    QSignalSpy spy(&wine, &WineSupport::checked);
    wine.refresh();
    QVERIFY(!spy.wait(300));
    QVERIFY(!wine.isAvailable());
}

void WineSupportTest::noticesWineBeingRemoved()
{
    QTemporaryDir dir;
    writeProgram(dir.path(), QStringLiteral("wine"), "echo wine-10.0");
    WineSupport wine;
    wine.setSupportedForTesting(true);
    wine.setSearchPaths({dir.path()});
    QVERIFY(waitForCheck(wine));
    QVERIFY(wine.isAvailable());

    QVERIFY(QFile::remove(dir.path() + QStringLiteral("/wine")));
    QSignalSpy changed(&wine, &WineSupport::changed);
    QVERIFY(waitForCheck(wine));
    QVERIFY(!wine.isAvailable());
    QVERIFY(!changed.isEmpty());
}

QTEST_GUILESS_MAIN(WineSupportTest)
#include "WineSupportTest.moc"
