#include "ControllerManager.h"
#include <QDebug>
#ifdef RECOMPSTATION_HAS_SDL2
#include <SDL.h>
#endif
ControllerManager::ControllerManager(QObject *parent) : QObject(parent) {
#ifdef RECOMPSTATION_HAS_SDL2
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        qWarning() << "[Controller] SDL initialization failed:" << SDL_GetError();
        return;
    }
    SDL_GameControllerEventState(SDL_IGNORE);
    m_navigationClock.start();
    connect(&m_pollTimer, &QTimer::timeout, this, &ControllerManager::pollController);
    m_pollTimer.start(16);
    pollController();
#else
    qInfo() << "[Controller] Built without SDL2; controller input unavailable.";
#endif
}
ControllerManager::~ControllerManager() {
#ifdef RECOMPSTATION_HAS_SDL2
    m_pollTimer.stop();
    if (m_controller) SDL_GameControllerClose(m_controller);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
#endif
}
bool ControllerManager::available() const {
#ifdef RECOMPSTATION_HAS_SDL2
    return true;
#else
    return false;
#endif
}
bool ControllerManager::connected() const { return m_connected; }
QString ControllerManager::controllerName() const { return m_controllerName; }
bool ControllerManager::inputEnabled() const { return m_inputEnabled; }
void ControllerManager::setInputEnabled(bool enabled) {
    if (m_inputEnabled == enabled) return;
    m_inputEnabled = enabled;
    m_currentDirection.clear();
    m_lastNavigationMs = 0;
    emit inputEnabledChanged();
    qInfo() << "[Controller] Input" << (enabled ? "resumed" : "suspended");
}
void ControllerManager::setConnected(bool connected, const QString &name) {
    if (m_connected == connected && m_controllerName == name) return;
    m_connected = connected;
    m_controllerName = name;
    emit connectedChanged();
    qInfo() << "[Controller]" << (connected ? "Connected:" : "Disconnected:")
            << (connected ? name : QString());
}
void ControllerManager::emitNavigation(const QString &direction) {
    if (direction.isEmpty()) return;
    const qint64 now = m_navigationClock.elapsed();
    if (direction != m_currentDirection || now - m_lastNavigationMs >= 220) {
        m_currentDirection = direction;
        m_lastNavigationMs = now;
        emit navigationRequested(direction);
    }
}
void ControllerManager::pollController() {
#ifdef RECOMPSTATION_HAS_SDL2
    SDL_GameControllerUpdate();
    if (m_controller && !SDL_GameControllerGetAttached(m_controller)) {
        SDL_GameControllerClose(m_controller);
        m_controller = nullptr;
        setConnected(false);
    }
    if (!m_controller) {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (!SDL_IsGameController(i)) continue;
            m_controller = SDL_GameControllerOpen(i);
            if (m_controller) {
                const char *name = SDL_GameControllerName(m_controller);
                setConnected(true, QString::fromUtf8(name ? name : "Game controller"));
                break;
            }
        }
    }
    if (!m_controller) return;
    const auto pressed = [this](SDL_GameControllerButton id) {
        return SDL_GameControllerGetButton(m_controller, id) != 0;
    };
    const bool a = pressed(SDL_CONTROLLER_BUTTON_A);
    const bool b = pressed(SDL_CONTROLLER_BUTTON_B);
    const bool start = pressed(SDL_CONTROLLER_BUTTON_START);
    const bool back = pressed(SDL_CONTROLLER_BUTTON_BACK);
    const bool l1 = pressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    const bool r1 = pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    if (!m_inputEnabled) {
        m_previousA=a; m_previousB=b; m_previousStart=start; m_previousBack=back;
        m_previousLeftShoulder=l1; m_previousRightShoulder=r1;
        m_currentDirection.clear();
        return;
    }
    if (a && !m_previousA) emit selectRequested();
    if (b && !m_previousB) emit backRequested();
    if ((start && !m_previousStart) || (back && !m_previousBack)) emit menuRequested();
    if (l1 && !m_previousLeftShoulder) emit previousSectionRequested();
    if (r1 && !m_previousRightShoulder) emit nextSectionRequested();
    m_previousA=a; m_previousB=b; m_previousStart=start; m_previousBack=back;
    m_previousLeftShoulder=l1; m_previousRightShoulder=r1;
    QString direction;
    if (pressed(SDL_CONTROLLER_BUTTON_DPAD_UP)) direction="up";
    else if (pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) direction="down";
    else if (pressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) direction="left";
    else if (pressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) direction="right";
    else {
        constexpr Sint16 deadZone=16000;
        const Sint16 x=SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTX);
        const Sint16 y=SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTY);
        if (qAbs(x)>qAbs(y) && x < -deadZone) direction="left";
        else if (qAbs(x)>qAbs(y) && x > deadZone) direction="right";
        else if (y < -deadZone) direction="up";
        else if (y > deadZone) direction="down";
    }
    if (direction.isEmpty()) { m_currentDirection.clear(); m_lastNavigationMs=0; }
    else emitNavigation(direction);
#endif
}
