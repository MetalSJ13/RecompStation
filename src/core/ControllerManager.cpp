#include "ControllerManager.h"

#include "Logging.h"
#include "SettingsManager.h"

#ifdef RECOMPSTATION_HAS_SDL2
#include <SDL.h>

#include <algorithm>

namespace {

// True while any button is held or any stick or trigger is well past its
// rest position. Used to hand control to the controller the player picks up.
bool controllerHasInput(SDL_GameController *controller)
{
    for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; ++button) {
        if (SDL_GameControllerGetButton(controller,
                                        static_cast<SDL_GameControllerButton>(button)))
            return true;
    }
    constexpr int activityThreshold = 16000;
    for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis) {
        const int value = SDL_GameControllerGetAxis(controller,
                                                    static_cast<SDL_GameControllerAxis>(axis));
        if (qAbs(value) > activityThreshold)
            return true;
    }
    return false;
}

SDL_JoystickID instanceId(SDL_GameController *controller)
{
    return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
}

} // namespace
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
    connect(&m_inputRouter, &ControllerInputRouter::detailsRequested,
            this, &ControllerManager::detailsRequested);
    connect(&m_inputRouter, &ControllerInputRouter::inputEnabledChanged,
            this, [this]() {
        emit inputEnabledChanged();
        emit navigationActiveChanged();
    });
    m_mappingCaptureTimer.setSingleShot(true);
    connect(&m_mappingCaptureTimer, &QTimer::timeout,
            this, &ControllerManager::cancelMappingCapture);

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
    for (SDL_GameController *controller : std::as_const(m_openControllers))
        SDL_GameControllerClose(controller);
    m_controller = nullptr;
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
bool ControllerManager::navigationActive() const { return m_inputRouter.navigationActive(); }
bool ControllerManager::gameSessionActive() const { return m_gameSessionActive; }
bool ControllerManager::navigationAccelerationEnabled() const { return m_navigationAccelerationEnabled; }

void ControllerManager::setSettingsManager(SettingsManager *settingsManager)
{
    m_settingsManager = settingsManager;
    loadControllerProfile();
}

void ControllerManager::reloadControllerProfile()
{
    loadControllerProfile();
}

bool ControllerManager::mappingCaptureActive() const
{
    return m_mappingCaptureActive;
}

QString ControllerManager::mappingCaptureAction() const
{
    return m_mappingCaptureAction;
}

int ControllerManager::profileRevision() const
{
    return m_profileRevision;
}

bool ControllerManager::mappingCaptureReplaceExisting() const
{
    return m_mappingCaptureReplaceExisting;
}

bool ControllerManager::startMappingCapture(const QString &action, bool replaceExisting)
{
    static const QStringList actions = {
        QStringLiteral("navigateUp"), QStringLiteral("navigateDown"),
        QStringLiteral("navigateLeft"), QStringLiteral("navigateRight"),
        QStringLiteral("select"), QStringLiteral("back"), QStringLiteral("menu"),
        QStringLiteral("previousSection"), QStringLiteral("nextSection"),
        QStringLiteral("details")
    };
    if (!m_connected || !m_settingsManager || !actions.contains(action))
        return false;

    m_mappingCaptureAction = action;
    m_mappingCaptureReplaceExisting = replaceExisting;
    m_mappingCaptureActive = true;
    m_captureAwaitingRelease = true;
    m_previousActiveInputs.clear();
    m_mappingCaptureTimer.start(15000);
    emit mappingCaptureStateChanged();
    qCInfo(lcApplication) << "Started controller mapping capture for action:" << action;
    return true;
}

void ControllerManager::cancelMappingCapture()
{
    if (!m_mappingCaptureActive)
        return;

    qCInfo(lcApplication) << "Cancelled controller mapping capture for action:"
                          << m_mappingCaptureAction;
    m_mappingCaptureTimer.stop();
    m_mappingCaptureActive = false;
    m_captureAwaitingRelease = false;
    m_mappingCaptureAction.clear();
    m_mappingCaptureReplaceExisting = false;
    m_previousActiveInputs.clear();
    m_captureInputsAwaitingRelease.clear();
    emit mappingCaptureStateChanged();
}

bool ControllerManager::resetConnectedControllerProfile()
{
    if (!m_connected || !m_settingsManager || m_controllerGuid.isEmpty())
        return false;

    if (!m_settingsManager->resetControllerProfile(m_controllerGuid))
        return false;

    loadControllerProfile();
    return true;
}

void ControllerManager::loadControllerProfile()
{
    m_controllerProfile = (m_settingsManager && !m_controllerGuid.isEmpty())
        ? m_settingsManager->controllerProfile(m_controllerGuid) : QVariantMap{};
    ++m_profileRevision;
    emit controllerProfileChanged();
    if (!m_controllerProfile.isEmpty())
        qCInfo(lcApplication) << "Loaded controller mappings for GUID:" << m_controllerGuid;
}

void ControllerManager::setNavigationAccelerationEnabled(bool enabled)
{
    if (m_navigationAccelerationEnabled == enabled)
        return;
    m_navigationAccelerationEnabled = enabled;
    m_inputRouter.setNavigationAccelerationEnabled(enabled);
}

void ControllerManager::setInputEnabled(bool enabled)
{
    if (m_inputRouter.inputEnabled() == enabled)
        return;

    m_inputRouter.setInputEnabled(enabled);
    qCInfo(lcApplication) << "Controller input preference"
                          << (enabled ? "enabled" : "disabled");
}

void ControllerManager::setGameSessionActive(bool active)
{
    if (m_gameSessionActive == active)
        return;

    m_gameSessionActive = active;
    m_inputRouter.setGameSessionActive(active);
    emit gameSessionActiveChanged();
    emit navigationActiveChanged();
    if (active) {
        // Stop any in-progress UI scroll easing as soon as game input takes over.
        emit rightStickScrollRequested(0.0);
    }
    qCInfo(lcApplication) << "External game session"
                          << (active ? "started; RecompStation controller navigation suspended"
                                     : "ended; RecompStation controller navigation restored");
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

void ControllerManager::setDisplayedInputs(const QStringList &inputs)
{
    if (m_displayedInputs == inputs)
        return;
    m_displayedInputs = inputs;
    emit activeInputsChanged();
}

#ifdef RECOMPSTATION_HAS_SDL2
// Make `controller` the one that drives the interface, and publish its
// name, layout and hardware GUID (which selects its mapping profile).
void ControllerManager::selectController(_SDL_GameController *controller)
{
    m_controller = controller;
    const char *rawName = SDL_GameControllerName(controller);
    const QString name = QString::fromUtf8(rawName ? rawName : "Game controller");

    QString type = QStringLiteral("generic");
    switch (SDL_GameControllerGetType(controller)) {
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
    const SDL_JoystickGUID hardwareGuid =
        SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(controller));
    SDL_JoystickGetGUIDString(hardwareGuid, guidBuffer, sizeof(guidBuffer));
    // Inputs held on the previous controller must not carry over.
    m_previousActiveInputs.clear();
    setConnected(true, name, type, QString::fromLatin1(guidBuffer));
}
#endif

void ControllerManager::pollController()
{
#ifdef RECOMPSTATION_HAS_SDL2
    // Refresh SDL's device list before checking for newly attached or
    // disconnected controllers. This keeps hot-plug detection reliable even
    // when the application is otherwise processing no SDL window events.
    SDL_PumpEvents();
    SDL_GameControllerUpdate();

    // Keep every attached controller open. The one used most recently drives
    // navigation, the button symbols and the mapping profile; a second
    // controller takes over as soon as it is used while the current one is idle.
    for (auto it = m_openControllers.begin(); it != m_openControllers.end();) {
        if (SDL_GameControllerGetAttached(*it)) {
            ++it;
            continue;
        }
        if (*it == m_controller)
            m_controller = nullptr;
        SDL_GameControllerClose(*it);
        it = m_openControllers.erase(it);
    }
    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
        if (!SDL_IsGameController(index))
            continue;
        const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
        const bool alreadyOpen = std::any_of(
            m_openControllers.cbegin(), m_openControllers.cend(),
            [id](SDL_GameController *controller) { return instanceId(controller) == id; });
        if (alreadyOpen)
            continue;
        if (SDL_GameController *controller = SDL_GameControllerOpen(index))
            m_openControllers.append(controller);
    }

    if (!m_controller || !controllerHasInput(m_controller)) {
        for (SDL_GameController *controller : std::as_const(m_openControllers)) {
            if (controller != m_controller && controllerHasInput(controller)) {
                selectController(controller);
                break;
            }
        }
    }
    if (!m_controller && !m_openControllers.isEmpty())
        selectController(m_openControllers.first());
    if (!m_controller && m_connected)
        setConnected(false);

    if (!m_controller) {
        setDisplayedInputs({});
        if (m_mappingCaptureActive)
            cancelMappingCapture();
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
    // Give stick clicks distinct tokens while retaining the legacy aliases
    // so existing controller profiles continue to work.
    addButton(SDL_CONTROLLER_BUTTON_LEFTSTICK, QStringLiteral("leftstick-click"));
    addButton(SDL_CONTROLLER_BUTTON_RIGHTSTICK, QStringLiteral("rightstick-click"));
    addButton(SDL_CONTROLLER_BUTTON_LEFTSTICK, QStringLiteral("leftstick"));
    addButton(SDL_CONTROLLER_BUTTON_RIGHTSTICK, QStringLiteral("rightstick"));
    addButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, QStringLiteral("leftshoulder"));
    addButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, QStringLiteral("rightshoulder"));
    const Sint16 leftTrigger = SDL_GameControllerGetAxis(
        m_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    const Sint16 rightTrigger = SDL_GameControllerGetAxis(
        m_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    if (ControllerInputRouter::triggerPressed(leftTrigger))
        activeInputs.append(QStringLiteral("lefttrigger"));
    if (ControllerInputRouter::triggerPressed(rightTrigger))
        activeInputs.append(QStringLiteral("righttrigger"));
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

    // The button test also shows the right stick, which is otherwise only
    // used for scrolling and is not a mappable input.
    QStringList displayed = activeInputs;
    const QString rightStickDirection = ControllerInputRouter::directionFromAxes(
        SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_RIGHTX),
        SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_RIGHTY));
    if (!rightStickDirection.isEmpty())
        displayed.append(QStringLiteral("rightstick-") + rightStickDirection);
    setDisplayedInputs(displayed);

    // A captured press belongs exclusively to the mapping operation. Keep its
    // physical input (including legacy stick-click aliases) out of normal
    // action routing until the user releases it.
    if (!m_captureInputsAwaitingRelease.isEmpty()) {
        bool capturedInputStillHeld = false;
        for (const QString &token : m_captureInputsAwaitingRelease) {
            if (activeInputs.contains(token)) {
                capturedInputStillHeld = true;
                break;
            }
        }
        if (capturedInputStillHeld) {
            for (const QString &token : m_captureInputsAwaitingRelease)
                activeInputs.removeAll(token);
        } else {
            m_captureInputsAwaitingRelease.clear();
        }
    }

    if (m_mappingCaptureActive) {
        if (m_captureAwaitingRelease) {
            if (activeInputs.isEmpty())
                m_captureAwaitingRelease = false;
        } else {
            const QString input = ControllerInputRouter::firstNewInput(
                activeInputs, m_previousActiveInputs);
            if (!input.isEmpty()) {
                    QVariantMap updatedProfile = m_controllerProfile;
                    QVariantList bindings = m_mappingCaptureReplaceExisting
                        ? QVariantList{} : updatedProfile.value(m_mappingCaptureAction).toList();
                    if (!bindings.contains(input))
                        bindings.append(input);
                    updatedProfile.insert(m_mappingCaptureAction, bindings);
                    if (m_settingsManager->saveControllerProfile(m_controllerGuid, updatedProfile)) {
                        const QString capturedAction = m_mappingCaptureAction;
                        m_mappingCaptureTimer.stop();
                        m_mappingCaptureActive = false;
                        m_captureAwaitingRelease = false;
                        m_mappingCaptureAction.clear();
                        m_mappingCaptureReplaceExisting = false;
                        m_previousActiveInputs.clear();
                        m_captureInputsAwaitingRelease = {input};
                        if (input == QStringLiteral("leftstick-click"))
                            m_captureInputsAwaitingRelease.append(QStringLiteral("leftstick"));
                        else if (input == QStringLiteral("rightstick-click"))
                            m_captureInputsAwaitingRelease.append(QStringLiteral("rightstick"));
                        else if (input == QStringLiteral("leftstick"))
                            m_captureInputsAwaitingRelease.append(QStringLiteral("leftstick-click"));
                        else if (input == QStringLiteral("rightstick"))
                            m_captureInputsAwaitingRelease.append(QStringLiteral("rightstick-click"));
                        loadControllerProfile();
                        emit mappingCaptureStateChanged();
                        emit mappingCaptured(capturedAction, input);
                        qCInfo(lcApplication) << "Saved controller mapping:"
                                               << capturedAction << "->" << input;
                    } else {
                        cancelMappingCapture();
                    }
            }
        }
        m_previousActiveInputs = activeInputs;
        m_inputRouter.processFrame({}, false, false, false, false, false,
                                   m_elapsedTimer.elapsed());
        return;
    }
    m_previousActiveInputs = activeInputs;

    const QString direction = ControllerInputRouter::directionFromProfile(
        m_controllerProfile, activeInputs);

    // The right stick is reserved for smooth scrolling in long UI panels.
    // SDL reports positive Y when the stick is pushed down. Ignore small
    // movements to prevent drift from slowly moving the page.
    const Sint16 rightY = SDL_GameControllerGetAxis(
        m_controller, SDL_CONTROLLER_AXIS_RIGHTY);
    constexpr int rightStickDeadZone = 8000;
    const qreal scrollAmount =
        qAbs(static_cast<int>(rightY)) > rightStickDeadZone
            ? qBound(-1.0, static_cast<qreal>(rightY) / 32767.0, 1.0)
            : 0.0;
    // Emit zero after the stick returns to its dead zone so the QML scroller
    // can apply a short, smooth deceleration instead of stopping abruptly.
    emit rightStickScrollRequested(navigationActive() ? scrollAmount : 0.0);

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
        m_elapsedTimer.elapsed(),
        ControllerInputRouter::actionPressed(m_controllerProfile,
                                             QStringLiteral("details"), activeInputs));
#endif
}
