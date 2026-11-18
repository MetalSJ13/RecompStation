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
    Q_PROPERTY(bool mappingCaptureActive READ mappingCaptureActive NOTIFY mappingCaptureStateChanged)
    Q_PROPERTY(QString mappingCaptureAction READ mappingCaptureAction NOTIFY mappingCaptureStateChanged)
    Q_PROPERTY(int profileRevision READ profileRevision NOTIFY controllerProfileChanged)

public:
    explicit ControllerManager(QObject *parent = nullptr);
    ~ControllerManager() override;

    bool available() const;
    bool connected() const;
    QString controllerName() const;
    QString controllerType() const;
    QString controllerGuid() const;
    bool inputEnabled() const;
    bool mappingCaptureActive() const;
    QString mappingCaptureAction() const;
    int profileRevision() const;

public slots:
    void setInputEnabled(bool enabled);
    void setSettingsManager(SettingsManager *settingsManager);
    Q_INVOKABLE void reloadControllerProfile();
    Q_INVOKABLE bool startMappingCapture(const QString &action);
    Q_INVOKABLE void cancelMappingCapture();

signals:
    void connectedChanged();
    void inputEnabledChanged();
    void navigationRequested(const QString &direction);
    void selectRequested();
    void backRequested();
    void menuRequested();
    void previousSectionRequested();
    void nextSectionRequested();
    void rightStickScrollRequested(qreal amount);
    void mappingCaptureStateChanged();
    void controllerProfileChanged();
    void mappingCaptured(const QString &action, const QString &input);

private slots:
    void pollController();

private:
    void loadControllerProfile();
    void setConnected(bool connected, const QString &name = {},
                      const QString &type = QStringLiteral("generic"),
                      const QString &guid = {});

    QTimer m_pollTimer;
    QElapsedTimer m_elapsedTimer;
    _SDL_GameController *m_controller = nullptr;
    ControllerInputRouter m_inputRouter;
    SettingsManager *m_settingsManager = nullptr;
    QVariantMap m_controllerProfile;
    QString m_mappingCaptureAction;
    QStringList m_previousActiveInputs;
    bool m_mappingCaptureActive = false;
    bool m_captureAwaitingRelease = false;
    int m_profileRevision = 0;
    QString m_controllerName;
    QString m_controllerType = QStringLiteral("generic");
    QString m_controllerGuid;
    bool m_connected = false;
    bool m_sdlInitialized = false;
};
