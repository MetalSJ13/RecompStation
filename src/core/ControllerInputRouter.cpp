#include "ControllerInputRouter.h"


ControllerInputRouter::ControllerInputRouter(QObject *parent)
    : QObject(parent)
{
}

QString ControllerInputRouter::directionFromAxes(qint16 x, qint16 y, int deadZone)
{
    // Promote SDL's signed 16-bit axis values before taking absolute values.
    // In particular, abs(-32768) cannot be represented by qint16.
    const int horizontal = static_cast<int>(x);
    const int vertical = static_cast<int>(y);
    // These promoted values are bounded to [-32768, 32767], so negating
    // them as int is always representable. Avoid qAbs() here entirely.
    const unsigned int horizontalMagnitude = horizontal < 0
        ? static_cast<unsigned int>(-horizontal)
        : static_cast<unsigned int>(horizontal);
    const unsigned int verticalMagnitude = vertical < 0
        ? static_cast<unsigned int>(-vertical)
        : static_cast<unsigned int>(vertical);
    if (horizontalMagnitude <= static_cast<unsigned int>(deadZone)
        && verticalMagnitude <= static_cast<unsigned int>(deadZone))
        return {};

    if (horizontalMagnitude > verticalMagnitude)
        return horizontal < 0 ? QStringLiteral("left")
                              : QStringLiteral("right");

    return vertical < 0 ? QStringLiteral("up")
                        : QStringLiteral("down");
}


QString ControllerInputRouter::firstNewInput(const QStringList &activeInputs,
                                                const QStringList &previousInputs)
{
    for (const QString &input : activeInputs) {
        if (!previousInputs.contains(input))
            return input;
    }
    return {};
}

bool ControllerInputRouter::actionPressed(const QVariantMap &profile,
                                          const QString &action,
                                          const QStringList &activeInputs)
{
    const QVariantList mappedInputs = profile.value(action).toList();
    for (const QVariant &mappedInput : mappedInputs) {
        if (activeInputs.contains(mappedInput.toString()))
            return true;
    }
    return false;
}

QString ControllerInputRouter::directionFromProfile(const QVariantMap &profile,
                                                    const QStringList &activeInputs)
{
    // Keep direction selection deterministic when a user maps multiple
    // simultaneously-held inputs to different directions.
    static const QList<QPair<QString, QString>> directions = {
        {QStringLiteral("navigateUp"), QStringLiteral("up")},
        {QStringLiteral("navigateDown"), QStringLiteral("down")},
        {QStringLiteral("navigateLeft"), QStringLiteral("left")},
        {QStringLiteral("navigateRight"), QStringLiteral("right")}
    };
    for (const auto &direction : directions) {
        if (actionPressed(profile, direction.first, activeInputs))
            return direction.second;
    }
    return {};
}

bool ControllerInputRouter::inputEnabled() const
{
    return m_inputEnabled;
}

bool ControllerInputRouter::navigationActive() const
{
    return m_inputEnabled && !m_gameSessionActive;
}

void ControllerInputRouter::setGameSessionActive(bool active)
{
    if (m_gameSessionActive == active)
        return;

    m_gameSessionActive = active;
    m_currentDirection.clear();
    m_lastNavigationMs = 0;
    m_directionStartedMs = 0;
}

void ControllerInputRouter::setInputEnabled(bool enabled)
{
    if (m_inputEnabled == enabled)
        return;

    m_inputEnabled = enabled;
    m_currentDirection.clear();
    m_lastNavigationMs = 0;
    m_directionStartedMs = 0;
    emit inputEnabledChanged();
}

void ControllerInputRouter::setNavigationAccelerationEnabled(bool enabled)
{
    if (m_navigationAccelerationEnabled == enabled)
        return;

    m_navigationAccelerationEnabled = enabled;
    m_currentDirection.clear();
    m_lastNavigationMs = 0;
    m_directionStartedMs = 0;
}

void ControllerInputRouter::processFrame(const QString &direction,
                                         bool selectPressed,
                                         bool backPressed,
                                         bool menuPressed,
                                         bool previousSectionPressed,
                                         bool nextSectionPressed,
                                         qint64 elapsedMs)
{
    // Always track physical button states while suspended. This prevents a
    // held button from firing unexpectedly when input is re-enabled.
    if (!navigationActive()) {
        m_previousSelect = selectPressed;
        m_previousBack = backPressed;
        m_previousMenu = menuPressed;
        m_previousPreviousSection = previousSectionPressed;
        m_previousNextSection = nextSectionPressed;
        m_currentDirection.clear();
        m_lastNavigationMs = 0;
        m_directionStartedMs = 0;
        return;
    }

    if (selectPressed && !m_previousSelect)
        emit selectRequested();
    if (backPressed && !m_previousBack)
        emit backRequested();
    if (menuPressed && !m_previousMenu)
        emit menuRequested();
    if (previousSectionPressed && !m_previousPreviousSection)
        emit previousSectionRequested();
    if (nextSectionPressed && !m_previousNextSection)
        emit nextSectionRequested();

    m_previousSelect = selectPressed;
    m_previousBack = backPressed;
    m_previousMenu = menuPressed;
    m_previousPreviousSection = previousSectionPressed;
    m_previousNextSection = nextSectionPressed;

    if (direction.isEmpty()) {
        m_currentDirection.clear();
        m_lastNavigationMs = 0;
        m_directionStartedMs = 0;
        return;
    }

    // A new direction fires immediately. In Library List View, held input
    // starts deliberately and progressively accelerates without skipping items.
    if (direction != m_currentDirection) {
        m_currentDirection = direction;
        m_directionStartedMs = elapsedMs;
        m_lastNavigationMs = elapsedMs;
        emit navigationRequested(direction);
        return;
    }

    qint64 repeatIntervalMs = 220;
    if (m_navigationAccelerationEnabled) {
        const qint64 heldMs = qMax<qint64>(0, elapsedMs - m_directionStartedMs);
        if (heldMs < 500)
            repeatIntervalMs = 280;
        else if (heldMs < 900)
            repeatIntervalMs = 150;
        else
            repeatIntervalMs = 75;
    }

    if (elapsedMs - m_lastNavigationMs >= repeatIntervalMs) {
        m_lastNavigationMs = elapsedMs;
        emit navigationRequested(direction);
    }
}
