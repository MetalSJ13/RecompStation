#pragma once

#include "UiSoundGate.h"

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>
#include <deque>
#include <vector>

class SettingsManager;

// Plays the interface sounds in assets/sounds (qrc:/sounds/<name>.wav)
// through SDL2 audio, the library already used for controllers. QML calls
// uiSounds.play("move"). Builds without SDL2 stay silent (available is false).
//
// A small mixer lets sounds overlap (a Move still ringing when the next one
// starts). The device is SDL's default output, which follows the desktop's
// default output device. Sounds follow Settings > Appearance > Sounds and
// stay silent while a game is running (suppressed).
class UiSoundPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(bool suppressed READ isSuppressed WRITE setSuppressed NOTIFY suppressedChanged)

public:
    explicit UiSoundPlayer(SettingsManager *settings, QObject *parent = nullptr);
    ~UiSoundPlayer() override;

    bool isAvailable() const;
    bool isSuppressed() const;
    void setSuppressed(bool suppressed);

    Q_INVOKABLE void play(const QString &name);
    // Plays a sound even when sounds are off, for the volume preview.
    Q_INVOKABLE void preview(const QString &name);
    // Playback gain for a Settings volume (0-100).
    static float gainForVolume(int percent);

    // Mixes the playing sounds into interleaved stereo 16-bit samples.
    // Called on SDL's audio thread.
    void mix(int16_t *out, int frames);

signals:
    void suppressedChanged();

private:
    struct Voice {
        const std::vector<int16_t> *samples = nullptr;  // interleaved stereo
        size_t position = 0;                             // in samples
    };
    static constexpr int MaxVoices = 8;

    bool openDevice();
    void loadSounds();
    void start(const QString &name);

    SettingsManager *m_settings = nullptr;
    UiSoundGate m_gate;
    QElapsedTimer m_clock;
    // Move has three slightly different pitches; the others one file each.
    QHash<QString, QList<const std::vector<int16_t> *>> m_sounds;
    std::deque<std::vector<int16_t>> m_storage;  // stable addresses
    Voice m_voices[MaxVoices];
    std::atomic<float> m_gain{0.46f};
    uint32_t m_device = 0;
    int m_lastMoveVariant = -1;
    bool m_suppressed = false;
    bool m_loggedFirstPlay = false;
};
