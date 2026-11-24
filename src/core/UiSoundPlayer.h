#pragma once

#include "UiSoundGate.h"

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

class QSoundEffect;
class SettingsManager;

// Plays the interface sounds in assets/sounds (qrc:/sounds/<name>.wav).
// QML calls uiSounds.play("move"). Playback needs Qt Multimedia; builds
// without it get a player that stays silent (available is false).
// Sounds follow Settings > Appearance > Sounds and stay silent while a game
// is running (suppressed).
class UiSoundPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(bool suppressed READ isSuppressed WRITE setSuppressed NOTIFY suppressedChanged)

public:
    explicit UiSoundPlayer(SettingsManager *settings, QObject *parent = nullptr);

    bool isAvailable() const;
    bool isSuppressed() const;
    void setSuppressed(bool suppressed);

    Q_INVOKABLE void play(const QString &name);
    // Plays a sound even when sounds are off, for the volume preview.
    Q_INVOKABLE void preview(const QString &name);

signals:
    void suppressedChanged();

private:
    void load();
    void applyVolume();
    void start(const QString &name);

    SettingsManager *m_settings = nullptr;
    UiSoundGate m_gate;
    QElapsedTimer m_clock;
    // Move has three slightly different pitches; the others one file each.
    QHash<QString, QList<QSoundEffect *>> m_effects;
    int m_lastMoveVariant = -1;
    bool m_suppressed = false;
};
