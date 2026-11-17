#include "ControllerInputRouter.h"

#include <QtMath>

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
    if (qAbs(horizontal) <= deadZone && qAbs(vertical) <= deadZone)
        return {};

    if (qAbs(horizontal) > qAbs(vertical))
        return horizontal < 0 ? QStringLiteral("left")
                              : QStringLiteral("right");

    return vertical < 0 ? QStringLiteral("up")
                        : QStringLiteral("down");
}

bool ControllerInputRouter::inputEnabled() const
{
    return m_inputEnabled;
}

void ControllerInputRouter::setInputEnabled(bool enabled)
{
    if (m_inputEnabled == enabled)
        return;

    m_inputEnabled = enabled;
    m_currentDirection.clear();
    m_lastNavigationMs = 0;
    emit inputEnabledChanged();
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
    if (!m_inputEnabled) {
        m_previousSelect = selectPressed;
        m_previousBack = backPressed;
        m_previousMenu = menuPressed;
        m_previousPreviousSection = previousSectionPressed;
        m_previousNextSection = nextSectionPressed;
        m_currentDirection.clear();
        m_lastNavigationMs = 0;
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
        return;
    }

    // A new direction fires immediately. A held direction repeats at a
    // controlled cadence rather than flooding the UI every polling frame.
    if (direction != m_currentDirection || elapsedMs - m_lastNavigationMs >= 220) {
        m_currentDirection = direction;
        m_lastNavigationMs = elapsedMs;
        emit navigationRequested(direction);
    }
}
