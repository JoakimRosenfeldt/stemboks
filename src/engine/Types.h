#pragma once
#include <QString>
#include <QVector>
#include <QImage>
#include <array>
#include <memory>

namespace stemboks {

constexpr int kNumDecks = 4;
constexpr int kNumStems = 4;          // vocals, drums, bass, other (HTDemucs order below)
constexpr int kNumHotcues = 8;        // A-H
constexpr int kNumSamplers = 8;

enum StemIndex { StemDrums = 0, StemBass = 1, StemOther = 2, StemVocals = 3 }; // HTDemucs output order

struct AudioBuffer {
    QVector<float> interleaved;       // stereo float32
    int sampleRate = 44100;
    qint64 frames() const { return interleaved.size() / 2; }
    double durationSec() const { return sampleRate ? double(frames()) / sampleRate : 0.0; }
    bool isEmpty() const { return interleaved.isEmpty(); }
};
using AudioBufferPtr = std::shared_ptr<const AudioBuffer>;

struct TrackInfo {
    QString path;
    QString title, artist, album, genre, comment;
    double durationSec = 0.0;
    int sampleRate = 0, bitrate = 0;
    double bpm = 0.0;                 // from tags, 0 if absent
    QString keyText;                  // from tags
    QImage artwork;
};

// Peak per pixel column at a fixed resolution (100 px per second in cache).
struct WaveformData {
    struct Peak { float low = 0, mid = 0, high = 0; }; // 3-band amplitudes 0..1
    QVector<Peak> peaks;
    double durationSec = 0.0;
    bool isEmpty() const { return peaks.isEmpty(); }
};

struct AnalysisResult {
    double bpm = 0.0;
    double firstBeatSec = 0.0;        // beatgrid offset
    int keyIndex = -1;                // 0..11 pitch class (C=0), -1 unknown
    bool keyMinor = false;
    double loudnessGainDb = 0.0;      // suggested autogain (ReplayGain-style)
    WaveformData waveform;
    bool ok = false;
    QString camelot() const;          // e.g. "8A"
    QString keyName() const;          // e.g. "Am"
};

struct Hotcue {
    double positionSec = -1.0;        // -1 = unset
    QString label;
};

struct LoadedTrack {
    TrackInfo info;
    AudioBufferPtr audio;                          // full mix
    std::array<AudioBufferPtr, kNumStems> stems;   // null if no stems
    bool hasStems = false;
    AnalysisResult analysis;
    std::array<Hotcue, kNumHotcues> hotcues;
    double cueSec = 0.0;
    double loopInSec = -1.0, loopOutSec = -1.0;
};
using LoadedTrackPtr = std::shared_ptr<LoadedTrack>;

} // namespace stemboks
