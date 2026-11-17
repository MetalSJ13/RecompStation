#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
struct SDL_GameController;
class ControllerManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString controllerName READ controllerName NOTIFY connectedChanged)
    Q_PROPERTY(bool inputEnabled READ inputEnabled WRITE setInputEnabled NOTIFY inputEnabledChanged)
public:
    explicit ControllerManager(QObject *parent = nullptr);
    ~ControllerManager() override;
    bool available() const;
    bool connected() const;
    QString controllerName() const;
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
    void setConnected(bool connected, const QString &name = {});
    void emitNavigation(const QString &direction);
    QTimer m_pollTimer;
    QElapsedTimer m_navigationClock;
    SDL_GameController *m_controller = nullptr;
    QString m_controllerName;
    QString m_currentDirection;
    qint64 m_lastNavigationMs = 0;
    bool m_connected = false;
    bool m_inputEnabled = true;
    bool m_previousA = false, m_previousB = false, m_previousStart = false;
    bool m_previousBack = false, m_previousLeftShoulder = false, m_previousRightShoulder = false;
};
