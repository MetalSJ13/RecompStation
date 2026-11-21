#include "SettingsManager.h"

#include <QSettings>
#include "Logging.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QStringList>
#include <QRegularExpression>
#include <QVariantList>

namespace {
const QString kDefaultTheme = QStringLiteral("midnight-violet");
const QString kDefaultPresentationSize = QStringLiteral("system");
const QString kDefaultDisplayMode = QStringLiteral("fullscreen");
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
    m_automaticUpdateChecksEnabled = settings.value(
        QStringLiteral("application/automaticUpdateChecksEnabled"), false).toBool();
    m_updatePreferencePromptCompleted = settings.value(
        QStringLiteral("application/updatePreferencePromptCompleted"), false).toBool();
    m_automaticMetadataMatchingEnabled = settings.value(
        QStringLiteral("metadata/automaticMatchingEnabled"), true).toBool();
    m_automaticArtworkDownloadsEnabled = settings.value(
        QStringLiteral("metadata/automaticArtworkDownloadsEnabled"), true).toBool();
    m_theGamesDbEnabled = settings.value(QStringLiteral("metadata/theGamesDbEnabled"), true).toBool();
    m_launchBoxEnabled = settings.value(QStringLiteral("metadata/launchBoxEnabled"), false).toBool();
    m_launchBoxLastUpdated = settings.value(QStringLiteral("metadata/launchBoxLastUpdated")).toString();

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
        QStringLiteral("windowed")
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

    // Console View is the first-run experience. Existing saved selections
    // remain untouched and continue to take precedence.
    const QString storedLibraryView = settings.value(
        QStringLiteral("library/view"), QStringLiteral("carousel")).toString();
    m_libraryView = isSupportedLibraryView(storedLibraryView)
        ? storedLibraryView : QStringLiteral("carousel");
    const QString storedLibraryDensity = settings.value(
        QStringLiteral("library/density"), QStringLiteral("comfortable")).toString();
    m_libraryDensity = isSupportedLibraryDensity(storedLibraryDensity)
        ? storedLibraryDensity : QStringLiteral("comfortable");
    const QString storedLibraryListDensity = settings.value(
        QStringLiteral("library/listDensity"), QStringLiteral("comfortable")).toString();
    m_libraryListDensity = isSupportedLibraryListDensity(storedLibraryListDensity)
        ? storedLibraryListDensity : QStringLiteral("comfortable");
    const QString storedLibraryConsoleDensity = settings.value(
        QStringLiteral("library/consoleDensity"), QStringLiteral("spacious")).toString();
    m_libraryConsoleDensity = isSupportedLibraryDensity(storedLibraryConsoleDensity)
        ? storedLibraryConsoleDensity : QStringLiteral("spacious");
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

bool SettingsManager::automaticUpdateChecksEnabled() const
{
    return m_automaticUpdateChecksEnabled;
}

bool SettingsManager::updatePreferencePromptCompleted() const
{
    return m_updatePreferencePromptCompleted;
}

void SettingsManager::setAutomaticUpdateChecksEnabled(bool enabled)
{
    if (m_automaticUpdateChecksEnabled == enabled && m_updatePreferencePromptCompleted)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("application/automaticUpdateChecksEnabled"), enabled);
    settings.setValue(QStringLiteral("application/updatePreferencePromptCompleted"), true);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist application update preferences.";
        return;
    }

    const bool enabledChanged = m_automaticUpdateChecksEnabled != enabled;
    const bool completionChanged = !m_updatePreferencePromptCompleted;
    m_automaticUpdateChecksEnabled = enabled;
    m_updatePreferencePromptCompleted = true;
    if (enabledChanged)
        emit automaticUpdateChecksEnabledChanged();
    if (completionChanged)
        emit updatePreferencePromptCompletedChanged();
    qCInfo(lcSettings) << "Automatic application update checks preference:" << enabled;
}

void SettingsManager::completeUpdatePreferencePrompt(bool enabled)
{
    setAutomaticUpdateChecksEnabled(enabled);
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


bool SettingsManager::automaticMetadataMatchingEnabled() const
{
    return m_automaticMetadataMatchingEnabled;
}

void SettingsManager::setAutomaticMetadataMatchingEnabled(bool enabled)
{
    if (m_automaticMetadataMatchingEnabled == enabled)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("metadata/automaticMatchingEnabled"), enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist automatic metadata matching preference.";
        return;
    }

    m_automaticMetadataMatchingEnabled = enabled;
    emit automaticMetadataMatchingEnabledChanged();
    qCInfo(lcSettings) << "Automatic metadata matching enabled:" << m_automaticMetadataMatchingEnabled;
}

bool SettingsManager::automaticArtworkDownloadsEnabled() const
{
    return m_automaticArtworkDownloadsEnabled;
}

void SettingsManager::setAutomaticArtworkDownloadsEnabled(bool enabled)
{
    if (m_automaticArtworkDownloadsEnabled == enabled)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("metadata/automaticArtworkDownloadsEnabled"), enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist automatic artwork download preference.";
        return;
    }

    m_automaticArtworkDownloadsEnabled = enabled;
    emit automaticArtworkDownloadsEnabledChanged();
    qCInfo(lcSettings) << "Automatic artwork downloads enabled:" << m_automaticArtworkDownloadsEnabled;
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

bool SettingsManager::launchBoxEnabled() const
{
    return m_launchBoxEnabled;
}

void SettingsManager::setLaunchBoxEnabled(bool enabled)
{
    if (m_launchBoxEnabled == enabled)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("metadata/launchBoxEnabled"), enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist LaunchBox metadata preference.";
        return;
    }

    m_launchBoxEnabled = enabled;
    emit launchBoxEnabledChanged();
    qCInfo(lcSettings) << "LaunchBox metadata fallback enabled:" << m_launchBoxEnabled;
}

QString SettingsManager::launchBoxLastUpdated() const
{
    return m_launchBoxLastUpdated;
}

void SettingsManager::setLaunchBoxLastUpdated(const QString &timestamp)
{
    if (timestamp.trimmed().isEmpty() || m_launchBoxLastUpdated == timestamp)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("metadata/launchBoxLastUpdated"), timestamp);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist LaunchBox catalog update timestamp.";
        return;
    }

    m_launchBoxLastUpdated = timestamp;
    emit launchBoxLastUpdatedChanged();
}


QVariantMap SettingsManager::defaultControllerProfile()
{
    return {
        {QStringLiteral("navigateUp"), QVariantList{QStringLiteral("dpad-up"), QStringLiteral("leftstick-up")}},
        {QStringLiteral("navigateDown"), QVariantList{QStringLiteral("dpad-down"), QStringLiteral("leftstick-down")}},
        {QStringLiteral("navigateLeft"), QVariantList{QStringLiteral("dpad-left"), QStringLiteral("leftstick-left")}},
        {QStringLiteral("navigateRight"), QVariantList{QStringLiteral("dpad-right"), QStringLiteral("leftstick-right")}},
        {QStringLiteral("select"), QVariantList{QStringLiteral("a")}},
        {QStringLiteral("back"), QVariantList{QStringLiteral("b"), QStringLiteral("guide")}},
        {QStringLiteral("menu"), QVariantList{QStringLiteral("start")}},
        {QStringLiteral("previousSection"), QVariantList{QStringLiteral("leftshoulder")}},
        {QStringLiteral("nextSection"), QVariantList{QStringLiteral("rightshoulder")}}
    };
}

bool SettingsManager::isValidControllerGuid(const QString &guid)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Fa-f0-9]{32}$"));
    return pattern.match(guid).hasMatch();
}

QVariantMap SettingsManager::controllerProfile(const QString &guid) const
{
    if (!isValidControllerGuid(guid))
        return {};

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    const QString key = QStringLiteral("controllers/profiles/") + guid.toLower()
        + QStringLiteral("/mappings");
    if (!settings.contains(key))
        return defaultControllerProfile();

    const QVariantMap stored = settings.value(key).toMap();
    const QVariantMap defaults = defaultControllerProfile();
    // Reject incomplete or malformed profiles as a whole. Returning defaults
    // is safer than exposing a partially corrupted mapping to the input layer.
    QStringList storedKeys = stored.keys();
    QStringList defaultKeys = defaults.keys();
    storedKeys.sort();
    defaultKeys.sort();
    if (storedKeys != defaultKeys)
        return defaults;
    return stored;
}

bool SettingsManager::saveControllerProfile(const QString &guid, const QVariantMap &profile)
{
    if (!isValidControllerGuid(guid))
        return false;

    const QVariantMap defaults = defaultControllerProfile();
    QStringList profileKeys = profile.keys();
    QStringList defaultKeys = defaults.keys();
    profileKeys.sort();
    defaultKeys.sort();
    if (profileKeys != defaultKeys)
        return false;

    static const QStringList supportedInputs = {
        QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("x"), QStringLiteral("y"),
        QStringLiteral("back"), QStringLiteral("guide"), QStringLiteral("start"),
        QStringLiteral("leftstick"), QStringLiteral("rightstick"),
        QStringLiteral("leftshoulder"), QStringLiteral("rightshoulder"),
        QStringLiteral("dpad-up"), QStringLiteral("dpad-down"),
        QStringLiteral("dpad-left"), QStringLiteral("dpad-right"),
        QStringLiteral("leftstick-up"), QStringLiteral("leftstick-down"),
        QStringLiteral("leftstick-left"), QStringLiteral("leftstick-right")
    };
    for (auto it = profile.cbegin(); it != profile.cend(); ++it) {
        const QVariantList inputs = it.value().toList();
        if (inputs.isEmpty())
            return false;
        for (const QVariant &input : inputs) {
            const QString token = input.toString();
            if (!supportedInputs.contains(token))
                return false;
        }
    }

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("controllers/profiles/") + guid.toLower()
                          + QStringLiteral("/mappings"), profile);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist controller profile for GUID:" << guid;
        return false;
    }
    qCInfo(lcSettings) << "Saved controller profile for GUID:" << guid;
    return true;
}

bool SettingsManager::resetControllerProfile(const QString &guid)
{
    if (!isValidControllerGuid(guid))
        return false;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.remove(QStringLiteral("controllers/profiles/") + guid.toLower());
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to reset controller profile for GUID:" << guid;
        return false;
    }
    qCInfo(lcSettings) << "Reset controller profile for GUID:" << guid;
    return true;
}

QStringList SettingsManager::controllerProfileIds() const
{
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("controllers/profiles"));
    QStringList ids = settings.childGroups();
    settings.endGroup();
    return ids;
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

QString SettingsManager::libraryListDensity() const
{
    return m_libraryListDensity;
}

void SettingsManager::setLibraryListDensity(const QString &density)
{
    if (!isSupportedLibraryListDensity(density) || m_libraryListDensity == density)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("library/listDensity"), density);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist library list density:" << density;
        return;
    }
    m_libraryListDensity = density;
    emit libraryListDensityChanged();
}

QString SettingsManager::libraryConsoleDensity() const
{
    return m_libraryConsoleDensity;
}

void SettingsManager::setLibraryConsoleDensity(const QString &density)
{
    if (!isSupportedLibraryDensity(density) || m_libraryConsoleDensity == density)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("library/consoleDensity"), density);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist Console view density:" << density;
        return;
    }
    m_libraryConsoleDensity = density;
    emit libraryConsoleDensityChanged();
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
    return view == QStringLiteral("grid")
        || view == QStringLiteral("list")
        || view == QStringLiteral("carousel");
}

bool SettingsManager::isSupportedLibraryDensity(const QString &density)
{
    return density == QStringLiteral("compact")
        || density == QStringLiteral("comfortable")
        || density == QStringLiteral("spacious");
}

bool SettingsManager::isSupportedLibraryListDensity(const QString &density)
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
