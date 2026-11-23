#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// Hardware-independent controller input state machine. SDL (or another input
// backend) supplies normalized button states; this class owns edge detection,
// repeat timing, and input suspension so those behaviors can be tested
// without a physical controller.
class ControllerInputRouter final : public QObject
{
    Q_OBJECT

public:
    explicit ControllerInputRouter(QObject *parent = nullptr);

    static QString directionFromAxes(qint16 x, qint16 y, int deadZone = 16000);
    static bool triggerPressed(qint16 value, int threshold = 16000);
    static QString firstNewInput(const QStringList &activeInputs,
                                  const QStringList &previousInputs);
    static bool actionPressed(const QVariantMap &profile, const QString &action,
                              const QStringList &activeInputs);
    static QString directionFromProfile(const QVariantMap &profile,
                                         const QStringList &activeInputs);

    void processFrame(const QString &direction,
                      bool selectPressed,
                      bool backPressed,
                      bool menuPressed,
                      bool previousSectionPressed,
                      bool nextSectionPressed,
                      qint64 elapsedMs,
                      bool detailsPressed = false);

    bool inputEnabled() const;
    bool navigationActive() const;
    void setInputEnabled(bool enabled);
    void setGameSessionActive(bool active);
    void setNavigationAccelerationEnabled(bool enabled);

signals:
    void navigationRequested(const QString &direction);
    void selectRequested();
    void backRequested();
    void menuRequested();
    void previousSectionRequested();
    void nextSectionRequested();
    void detailsRequested();
    void inputEnabledChanged();

private:
    bool m_inputEnabled = true;
    bool m_gameSessionActive = false;
    bool m_navigationAccelerationEnabled = false;
    bool m_previousSelect = false;
    bool m_previousBack = false;
    bool m_previousMenu = false;
    bool m_previousPreviousSection = false;
    bool m_previousNextSection = false;
    bool m_previousDetails = false;
    QString m_currentDirection;
    qint64 m_lastNavigationMs = 0;
    qint64 m_directionStartedMs = 0;
};
