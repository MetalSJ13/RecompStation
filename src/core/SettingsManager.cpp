#include "SettingsManager.h"

#include <QSettings>
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
}

QString SettingsManager::themeName() const
{
    return m_themeName;
}

void SettingsManager::setThemeName(const QString &themeName)
{
    if (!isSupportedTheme(themeName) || m_themeName == themeName)
        return;

    m_themeName = themeName;

    QSettings settings;
    settings.setValue(QStringLiteral("appearance/theme"), m_themeName);
    settings.sync();

    emit themeNameChanged();
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
