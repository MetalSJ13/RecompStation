#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantMap>

#include "ControllerInputRouter.h"

class SettingsManager;

struct _SDL_GameController;

class ControllerManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString controllerName READ controllerName NOTIFY connectedChanged)
    Q_PROPERTY(QString controllerType READ controllerType NOTIFY connectedChanged)
    Q_PROPERTY(QString controllerGuid READ controllerGuid NOTIFY connectedChanged)
    Q_PROPERTY(bool inputEnabled READ inputEnabled WRITE setInputEnabled NOTIFY inputEnabledChanged)
    Q_PROPERTY(bool navigationActive READ navigationActive NOTIFY navigationActiveChanged)
    Q_PROPERTY(bool gameSessionActive READ gameSessionActive WRITE setGameSessionActive NOTIFY gameSessionActiveChanged)
    Q_PROPERTY(bool navigationAccelerationEnabled READ navigationAccelerationEnabled WRITE setNavigationAccelerationEnabled)
    Q_PROPERTY(bool mappingCaptureActive READ mappingCaptureActive NOTIFY mappingCaptureStateChanged)
    Q_PROPERTY(QString mappingCaptureAction READ mappingCaptureAction NOTIFY mappingCaptureStateChanged)
    Q_PROPERTY(bool mappingCaptureReplaceExisting READ mappingCaptureReplaceExisting NOTIFY mappingCaptureStateChanged)
    // True right after a capture starts, until the button used to start it
    // (mappingCaptureHeldInput, a token like "a") is let go.
    Q_PROPERTY(bool mappingCaptureWaitingForRelease READ mappingCaptureWaitingForRelease NOTIFY mappingCaptureStateChanged)
    Q_PROPERTY(QString mappingCaptureHeldInput READ mappingCaptureHeldInput NOTIFY mappingCaptureStateChanged)
    // How long a capture waits for an input before it gives up.
    Q_PROPERTY(int mappingCaptureTimeoutMs READ mappingCaptureTimeoutMs CONSTANT)
    Q_PROPERTY(int profileRevision READ profileRevision NOTIFY controllerProfileChanged)
    // Inputs held right now (button tokens, stick directions, triggers), for
    // the button test in Settings. Display only; not used for navigation.
    Q_PROPERTY(QStringList activeInputs READ activeInputs NOTIFY activeInputsChanged)

public:
    explicit ControllerManager(QObject *parent = nullptr);
    ~ControllerManager() override;

    bool available() const;
    bool connected() const;
    QString controllerName() const;
    QString controllerType() const;
    QString controllerGuid() const;
    bool inputEnabled() const;
    bool navigationActive() const;
    bool gameSessionActive() const;
    bool navigationAccelerationEnabled() const;
    bool mappingCaptureActive() const;
    QString mappingCaptureAction() const;
    bool mappingCaptureReplaceExisting() const;
    bool mappingCaptureWaitingForRelease() const { return m_mappingCaptureActive && m_captureAwaitingRelease; }
    QString mappingCaptureHeldInput() const { return m_captureHeldInput; }
    static constexpr int MappingCaptureTimeoutMs = 15000;
    int mappingCaptureTimeoutMs() const { return MappingCaptureTimeoutMs; }
    int profileRevision() const;

public slots:
    void setInputEnabled(bool enabled);
    void setGameSessionActive(bool active);
    void setNavigationAccelerationEnabled(bool enabled);
    void setSettingsManager(SettingsManager *settingsManager);
    Q_INVOKABLE void reloadControllerProfile();
    Q_INVOKABLE bool startMappingCapture(const QString &action, bool replaceExisting = false);
    Q_INVOKABLE bool resetConnectedControllerProfile();
    Q_INVOKABLE void cancelMappingCapture();

signals:
    void connectedChanged();
    void inputEnabledChanged();
    void navigationActiveChanged();
    void gameSessionActiveChanged();
    void navigationRequested(const QString &direction);
    void selectRequested();
    void backRequested();
    void menuRequested();
    void previousSectionRequested();
    void nextSectionRequested();
    void detailsRequested();
    void rightStickScrollRequested(qreal amount);
    void mappingCaptureStateChanged();
    void controllerProfileChanged();
    void mappingCaptured(const QString &action, const QString &input);
    // The capture ended because nothing was pressed in time.
    void mappingCaptureTimedOut(const QString &action);
    void activeInputsChanged();

private slots:
    void pollController();

private:
    void loadControllerProfile();
    void setConnected(bool connected, const QString &name = {},
                      const QString &type = QStringLiteral("generic"),
                      const QString &guid = {});

    QTimer m_pollTimer;
    QTimer m_mappingCaptureTimer;
    QElapsedTimer m_elapsedTimer;
    // The controller that drives the interface, and every attached one.
    _SDL_GameController *m_controller = nullptr;
    QList<_SDL_GameController *> m_openControllers;
    void selectController(_SDL_GameController *controller);
    ControllerInputRouter m_inputRouter;
    SettingsManager *m_settingsManager = nullptr;
    QVariantMap m_controllerProfile;
    QString m_mappingCaptureAction;
    QStringList m_previousActiveInputs;
    QStringList m_displayedInputs;
    void setDisplayedInputs(const QStringList &inputs);
public:
    QStringList activeInputs() const { return m_displayedInputs; }
private:
    QStringList m_captureInputsAwaitingRelease;
    bool m_mappingCaptureActive = false;
    bool m_mappingCaptureReplaceExisting = false;
    bool m_captureAwaitingRelease = false;
    QString m_captureHeldInput;
    int m_profileRevision = 0;
    QString m_controllerName;
    QString m_controllerType = QStringLiteral("generic");
    QString m_controllerGuid;
    bool m_connected = false;
    bool m_gameSessionActive = false;
    bool m_navigationAccelerationEnabled = false;
    bool m_sdlInitialized = false;
};
