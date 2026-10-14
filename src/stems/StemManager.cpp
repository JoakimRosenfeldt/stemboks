#include "StemManager.h"
#include "../control/AppSettings.h"
#include "../engine/Decoder.h"
#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMetaObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>
#ifdef STEMBOKS_HAVE_ONNX
#include <onnxruntime_cxx_api.h>
#endif

namespace {
const std::array<QString, kNumStems> names = {"drums", "bass", "other", "vocals"};
const QString unavailable = QStringLiteral("ONNX Runtime/model not available — place stems-hdemucs-mmi-v1.onnx in models folder");
QString baseName(const QString& path) {
    QString name = QFileInfo(path).completeBaseName();
    if (name.endsWith(".stem", Qt::CaseInsensitive)) name.chop(5);
    return name;
}
QStringList folders(const QString& path) {
    const QFileInfo info(path);
    QStringList result{info.dir().filePath(baseName(path) + ".stems")};
    const QString configured = AppSettings::instance().get("[Stembox]", "stems_folder");
    if (!configured.isEmpty()) {
        result.append(QDir(configured).filePath(baseName(path) + ".stems"));
        result.append(QDir(configured).filePath(baseName(path)));
    }
    result.removeDuplicates(); return result;
}
QStringList containers(const QString& path) {
    const QFileInfo info(path);
    QStringList result;
    if (path.endsWith(".stem.mp4", Qt::CaseInsensitive) || path.endsWith(".stem.m4a", Qt::CaseInsensitive)) result.append(path);
    for (const auto& ext : {QStringLiteral(".stem.mp4"), QStringLiteral(".stem.m4a")}) {
        result.append(info.dir().filePath(baseName(path) + ext));
        result.append(path + ext);
    }
    result.removeDuplicates(); return result;
}
QStringList stemFiles(const QString& folder) {
    QStringList files;
    const QDir directory(folder);
    for (const auto& name : names) {
        QString found;
        for (const auto& ext : {QStringLiteral("wav"), QStringLiteral("flac"), QStringLiteral("mp3")}) {
            const QString path = directory.filePath(name + '.' + ext);
            if (QFileInfo(path).isFile()) { found = path; break; }
        }
        if (found.isEmpty()) return {};
        files.append(found);
    }
    return files;
}
using Stems = std::array<std::vector<float>, kNumStems>;
using Progress = std::function<void(int, const QString&)>;
std::atomic<bool> cancelled{false};
void checkCancelled() {
    if (cancelled.load(std::memory_order_relaxed)) throw std::runtime_error("Cancelled during shutdown");
}
void writeWav(const QString& path, const std::vector<float>& samples, int rate) {
    if (samples.size() > (std::numeric_limits<quint32>::max() - 36) / sizeof(float)) throw std::runtime_error("Stem exceeds WAV 4 GB limit");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) throw std::runtime_error(file.errorString().toStdString());
    QDataStream out(&file); out.setByteOrder(QDataStream::LittleEndian); out.setFloatingPointPrecision(QDataStream::SinglePrecision);
    const quint32 bytes = static_cast<quint32>(samples.size() * sizeof(float));
    out.writeRawData("RIFF", 4); out << quint32(36 + bytes); out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(3) << quint16(2) << quint32(rate) << quint32(rate * 8) << quint16(8) << quint16(32);
    out.writeRawData("data", 4); out << bytes;
    size_t written = 0;
    for (float sample : samples) {
        if ((written++ & 4095) == 0) checkCancelled();
        if (!std::isfinite(sample)) throw std::runtime_error("Separation returned non-finite audio");
        out << sample;
    }
    if (out.status() != QDataStream::Ok || !file.commit()) throw std::runtime_error("Failed to write stem WAV");
}
Stems heuristic(const AudioBuffer& audio, const Progress& progress) {
    Stems stems;
    for (auto& stem : stems) stem.resize(audio.interleaved.size());
    float low = 0, smooth = 0;
    const float lowAlpha = 1 - std::exp(-2 * 3.14159265358979323846 * 180 / audio.sampleRate);
    const float smoothAlpha = 1 - std::exp(-2 * 3.14159265358979323846 * 2500 / audio.sampleRate);
    for (qint64 i = 0; i < audio.frames(); ++i) {
        if ((i & 4095) == 0) checkCancelled();
        const float left = audio.interleaved[2 * i], right = audio.interleaved[2 * i + 1];
        const float mid = (left + right) * 0.5f, side = (left - right) * 0.5f;
        low += lowAlpha * (mid - low); smooth += smoothAlpha * (mid - smooth);
        const float vocals = smooth - low, drums = mid - smooth;
        stems[StemBass][2 * i] = stems[StemBass][2 * i + 1] = low;
        stems[StemVocals][2 * i] = stems[StemVocals][2 * i + 1] = vocals;
        stems[StemDrums][2 * i] = stems[StemDrums][2 * i + 1] = drums;
        stems[StemOther][2 * i] = side; stems[StemOther][2 * i + 1] = -side;
        if (i % audio.sampleRate == 0) progress(5 + int(85 * i / audio.frames()), "Mid/side heuristic approximation (not model separation)");
    }
    return stems;
}

#ifdef STEMBOKS_HAVE_ONNX
constexpr int fftSize = 4096, hop = 1024, bins = 2048, chunkFrames = 343980;
constexpr double pi = 3.14159265358979323846;
using Complex = std::complex<float>;
void fft(std::vector<Complex>& data, bool inverse) {
    for (int i = 1, j = 0; i < fftSize; ++i) {
        int bit = fftSize >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit; if (i < j) std::swap(data[i], data[j]);
    }
    for (int length = 2; length <= fftSize; length <<= 1) {
        const Complex rotation = std::polar(1.0f, float((inverse ? 2 : -2) * pi / length));
        for (int start = 0; start < fftSize; start += length) {
            Complex factor(1, 0);
            for (int j = 0; j < length / 2; ++j) {
                const Complex even = data[start + j], odd = data[start + j + length / 2] * factor;
                data[start + j] = even + odd; data[start + j + length / 2] = even - odd; factor *= rotation;
            }
        }
    }
    if (inverse) for (auto& value : data) value /= float(fftSize);
}
std::vector<float> window() {
    std::vector<float> result(fftSize);
    for (int i = 0; i < fftSize; ++i) result[i] = float(0.5 - 0.5 * std::cos(2 * pi * i / fftSize));
    return result;
}
int reflected(int frame, int length) {
    if (length <= 1) return 0;
    while (frame < 0 || frame >= length) frame = frame < 0 ? -frame : 2 * length - 2 - frame;
    return frame;
}
// Demucs _spec: reflect-pad 3*hop/2 on the left and round up to a hop
// on the right, center the STFT, then keep frames [2, 2+ceil(T/hop)).
// _ispec restores two zero frames on each side and crops the same padding.
constexpr int specPad = 3 * hop / 2;
std::vector<float> stft(const std::vector<float>& audio, int times, const std::vector<float>& hann) {
    std::vector<float> result(size_t(4) * bins * times);
    std::vector<Complex> data(fftSize);
    const int paddedLength = times * hop + 2 * specPad;
    const float scale = 1 / std::sqrt(float(fftSize));
    for (int channel = 0; channel < 2; ++channel) for (int t = 0; t < times; ++t) {
        checkCancelled();
        for (int i = 0; i < fftSize; ++i) {
            const int padded = reflected((t + 2) * hop + i - fftSize / 2, paddedLength);
            const int frame = reflected(padded - specPad, chunkFrames);
            data[i] = audio[size_t(frame) * 2 + channel] * hann[i];
        }
        fft(data, false);
        // CaC order: left-real, left-imaginary, right-real, right-imaginary.
        // normalized=True divides by sqrt(nfft); Demucs drops Nyquist bin 2048.
        for (int f = 0; f < bins; ++f) {
            result[(size_t(channel * 2) * bins + f) * times + t] = data[f].real() * scale;
            result[(size_t(channel * 2 + 1) * bins + f) * times + t] = data[f].imag() * scale;
        }
    }
    return result;
}
std::vector<float> istft(const float* spec, int times, const std::vector<float>& hann) {
    std::vector<float> result(size_t(chunkFrames) * 2, 0), weights(chunkFrames, 0);
    std::vector<Complex> data(fftSize);
    const float scale = std::sqrt(float(fftSize));
    for (int channel = 0; channel < 2; ++channel) for (int t = 0; t < times + 4; ++t) {
        checkCancelled();
        std::fill(data.begin(), data.end(), Complex{});
        if (t >= 2 && t < times + 2) for (int f = 0; f < bins; ++f) {
            data[f] = Complex(spec[(size_t(channel * 2) * bins + f) * times + t - 2],
                              spec[(size_t(channel * 2 + 1) * bins + f) * times + t - 2]) * scale;
            if (f > 0) data[fftSize - f] = std::conj(data[f]);
        }
        fft(data, true);
        for (int i = 0; i < fftSize; ++i) {
            const int frame = t * hop + i - fftSize / 2 - specPad;
            if (frame < 0 || frame >= chunkFrames) continue;
            result[size_t(frame) * 2 + channel] += data[i].real() * hann[i];
            // Zero-padded edge frames still contribute to torch.istft's envelope.
            if (channel == 0) weights[frame] += hann[i] * hann[i];
        }
    }
    for (int frame = 0; frame < chunkFrames; ++frame) {
        if (weights[frame] <= 1e-8f) throw std::runtime_error("Model spectrogram does not cover the complete chunk");
        for (int ch = 0; ch < 2; ++ch) result[size_t(frame) * 2 + ch] /= weights[frame];
    }
    return result;
}
bool matches(const std::vector<int64_t>& actual, const std::vector<int64_t>& wanted) {
    if (actual.size() != wanted.size()) return false;
    for (size_t i = 0; i < actual.size(); ++i) if (actual[i] > 0 && actual[i] != wanted[i]) return false;
    return true;
}
Stems separate(const AudioBuffer& audio, const QString& model, const Progress& progress) {
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "stemboks-stems");
    Ort::SessionOptions options; options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
#ifdef Q_OS_WIN
    const std::wstring modelName = model.toStdWString();
#else
    const std::string modelName = QFile::encodeName(model).toStdString();
#endif
    Ort::Session session(env, modelName.c_str(), options);
    const size_t inputCount = session.GetInputCount(), outputCount = session.GetOutputCount();
    if (inputCount < 1 || inputCount > 2) throw std::runtime_error("Model mismatch: expected one spectrogram input and optional waveform input");
    if (outputCount < 1 || outputCount > 2) throw std::runtime_error("Model mismatch: expected one stem output or spectral and temporal outputs");
    const int times = (chunkFrames + hop - 1) / hop;
    const std::vector<int64_t> inputShape{1, 4, bins, times}, waveShape{1, 2, chunkFrames};
    // Waveform exports (e.g. stems-hdemucs-mmi-v1.onnx: input "mix" [1,2,343980],
    // STFT inside the graph) take raw audio and return temporal stems directly.
    bool waveformMode = false;
    if (inputCount >= 1) {
        const auto first = session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        waveformMode = first.size() == 3 && matches(first, waveShape);
    }
    Ort::AllocatorWithDefaultOptions allocator;
    std::vector<Ort::AllocatedStringPtr> inputNames, outputNames;
    std::vector<const char*> inputs, outputs;
    for (size_t i = 0; i < inputCount; ++i) {
        const auto type = session.GetInputTypeInfo(i);
        if (type.GetONNXType() != ONNX_TYPE_TENSOR) throw std::runtime_error("Model input mismatch: expected float32 tensors");
        const auto info = type.GetTensorTypeAndShapeInfo();
        const auto& wanted = waveformMode ? waveShape : (i == 0 ? inputShape : waveShape);
        if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || !matches(info.GetShape(), wanted))
            throw std::runtime_error(i == 0 ? "Model input 0 mismatch: expected float32 [1,4,2048,336] spectrogram or [1,2,343980] waveform"
                                          : "Model input 1 mismatch: expected float32 [1,2,343980] waveform");
        inputNames.push_back(session.GetInputNameAllocated(i, allocator)); inputs.push_back(inputNames.back().get());
    }
    for (size_t i = 0; i < outputCount; ++i) {
        outputNames.push_back(session.GetOutputNameAllocated(i, allocator)); outputs.push_back(outputNames.back().get());
    }
    auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    const auto hann = window();
    Stems result; for (auto& stem : result) stem.resize(audio.interleaved.size(), 0);
    std::vector<float> weights(size_t(audio.frames()), 0);
    constexpr int stride = chunkFrames * 3 / 4;
    for (qint64 start = 0; start < audio.frames(); start += stride) {
        checkCancelled();
        progress(5 + int(80 * start / audio.frames()), "Separating with HTDemucs ONNX");
        std::vector<float> chunk(size_t(chunkFrames) * 2, 0);
        const int count = int(std::min<qint64>(chunkFrames, audio.frames() - start));
        std::copy_n(audio.interleaved.constData() + start * 2, size_t(count) * 2, chunk.begin());
        std::vector<Ort::Value> tensors;
        std::vector<float> spec, wave;
        if (waveformMode) {
            wave.resize(size_t(chunkFrames) * 2);
            for (int channel = 0; channel < 2; ++channel) for (int frame = 0; frame < chunkFrames; ++frame)
                wave[size_t(channel) * chunkFrames + frame] = chunk[size_t(frame) * 2 + channel];
            tensors.push_back(Ort::Value::CreateTensor<float>(memory, wave.data(), wave.size(), waveShape.data(), waveShape.size()));
        } else {
            spec = stft(chunk, times, hann);
            tensors.push_back(Ort::Value::CreateTensor<float>(memory, spec.data(), spec.size(), inputShape.data(), inputShape.size()));
        }
        if (!waveformMode && inputCount == 2) {
            wave.resize(size_t(chunkFrames) * 2);
            for (int channel = 0; channel < 2; ++channel) for (int frame = 0; frame < chunkFrames; ++frame)
                wave[size_t(channel) * chunkFrames + frame] = chunk[size_t(frame) * 2 + channel];
            tensors.push_back(Ort::Value::CreateTensor<float>(memory, wave.data(), wave.size(), waveShape.data(), waveShape.size()));
        }
        auto output = session.Run(Ort::RunOptions{nullptr}, inputs.data(), tensors.data(), tensors.size(), outputs.data(), outputs.size());
        checkCancelled();
        int stemCount = 0;
        Stems decoded;
        for (auto& stem : decoded) stem.resize(size_t(chunkFrames) * 2, 0);
        if (output.size() != outputCount) throw std::runtime_error("Model returned an unexpected number of outputs");
        for (size_t o = 0; o < output.size(); ++o) {
            if (!output[o].IsTensor()) throw std::runtime_error("Model output mismatch: expected stem tensor");
            const auto info = output[o].GetTensorTypeAndShapeInfo(); const auto dims = info.GetShape();
            if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) throw std::runtime_error("Model output mismatch: expected float32 stems");
            int countStems = 0; bool spectral = false, temporal = false;
            if (dims.size() == 5 && dims[0] == 1 && (dims[1] == 3 || dims[1] == 4) && dims[2] == 4 && dims[3] == bins && dims[4] == times) {
                spectral = true; countStems = int(dims[1]);
            } else if (dims.size() == 4 && dims[0] == 1 && dims[2] == bins && dims[3] == times && (dims[1] == 12 || dims[1] == 16)) {
                spectral = true; countStems = int(dims[1] / 4);
            } else if (dims.size() == 4 && dims[0] == 1 && (dims[1] == 3 || dims[1] == 4) && dims[2] == 2 && dims[3] == chunkFrames) {
                temporal = true; countStems = int(dims[1]);
            }
            if ((!spectral && !temporal) || (countStems != 3 && countStems != 4))
                throw std::runtime_error("Model output mismatch: expected [1,S,4,2048,336], [1,S*4,2048,336] or [1,S,2,343980], S=3 or 4");
            if (outputCount == 2 && ((o == 0 && !spectral) || (o == 1 && !temporal)))
                throw std::runtime_error("Hybrid model output mismatch: output 0 must be spectral and output 1 temporal");
            if (stemCount && stemCount != countStems) throw std::runtime_error("Model output mismatch: spectral and temporal stem counts differ");
            stemCount = countStems;
            const float* data = output[o].GetTensorData<float>();
            for (int stem = 0; stem < stemCount; ++stem) {
                checkCancelled();
                const int target = stemCount == 3 ? (stem == 0 ? StemDrums : stem == 1 ? StemOther : StemVocals) : stem;
                std::vector<float> spectralAudio;
                if (spectral) spectralAudio = istft(data + size_t(stem) * 4 * bins * times, times, hann);
                for (int frame = 0; frame < chunkFrames; ++frame) for (int channel = 0; channel < 2; ++channel) {
                    const float value = spectral ? spectralAudio[size_t(frame) * 2 + channel]
                        : data[(size_t(stem) * 2 + channel) * chunkFrames + frame];
                    if (!std::isfinite(value)) throw std::runtime_error("Model returned non-finite samples");
                    decoded[target][size_t(frame) * 2 + channel] += value;
                }
            }
        }
        for (int frame = 0; frame < count; ++frame) {
            // Positive triangular weights avoid zero gain at either file boundary.
            const float weight = float(std::min(frame + 1, chunkFrames - frame)) / (chunkFrames / 2);
            weights[size_t(start + frame)] += weight;
            for (int stem = 0; stem < kNumStems; ++stem) for (int channel = 0; channel < 2; ++channel)
                result[stem][size_t(start + frame) * 2 + channel] += decoded[stem][size_t(frame) * 2 + channel] * weight;
        }
        if (start + count >= audio.frames()) break;
    }
    for (qint64 frame = 0; frame < audio.frames(); ++frame) {
        if ((frame & 4095) == 0) checkCancelled();
        if (weights[size_t(frame)] <= 0) throw std::runtime_error("Model chunks did not cover the complete track");
        for (auto& stem : result) for (int channel = 0; channel < 2; ++channel)
            stem[size_t(frame) * 2 + channel] /= weights[size_t(frame)];
    }
    return result;
}
#endif
struct Result { bool ok; QString status; };
Result runJob(const QString& path, const QString& folder, const QString& model, bool fallback, const Progress& progress) {
    try {
        checkCancelled();
        QString error; progress(1, fallback ? "Decoding for mid/side heuristic approximation" : "Decoding for HTDemucs");
        const auto audio = Decoder::decodeFile(path, 44100, &error);
        if (!audio || audio->isEmpty() || audio->sampleRate != 44100) return {false, "Decode failed: " + error};
        Stems stems;
        if (fallback) stems = heuristic(*audio, progress);
        else {
#ifdef STEMBOKS_HAVE_ONNX
            stems = separate(*audio, model, progress);
#else
            Q_UNUSED(model);
            return {false, unavailable};
#endif
        }
        checkCancelled();
        const QFileInfo destination(folder);
        if (!QDir().mkpath(destination.absolutePath())) throw std::runtime_error("Cannot create stems directory");
        QTemporaryDir staging(destination.absolutePath() + "/.stemboks-stems-XXXXXX");
        if (!staging.isValid()) throw std::runtime_error("Cannot stage stem WAV files");
        for (int i = 0; i < kNumStems; ++i) {
            progress(90 + i * 2, fallback ? "Writing mid/side heuristic approximation" : "Writing stem WAVs");
            writeWav(staging.filePath(names[i] + ".wav"), stems[i], audio->sampleRate);
        }
        checkCancelled();
        if (QFileInfo::exists(folder)) throw std::runtime_error("Stems folder already exists; refusing to overwrite it");
        if (!QDir().rename(staging.path(), folder)) throw std::runtime_error("Cannot publish stems folder");
        staging.setAutoRemove(false);
        return {true, fallback ? "Finished: mid/side heuristic approximation (not model separation)" : "Finished: HTDemucs ONNX"};
    } catch (const std::exception& e) { return {false, "Separation failed: " + QString::fromUtf8(e.what())}; }
    catch (...) { return {false, "Separation failed: unknown error"}; }
}
}

StemManager& StemManager::instance() {
    static StemManager manager;
    static const bool connected = [] {
        if (QCoreApplication::instance()) QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, &manager, [] {
            cancelled.store(true, std::memory_order_relaxed);
            for (auto* watcher : manager.findChildren<QFutureWatcherBase*>()) watcher->waitForFinished();
        });
        return true;
    }();
    Q_UNUSED(connected);
    return manager;
}
QString StemManager::stemsFolderFor(const QString& path) {
    const auto candidates = folders(path);
    for (const auto& folder : candidates) if (!stemFiles(folder).isEmpty()) return folder;
    const QString configured = AppSettings::instance().get("[Stembox]", "stems_folder");
    return configured.isEmpty() ? candidates.front() : QDir(configured).filePath(baseName(path) + ".stems");
}
bool StemManager::stemsExistFor(const QString& path) {
    for (const auto& file : containers(path)) if (QFileInfo(file).isFile() && Decoder::audioStreamCount(file) >= 5) return true;
    for (const auto& folder : folders(path)) if (!stemFiles(folder).isEmpty()) return true;
    return false;
}
bool StemManager::loadStems(const QString& path, int rate, std::array<AudioBufferPtr, kNumStems>* out) {
    if (!out || rate <= 0) return false;
    *out = {};
    auto valid = [rate](std::array<AudioBufferPtr, kNumStems>& stems) {
        qint64 shortest = std::numeric_limits<qint64>::max(), longest = 0;
        for (const auto& stem : stems) {
            if (!stem || stem->isEmpty() || stem->sampleRate != rate) return false;
            shortest = std::min(shortest, stem->frames()); longest = std::max(longest, stem->frames());
        }
        if (longest - shortest > 200000) return false;
        for (auto& stem : stems) if (stem->frames() != longest) {
            auto padded = std::make_shared<AudioBuffer>(*stem);
            padded->interleaved.resize(longest * 2, 0.0f);
            stem = std::move(padded);
        }
        return true;
    };
    for (const auto& file : containers(path)) {
        if (!QFileInfo(file).isFile() || Decoder::audioStreamCount(file) < 5) continue;
        std::array<AudioBufferPtr, kNumStems> stems;
        for (int i = 0; i < kNumStems; ++i) stems[i] = Decoder::decodeStream(file, i + 1, rate);
        if (valid(stems)) { *out = stems; return true; }
    }
    for (const auto& folder : folders(path)) {
        const auto files = stemFiles(folder); if (files.isEmpty()) continue;
        std::array<AudioBufferPtr, kNumStems> stems;
        for (int i = 0; i < kNumStems; ++i) stems[i] = Decoder::decodeFile(files[i], rate);
        if (valid(stems)) { *out = stems; return true; }
    }
    return false;
}
QString StemManager::modelPath() const {
    const QString configured = AppSettings::instance().get("[Stembox]", "stems_model_path");
    return configured.isEmpty() ? AppSettings::dataDir() + "/models/stems-hdemucs-mmi-v1.onnx" : configured;
}
bool StemManager::separationAvailable(QString* whyNot) const {
    if (whyNot) whyNot->clear();
    if (AppSettings::instance().getInt("[Stembox]", "stems_fallback") == 1) return true;
#ifdef STEMBOKS_HAVE_ONNX
    if (QFileInfo(modelPath()).isFile()) return true;
#endif
    if (whyNot) *whyNot = unavailable;
    return false;
}
void StemManager::enqueue(const QString& path) {
    if (path.isEmpty() || cancelled.load(std::memory_order_relaxed)) return;
    for (const auto& job : m_jobs) if (job.trackPath == path && job.progress >= 0 && job.progress < 100) return;
    m_jobs.append({path, 0, "Queued"}); emit jobsChanged(); pump();
}
void StemManager::clearFinished() {
    m_jobs.erase(std::remove_if(m_jobs.begin(), m_jobs.end(), [](const Job& job) { return job.progress < 0 || job.progress >= 100; }), m_jobs.end());
    emit jobsChanged();
}
void StemManager::pump() {
    if (m_running || cancelled.load(std::memory_order_relaxed)) return;
    auto it = std::find_if(m_jobs.begin(), m_jobs.end(), [](const Job& job) { return job.progress == 0 && job.status == "Queued"; });
    if (it == m_jobs.end()) return;
    const QString path = it->trackPath;
    if (stemsExistFor(path)) { it->progress = 100; it->status = "Finished: stems already available"; emit jobsChanged(); emit jobFinished(path, true); QMetaObject::invokeMethod(this, [this] { pump(); }, Qt::QueuedConnection); return; }
    QString why;
    if (!separationAvailable(&why)) { it->progress = -1; it->status = why; emit jobsChanged(); emit jobFinished(path, false); QMetaObject::invokeMethod(this, [this] { pump(); }, Qt::QueuedConnection); return; }
    bool fallback = true;
#ifdef STEMBOKS_HAVE_ONNX
    fallback = !QFileInfo(modelPath()).isFile();
#endif
    const QString model = modelPath(), folder = stemsFolderFor(path);
    m_running = true; it->progress = 1; it->status = fallback ? "Starting mid/side heuristic approximation" : "Starting HTDemucs ONNX";
    emit jobsChanged();
    const Progress progress = [this, path](int value, const QString& status) {
        QMetaObject::invokeMethod(this, [this, path, value, status] {
            for (auto& job : m_jobs) if (job.trackPath == path && job.progress > 0 && job.progress < 100) { job.progress = value; job.status = status; emit jobsChanged(); break; }
        }, Qt::QueuedConnection);
    };
    auto* watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, path] {
        const auto result = watcher->result(); watcher->deleteLater();
        for (auto& job : m_jobs) if (job.trackPath == path && job.progress > 0 && job.progress < 100) { job.progress = result.ok ? 100 : -1; job.status = result.status; break; }
        m_running = false; emit jobsChanged(); emit jobFinished(path, result.ok); pump();
    });
    watcher->setFuture(QtConcurrent::run([path, folder, model, fallback, progress] { return runJob(path, folder, model, fallback, progress); }));
}
