#include <QtTest>
#include <QSignalSpy>

#include "core/ControllerInputRouter.h"

class ControllerInputRouterTest : public QObject
{
    Q_OBJECT

private slots:
    void mapsAnalogStickDirections();
    void handlesExtremeAnalogAxisValues();
    void respectsAnalogDeadZone();
    void emitsButtonActionsOnlyOnPressEdges();
    void emitsNavigationImmediatelyAndRepeatsAtCadence();
    void changingDirectionNavigatesImmediately();
    void releasingDirectionResetsRepeatTimer();
    void suspendedInputDoesNotEmitActions();
    void heldButtonsDoNotFireWhenInputResumes();
    void emitsInputEnabledChangeOnlyWhenStateChanges();
    void resolvesActionsFromCustomMappings();
    void resolvesNavigationFromCustomMappings();
    void supportsMultipleInputsForOneAction();
};

void ControllerInputRouterTest::mapsAnalogStickDirections()
{
    QCOMPARE(ControllerInputRouter::directionFromAxes(-20000, 0), QStringLiteral("left"));
    QCOMPARE(ControllerInputRouter::directionFromAxes(20000, 0), QStringLiteral("right"));
    QCOMPARE(ControllerInputRouter::directionFromAxes(0, -20000), QStringLiteral("up"));
    QCOMPARE(ControllerInputRouter::directionFromAxes(0, 20000), QStringLiteral("down"));
    QCOMPARE(ControllerInputRouter::directionFromAxes(12000, 22000), QStringLiteral("down"));
    QCOMPARE(ControllerInputRouter::directionFromAxes(-22000, 12000), QStringLiteral("left"));
}

void ControllerInputRouterTest::handlesExtremeAnalogAxisValues()
{
    // SDL axes are signed 16-bit values; both extrema must be safe.
    QCOMPARE(ControllerInputRouter::directionFromAxes(-32768, 0), QStringLiteral("left"));
    QCOMPARE(ControllerInputRouter::directionFromAxes(32767, 0), QStringLiteral("right"));
    QCOMPARE(ControllerInputRouter::directionFromAxes(0, -32768), QStringLiteral("up"));
    QCOMPARE(ControllerInputRouter::directionFromAxes(0, 32767), QStringLiteral("down"));
    QCOMPARE(ControllerInputRouter::directionFromAxes(-32768, 32767), QStringLiteral("left"));
}

void ControllerInputRouterTest::respectsAnalogDeadZone()
{
    QCOMPARE(ControllerInputRouter::directionFromAxes(15999, 0), QString());
    QCOMPARE(ControllerInputRouter::directionFromAxes(0, -16000), QString());
    QCOMPARE(ControllerInputRouter::directionFromAxes(16001, 0), QStringLiteral("right"));
    QCOMPARE(ControllerInputRouter::directionFromAxes(0, -16001), QStringLiteral("up"));
}

void ControllerInputRouterTest::emitsButtonActionsOnlyOnPressEdges()
{
    ControllerInputRouter router;
    QSignalSpy select(&router, &ControllerInputRouter::selectRequested);
    QSignalSpy back(&router, &ControllerInputRouter::backRequested);
    QSignalSpy menu(&router, &ControllerInputRouter::menuRequested);
    QSignalSpy previous(&router, &ControllerInputRouter::previousSectionRequested);
    QSignalSpy next(&router, &ControllerInputRouter::nextSectionRequested);

    router.processFrame({}, true, true, true, true, true, 0);
    router.processFrame({}, true, true, true, true, true, 16);
    QCOMPARE(select.count(), 1);
    QCOMPARE(back.count(), 1);
    QCOMPARE(menu.count(), 1);
    QCOMPARE(previous.count(), 1);
    QCOMPARE(next.count(), 1);

    router.processFrame({}, false, false, false, false, false, 32);
    router.processFrame({}, true, true, true, true, true, 48);
    QCOMPARE(select.count(), 2);
    QCOMPARE(back.count(), 2);
    QCOMPARE(menu.count(), 2);
    QCOMPARE(previous.count(), 2);
    QCOMPARE(next.count(), 2);
}

void ControllerInputRouterTest::emitsNavigationImmediatelyAndRepeatsAtCadence()
{
    ControllerInputRouter router;
    QSignalSpy navigation(&router, &ControllerInputRouter::navigationRequested);

    router.processFrame("down", false, false, false, false, false, 0);
    router.processFrame("down", false, false, false, false, false, 100);
    QCOMPARE(navigation.count(), 1);

    router.processFrame("down", false, false, false, false, false, 219);
    QCOMPARE(navigation.count(), 1);
    router.processFrame("down", false, false, false, false, false, 220);
    QCOMPARE(navigation.count(), 2);
    QCOMPARE(navigation.at(1).at(0).toString(), QStringLiteral("down"));
}

void ControllerInputRouterTest::changingDirectionNavigatesImmediately()
{
    ControllerInputRouter router;
    QSignalSpy navigation(&router, &ControllerInputRouter::navigationRequested);

    router.processFrame("left", false, false, false, false, false, 0);
    router.processFrame("right", false, false, false, false, false, 10);

    QCOMPARE(navigation.count(), 2);
    QCOMPARE(navigation.at(0).at(0).toString(), QStringLiteral("left"));
    QCOMPARE(navigation.at(1).at(0).toString(), QStringLiteral("right"));
}

void ControllerInputRouterTest::releasingDirectionResetsRepeatTimer()
{
    ControllerInputRouter router;
    QSignalSpy navigation(&router, &ControllerInputRouter::navigationRequested);

    router.processFrame("up", false, false, false, false, false, 0);
    router.processFrame({}, false, false, false, false, false, 100);
    router.processFrame("up", false, false, false, false, false, 101);

    QCOMPARE(navigation.count(), 2);
}

void ControllerInputRouterTest::suspendedInputDoesNotEmitActions()
{
    ControllerInputRouter router;
    QSignalSpy select(&router, &ControllerInputRouter::selectRequested);
    QSignalSpy navigation(&router, &ControllerInputRouter::navigationRequested);

    router.setInputEnabled(false);
    router.processFrame("up", true, false, false, false, false, 0);
    QCOMPARE(select.count(), 0);
    QCOMPARE(navigation.count(), 0);
}

void ControllerInputRouterTest::heldButtonsDoNotFireWhenInputResumes()
{
    ControllerInputRouter router;
    QSignalSpy select(&router, &ControllerInputRouter::selectRequested);

    router.setInputEnabled(false);
    router.processFrame({}, true, false, false, false, false, 0);
    router.setInputEnabled(true);
    router.processFrame({}, true, false, false, false, false, 16);
    QCOMPARE(select.count(), 0);

    router.processFrame({}, false, false, false, false, false, 32);
    router.processFrame({}, true, false, false, false, false, 48);
    QCOMPARE(select.count(), 1);
}

void ControllerInputRouterTest::emitsInputEnabledChangeOnlyWhenStateChanges()
{
    ControllerInputRouter router;
    QSignalSpy changed(&router, &ControllerInputRouter::inputEnabledChanged);

    router.setInputEnabled(true);
    QCOMPARE(changed.count(), 0);
    router.setInputEnabled(false);
    router.setInputEnabled(false);
    router.setInputEnabled(true);
    QCOMPARE(changed.count(), 2);
}


void ControllerInputRouterTest::resolvesActionsFromCustomMappings()
{
    const QVariantMap profile{
        {QStringLiteral("select"), QVariantList{QStringLiteral("x")}},
        {QStringLiteral("back"), QVariantList{QStringLiteral("y")}}
    };

    QVERIFY(ControllerInputRouter::actionPressed(
        profile, QStringLiteral("select"), {QStringLiteral("x")}));
    QVERIFY(!ControllerInputRouter::actionPressed(
        profile, QStringLiteral("select"), {QStringLiteral("a")}));
    QVERIFY(ControllerInputRouter::actionPressed(
        profile, QStringLiteral("back"), {QStringLiteral("y")}));
}

void ControllerInputRouterTest::resolvesNavigationFromCustomMappings()
{
    const QVariantMap profile{
        {QStringLiteral("navigateUp"), QVariantList{QStringLiteral("x")}},
        {QStringLiteral("navigateDown"), QVariantList{QStringLiteral("b")}},
        {QStringLiteral("navigateLeft"), QVariantList{QStringLiteral("leftstick-left")}},
        {QStringLiteral("navigateRight"), QVariantList{QStringLiteral("dpad-right")}}
    };

    QCOMPARE(ControllerInputRouter::directionFromProfile(
                 profile, {QStringLiteral("x")}), QStringLiteral("up"));
    QCOMPARE(ControllerInputRouter::directionFromProfile(
                 profile, {QStringLiteral("leftstick-left")}), QStringLiteral("left"));
    QCOMPARE(ControllerInputRouter::directionFromProfile(
                 profile, {QStringLiteral("dpad-right")}), QStringLiteral("right"));
    QVERIFY(ControllerInputRouter::directionFromProfile(profile, {}).isEmpty());
}

void ControllerInputRouterTest::supportsMultipleInputsForOneAction()
{
    const QVariantMap profile{
        {QStringLiteral("select"), QVariantList{
             QStringLiteral("a"), QStringLiteral("rightshoulder")}}
    };

    QVERIFY(ControllerInputRouter::actionPressed(
        profile, QStringLiteral("select"), {QStringLiteral("rightshoulder")}));
}

QTEST_GUILESS_MAIN(ControllerInputRouterTest)
#include "ControllerInputRouterTest.moc"
