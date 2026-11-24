#include "UiSoundPlayer.h"

#include "Logging.h"
#include "SettingsManager.h"

#include <QRandomGenerator>
#include <QUrl>

#include <cmath>

#ifdef RECOMPSTATION_HAS_MULTIMEDIA
#include <QAudioDevice>
#include <QMediaDevices>
#include <QSoundEffect>
#endif

UiSoundPlayer::UiSoundPlayer(SettingsManager *settings, QObject *parent)
    : QObject(parent),
      m_settings(settings)
{
    m_clock.start();
    load();
    if (m_settings) {
        connect(m_settings, &SettingsManager::soundVolumeChanged,
                this, &UiSoundPlayer::applyVolume);
    }
    applyVolume();
}

bool UiSoundPlayer::isAvailable() const
{
#ifdef RECOMPSTATION_HAS_MULTIMEDIA
    return true;
#else
    return false;
#endif
}

bool UiSoundPlayer::isSuppressed() const { return m_suppressed; }

void UiSoundPlayer::setSuppressed(bool suppressed)
{
    if (m_suppressed == suppressed)
        return;
    m_suppressed = suppressed;
    emit suppressedChanged();
}

void UiSoundPlayer::load()
{
#ifdef RECOMPSTATION_HAS_MULTIMEDIA
    const QStringList single{
        QStringLiteral("edge"), QStringLiteral("select"), QStringLiteral("back"),
        QStringLiteral("switch"), QStringLiteral("menu"), QStringLiteral("toggle-on"),
        QStringLiteral("toggle-off"), QStringLiteral("launch"), QStringLiteral("error")};
    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull())
        qCWarning(lcApplication) << "Interface sounds: no audio output device found.";
    else
        qCInfo(lcApplication) << "Interface sounds play on:" << device.description();
    auto make = [this, device](const QString &file) {
        auto *effect = new QSoundEffect(device, this);
        effect->setSource(QUrl(QStringLiteral("qrc:/sounds/%1.wav").arg(file)));
        connect(effect, &QSoundEffect::statusChanged, this, [effect]() {
            if (effect->status() == QSoundEffect::Error)
                qCWarning(lcApplication) << "Interface sound could not be loaded:"
                                         << effect->source().toString();
        });
        return effect;
    };
    for (int i = 1; i <= 3; ++i)
        m_effects[QStringLiteral("move")].append(make(QStringLiteral("move-%1").arg(i)));
    for (const QString &name : single)
        m_effects[name].append(make(name));
    // Follow the system's default output when it changes (headphones
    // plugged in, another device picked in the desktop's sound settings).
    auto *devices = new QMediaDevices(this);
    connect(devices, &QMediaDevices::audioOutputsChanged, this, [this]() {
        const QAudioDevice output = QMediaDevices::defaultAudioOutput();
        qCInfo(lcApplication) << "Interface sounds now play on:" << output.description();
        for (const QList<QSoundEffect *> &effects : std::as_const(m_effects)) {
            for (QSoundEffect *effect : effects)
                effect->setAudioDevice(output);
        }
    });
    qCInfo(lcApplication) << "Interface sounds loaded.";
#else
    qCInfo(lcApplication) << "Interface sounds unavailable: built without Qt Multimedia.";
#endif
}

void UiSoundPlayer::applyVolume()
{
#ifdef RECOMPSTATION_HAS_MULTIMEDIA
    const int percent = m_settings ? m_settings->soundVolume() : 60;
    const qreal linear = gainForVolume(percent);
    for (const QList<QSoundEffect *> &effects : std::as_const(m_effects)) {
        for (QSoundEffect *effect : effects)
            effect->setVolume(linear);
    }
#endif
}

qreal UiSoundPlayer::gainForVolume(int percent)
{
    // 100% plays the files as made; lower settings fall off gently
    // (60% is about -7 dB, 10% about -30 dB).
    return std::pow(qBound(0, percent, 100) / 100.0, 1.5);
}

void UiSoundPlayer::play(const QString &name)
{
    if (m_suppressed || !m_settings || !m_settings->soundsEnabled())
        return;
    if (!m_gate.allow(name, m_clock.elapsed()))
        return;
    start(name);
}

void UiSoundPlayer::preview(const QString &name)
{
    if (m_suppressed)
        return;
    start(name);
}

void UiSoundPlayer::start(const QString &name)
{
#ifdef RECOMPSTATION_HAS_MULTIMEDIA
    const QList<QSoundEffect *> effects = m_effects.value(name);
    if (effects.isEmpty())
        return;
    int index = 0;
    if (effects.size() > 1) {
        // A different pitch each time, never the same one twice in a row.
        index = QRandomGenerator::global()->bounded(effects.size() - 1);
        if (index >= m_lastMoveVariant && m_lastMoveVariant >= 0)
            ++index;
        m_lastMoveVariant = index;
    }
    QSoundEffect *effect = effects.at(index);
    if (!m_loggedFirstPlay) {
        // One line per session, to tell "never played" from "played silently".
        m_loggedFirstPlay = true;
        qCInfo(lcApplication) << "First interface sound:" << name
                              << "status" << effect->status()
                              << "volume" << effect->volume()
                              << "muted" << effect->isMuted();
    }
    if (effect->status() == QSoundEffect::Error)
        qCWarning(lcApplication) << "Interface sound" << name << "is not loaded.";
    effect->play();
#else
    Q_UNUSED(name);
#endif
}
