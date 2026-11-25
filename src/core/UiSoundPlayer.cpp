#include "UiSoundPlayer.h"

#include "Logging.h"
#include "SettingsManager.h"

#include <QFile>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef RECOMPSTATION_HAS_SDL2
#include <SDL.h>

namespace {
constexpr int SampleRate = 48000;
constexpr int Channels = 2;

void audioCallback(void *userdata, Uint8 *stream, int length)
{
    static_cast<UiSoundPlayer *>(userdata)->mix(
        reinterpret_cast<int16_t *>(stream), length / int(sizeof(int16_t) * Channels));
}
}
#endif

UiSoundPlayer::UiSoundPlayer(SettingsManager *settings, QObject *parent)
    : QObject(parent),
      m_settings(settings)
{
    m_clock.start();
    if (m_settings) {
        m_gain = gainForVolume(m_settings->soundVolume());
        connect(m_settings, &SettingsManager::soundVolumeChanged, this, [this]() {
            m_gain = gainForVolume(m_settings->soundVolume());
        });
    }
    // Sounds are loaded before the device starts pulling from the mixer.
    loadSounds();
    openDevice();
}

UiSoundPlayer::~UiSoundPlayer()
{
#ifdef RECOMPSTATION_HAS_SDL2
    if (m_device != 0) {
        SDL_CloseAudioDevice(m_device);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
#endif
}

bool UiSoundPlayer::isAvailable() const
{
    return m_device != 0;
}

bool UiSoundPlayer::isSuppressed() const { return m_suppressed; }

void UiSoundPlayer::setSuppressed(bool suppressed)
{
    if (m_suppressed == suppressed)
        return;
    m_suppressed = suppressed;
    emit suppressedChanged();
}

float UiSoundPlayer::gainForVolume(int percent)
{
    // 100% plays the files as made; lower settings fall off gently
    // (60% is about -7 dB, 10% about -30 dB).
    return float(std::pow(qBound(0, percent, 100) / 100.0, 1.5));
}

bool UiSoundPlayer::openDevice()
{
#ifdef RECOMPSTATION_HAS_SDL2
    // Names shown in the desktop's volume mixer.
#ifdef SDL_HINT_AUDIO_DEVICE_APP_NAME
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_APP_NAME, "RecompStation");
#endif
#ifdef SDL_HINT_AUDIO_DEVICE_STREAM_NAME
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_STREAM_NAME, "Interface sounds");
#endif
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        qCWarning(lcApplication) << "Interface sounds unavailable: SDL audio failed to start:"
                                 << SDL_GetError();
        return false;
    }
    SDL_AudioSpec wanted;
    SDL_zero(wanted);
    wanted.freq = SampleRate;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = Channels;
    wanted.samples = 512;  // About 10 ms, so a sound starts right away.
    wanted.callback = audioCallback;
    wanted.userdata = this;
    SDL_AudioSpec obtained;
    // No allowed changes: SDL converts to whatever the device needs.
    m_device = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, 0);
    if (m_device == 0) {
        qCWarning(lcApplication) << "Interface sounds unavailable: no audio output could be opened:"
                                 << SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    qCInfo(lcApplication) << "Interface sounds play through SDL audio driver"
                          << SDL_GetCurrentAudioDriver() << "on the default output.";
    SDL_PauseAudioDevice(m_device, 0);
    return true;
#else
    qCInfo(lcApplication) << "Interface sounds unavailable: built without SDL2.";
    return false;
#endif
}

void UiSoundPlayer::loadSounds()
{
#ifdef RECOMPSTATION_HAS_SDL2
    auto load = [this](const QString &file) -> const std::vector<int16_t> * {
        QFile source(QStringLiteral(":/sounds/%1.wav").arg(file));
        if (!source.open(QIODevice::ReadOnly)) {
            qCWarning(lcApplication) << "Interface sound missing:" << file;
            return nullptr;
        }
        const QByteArray bytes = source.readAll();
        SDL_RWops *stream = SDL_RWFromConstMem(bytes.constData(), int(bytes.size()));
        SDL_AudioSpec spec;
        Uint8 *buffer = nullptr;
        Uint32 length = 0;
        if (!SDL_LoadWAV_RW(stream, 1, &spec, &buffer, &length)) {
            qCWarning(lcApplication) << "Interface sound could not be read:" << file << SDL_GetError();
            return nullptr;
        }
        // The files are made as 48 kHz 16-bit stereo; convert anything else.
        SDL_AudioCVT convert;
        if (SDL_BuildAudioCVT(&convert, spec.format, spec.channels, spec.freq,
                              AUDIO_S16SYS, Channels, SampleRate) < 0) {
            SDL_FreeWAV(buffer);
            return nullptr;
        }
        std::vector<Uint8> work(size_t(length) * size_t(std::max(1, convert.len_mult)));
        std::memcpy(work.data(), buffer, length);
        SDL_FreeWAV(buffer);
        int converted = int(length);
        if (convert.needed) {
            convert.len = int(length);
            convert.buf = work.data();
            if (SDL_ConvertAudio(&convert) != 0)
                return nullptr;
            converted = convert.len_cvt;
        }
        // Whole stereo frames only.
        const size_t count = (size_t(converted) / sizeof(int16_t)) & ~size_t(1);
        std::vector<int16_t> samples(count);
        std::memcpy(samples.data(), work.data(), count * sizeof(int16_t));
        m_storage.push_back(std::move(samples));
        return &m_storage.back();
    };

    int loaded = 0;
    for (int i = 1; i <= 3; ++i) {
        if (const auto *sound = load(QStringLiteral("move-%1").arg(i))) {
            m_sounds[QStringLiteral("move")].append(sound);
            ++loaded;
        }
    }
    for (const QString &name : {QStringLiteral("select"), QStringLiteral("cycle"),
                                QStringLiteral("back"), QStringLiteral("switch"),
                                QStringLiteral("menu"), QStringLiteral("pause"), QStringLiteral("toggle-on"),
                                QStringLiteral("toggle-off"), QStringLiteral("launch"),
                                QStringLiteral("error")}) {
        if (const auto *sound = load(name)) {
            m_sounds[name].append(sound);
            ++loaded;
        }
    }
    qCInfo(lcApplication) << "Interface sounds loaded:" << loaded << "of 12.";
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
#ifdef RECOMPSTATION_HAS_SDL2
    if (m_device == 0)
        return;
    const QList<const std::vector<int16_t> *> variants = m_sounds.value(name);
    if (variants.isEmpty())
        return;
    int index = 0;
    if (variants.size() > 1) {
        // A different pitch each time, never the same one twice in a row.
        index = QRandomGenerator::global()->bounded(int(variants.size()) - 1);
        if (m_lastMoveVariant >= 0 && index >= m_lastMoveVariant)
            ++index;
        m_lastMoveVariant = index;
    }

    SDL_LockAudioDevice(m_device);
    // A free voice, or else the one closest to finishing.
    int chosen = 0;
    size_t leastLeft = SIZE_MAX;
    for (int i = 0; i < MaxVoices; ++i) {
        const Voice &voice = m_voices[i];
        if (!voice.samples) {
            chosen = i;
            break;
        }
        const size_t left = voice.samples->size() - voice.position;
        if (left < leastLeft) {
            leastLeft = left;
            chosen = i;
        }
    }
    m_voices[chosen] = Voice{variants.at(index), 0};
    SDL_UnlockAudioDevice(m_device);

    if (!m_loggedFirstPlay) {
        // One line per session, to tell "never played" from "played silently".
        m_loggedFirstPlay = true;
        qCInfo(lcApplication) << "First interface sound:" << name << "gain" << m_gain.load();
    }
#else
    Q_UNUSED(name);
#endif
}

void UiSoundPlayer::mix(int16_t *out, int frames)
{
    const size_t count = size_t(frames) * 2;
    const float gain = m_gain.load();
    std::fill(out, out + count, int16_t(0));
    float accumulator[1024];
    for (size_t offset = 0; offset < count; offset += 1024) {
        const size_t chunk = std::min<size_t>(1024, count - offset);
        std::fill(accumulator, accumulator + chunk, 0.0f);
        bool any = false;
        for (Voice &voice : m_voices) {
            if (!voice.samples)
                continue;
            const size_t left = voice.samples->size() - voice.position;
            const size_t n = std::min(chunk, left);
            const int16_t *source = voice.samples->data() + voice.position;
            for (size_t i = 0; i < n; ++i)
                accumulator[i] += source[i];
            voice.position += n;
            if (voice.position >= voice.samples->size())
                voice.samples = nullptr;
            any = true;
        }
        if (!any)
            continue;
        for (size_t i = 0; i < chunk; ++i)
            out[offset + i] = int16_t(std::clamp(accumulator[i] * gain, -32768.0f, 32767.0f));
    }
}
