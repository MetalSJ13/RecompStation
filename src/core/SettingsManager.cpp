#include "SettingsManager.h"

#include <QSettings>
#include "Logging.h"

#include <QDebug>
#include <QStringList>

namespace {
const QString kDefaultTheme = QStringLiteral("midnight-violet");
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
    qCInfo(lcSettings) << "Loaded theme:" << m_themeName;
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

bool SettingsManager::isSupportedTheme(const QString &themeName)
{
    static const QStringList supportedThemes = {
        QStringLiteral("midnight-violet"),
        QStringLiteral("electric-blue"),
        QStringLiteral("ember")
    };

    return supportedThemes.contains(themeName);
}
