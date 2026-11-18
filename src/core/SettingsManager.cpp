#include "SettingsManager.h"

#include <QSettings>
#include "Logging.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QStringList>

namespace {
const QString kDefaultTheme = QStringLiteral("midnight-violet");
const QString kDefaultPresentationSize = QStringLiteral("system");
const QString kDefaultDisplayMode = QStringLiteral("windowed");
const QString kDefaultFontSize = QStringLiteral("medium");
}

SettingsManager::SettingsManager(QObject *parent)
    : SettingsManager(QSettings().fileName(), parent)
{
}

SettingsManager::SettingsManager(const QString &settingsFilePath, QObject *parent)
    : QObject(parent)
    , m_settingsFilePath(settingsFilePath)
{
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    const QString storedTheme = settings.value(
        QStringLiteral("appearance/theme"),
        kDefaultTheme
    ).toString();

    m_themeName = isSupportedTheme(storedTheme)
        ? storedTheme
        : kDefaultTheme;
    m_scanOnStartup = settings.value(QStringLiteral("library/scanOnStartup"), true).toBool();
    m_theGamesDbEnabled = settings.value(QStringLiteral("metadata/theGamesDbEnabled"), true).toBool();

    const QString storedPresentationSize = settings.value(
        QStringLiteral("display/preferredPresentationSize"),
        kDefaultPresentationSize
    ).toString();
    m_preferredPresentationSize = isSupportedPresentationSize(storedPresentationSize)
        ? storedPresentationSize
        : kDefaultPresentationSize;

    const QString storedDisplayMode = settings.value(
        QStringLiteral("display/windowMode"),
        kDefaultDisplayMode
    ).toString();
    m_displayMode = isSupportedDisplayMode(storedDisplayMode)
        ? storedDisplayMode
        : kDefaultDisplayMode;
    const QString storedRestoreDisplayMode = settings.value(
        QStringLiteral("display/restoreWindowMode"),
        kDefaultDisplayMode
    ).toString();
    m_restoreDisplayMode = storedRestoreDisplayMode == QStringLiteral("maximized")
        ? QStringLiteral("maximized") : QStringLiteral("windowed");

    const QString storedFontSize = settings.value(
        QStringLiteral("appearance/fontSize"),
        kDefaultFontSize
    ).toString();
    m_fontSize = isSupportedFontSize(storedFontSize)
        ? storedFontSize
        : kDefaultFontSize;
    m_reducedMotion = settings.value(
        QStringLiteral("appearance/reducedMotion"),
        false
    ).toBool();

    const QString storedLibraryView = settings.value(
        QStringLiteral("library/view"), QStringLiteral("grid")).toString();
    m_libraryView = isSupportedLibraryView(storedLibraryView)
        ? storedLibraryView : QStringLiteral("grid");
    const QString storedLibraryDensity = settings.value(
        QStringLiteral("library/density"), QStringLiteral("comfortable")).toString();
    m_libraryDensity = isSupportedLibraryDensity(storedLibraryDensity)
        ? storedLibraryDensity : QStringLiteral("comfortable");
    const QString storedBackgroundType = settings.value(
        QStringLiteral("appearance/libraryBackgroundType"), QStringLiteral("static")).toString();
    m_libraryBackgroundType = isSupportedLibraryBackgroundType(storedBackgroundType)
        ? storedBackgroundType : QStringLiteral("static");
    m_customLibraryBackground = settings.value(
        QStringLiteral("appearance/customLibraryBackground")).toString();

    qCInfo(lcSettings) << "Loaded theme:" << m_themeName;
    qCInfo(lcSettings) << "Loaded preferred presentation size:" << m_preferredPresentationSize;
    qCInfo(lcSettings) << "Loaded display mode:" << m_displayMode;
    qCInfo(lcSettings) << "Loaded restore display mode:" << m_restoreDisplayMode;
    qCInfo(lcSettings) << "Loaded font size:" << m_fontSize;
    qCInfo(lcSettings) << "Loaded reduced motion:" << m_reducedMotion;
}

bool SettingsManager::scanOnStartup() const
{
    return m_scanOnStartup;
}

void SettingsManager::setScanOnStartup(bool enabled)
{
    if (m_scanOnStartup == enabled)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("library/scanOnStartup"), enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist scan-on-startup preference.";
        return;
    }

    m_scanOnStartup = enabled;
    emit scanOnStartupChanged();
}


bool SettingsManager::theGamesDbEnabled() const
{
    return m_theGamesDbEnabled;
}

void SettingsManager::setTheGamesDbEnabled(bool enabled)
{
    if (m_theGamesDbEnabled == enabled)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("metadata/theGamesDbEnabled"), enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist TheGamesDB provider preference.";
        return;
    }

    m_theGamesDbEnabled = enabled;
    emit theGamesDbEnabledChanged();
    qCInfo(lcSettings) << "TheGamesDB provider enabled:" << m_theGamesDbEnabled;
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

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
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

QString SettingsManager::preferredPresentationSize() const
{
    return m_preferredPresentationSize;
}

void SettingsManager::setPreferredPresentationSize(const QString &size)
{
    if (!isSupportedPresentationSize(size)) {
        qCWarning(lcSettings) << "Rejected unsupported preferred presentation size:" << size;
        return;
    }
    if (m_preferredPresentationSize == size)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("display/preferredPresentationSize"), size);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist preferred presentation size:" << size;
        return;
    }

    m_preferredPresentationSize = size;
    emit preferredPresentationSizeChanged();
    qCInfo(lcSettings) << "Saved preferred presentation size:" << size;
}

QString SettingsManager::displayMode() const
{
    return m_displayMode;
}

void SettingsManager::setDisplayMode(const QString &mode)
{
    if (!isSupportedDisplayMode(mode)) {
        qCWarning(lcSettings) << "Rejected unsupported display mode:" << mode;
        return;
    }
    if (m_displayMode == mode)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("display/windowMode"), mode);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist display mode:" << mode;
        return;
    }

    m_displayMode = mode;
    if (mode != QStringLiteral("fullscreen"))
        setRestoreDisplayMode(mode);
    emit displayModeChanged();
    qCInfo(lcSettings) << "Saved display mode:" << mode;
}

QString SettingsManager::restoreDisplayMode() const
{
    return m_restoreDisplayMode;
}

void SettingsManager::setRestoreDisplayMode(const QString &mode)
{
    if (mode != QStringLiteral("windowed") && mode != QStringLiteral("maximized")) {
        qCWarning(lcSettings) << "Rejected unsupported restore display mode:" << mode;
        return;
    }
    if (m_restoreDisplayMode == mode)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("display/restoreWindowMode"), mode);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist restore display mode:" << mode;
        return;
    }

    m_restoreDisplayMode = mode;
    emit restoreDisplayModeChanged();
}

QString SettingsManager::fontSize() const
{
    return m_fontSize;
}

void SettingsManager::setFontSize(const QString &size)
{
    if (!isSupportedFontSize(size)) {
        qCWarning(lcSettings) << "Rejected unsupported font size:" << size;
        return;
    }
    if (m_fontSize == size)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/fontSize"), size);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist font size:" << size;
        return;
    }

    m_fontSize = size;
    emit fontSizeChanged();
    qCInfo(lcSettings) << "Saved font size:" << size;
}

bool SettingsManager::reducedMotion() const
{
    return m_reducedMotion;
}

void SettingsManager::setReducedMotion(bool enabled)
{
    if (m_reducedMotion == enabled)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/reducedMotion"), enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist reduced-motion preference.";
        return;
    }

    m_reducedMotion = enabled;
    emit reducedMotionChanged();
    qCInfo(lcSettings) << "Saved reduced motion:" << m_reducedMotion;
}

QString SettingsManager::libraryView() const
{
    return m_libraryView;
}

void SettingsManager::setLibraryView(const QString &view)
{
    if (!isSupportedLibraryView(view) || m_libraryView == view)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("library/view"), view);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist library view:" << view;
        return;
    }
    m_libraryView = view;
    emit libraryViewChanged();
}

QString SettingsManager::libraryDensity() const
{
    return m_libraryDensity;
}

void SettingsManager::setLibraryDensity(const QString &density)
{
    if (!isSupportedLibraryDensity(density) || m_libraryDensity == density)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("library/density"), density);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist library density:" << density;
        return;
    }
    m_libraryDensity = density;
    emit libraryDensityChanged();
}

QString SettingsManager::libraryBackgroundType() const
{
    return m_libraryBackgroundType;
}

void SettingsManager::setLibraryBackgroundType(const QString &type)
{
    if (!isSupportedLibraryBackgroundType(type) || m_libraryBackgroundType == type)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/libraryBackgroundType"), type);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist library background type:" << type;
        return;
    }
    m_libraryBackgroundType = type;
    emit libraryBackgroundTypeChanged();
}

QString SettingsManager::customLibraryBackground() const
{
    return m_customLibraryBackground;
}

void SettingsManager::setCustomLibraryBackground(const QString &path)
{
    if (m_customLibraryBackground == path)
        return;

    QString savedPath;
    if (!path.isEmpty()) {
        const QUrl sourceUrl(path);
        const QString sourcePath = sourceUrl.isLocalFile() ? sourceUrl.toLocalFile() : path;
        const QFileInfo sourceInfo(sourcePath);
        const QString extension = sourceInfo.suffix().toLower();
        static const QStringList supportedExtensions = {
            QStringLiteral("png"), QStringLiteral("jpg"),
            QStringLiteral("jpeg"), QStringLiteral("webp")
        };
        if (!sourceInfo.isFile() || !supportedExtensions.contains(extension)) {
            qCWarning(lcSettings) << "Rejected invalid custom library background:" << path;
            return;
        }

        const QString dataDirectory = QFileInfo(m_settingsFilePath).absolutePath();
        const QString backgroundDirectory = QDir(dataDirectory).filePath(
            QStringLiteral("../backgrounds"));
        if (!QDir().mkpath(backgroundDirectory)) {
            qCCritical(lcSettings) << "Could not create custom background directory:"
                                   << backgroundDirectory;
            return;
        }

        const QString destinationPath = QDir::cleanPath(QDir(backgroundDirectory).filePath(
            QStringLiteral("library-background.") + extension));
        if (QFileInfo(sourcePath).absoluteFilePath() != QFileInfo(destinationPath).absoluteFilePath()) {
            if (QFileInfo::exists(destinationPath) && !QFile::remove(destinationPath)) {
                qCCritical(lcSettings) << "Could not replace previous custom library background.";
                return;
            }
            if (!QFile::copy(sourcePath, destinationPath)) {
                qCCritical(lcSettings) << "Could not copy custom library background:" << sourcePath;
                return;
            }
        }
        savedPath = QUrl::fromLocalFile(destinationPath).toString();
    }

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/customLibraryBackground"), savedPath);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist custom library background.";
        return;
    }
    m_customLibraryBackground = savedPath;
    emit customLibraryBackgroundChanged();
}

bool SettingsManager::isSupportedLibraryView(const QString &view)
{
    return view == QStringLiteral("grid") || view == QStringLiteral("list");
}

bool SettingsManager::isSupportedLibraryDensity(const QString &density)
{
    return density == QStringLiteral("compact")
        || density == QStringLiteral("comfortable")
        || density == QStringLiteral("spacious");
}

bool SettingsManager::isSupportedLibraryBackgroundType(const QString &type)
{
    return type == QStringLiteral("static")
        || type == QStringLiteral("animated")
        || type == QStringLiteral("custom");
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

bool SettingsManager::isSupportedPresentationSize(const QString &size)
{
    static const QStringList supportedSizes = {
        QStringLiteral("system"),
        QStringLiteral("1280x720"),
        QStringLiteral("1920x1080"),
        QStringLiteral("1920x1440"),
        QStringLiteral("3440x1440"),
        QStringLiteral("3840x2160"),
        QStringLiteral("5120x2160")
    };

    return supportedSizes.contains(size);
}

bool SettingsManager::isSupportedDisplayMode(const QString &mode)
{
    static const QStringList supportedModes = {
        QStringLiteral("windowed"),
        QStringLiteral("maximized"),
        QStringLiteral("fullscreen")
    };

    return supportedModes.contains(mode);
}

bool SettingsManager::isSupportedFontSize(const QString &size)
{
    static const QStringList supportedSizes = {
        QStringLiteral("small"),
        QStringLiteral("medium"),
        QStringLiteral("large")
    };

    return supportedSizes.contains(size);
}
