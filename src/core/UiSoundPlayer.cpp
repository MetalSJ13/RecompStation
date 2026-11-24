#include "UiSoundPlayer.h"

#include "Logging.h"
#include "SettingsManager.h"

#include <QRandomGenerator>
#include <QUrl>

#ifdef RECOMPSTATION_HAS_MULTIMEDIA
#include <QAudio>
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
    auto make = [this](const QString &file) {
        auto *effect = new QSoundEffect(this);
        effect->setSource(QUrl(QStringLiteral("qrc:/sounds/%1.wav").arg(file)));
        return effect;
    };
    for (int i = 1; i <= 3; ++i)
        m_effects[QStringLiteral("move")].append(make(QStringLiteral("move-%1").arg(i)));
    for (const QString &name : single)
        m_effects[name].append(make(name));
    qCInfo(lcApplication) << "Interface sounds loaded.";
#else
    qCInfo(lcApplication) << "Interface sounds unavailable: built without Qt Multimedia.";
#endif
}

void UiSoundPlayer::applyVolume()
{
#ifdef RECOMPSTATION_HAS_MULTIMEDIA
    const int percent = m_settings ? m_settings->soundVolume() : 60;
    // Perceptual scale: each step of the slider sounds like the same change.
    const qreal linear = QAudio::convertVolume(qBound(0, percent, 100) / 100.0,
                                               QAudio::LogarithmicVolumeScale,
                                               QAudio::LinearVolumeScale);
    for (const QList<QSoundEffect *> &effects : std::as_const(m_effects)) {
        for (QSoundEffect *effect : effects)
            effect->setVolume(linear);
    }
#endif
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
    effects.at(index)->play();
#else
    Q_UNUSED(name);
#endif
}
