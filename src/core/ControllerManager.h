#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>

#include "ControllerInputRouter.h"

struct _SDL_GameController;

class ControllerManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString controllerName READ controllerName NOTIFY connectedChanged)
    Q_PROPERTY(QString controllerType READ controllerType NOTIFY connectedChanged)
    Q_PROPERTY(bool inputEnabled READ inputEnabled WRITE setInputEnabled NOTIFY inputEnabledChanged)

public:
    explicit ControllerManager(QObject *parent = nullptr);
    ~ControllerManager() override;

    bool available() const;
    bool connected() const;
    QString controllerName() const;
    QString controllerType() const;
    bool inputEnabled() const;

public slots:
    void setInputEnabled(bool enabled);

signals:
    void connectedChanged();
    void inputEnabledChanged();
    void navigationRequested(const QString &direction);
    void selectRequested();
    void backRequested();
    void menuRequested();
    void previousSectionRequested();
    void nextSectionRequested();

private slots:
    void pollController();

private:
    void setConnected(bool connected, const QString &name = {},
                      const QString &type = QStringLiteral("generic"));

    QTimer m_pollTimer;
    QElapsedTimer m_elapsedTimer;
    _SDL_GameController *m_controller = nullptr;
    ControllerInputRouter m_inputRouter;
    QString m_controllerName;
    QString m_controllerType = QStringLiteral("generic");
    bool m_connected = false;
    bool m_sdlInitialized = false;
};
