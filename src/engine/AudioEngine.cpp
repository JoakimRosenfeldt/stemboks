#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#if defined(__APPLE__)
// CoreAudio also declares a global AudioBuffer; keep its type local to this TU.
#define AudioBuffer StemboksCoreAudioBuffer
#endif
#include "miniaudio.h"
#if defined(__APPLE__)
#undef AudioBuffer
#endif
#include "AudioEngine.h"
#include "Decoder.h"
#include "control/AppSettings.h"
#include "control/ControlRegistry.h"
#include <algorithm>
#include <cmath>
#include <mutex>

namespace {
constexpr int blockFrames = 512;
constexpr size_t headCapacity = 32768;
constexpr double pi = 3.14159265358979323846;
struct EngineState {
    std::recursive_mutex mutex;
    ma_context context{};
    bool contextReady = false;
    std::array<QString, kNumDecks> volume, pfl;
    std::array<std::array<QString, 2>, kNumDecks> sends;
    std::array<QString, kNumSamplers> samplerPlay, samplerGain;
    std::array<QString, 2> fxName, fxEnabled, fxMix, fxParameter;
    std::array<QString, 8> eq;
    std::array<double, 8> eqValues{};
    int eqRate = 0;
    QString crossfader = QStringLiteral("mixer.crossfader.position");
    QString masterGain = QStringLiteral("mixer.master.gain");
    QString headMix = QStringLiteral("mixer.head.mix");
    QString headGain = QStringLiteral("mixer.head.gain");
    QString mode = QStringLiteral("app.xfaderMode");
    QString curve = QStringLiteral("app.xfaderCurve");
    QString disable = QStringLiteral("app.xfaderDisable");
    QString reverse = QStringLiteral("app.xfaderReverse");
    QString limiter = QStringLiteral("app.masterLimiter");
    std::array<std::array<float, blockFrames * 2>, 2> fxBuffers{}, fxDry{};
    std::array<float, headCapacity * 2> headQueue{};
    std::atomic<size_t> headRead{0}, headWrite{0};
    std::atomic<bool> headEnabled{false};
};
EngineState& state() { static EngineState s; return s; }
void mainCallback(ma_device* device, void* output, const void*, ma_uint32 frames) {
    static_cast<AudioEngine*>(device->pUserData)->processMain(static_cast<float*>(output), int(frames));
}
void headCallback(ma_device* device, void* output, const void*, ma_uint32 frames) {
    static_cast<AudioEngine*>(device->pUserData)->processHead(static_cast<float*>(output), int(frames));
}
void closeDevice(void*& pointer) {
    if (!pointer) return;
    auto* device = static_cast<ma_device*>(pointer);
    ma_device_uninit(device);
    delete device;
    pointer = nullptr;
}
void prepareBeatFx(EffectSlot& fx, const QString& name, int rate, float* scratch) {
    fx.setSampleRate(rate); fx.setEnabled(true); fx.setMix(0); fx.setParameter(8); fx.setBpm(60);
    for (const QString& warm : {QStringLiteral("echo"), QStringLiteral("roll"), QStringLiteral("reverb")}) {
        fx.setEffect(warm); fx.process(scratch, 1);
    }
    fx.setEffect(name); fx.setParameter(.5); fx.setBpm(120); fx.process(scratch, 1);
    fx.setEnabled(false);
}
std::array<double, 2> crossfaderGains(const EngineState& s) {
    auto& registry = ControlRegistry::instance();
    if (registry.getBool(s.disable)) return {1, 1};
    double position = std::clamp(registry.get(s.crossfader), -1.0, 1.0);
    if (registry.getBool(s.reverse)) position = -position;
    const double x = (position + 1) * .5;
    const int curve = int(registry.get(s.curve));
    double left = 1 - x, right = x;
    if (curve == 1) {
        left = std::min(1.0, (1 - x) * 2); right = std::min(1.0, x * 2);
    } else if (curve == 2) {
        left = std::min(1.0, (1 - x) / .05); right = std::min(1.0, x / .05);
    }
    if (registry.get(s.mode) == 0) {
        left = std::sin(left * pi * .5); right = std::sin(right * pi * .5);
        const double norm = std::hypot(left, right);
        if (norm > 0) { left /= norm; right /= norm; }
    } else if (curve == 0) {
        left = std::min(1.0, left * 2); right = std::min(1.0, right * 2);
    }
    return {left, right};
}
}

AudioEngine& AudioEngine::instance() { static AudioEngine engine; return engine; }
AudioEngine::AudioEngine() : m_recorder(std::make_unique<Recorder>()) {
    auto& s = state();
    auto& registry = ControlRegistry::instance();
    const int fxRate = std::clamp(AppSettings::instance().getInt(QStringLiteral("[Stembox]"),
        QStringLiteral("sound_sample_rate"), 44100), 8000, 192000);
    m_mixBuf.resize(blockFrames * 2); m_headBuf.resize(blockFrames * 2); m_deckBuf.resize(blockFrames * 2);
    for (int i = 0; i < kNumDecks; ++i) {
        m_decks[i] = std::make_unique<Deck>(i, this);
        s.volume[i] = deckKey(i, "level.volume"); s.pfl[i] = deckKey(i, "level.pfl");
        for (int u = 0; u < 2; ++u) s.sends[i][u] = deckKey(i, u == 0 ? "fx.unit1" : "fx.unit2");
    }
    for (int i = 0; i < kNumSamplers; ++i) {
        const QString prefix = QStringLiteral("sampler.%1.").arg(i + 1);
        s.samplerPlay[i] = prefix + QStringLiteral("play");
        s.samplerGain[i] = prefix + QStringLiteral("gain");
        registry.setSilent(s.samplerPlay[i], 0);
        registry.setSilent(s.samplerGain[i], registry.get(s.samplerGain[i], 1));
    }
    for (int u = 0; u < 2; ++u) {
        const QString prefix = QStringLiteral("fx.unit.%1.").arg(u + 1);
        s.fxName[u] = prefix + QStringLiteral("name");
        s.fxEnabled[u] = prefix + QStringLiteral("enabled");
        s.fxMix[u] = prefix + QStringLiteral("mix");
        s.fxParameter[u] = prefix + QStringLiteral("parameter");
        prepareBeatFx(m_fxUnit[u], registry.getString(s.fxName[u], QStringLiteral("echo")),
                      fxRate, s.fxBuffers[u].data());
    }
    for (int i = 0; i < 8; ++i) s.eq[i] = QStringLiteral("mixer.eq.%1").arg(i + 1);
    registry.setSilent(s.masterGain, registry.get(s.masterGain, 1));
    registry.setSilent(s.headGain, registry.get(s.headGain, 1));
    connect(&registry, &ControlRegistry::changed, this, [this](const QString& name, double value) {
        auto& s = state();
        for (int i = 0; i < kNumSamplers; ++i)
            if (name == s.samplerPlay[i]) { triggerSampler(i, value != 0); break; }
    }, Qt::DirectConnection);
    connect(&registry, &ControlRegistry::stringChanged, this, [this](const QString& name, const QString& value) {
        auto& s = state();
        if (name != s.fxName[0] && name != s.fxName[1]) return;
        std::lock_guard<std::recursive_mutex> lock(s.mutex);
        for (int u = 0; u < 2; ++u) if (name == s.fxName[u]) {
            m_fxUnit[u].setEffect(value);
            // Prepare any effect delay storage on the control thread.
            const bool enabled = m_fxUnit[u].enabled();
            m_fxUnit[u].setEnabled(true);
            std::fill(s.fxBuffers[u].begin(), s.fxBuffers[u].end(), 0);
            m_fxUnit[u].process(s.fxBuffers[u].data(), blockFrames);
            m_fxUnit[u].setEnabled(enabled);
        }
    }, Qt::DirectConnection);
}

bool AudioEngine::start() {
    if (m_mainDevice) return true;
    auto& s = state();
    auto& settings = AppSettings::instance();
    const QString group = QStringLiteral("[Stembox]");
    m_sampleRate = std::clamp(settings.getInt(group, QStringLiteral("sound_sample_rate"), 44100), 8000, 192000);
    const int bufferFrames = std::clamp(settings.getInt(group, QStringLiteral("sound_buffer_frames"), 512), 32, 8192);
    if (ma_context_init(nullptr, 0, nullptr, &s.context) != MA_SUCCESS) return false;
    s.contextReady = true;
    ma_device_info* infos = nullptr;
    ma_uint32 count = 0;
    if (ma_context_get_devices(&s.context, &infos, &count, nullptr, nullptr) != MA_SUCCESS) { stop(); return false; }
    const QString mainName = settings.get(group, QStringLiteral("sound_main_device"),
                                         settings.get(group, QStringLiteral("sound_master_device"), QStringLiteral("---")));
    const QString headName = settings.get(group, QStringLiteral("sound_headphones_device"), QStringLiteral("---"));
    auto select = [&](const QString& name) -> const ma_device_id* {
        for (ma_uint32 i = 0; i < count; ++i)
            if (QString::fromUtf8(infos[i].name) == name) return &infos[i].id;
        bool numeric = false;
        const unsigned int index = name.toUInt(&numeric);
        return numeric && index < count ? &infos[index].id : nullptr;
    };
    const ma_device_id* mainId = select(mainName);
    const ma_device_id* headId = select(headName);
    // A selected default device is still the same physical main output.
    const ma_device_id* effectiveMain = mainId;
    if (!effectiveMain) for (ma_uint32 i = 0; i < count; ++i)
        if (infos[i].isDefault) { effectiveMain = &infos[i].id; break; }
    const bool separateHead = !headName.isEmpty() && headName != QStringLiteral("---") && headId
        && headId != effectiveMain;
    auto open = [&](const ma_device_id* id, ma_device_data_proc callback, void*& target) {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.pDeviceID = id;
        config.playback.format = ma_format_f32;
        config.playback.channels = 2;
        config.sampleRate = ma_uint32(m_sampleRate);
        config.periodSizeInFrames = ma_uint32(bufferFrames);
        config.dataCallback = callback;
        config.pUserData = this;
        auto device = std::make_unique<ma_device>();
        if (ma_device_init(&s.context, &config, device.get()) != MA_SUCCESS) return false;
        target = device.release();
        return true;
    };
    if (!open(mainId, mainCallback, m_mainDevice) || (separateHead && !open(headId, headCallback, m_headDevice))) {
        stop(); return false;
    }
    for (int u = 0; u < 2; ++u)
        prepareBeatFx(m_fxUnit[u], m_fxUnit[u].effect(), m_sampleRate, s.fxBuffers[u].data());
    s.headRead = 0; s.headWrite = 0;
    s.headEnabled = separateHead;
    if (ma_device_start(static_cast<ma_device*>(m_mainDevice)) != MA_SUCCESS
        || (m_headDevice && ma_device_start(static_cast<ma_device*>(m_headDevice)) != MA_SUCCESS)) {
        stop(); return false;
    }
    emit started();
    return true;
}
void AudioEngine::stop() {
    // Do not hold the mixer mutex while waiting for a callback to finish.
    closeDevice(m_mainDevice); closeDevice(m_headDevice);
    auto& s = state();
    std::lock_guard<std::recursive_mutex> lock(s.mutex);
    if (s.contextReady) { ma_context_uninit(&s.context); s.contextReady = false; }
    m_recording = false;
    m_recorder->close();
    s.headEnabled = false;
    s.headRead = 0; s.headWrite = 0;
}
QVector<AudioEngine::DeviceInfo> AudioEngine::outputDevices() const {
    QVector<DeviceInfo> result;
    ma_context context;
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) return result;
    ma_device_info* infos = nullptr;
    ma_uint32 count = 0;
    if (ma_context_get_devices(&context, &infos, &count, nullptr, nullptr) == MA_SUCCESS)
        for (ma_uint32 i = 0; i < count; ++i) result.append({QString::fromUtf8(infos[i].name), i});
    ma_context_uninit(&context);
    return result;
}
QStringList AudioEngine::apiNames() const {
#if defined(Q_OS_MACOS)
    return {QStringLiteral("CoreAudio")};
#elif defined(Q_OS_WIN)
    return {QStringLiteral("WASAPI"), QStringLiteral("ASIO")};
#else
    return {QStringLiteral("ALSA"), QStringLiteral("PulseAudio"), QStringLiteral("JACK")};
#endif
}
void AudioEngine::loadSampler(int slot, AudioBufferPtr buf, const QString& label) {
    if (slot < 0 || slot >= kNumSamplers) return;
    std::lock_guard<std::recursive_mutex> lock(state().mutex);
    m_samplers[slot].pos = -1; m_samplers[slot].buf = std::move(buf); m_samplers[slot].label = label;
    ControlRegistry::instance().setSilent(state().samplerPlay[slot], 0);
}
void AudioEngine::triggerSampler(int slot, bool on) {
    if (slot < 0 || slot >= kNumSamplers) return;
    std::lock_guard<std::recursive_mutex> lock(state().mutex);
    const auto& buf = m_samplers[slot].buf;
    const bool playing = on && buf && !buf->isEmpty();
    m_samplers[slot].pos = playing ? 0 : -1;
    ControlRegistry::instance().setSilent(state().samplerPlay[slot], playing);
}
bool AudioEngine::samplerPlaying(int slot) const {
    return slot >= 0 && slot < kNumSamplers && m_samplers[slot].pos >= 0;
}
void AudioEngine::setRecording(bool on, const QString& path, bool mp3, int bitDepth) {
    std::lock_guard<std::recursive_mutex> lock(state().mutex);
    m_recording = false; m_recorder->close();
    if (on) m_recording = m_recorder->open(path, m_sampleRate, mp3, bitDepth);
}
double AudioEngine::recordedSeconds() const {
    std::lock_guard<std::recursive_mutex> lock(state().mutex);
    return m_recorder->secondsRecorded();
}

void AudioEngine::renderMix(float* mainOut, float* headOut, int frames) {
    auto& s = state();
    std::lock_guard<std::recursive_mutex> lock(s.mutex);
    auto& registry = ControlRegistry::instance();
    std::fill_n(mainOut, frames * 2, 0.f); std::fill_n(headOut, frames * 2, 0.f);
    const auto xfader = crossfaderGains(s);
    double bpm = 0;
    for (int u = 0; u < 2; ++u) std::fill_n(s.fxBuffers[u].data(), frames * 2, 0.f);
    for (int deck = 0; deck < kNumDecks; ++deck) {
        m_decks[deck]->render(m_deckBuf.data(), frames, m_sampleRate);
        const double volume = std::clamp(registry.get(s.volume[deck], 1), 0.0, 1.0) * xfader[deck % 2];
        const bool pfl = registry.getBool(s.pfl[deck]);
        if (m_decks[deck]->playing() && bpm == 0) bpm = m_decks[deck]->effectiveBpm();
        for (int f = 0; f < frames * 2; ++f) {
            mainOut[f] += float(m_deckBuf[f] * volume);
            if (pfl) headOut[f] += m_deckBuf[f];
        }
        for (int u = 0; u < 2; ++u) {
            const double send = std::clamp(registry.get(s.sends[deck][u]), 0.0, 1.0);
            if (send > 0) for (int f = 0; f < frames * 2; ++f)
                s.fxBuffers[u][f] += float(m_deckBuf[f] * volume * send);
        }
    }
    for (int u = 0; u < 2; ++u) {
        auto& fx = m_fxUnit[u];
        fx.setEnabled(registry.getBool(s.fxEnabled[u]));
        fx.setMix(std::clamp(registry.get(s.fxMix[u], .5), 0.0, 1.0));
        fx.setParameter(registry.get(s.fxParameter[u], .5)); fx.setBpm(bpm > 0 ? bpm : 120);
        if (!fx.enabled()) continue;
        std::copy_n(s.fxBuffers[u].data(), frames * 2, s.fxDry[u].data());
        fx.process(s.fxBuffers[u].data(), frames);
        // EffectSlot includes its dry input, which is already in the main bus.
        for (int f = 0; f < frames * 2; ++f) mainOut[f] += s.fxBuffers[u][f] - s.fxDry[u][f];
    }
    for (int slot = 0; slot < kNumSamplers; ++slot) {
        auto& sampler = m_samplers[slot];
        qint64 position = sampler.pos.load();
        if (position < 0 || !sampler.buf) continue;
        const double gain = std::clamp(registry.get(s.samplerGain[slot], 1), 0.0, 2.0);
        const double rate = double(sampler.buf->sampleRate) / m_sampleRate;
        for (int f = 0; f < frames; ++f) {
            const qint64 src = position + qint64(f * rate);
            if (src >= sampler.buf->frames()) break;
            for (int ch = 0; ch < 2; ++ch) mainOut[f * 2 + ch] += float(sampler.buf->interleaved[src * 2 + ch] * gain);
        }
        position += std::max<qint64>(1, qint64(frames * rate));
        if (position >= sampler.buf->frames()) {
            sampler.pos = -1; registry.setSilent(s.samplerPlay[slot], 0);
        } else sampler.pos = position;
    }
    const double frequencies[] = {45, 100, 220, 500, 1100, 2500, 5500, 12000};
    for (int band = 0; band < 8; ++band) {
        const double db = std::clamp(registry.get(s.eq[band]), -12.0, 12.0);
        if (s.eqRate != m_sampleRate || db != s.eqValues[band]) {
            for (int ch = 0; ch < 2; ++ch)
                m_masterEq[band][ch].setPeaking(m_sampleRate, std::min(frequencies[band], m_sampleRate * .45), 1, db);
            s.eqValues[band] = db;
        }
    }
    s.eqRate = m_sampleRate;
    const double master = std::max(0.0, registry.get(s.masterGain, 1));
    const bool limiting = registry.getBool(s.limiter, true);
    float inputPeak = 0;
    for (int f = 0; f < frames; ++f) for (int ch = 0; ch < 2; ++ch) {
        float sample = mainOut[f * 2 + ch];
        for (int band = 0; band < 8; ++band) sample = m_masterEq[band][ch].process(sample);
        mainOut[f * 2 + ch] = float(sample * master);
        inputPeak = std::max(inputPeak, std::abs(mainOut[f * 2 + ch]));
    }
    const float target = inputPeak > 1 ? 1 / inputPeak : 1;
    if (limiting) {
        if (target < m_limiterGain) m_limiterGain = target;
        else m_limiterGain += (target - m_limiterGain) * float(1 - std::exp(-double(frames) / m_sampleRate / .1));
    } else m_limiterGain = 1;
    const double mix = std::clamp(registry.get(s.headMix), 0.0, 1.0);
    const double headphone = std::max(0.0, registry.get(s.headGain, 1));
    float peak[2]{};
    for (int f = 0; f < frames; ++f) for (int ch = 0; ch < 2; ++ch) {
        const int i = f * 2 + ch;
        if (limiting) mainOut[i] = std::tanh(mainOut[i] * m_limiterGain);
        headOut[i] = float((headOut[i] * (1 - mix) + mainOut[i] * mix) * headphone);
        peak[ch] = std::max(peak[ch], std::abs(mainOut[i]));
    }
    const float decay = float(std::exp(-double(frames) / m_sampleRate / .3));
    m_levelL = std::max(peak[0], m_levelL.load() * decay);
    m_levelR = std::max(peak[1], m_levelR.load() * decay);
    if (m_recording) m_recorder->write(mainOut, frames);
}
void AudioEngine::processMain(float* out, int frames) {
    if (!out || frames <= 0) return;
    auto& s = state();
    for (int offset = 0; offset < frames; offset += blockFrames) {
        const int count = std::min(blockFrames, frames - offset);
        renderMix(m_mixBuf.data(), m_headBuf.data(), count);
        std::copy_n(m_mixBuf.constData(), count * 2, out + offset * 2);
        if (!s.headEnabled.load(std::memory_order_relaxed)) continue;
        const size_t read = s.headRead.load(std::memory_order_acquire);
        size_t write = s.headWrite.load(std::memory_order_relaxed);
        const size_t available = headCapacity - (write - read);
        const size_t queued = std::min(size_t(count), available);
        for (size_t f = 0; f < queued; ++f) {
            const size_t index = (write + f) % headCapacity;
            s.headQueue[index * 2] = m_headBuf[qsizetype(f * 2)];
            s.headQueue[index * 2 + 1] = m_headBuf[qsizetype(f * 2 + 1)];
        }
        s.headWrite.store(write + queued, std::memory_order_release);
    }
}
void AudioEngine::processHead(float* out, int frames) {
    if (!out || frames <= 0) return;
    auto& s = state();
    std::fill_n(out, frames * 2, 0.f);
    const size_t read = s.headRead.load(std::memory_order_relaxed);
    const size_t write = s.headWrite.load(std::memory_order_acquire);
    const size_t count = std::min(size_t(frames), write - read);
    for (size_t f = 0; f < count; ++f) {
        const size_t index = (read + f) % headCapacity;
        out[f * 2] = s.headQueue[index * 2]; out[f * 2 + 1] = s.headQueue[index * 2 + 1];
    }
    s.headRead.store(read + count, std::memory_order_release);
}
