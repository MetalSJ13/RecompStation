#include "SettingsManager.h"

#include <QSettings>
#include "Logging.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
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

    // Ember was retired in favour of Aurora and Carbon. Its users move to
    // Electric Blue, and the stored value is rewritten so the log stays quiet.
    if (storedTheme == QStringLiteral("ember")) {
        m_themeName = QStringLiteral("electric-blue");
        settings.setValue(QStringLiteral("appearance/theme"), m_themeName);
        qCInfo(lcSettings) << "Migrated retired theme ember to" << m_themeName;
    } else {
        m_themeName = isSupportedTheme(storedTheme)
            ? storedTheme
            : kDefaultTheme;
    }
    const QString storedScale = settings.value(
        QStringLiteral("display/interfaceScale"), QStringLiteral("auto")).toString();
    m_interfaceScale = isSupportedInterfaceScale(storedScale)
        ? storedScale : QStringLiteral("auto");
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
    m_prepareGameFiles = settings.value(QStringLiteral("launch/prepareGameFiles"), true).toBool();
    m_detailedLogging = settings.value(QStringLiteral("application/detailedLogging"), false).toBool();
    m_startGamesFullscreen = settings.value(QStringLiteral("launch/startGamesFullscreen"), true).toBool();
    m_windowsLaunchersThroughWine = settings.value(QStringLiteral("launch/windowsThroughWine"), false).toBool();
    const QString storedOrder = settings.value(
        QStringLiteral("metadata/providerOrder"), QStringLiteral("launchbox")).toString();
    m_metadataProviderOrder = storedOrder == QStringLiteral("thegamesdb")
        ? storedOrder : QStringLiteral("launchbox");
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
    m_soundsEnabled = settings.value(
        QStringLiteral("appearance/soundsEnabled"), true).toBool();
    m_soundVolume = qBound(0, settings.value(
        QStringLiteral("appearance/soundVolume"), 60).toInt(), 100);

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

    qCDebug(lcSettings) << "Loaded library settings:"
                          << "file:" << m_settingsFilePath
                          << "view:" << m_libraryView
                          << "grid density:" << m_libraryDensity
                          << "list density:" << m_libraryListDensity
                          << "carousel density:" << m_libraryConsoleDensity;
    qCDebug(lcSettings) << "Loaded theme:" << m_themeName;
    qCDebug(lcSettings) << "Loaded preferred presentation size:" << m_preferredPresentationSize;
    qCDebug(lcSettings) << "Loaded display mode:" << m_displayMode;
    qCDebug(lcSettings) << "Loaded restore display mode:" << m_restoreDisplayMode;
    qCDebug(lcSettings) << "Loaded font size:" << m_fontSize;
    qCDebug(lcSettings) << "Loaded reduced motion:" << m_reducedMotion;
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

QString SettingsManager::metadataProviderOrder() const
{
    return m_metadataProviderOrder;
}

void SettingsManager::setMetadataProviderOrder(const QString &order)
{
    if (order != QStringLiteral("launchbox") && order != QStringLiteral("thegamesdb")) {
        qCWarning(lcSettings) << "Rejected unsupported metadata provider order:" << order;
        return;
    }
    if (m_metadataProviderOrder == order)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("metadata/providerOrder"), order);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist metadata provider order:" << order;
        return;
    }
    m_metadataProviderOrder = order;
    emit metadataProviderOrderChanged();
    qCInfo(lcSettings) << "Saved metadata provider order:" << order;
}

QString SettingsManager::theGamesDbKeyFilePath() const
{
    return QFileInfo(m_settingsFilePath).absoluteDir().filePath(
        QStringLiteral("thegamesdb.key"));
}

QString SettingsManager::theGamesDbApiKey() const
{
    const QString environmentKey = qEnvironmentVariable("RECOMPSTATION_TGDB_API_KEY").trimmed();
    if (!environmentKey.isEmpty())
        return environmentKey;
    QFile file(theGamesDbKeyFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString SettingsManager::theGamesDbKeySource() const
{
    if (!qEnvironmentVariable("RECOMPSTATION_TGDB_API_KEY").trimmed().isEmpty())
        return QStringLiteral("environment");
    return theGamesDbApiKey().isEmpty() ? QStringLiteral("none") : QStringLiteral("saved");
}

QString SettingsManager::theGamesDbKeyHint() const
{
    const QString key = theGamesDbApiKey();
    return key.size() >= 8 ? key.right(4) : QString();
}

bool SettingsManager::isPlausibleTheGamesDbApiKey(const QString &key)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9]{16,128}$"));
    return pattern.match(key.trimmed()).hasMatch();
}

bool SettingsManager::saveTheGamesDbApiKey(const QString &key)
{
    const QString trimmed = key.trimmed();
    if (!isPlausibleTheGamesDbApiKey(trimmed)) {
        qCWarning(lcSettings) << "Rejected a TheGamesDB API key that is not 16-128 letters and digits.";
        return false;
    }
    QDir().mkpath(QFileInfo(theGamesDbKeyFilePath()).absolutePath());
    QSaveFile file(theGamesDbKeyFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCCritical(lcSettings) << "Could not save the TheGamesDB API key:" << file.errorString();
        return false;
    }
    // Owner read/write only, applied to the temporary file before it replaces
    // the old one, so the key is never readable by other users.
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(trimmed.toUtf8());
    file.write("\n");
    if (!file.commit()) {
        qCCritical(lcSettings) << "Could not save the TheGamesDB API key:" << file.errorString();
        return false;
    }
    qCInfo(lcSettings) << "Saved a TheGamesDB API key ending in" << trimmed.right(4);
    emit theGamesDbApiKeyChanged();
    return true;
}

bool SettingsManager::clearTheGamesDbApiKey()
{
    const QString path = theGamesDbKeyFilePath();
    if (QFileInfo::exists(path) && !QFile::remove(path)) {
        qCCritical(lcSettings) << "Could not remove the saved TheGamesDB API key.";
        return false;
    }
    qCInfo(lcSettings) << "Removed the saved TheGamesDB API key.";
    emit theGamesDbApiKeyChanged();
    return true;
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
        {QStringLiteral("nextSection"), QVariantList{QStringLiteral("rightshoulder")}},
        {QStringLiteral("details"), QVariantList{QStringLiteral("y")}}
    };
}

// Actions added after controller profiles were first saved. A stored profile
// that predates one of these keeps its mappings and receives the default.
static const QStringList &controllerActionsAddedLater()
{
    static const QStringList actions = { QStringLiteral("details") };
    return actions;
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

    QVariantMap stored = settings.value(key).toMap();
    const QVariantMap defaults = defaultControllerProfile();
    // Profiles saved before an action existed receive that action's default.
    for (const QString &action : controllerActionsAddedLater()) {
        if (!stored.contains(action))
            stored.insert(action, defaults.value(action));
    }
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
        QStringLiteral("leftstick-click"), QStringLiteral("rightstick-click"),
        QStringLiteral("leftshoulder"), QStringLiteral("rightshoulder"),
        QStringLiteral("lefttrigger"), QStringLiteral("righttrigger"),
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
    qCDebug(lcSettings) << "Theme change requested:" << themeName
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

QString SettingsManager::interfaceScale() const
{
    return m_interfaceScale;
}

void SettingsManager::setInterfaceScale(const QString &scale)
{
    if (!isSupportedInterfaceScale(scale)) {
        qCWarning(lcSettings) << "Rejected unsupported interface scale:" << scale;
        return;
    }
    if (m_interfaceScale == scale)
        return;

    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("display/interfaceScale"), scale);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist interface scale:" << scale;
        return;
    }

    m_interfaceScale = scale;
    emit interfaceScaleChanged();
    qCInfo(lcSettings) << "Saved interface scale:" << scale;
}

QVariantList SettingsManager::interfaceScaleOptions() const
{
    QVariantList options;
    options.append(QStringLiteral("auto"));
    for (int percent : interfaceScalePercents())
        options.append(QString::number(percent));
    return options;
}

QList<int> SettingsManager::interfaceScalePercents()
{
    return {75, 85, 100, 115, 125, 150, 175, 200};
}

bool SettingsManager::isSupportedInterfaceScale(const QString &scale)
{
    if (scale == QStringLiteral("auto"))
        return true;
    bool ok = false;
    const int percent = scale.toInt(&ok);
    return ok && interfaceScalePercents().contains(percent);
}

int SettingsManager::autoInterfaceScalePercent(int logicalScreenHeight)
{
    if (logicalScreenHeight <= 0)
        return 100;
    if (logicalScreenHeight < 900)
        return 85;     // 720p, 768p, Steam Deck 800p
    if (logicalScreenHeight < 1300)
        return 100;    // 1080p, 1200p
    if (logicalScreenHeight < 1700)
        return 125;    // 1440p, 1600p
    if (logicalScreenHeight < 2000)
        return 150;    // 1800p
    if (logicalScreenHeight < 2600)
        return 175;    // 2160p (4K)
    return 200;        // 2880p and above
}

QString SettingsManager::storedInterfaceScale(const QString &settingsFilePath)
{
    QSettings settings(settingsFilePath, QSettings::IniFormat);
    const QString value = settings.value(
        QStringLiteral("display/interfaceScale"), QStringLiteral("auto")).toString();
    return isSupportedInterfaceScale(value) ? value : QStringLiteral("auto");
}

bool SettingsManager::resetPreferences()
{
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    for (const QString &group : {QStringLiteral("appearance"), QStringLiteral("display"),
                                 QStringLiteral("controllers"), QStringLiteral("launch")})
        settings.remove(group);
    for (const QString &key : {QStringLiteral("library/view"), QStringLiteral("library/density"),
                               QStringLiteral("library/listDensity"), QStringLiteral("library/consoleDensity"),
                               QStringLiteral("library/scanOnStartup"),
                               QStringLiteral("metadata/providerOrder"),
                               QStringLiteral("metadata/automaticMatchingEnabled"),
                               QStringLiteral("metadata/automaticArtworkDownloadsEnabled"),
                               QStringLiteral("metadata/theGamesDbEnabled")})
        settings.remove(key);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Could not reset preferences.";
        return false;
    }
    qCInfo(lcSettings) << "Preferences reset to defaults; restart to apply.";
    return true;
}

void SettingsManager::requestApplicationRestart(const QString &settingsSection)
{
    emit applicationRestartRequested(settingsSection);
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

bool SettingsManager::soundsEnabled() const
{
    return m_soundsEnabled;
}

void SettingsManager::setSoundsEnabled(bool enabled)
{
    if (m_soundsEnabled == enabled)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/soundsEnabled"), enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist the interface sounds preference.";
        return;
    }
    m_soundsEnabled = enabled;
    emit soundsEnabledChanged();
}

int SettingsManager::soundVolume() const
{
    return m_soundVolume;
}

void SettingsManager::setSoundVolume(int percent)
{
    percent = qBound(0, percent, 100);
    if (m_soundVolume == percent)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/soundVolume"), percent);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist the interface sound volume.";
        return;
    }
    m_soundVolume = percent;
    emit soundVolumeChanged();
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
        || view == QStringLiteral("carousel")
        || view == QStringLiteral("shelves");
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
        QStringLiteral("aurora"),
        QStringLiteral("carbon")
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

bool SettingsManager::prepareGameFiles() const
{
    return m_prepareGameFiles;
}

void SettingsManager::setPrepareGameFiles(bool enabled)
{
    if (m_prepareGameFiles == enabled)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("launch/prepareGameFiles"), enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist the prepare-game-files preference.";
        return;
    }
    m_prepareGameFiles = enabled;
    ++m_launchSettingsRevision;
    emit launchSettingsChanged();
}

bool SettingsManager::startGamesFullscreen() const
{
    return m_startGamesFullscreen;
}

void SettingsManager::setStartGamesFullscreen(bool enabled)
{
    if (m_startGamesFullscreen == enabled)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("launch/startGamesFullscreen"), enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist the fullscreen preference.";
        return;
    }
    m_startGamesFullscreen = enabled;
    ++m_launchSettingsRevision;
    emit launchSettingsChanged();
}

bool SettingsManager::windowsLaunchersThroughWine() const
{
    return m_windowsLaunchersThroughWine;
}

void SettingsManager::setWindowsLaunchersThroughWine(bool enabled)
{
    if (m_windowsLaunchersThroughWine == enabled)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("launch/windowsThroughWine"), enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist the Wine preference.";
        return;
    }
    m_windowsLaunchersThroughWine = enabled;
    ++m_launchSettingsRevision;
    emit launchSettingsChanged();
}

QString SettingsManager::launchWindowMode(const QString &projectId) const
{
    static const QRegularExpression idPattern(QStringLiteral("^[a-z0-9][a-z0-9-]*$"));
    if (!idPattern.match(projectId).hasMatch())
        return QStringLiteral("default");
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    const QString mode = settings.value(QStringLiteral("launch/windowMode/") + projectId).toString();
    return mode == QStringLiteral("fullscreen") || mode == QStringLiteral("windowed")
        ? mode : QStringLiteral("default");
}

void SettingsManager::setLaunchWindowMode(const QString &projectId, const QString &mode)
{
    static const QRegularExpression idPattern(QStringLiteral("^[a-z0-9][a-z0-9-]*$"));
    if (!idPattern.match(projectId).hasMatch()
        || !QStringList{QStringLiteral("default"), QStringLiteral("fullscreen"),
                        QStringLiteral("windowed")}.contains(mode)
        || launchWindowMode(projectId) == mode)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    if (mode == QStringLiteral("default"))
        settings.remove(QStringLiteral("launch/windowMode/") + projectId);
    else
        settings.setValue(QStringLiteral("launch/windowMode/") + projectId, mode);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist the window mode for" << projectId;
        return;
    }
    ++m_launchSettingsRevision;
    emit launchSettingsChanged();
}

int SettingsManager::launchSettingsRevision() const
{
    return m_launchSettingsRevision;
}

bool SettingsManager::defaultFullscreenApplied(const QString &projectId) const
{
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    return settings.value(QStringLiteral("launch/fullscreenApplied/") + projectId, false).toBool();
}

void SettingsManager::markDefaultFullscreenApplied(const QString &projectId)
{
    static const QRegularExpression idPattern(QStringLiteral("^[a-z0-9][a-z0-9-]*$"));
    if (!idPattern.match(projectId).hasMatch())
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("launch/fullscreenApplied/") + projectId, true);
    settings.sync();
}

bool SettingsManager::detailedLogging() const
{
    return m_detailedLogging;
}

void SettingsManager::setDetailedLogging(bool enabled)
{
    if (m_detailedLogging == enabled)
        return;
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("application/detailedLogging"), enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCCritical(lcSettings) << "Failed to persist the detailed logging preference.";
        return;
    }
    m_detailedLogging = enabled;
    emit detailedLoggingChanged();
}
