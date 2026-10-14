#pragma once
#include "Types.h"
#include "TimeStretch.h"
#include "Effects.h"
#include <QObject>
#include <QMutex>
#include <array>
#include <atomic>

// One DJ deck. Control surface lives in ControlRegistry under deck.{n}.*:
//  transport.playing, transport.cue, transport.reverseSlip, position (0..1, out),
//  tempo.fader (-1..1), tempo.range (6/10/16/100), tempo.ratio (out), key.lock (MT),
//  key.shift (semitones), sync.enabled, grid.quantize, slip.enabled,
//  loop.enabled, loop.setIn/setOut/double/halve (triggers), loop.size (beats),
//  jump.forward/backward, jump.size, hotcue.{1..8}.activate/.set (triggers),
//  stems.{drums,bass,other,vocals}.gain/.mute, level.gain, level.volume, level.pfl,
//  eq.low/mid/high (-1..1), colorfx.amount, colorfx.select, colorfx.enabled,
//  fx.enabled (beat fx send), jog.touch, jog.move (delta), pitchbend (+/-)
// Strings: deck.{n}.track.title/.artist/.path, deck.{n}.colorfx.name, deck.{n}.fx.name
class Deck : public QObject {
    Q_OBJECT
public:
    explicit Deck(int index, QObject* parent = nullptr);
    int index() const { return m_index; }

    void loadTrack(LoadedTrackPtr track);   // thread-safe swap
    void unload();
    LoadedTrackPtr track() const;
    bool hasTrack() const { return track() != nullptr; }

    // Called from audio thread. Writes stereo frames into out (added by caller mixer? no: overwrite).
    void render(float* out, int frames, int sampleRate);

    double positionSec() const { return m_positionFrames.load() / double(m_sampleRate); }
    double durationSec() const;
    double effectiveBpm() const;            // bpm * tempo ratio (0 if unknown)
    bool playing() const { return m_playing.load(); }

    void play(); void pause(); void cuePress(); void cueRelease();
    void seekSec(double sec);
    void jogBy(double deltaBeats);
    void scratchTo(double posSec);

signals:
    void trackLoaded(int deck);
    void trackEnded(int deck);

private:
    void onControlChanged(const QString& name, double value);
    QString key(const char* k) const;
    double ctrl(const char* k, double def = 0.0) const;

    int m_index;
    int m_sampleRate = 44100;
    mutable QMutex m_trackMutex;
    LoadedTrackPtr m_track;

    std::atomic<double> m_positionFrames {0.0};
    std::atomic<bool> m_playing {false};
    bool m_cueHeld = false;
    double m_slipPositionFrames = 0.0;
    bool m_slipping = false;
    double m_brakeFactor = 1.0;   // vinyl brake / backspin ramp
    bool m_braking = false;
    TimeStretcher m_stretcher;
    TimeStretcher m_stemStretcher[kNumStems];
    Biquad m_eqLow[2], m_eqMid[2], m_eqHigh[2];
    Biquad m_colorFilter[2];
    EffectSlot m_colorFx;
    bool m_eqDirty = true;
};
