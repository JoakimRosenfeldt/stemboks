#pragma once
#include "Deck.h"
#include "Effects.h"
#include <QObject>
#include <array>
#include <atomic>

class Recorder;

// Owns 4 decks, 8 sampler slots, 2 beat-FX units, mixer, devices (miniaudio).
// Mixer controls in registry: mixer.crossfader.position, mixer.master.gain,
// mixer.head.mix, mixer.head.gain, mixer.eq.{1..8}, sampler.{n}.play/.gain,
// fx.unit.{1,2}.enabled/.mix/.parameter, deck sends: deck.{n}.fx.unit1/unit2.
class AudioEngine : public QObject {
    Q_OBJECT
public:
    static AudioEngine& instance();
    Deck* deck(int i) { return m_decks.at(i).get(); }

    bool start();
    void stop();
    int sampleRate() const { return m_sampleRate; }

    struct DeviceInfo { QString name; unsigned int id; };
    QVector<DeviceInfo> outputDevices() const;
    // API names mirror the original per-platform choices.
    QStringList apiNames() const; // CoreAudio / WASAPI+ASIO / ALSA+PulseAudio+JACK

    void loadSampler(int slot, AudioBufferPtr buf, const QString& label);
    void triggerSampler(int slot, bool on);
    bool samplerPlaying(int slot) const;

    void setRecording(bool on, const QString& path, bool mp3, int bitDepth);
    bool recording() const { return m_recording.load(); }
    double recordedSeconds() const;

    float masterLevelL() const { return m_levelL.load(); }
    float masterLevelR() const { return m_levelR.load(); }

    // miniaudio callbacks (public for C callback trampolines)
    void processMain(float* out, int frames);
    void processHead(float* out, int frames);

signals:
    void started();

private:
    AudioEngine();
    void renderMix(float* mainOut, float* headOut, int frames);
    std::array<std::unique_ptr<Deck>, kNumDecks> m_decks;
    struct SamplerSlot { AudioBufferPtr buf; std::atomic<qint64> pos{-1}; QString label; };
    std::array<SamplerSlot, kNumSamplers> m_samplers;
    EffectSlot m_fxUnit[2];
    Biquad m_masterEq[8][2];
    int m_sampleRate = 44100;
    void* m_mainDevice = nullptr;   // ma_device*
    void* m_headDevice = nullptr;
    QVector<float> m_mixBuf, m_headBuf, m_deckBuf;
    std::unique_ptr<Recorder> m_recorder;
    std::atomic<bool> m_recording{false};
    std::atomic<float> m_levelL{0}, m_levelR{0};
    float m_limiterGain = 1.0f;
};
