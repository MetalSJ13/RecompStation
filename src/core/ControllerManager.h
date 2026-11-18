#pragma once

#include <QObject>
#include <QString>
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

public:
    explicit ControllerManager(QObject *parent = nullptr);
    ~ControllerManager() override;

    bool available() const;
    bool connected() const;
    QString controllerName() const;
    QString controllerType() const;
    QString controllerGuid() const;
    bool inputEnabled() const;

public slots:
    void setInputEnabled(bool enabled);
    void setSettingsManager(SettingsManager *settingsManager);
    Q_INVOKABLE void reloadControllerProfile();

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
    QString m_controllerName;
    QString m_controllerType = QStringLiteral("generic");
    QString m_controllerGuid;
    bool m_connected = false;
    bool m_sdlInitialized = false;
};
