#include "TimeStretch.h"
#include <algorithm>
#include <cmath>
#include <limits>

void TimeStretcher::reset() { m_overlap.clear(); m_overlapLen = 0; m_expectedPos = -1; }
double TimeStretcher::process(const float* src, qint64 srcFrames, double position, double rate, float* out, int frames) {
    if (!out || frames <= 0) return position;
    if (!src || srcFrames <= 0 || !std::isfinite(position) || !std::isfinite(rate) || rate <= 0) {
        std::fill_n(out, size_t(frames)*2, 0.0f); reset(); return std::isfinite(position) ? position : 0;
    }
    position = std::clamp(position, 0.0, double(srcFrames));
    if (m_expectedPos < 0 || std::abs(position-m_expectedPos) > 2) reset();
    constexpr int hop = kWin/2, search = kWin/8;
    auto read = [&](qint64 frame, int channel) { return frame >= 0 && frame < srcFrames ? src[2*frame+channel] : 0.0f; };
    int written = 0;
    while (written < frames) {
        if (m_overlapLen == 0) {
            qint64 nominal = qint64(position), chosen = nominal;
            bool previous = !m_overlap.isEmpty();
            if (previous && nominal < srcFrames) {
                double best = -std::numeric_limits<double>::infinity();
                qint64 begin = std::max<qint64>(0, nominal-search), end = std::min(srcFrames-1, nominal+search);
                for (qint64 candidate = begin; candidate <= end; candidate += 2) {
                    double dot = 0, a = 0, b = 0;
                    for (int i = 0; i < hop; i += 4) for (int c = 0; c < 2; ++c) {
                        double x = m_overlap[2*(hop+i)+c], y = read(candidate+i, c);
                        dot += x*y; a += x*x; b += y*y;
                    }
                    double score = a*b > 1e-16 ? dot/std::sqrt(a*b) : 0;
                    score -= 0.001*std::abs(double(candidate-nominal))/search;
                    if (score > best) { best = score; chosen = candidate; }
                }
            }
            if (!previous) m_overlap.resize(2*kWin);
            for (int i = 0; i < hop; ++i) {
                float fade = float(i)/float(hop-1);
                for (int c = 0; c < 2; ++c) {
                    float x = read(chosen+i, c);
                    m_overlap[2*i+c] = previous ? (1-fade)*m_overlap[2*(hop+i)+c]+fade*x : x;
                    m_overlap[2*(hop+i)+c] = read(chosen+hop+i, c);
                }
            }
            m_overlapLen = hop;
        }
        int count = std::min(frames-written, m_overlapLen), offset = hop-m_overlapLen;
        std::copy_n(m_overlap.constData()+2*offset, 2*count, out+2*written);
        // The grid follows the requested rate, independent of correlation adjustments.
        position = std::min(double(srcFrames), position+rate*count);
        m_overlapLen -= count; written += count;
        if (position >= srcFrames) {
            std::fill_n(out+2*written, size_t(frames-written)*2, 0.0f); reset(); break;
        }
    }
    m_expectedPos = position; return position;
}
