#pragma once
#include "Types.h"

// Offline track analysis: BPM + beatgrid offset, musical key, 3-band waveform, loudness.
namespace Analysis {
    AnalysisResult analyze(const AudioBuffer& audio);

    // Waveform cache in the style of Stembox's cache/ondes/<sha1>.onde
    QString waveformCachePath(const QString& trackPath);
    bool loadWaveformCache(const QString& trackPath, WaveformData* out);
    void saveWaveformCache(const QString& trackPath, const WaveformData& data);

    // Beat position helpers (constant-tempo grid, like the original's stored grids)
    double beatLengthSec(double bpm);
    double quantizeToBeat(double posSec, double firstBeatSec, double bpm);
    double beatAt(double posSec, double firstBeatSec, double bpm); // fractional beat number
}
