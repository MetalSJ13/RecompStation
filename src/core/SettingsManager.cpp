#include "SettingsManager.h"

#include <QSettings>
#include "Logging.h"

#include <QDebug>
#include <QStringList>

namespace {
const QString kDefaultTheme = QStringLiteral("midnight-violet");
const QString kDefaultDisplayMode = QStringLiteral("fullscreen");
}

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    const QString storedTheme = settings.value(
        QStringLiteral("appearance/theme"),
        kDefaultTheme
    ).toString();

    m_themeName = isSupportedTheme(storedTheme)
        ? storedTheme
        : kDefaultTheme;
    m_displayMode = settings.value(
        QStringLiteral("display/mode"),
        kDefaultDisplayMode
    ).toString();
    if (!isSupportedDisplayMode(m_displayMode))
        m_displayMode = kDefaultDisplayMode;

    qCInfo(lcSettings) << "Loaded theme:" << m_themeName;
    qCInfo(lcSettings) << "Loaded display mode:" << m_displayMode;
}

QString SettingsManager::themeName() const
{
    return m_themeName;
}

void SettingsManager::setThemeName(const QString &themeName)
{
    qCInfo(lcSettings) << "Theme change requested:" << themeName
                       << "current:" << m_themeName;
    if (!isSupportedTheme(themeName)) {
        qCWarning(lcSettings) << "Rejected unsupported theme:" << themeName;
        return;
    }
    if (m_themeName == themeName) {
        qCDebug(lcSettings) << "Theme already selected; no change needed.";
        return;
    }

    m_themeName = themeName;

    QSettings settings;
    settings.setValue(QStringLiteral("appearance/theme"), m_themeName);
    settings.sync();
    if (settings.status() == QSettings::NoError)
        qCInfo(lcSettings) << "Saved theme:" << m_themeName;
    else
        qCCritical(lcSettings) << "Failed to persist theme:" << m_themeName
                               << "QSettings status:" << settings.status();

    emit themeNameChanged();
    qCDebug(lcSettings) << "themeNameChanged emitted.";
}

QString SettingsManager::displayMode() const
{
    return m_displayMode;
}

void SettingsManager::setDisplayMode(const QString &displayMode)
{
    if (!isSupportedDisplayMode(displayMode)) {
        qCWarning(lcSettings) << "Rejected unsupported display mode:" << displayMode;
        return;
    }
    if (m_displayMode == displayMode)
        return;

    m_displayMode = displayMode;
    QSettings settings;
    settings.setValue(QStringLiteral("display/mode"), m_displayMode);
    settings.sync();
    if (settings.status() == QSettings::NoError)
        qCInfo(lcSettings) << "Saved display mode:" << m_displayMode;
    else
        qCCritical(lcSettings) << "Failed to persist display mode:" << m_displayMode
                               << "QSettings status:" << settings.status();

    emit displayModeChanged();
}

bool SettingsManager::isSupportedDisplayMode(const QString &displayMode)
{
    static const QStringList supportedModes = {
        QStringLiteral("fullscreen"),
        QStringLiteral("windowed"),
        QStringLiteral("borderless")
    };
    return supportedModes.contains(displayMode);
}

bool SettingsManager::isSupportedTheme(const QString &themeName)
{
    static const QStringList supportedThemes = {
        QStringLiteral("midnight-violet"),
        QStringLiteral("electric-blue"),
        QStringLiteral("ember")
    };

    return supportedThemes.contains(themeName);
}
