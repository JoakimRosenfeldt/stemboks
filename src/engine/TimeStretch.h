#pragma once
#include <QVector>

// WSOLA time-stretcher for keylock (master tempo) and key shift.
// Feed playback position sequentially; call reset() on seek/rate discontinuity.
class TimeStretcher {
public:
    void reset();
    // Render `outFrames` stereo frames starting near source position `posFrames`
    // advancing at `rate` source-frames per output-frame, time-stretched so pitch
    // stays constant. Returns new source position. src is interleaved stereo.
    double process(const float* src, qint64 srcFrames, double posFrames, double rate,
                   float* out, int outFrames);
private:
    static constexpr int kWin = 1024;
    QVector<float> m_overlap; int m_overlapLen = 0;
    double m_expectedPos = -1;
};
