#include "Analysis.h"
#include "Effects.h"
#include "control/AppSettings.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <algorithm>
#include <climits>
#include <cmath>
#include <complex>
#include <numeric>
#include <vector>

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr quint32 cacheMagic = 0x4f4e4445, cacheVersion = 1;
float sample(float value) { return std::isfinite(value) ? value : 0.0f; }
// A radix-2 DFT supplies the chromagram bins without a DSP dependency.
void dft(std::vector<std::complex<double>>& bins) {
    const int n = int(bins.size());
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit; if (i < j) std::swap(bins[i], bins[j]);
    }
    for (int len = 2; len <= n; len *= 2) {
        auto step = std::polar(1.0, -2*pi/len);
        for (int i = 0; i < n; i += len) {
            std::complex<double> phase = 1;
            for (int j = 0; j < len/2; ++j, phase *= step) {
                auto a = bins[i+j], b = phase * bins[i+j+len/2];
                bins[i+j] = a+b; bins[i+j+len/2] = a-b;
            }
        }
    }
}
double keyCorrelation(const std::array<double, 12>& chroma, const double* profile, int root) {
    double mean = std::accumulate(chroma.begin(), chroma.end(), 0.0) / 12;
    double pmean = std::accumulate(profile, profile+12, 0.0) / 12;
    double dot = 0, a = 0, b = 0;
    for (int i = 0; i < 12; ++i) {
        double x = chroma[(i+root)%12]-mean, y = profile[i]-pmean;
        dot += x*y; a += x*x; b += y*y;
    }
    return a*b > 1e-20 ? dot/std::sqrt(a*b) : -1;
}
}

QString AnalysisResult::camelot() const {
    static const int major[] = {8,3,10,5,12,7,2,9,4,11,6,1};
    static const int minor[] = {5,12,7,2,9,4,11,6,1,8,3,10};
    return keyIndex < 0 || keyIndex >= 12 ? QString() : QString::number((keyMinor ? minor : major)[keyIndex]) + (keyMinor ? "A" : "B");
}
QString AnalysisResult::keyName() const {
    static const char* names[] = {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    return keyIndex < 0 || keyIndex >= 12 ? QString() : QString::fromLatin1(names[keyIndex]) + (keyMinor ? "m" : "");
}
AnalysisResult Analysis::analyze(const AudioBuffer& audio) {
    AnalysisResult result;
    if (audio.sampleRate <= 0 || audio.frames() == 0) return result;
    const qint64 frames = audio.frames(); const int sr = audio.sampleRate;
    result.waveform.durationSec = audio.durationSec();
    const qint64 columns = (frames*100 + sr-1)/sr;
    result.waveform.peaks.resize(columns);
    Biquad low[2], upper[2];
    for (int c = 0; c < 2; ++c) { low[c].setLowpass(sr, 200); upper[c].setLowpass(sr, 4000); }
    const int hop = std::max(1, sr/200);
    std::vector<double> envelope(size_t((frames+hop-1)/hop), 0.0);
    double sumSquares = 0, samplePeak = 0;
    for (qint64 i = 0; i < frames; ++i) {
        auto& peak = result.waveform.peaks[i*100/sr];
        for (int c = 0; c < 2; ++c) {
            float x = sample(audio.interleaved[2*i+c]);
            samplePeak = std::max(samplePeak, double(std::abs(x)));
            float lo = low[c].process(x), hiLow = upper[c].process(x);
            peak.low = std::max(peak.low, std::min(1.0f, std::abs(lo)));
            peak.mid = std::max(peak.mid, std::min(1.0f, std::abs(hiLow-lo)));
            peak.high = std::max(peak.high, std::min(1.0f, std::abs(x-hiLow)));
            sumSquares += double(x)*x; envelope[size_t(i/hop)] += double(x)*x;
        }
        if ((i & 4095) == 0) for (int c = 0; c < 2; ++c) {
            if (std::abs(low[c].y1) < 1e-20) low[c].reset();
            if (std::abs(upper[c].y1) < 1e-20) upper[c].reset();
        }
    }
    double rms = std::sqrt(sumSquares/(2*frames));
    result.loudnessGainDb = rms > 1e-9 ? std::min(
        std::clamp(-14.0 - 20*std::log10(rms), -24.0, 12.0),
        -20*std::log10(samplePeak)-1) : 0;
    double previous = 0;
    for (double& value : envelope) {
        double energy = std::sqrt(value/(2*hop));
        value = std::max(0.0, energy-previous); previous = energy;
    }
    double envRate = double(sr)/hop;
    int minLag = std::max(1, int(std::floor(envRate*60/200))), maxLag = int(std::ceil(envRate));
    std::vector<double> correlation(size_t(maxLag+1), 0);
    if (envelope.size() > size_t(2*maxLag)) {
        for (int lag = minLag; lag <= maxLag; ++lag) {
            double dot = 0, left = 0, right = 0;
            for (size_t i = size_t(lag); i < envelope.size(); ++i) {
                double a = envelope[i], b = envelope[i-lag]; dot += a*b; left += a*a; right += b*b;
            }
            correlation[lag] = left*right > 1e-20 ? dot/std::sqrt(left*right) : 0;
        }
        int best = minLag; double bestScore = 0;
        for (int lag = minLag; lag <= maxLag; ++lag) {
            double bpm = envRate*60/lag;
            if (bpm < 60 || bpm > 200) continue;
            double score = correlation[lag];
            if (2*lag <= maxLag) score += 0.15*correlation[2*lag];
            // Resolve otherwise similar half/double-tempo grids toward dance tempos.
            score *= 0.9 + 0.1*std::exp(-0.5*std::pow(std::log2(bpm/120.0)/0.45, 2));
            if (score > bestScore) { bestScore = score; best = lag; }
        }
        if (bestScore > 0.05) {
            double lag = best;
            if (best > minLag && best < maxLag) {
                double a = correlation[best-1], b = correlation[best], c = correlation[best+1];
                if (std::abs(a-2*b+c) > 1e-12) lag += std::clamp(0.5*(a-c)/(a-2*b+c), -0.5, 0.5);
            }
            result.bpm = std::clamp(envRate*60/lag, 60.0, 200.0);
            double period = envRate*60/result.bpm;
            int phases = std::max(1, int(std::ceil(period)));
            std::vector<double> strength(size_t(phases), 0);
            for (size_t i = 1; i+1 < envelope.size(); ++i)
                if (envelope[i] > envelope[i-1] && envelope[i] >= envelope[i+1]) {
                    double phase = std::fmod(double(i), period);
                    for (int shift = -1; shift <= 1; ++shift) {
                        int bin = (int(std::lround(phase))+shift+phases)%phases;
                        strength[bin] += envelope[i]*(shift == 0 ? 1.0 : 0.5);
                    }
                }
            int phase = int(std::max_element(strength.begin(), strength.end())-strength.begin());
            result.firstBeatSec = phase/envRate;
        }
    }
    constexpr int window = 2048;
    std::vector<std::complex<double>> bins(window);
    std::array<double,12> chroma{};
    std::array<double,window> hann{};
    std::array<int,window/2> pitch{}; pitch.fill(-1);
    for (int i = 0; i < window; ++i) hann[i] = 0.5-0.5*std::cos(2*pi*i/(window-1));
    for (int bin = 1; bin < window/2; ++bin) {
        double frequency = double(bin)*sr/window;
        if (frequency >= 80 && frequency <= 4000) pitch[bin] = (int(std::lround(69+12*std::log2(frequency/440)))%12+12)%12;
    }
    for (qint64 start = 0; start+window <= frames; start += window) {
        for (int i = 0; i < window; ++i)
            bins[i] = 0.5*(sample(audio.interleaved[2*(start+i)])+sample(audio.interleaved[2*(start+i)+1]))*hann[i];
        dft(bins);
        double frameMax = 0;
        for (int bin = 1; bin < window/2; ++bin)
            if (pitch[bin] >= 0) frameMax = std::max(frameMax, std::abs(bins[bin]));
        for (int bin = 1; bin < window/2; ++bin)
            if (pitch[bin] >= 0 && std::abs(bins[bin]) >= frameMax*0.1)
                chroma[pitch[bin]] += std::norm(bins[bin]);
    }
    static const double major[] = {6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88};
    static const double minor[] = {6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17};
    double best = -1;
    if (std::accumulate(chroma.begin(), chroma.end(), 0.0) > 1e-8)
        for (int root = 0; root < 12; ++root) for (int mode = 0; mode < 2; ++mode) {
            double score = keyCorrelation(chroma, mode ? minor : major, root);
            if (score > best) { best = score; result.keyIndex = root; result.keyMinor = mode != 0; }
        }
    result.ok = true; return result;
}
QString Analysis::waveformCachePath(const QString& path) {
    QFileInfo file(path);
    QByteArray identity = file.absoluteFilePath().toUtf8() + '\0' + QByteArray::number(file.size()) + '\0' + QByteArray::number(file.lastModified().toMSecsSinceEpoch());
    QString directory = AppSettings::cacheDir()+"/ondes";
    QDir().mkpath(directory);
    return directory + '/' + QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha1).toHex()) + ".onde";
}
bool Analysis::loadWaveformCache(const QString& path, WaveformData* out) {
    if (!out) return false;
    QFile file(waveformCachePath(path)); if (!file.open(QIODevice::ReadOnly)) return false;
    QDataStream stream(&file); stream.setByteOrder(QDataStream::LittleEndian); stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    quint32 magic, version, count; double duration;
    stream >> magic >> version;
    stream.setFloatingPointPrecision(QDataStream::DoublePrecision); stream >> duration >> count;
    if (stream.status() != QDataStream::Ok || magic != cacheMagic || version != cacheVersion ||
        !std::isfinite(duration) || duration <= 0 || count == 0 || count > INT_MAX || file.size() != 20+qint64(count)*12 ||
        std::abs(double(count)-duration*100) > 1.01) return false;
    WaveformData data; data.durationSec = duration; data.peaks.resize(count);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    for (auto& peak : data.peaks) {
        stream >> peak.low >> peak.mid >> peak.high;
        for (float v : {peak.low, peak.mid, peak.high}) if (!std::isfinite(v) || v < 0 || v > 1) return false;
    }
    if (stream.status() != QDataStream::Ok) return false;
    *out = std::move(data); return true;
}
void Analysis::saveWaveformCache(const QString& path, const WaveformData& data) {
    if (data.isEmpty() || !std::isfinite(data.durationSec) || data.durationSec <= 0 || data.peaks.size() > INT_MAX ||
        std::abs(double(data.peaks.size())-data.durationSec*100) > 1.01) return;
    QSaveFile file(waveformCachePath(path)); if (!file.open(QIODevice::WriteOnly)) return;
    QDataStream stream(&file); stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
    stream << cacheMagic << cacheVersion << data.durationSec << quint32(data.peaks.size());
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    for (const auto& peak : data.peaks) {
        for (float v : {peak.low, peak.mid, peak.high}) if (!std::isfinite(v) || v < 0 || v > 1) { file.cancelWriting(); return; }
        stream << peak.low << peak.mid << peak.high;
    }
    if (stream.status() == QDataStream::Ok) file.commit(); else file.cancelWriting();
}
double Analysis::beatLengthSec(double bpm) { return std::isfinite(bpm) && bpm > 0 ? 60/bpm : 0; }
double Analysis::beatAt(double position, double first, double bpm) {
    double length = beatLengthSec(bpm); return length > 0 && std::isfinite(position) && std::isfinite(first) ? (position-first)/length : 0;
}
double Analysis::quantizeToBeat(double position, double first, double bpm) {
    double length = beatLengthSec(bpm);
    return length > 0 && std::isfinite(position) && std::isfinite(first) ? first+std::round(beatAt(position, first, bpm))*length : position;
}
