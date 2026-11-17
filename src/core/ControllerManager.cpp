#include "ControllerManager.h"

#include "Logging.h"

#include <QtMath>

#ifdef RECOMPSTATION_HAS_SDL2
#include <SDL.h>
#endif

ControllerManager::ControllerManager(QObject *parent)
    : QObject(parent)
    , m_inputRouter()
{
    connect(&m_inputRouter, &ControllerInputRouter::navigationRequested,
            this, &ControllerManager::navigationRequested);
    connect(&m_inputRouter, &ControllerInputRouter::selectRequested,
            this, &ControllerManager::selectRequested);
    connect(&m_inputRouter, &ControllerInputRouter::backRequested,
            this, &ControllerManager::backRequested);
    connect(&m_inputRouter, &ControllerInputRouter::menuRequested,
            this, &ControllerManager::menuRequested);
    connect(&m_inputRouter, &ControllerInputRouter::previousSectionRequested,
            this, &ControllerManager::previousSectionRequested);
    connect(&m_inputRouter, &ControllerInputRouter::nextSectionRequested,
            this, &ControllerManager::nextSectionRequested);
    connect(&m_inputRouter, &ControllerInputRouter::inputEnabledChanged,
            this, &ControllerManager::inputEnabledChanged);

#ifdef RECOMPSTATION_HAS_SDL2
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
        qCWarning(lcApplication) << "Controller subsystem initialization failed:"
                                 << SDL_GetError();
        return;
    }

    m_sdlInitialized = true;
    SDL_GameControllerEventState(SDL_IGNORE);
    SDL_JoystickEventState(SDL_IGNORE);
    m_elapsedTimer.start();
    connect(&m_pollTimer, &QTimer::timeout,
            this, &ControllerManager::pollController);
    m_pollTimer.start(16);
    pollController();
#else
    qCInfo(lcApplication) << "Built without SDL2; controller input is unavailable.";
#endif
}

ControllerManager::~ControllerManager()
{
#ifdef RECOMPSTATION_HAS_SDL2
    m_pollTimer.stop();
    if (m_controller)
        SDL_GameControllerClose(m_controller);
    if (m_sdlInitialized)
        SDL_Quit();
#endif
}

bool ControllerManager::available() const
{
#ifdef RECOMPSTATION_HAS_SDL2
    return m_sdlInitialized;
#else
    return false;
#endif
}

bool ControllerManager::connected() const { return m_connected; }
QString ControllerManager::controllerName() const { return m_controllerName; }
QString ControllerManager::controllerType() const { return m_controllerType; }
bool ControllerManager::inputEnabled() const { return m_inputRouter.inputEnabled(); }

void ControllerManager::setInputEnabled(bool enabled)
{
    m_inputRouter.setInputEnabled(enabled);
    qCInfo(lcApplication) << "Controller input"
                          << (enabled ? "resumed" : "suspended");
}

void ControllerManager::setConnected(bool connected, const QString &name,
                                     const QString &type)
{
    if (m_connected == connected && m_controllerName == name
        && m_controllerType == type)
        return;

    m_connected = connected;
    m_controllerName = connected ? name : QString();
    m_controllerType = connected ? type : QStringLiteral("generic");
    emit connectedChanged();

    if (connected)
        qCInfo(lcApplication) << "Controller connected:" << m_controllerName
                              << "type:" << m_controllerType;
    else
        qCInfo(lcApplication) << "Controller disconnected.";
}

void ControllerManager::pollController()
{
#ifdef RECOMPSTATION_HAS_SDL2
    SDL_GameControllerUpdate();

    if (m_controller && !SDL_GameControllerGetAttached(m_controller)) {
        SDL_GameControllerClose(m_controller);
        m_controller = nullptr;
        setConnected(false);
    }

    if (!m_controller) {
        for (int index = 0; index < SDL_NumJoysticks(); ++index) {
            if (!SDL_IsGameController(index))
                continue;

            m_controller = SDL_GameControllerOpen(index);
            if (!m_controller)
                continue;

            const char *rawName = SDL_GameControllerName(m_controller);
            const QString name = QString::fromUtf8(
                rawName ? rawName : "Game controller");

            QString type = QStringLiteral("generic");
            switch (SDL_GameControllerGetType(m_controller)) {
            case SDL_CONTROLLER_TYPE_XBOX360:
            case SDL_CONTROLLER_TYPE_XBOXONE:
                type = QStringLiteral("xbox");
                break;
            case SDL_CONTROLLER_TYPE_PS3:
            case SDL_CONTROLLER_TYPE_PS4:
            case SDL_CONTROLLER_TYPE_PS5:
                type = QStringLiteral("playstation");
                break;
            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
                type = QStringLiteral("nintendo");
                break;
            default:
                break;
            }

            setConnected(true, name, type);
            break;
        }
    }

    if (!m_controller) {
        m_inputRouter.processFrame({}, false, false, false, false, false,
                                   m_elapsedTimer.elapsed());
        return;
    }

    const auto pressed = [this](SDL_GameControllerButton button) {
        return SDL_GameControllerGetButton(m_controller, button) != 0;
    };

    QString direction;
    if (pressed(SDL_CONTROLLER_BUTTON_DPAD_UP))
        direction = QStringLiteral("up");
    else if (pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN))
        direction = QStringLiteral("down");
    else if (pressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT))
        direction = QStringLiteral("left");
    else if (pressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
        direction = QStringLiteral("right");
    else {
        constexpr Sint16 deadZone = 16000;
        const Sint16 x = SDL_GameControllerGetAxis(
            m_controller, SDL_CONTROLLER_AXIS_LEFTX);
        const Sint16 y = SDL_GameControllerGetAxis(
            m_controller, SDL_CONTROLLER_AXIS_LEFTY);

        if (qAbs(x) > qAbs(y) && x < -deadZone)
            direction = QStringLiteral("left");
        else if (qAbs(x) > qAbs(y) && x > deadZone)
            direction = QStringLiteral("right");
        else if (y < -deadZone)
            direction = QStringLiteral("up");
        else if (y > deadZone)
            direction = QStringLiteral("down");
    }

    m_inputRouter.processFrame(
        direction,
        pressed(SDL_CONTROLLER_BUTTON_A),
        pressed(SDL_CONTROLLER_BUTTON_B),
        pressed(SDL_CONTROLLER_BUTTON_START)
            || pressed(SDL_CONTROLLER_BUTTON_BACK),
        pressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER),
        pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER),
        m_elapsedTimer.elapsed());
#endif
}
