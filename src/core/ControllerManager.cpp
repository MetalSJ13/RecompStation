#include "ControllerManager.h"

#include "Logging.h"
#include "SettingsManager.h"

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
QString ControllerManager::controllerGuid() const { return m_controllerGuid; }
bool ControllerManager::inputEnabled() const { return m_inputRouter.inputEnabled(); }

void ControllerManager::setSettingsManager(SettingsManager *settingsManager)
{
    m_settingsManager = settingsManager;
    loadControllerProfile();
}

void ControllerManager::loadControllerProfile()
{
    m_controllerProfile = (m_settingsManager && !m_controllerGuid.isEmpty())
        ? m_settingsManager->controllerProfile(m_controllerGuid) : QVariantMap{};
    if (!m_controllerProfile.isEmpty())
        qCInfo(lcApplication) << "Loaded controller mappings for GUID:" << m_controllerGuid;
}

void ControllerManager::setInputEnabled(bool enabled)
{
    m_inputRouter.setInputEnabled(enabled);
    qCInfo(lcApplication) << "Controller input"
                          << (enabled ? "resumed" : "suspended");
}

void ControllerManager::setConnected(bool connected, const QString &name,
                                     const QString &type, const QString &guid)
{
    if (m_connected == connected && m_controllerName == name
        && m_controllerType == type && m_controllerGuid == guid)
        return;

    m_connected = connected;
    m_controllerName = connected ? name : QString();
    m_controllerType = connected ? type : QStringLiteral("generic");
    m_controllerGuid = connected ? guid : QString();
    loadControllerProfile();
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
    // Refresh SDL's device list before checking for newly attached or
    // disconnected controllers. This keeps hot-plug detection reliable even
    // when the application is otherwise processing no SDL window events.
    SDL_PumpEvents();
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
                type = QStringLiteral("nintendo");
                break;
            default:
                break;
            }

            char guidBuffer[64] = {};
            const SDL_JoystickGUID hardwareGuid = SDL_JoystickGetGUID(
                SDL_GameControllerGetJoystick(m_controller));
            SDL_JoystickGetGUIDString(hardwareGuid, guidBuffer, sizeof(guidBuffer));
            setConnected(true, name, type, QString::fromLatin1(guidBuffer));
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

    // Normalize the physical SDL state into stable tokens. Profiles map
    // logical RecompStation actions to these tokens, independent of layout.
    QStringList activeInputs;
    const auto addButton = [&activeInputs, &pressed](SDL_GameControllerButton button,
                                                      const QString &token) {
        if (pressed(button))
            activeInputs.append(token);
    };
    addButton(SDL_CONTROLLER_BUTTON_A, QStringLiteral("a"));
    addButton(SDL_CONTROLLER_BUTTON_B, QStringLiteral("b"));
    addButton(SDL_CONTROLLER_BUTTON_X, QStringLiteral("x"));
    addButton(SDL_CONTROLLER_BUTTON_Y, QStringLiteral("y"));
    addButton(SDL_CONTROLLER_BUTTON_BACK, QStringLiteral("back"));
    addButton(SDL_CONTROLLER_BUTTON_GUIDE, QStringLiteral("guide"));
    addButton(SDL_CONTROLLER_BUTTON_START, QStringLiteral("start"));
    addButton(SDL_CONTROLLER_BUTTON_LEFTSTICK, QStringLiteral("leftstick"));
    addButton(SDL_CONTROLLER_BUTTON_RIGHTSTICK, QStringLiteral("rightstick"));
    addButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, QStringLiteral("leftshoulder"));
    addButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, QStringLiteral("rightshoulder"));
    addButton(SDL_CONTROLLER_BUTTON_DPAD_UP, QStringLiteral("dpad-up"));
    addButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN, QStringLiteral("dpad-down"));
    addButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT, QStringLiteral("dpad-left"));
    addButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, QStringLiteral("dpad-right"));

    const Sint16 leftX = SDL_GameControllerGetAxis(
        m_controller, SDL_CONTROLLER_AXIS_LEFTX);
    const Sint16 leftY = SDL_GameControllerGetAxis(
        m_controller, SDL_CONTROLLER_AXIS_LEFTY);
    const QString leftStickDirection =
        ControllerInputRouter::directionFromAxes(leftX, leftY);
    if (!leftStickDirection.isEmpty())
        activeInputs.append(QStringLiteral("leftstick-") + leftStickDirection);

    const QString direction = ControllerInputRouter::directionFromProfile(
        m_controllerProfile, activeInputs);

    // The right stick is reserved for smooth scrolling in long UI panels.
    // SDL reports positive Y when the stick is pushed down. Ignore small
    // movements to prevent drift from slowly moving the page.
    const Sint16 rightY = SDL_GameControllerGetAxis(
        m_controller, SDL_CONTROLLER_AXIS_RIGHTY);
    constexpr int rightStickDeadZone = 8000;
    if (qAbs(static_cast<int>(rightY)) > rightStickDeadZone) {
        const qreal scrollAmount = static_cast<qreal>(rightY) / 32767.0;
        emit rightStickScrollRequested(scrollAmount);
    }

    m_inputRouter.processFrame(
        direction,
        ControllerInputRouter::actionPressed(m_controllerProfile,
                                             QStringLiteral("select"), activeInputs),
        ControllerInputRouter::actionPressed(m_controllerProfile,
                                             QStringLiteral("back"), activeInputs),
        ControllerInputRouter::actionPressed(m_controllerProfile,
                                             QStringLiteral("menu"), activeInputs),
        ControllerInputRouter::actionPressed(m_controllerProfile,
                                             QStringLiteral("previousSection"), activeInputs),
        ControllerInputRouter::actionPressed(m_controllerProfile,
                                             QStringLiteral("nextSection"), activeInputs),
        m_elapsedTimer.elapsed());
#endif
}
