#include <QtTest>

#include "core/SettingsManager.h"

#include <QSettings>
#include <QSignalSpy>

class SettingsManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void defaultsToScanningOnStartup();
    void persistsScanOnStartupPreference();
};

void SettingsManagerTest::init()
{
    QCoreApplication::setOrganizationName(QStringLiteral("RecompStationSettingsTests"));
    QCoreApplication::setApplicationName(QStringLiteral("SettingsManagerTests"));
    QSettings().clear();
}

void SettingsManagerTest::defaultsToScanningOnStartup()
{
    SettingsManager manager;
    QVERIFY(manager.scanOnStartup());
}

void SettingsManagerTest::persistsScanOnStartupPreference()
{
    SettingsManager manager;
    QSignalSpy changed(&manager, &SettingsManager::scanOnStartupChanged);

    manager.setScanOnStartup(false);
    QVERIFY(!manager.scanOnStartup());
    QCOMPARE(changed.count(), 1);

    SettingsManager reloaded;
    QVERIFY(!reloaded.scanOnStartup());

    reloaded.setScanOnStartup(true);
    QVERIFY(reloaded.scanOnStartup());
    QCOMPARE(QSettings().value(QStringLiteral("library/scanOnStartup")).toBool(), true);
    QSettings().clear();
}

QTEST_GUILESS_MAIN(SettingsManagerTest)
#include "SettingsManagerTest.moc"
