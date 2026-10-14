#include "Deck.h"
#include "control/ControlRegistry.h"
#include "control/AppSettings.h"
#include <QMutexLocker>
#include <algorithm>
#include <cmath>
#include <mutex>

namespace {
constexpr int blockFrames = 512;
const char* stemNames[] = {"drums", "bass", "other", "vocals"};
enum Control {
    Playing, Cue, Reverse, Fader, Range, Ratio, KeyLock, KeyShift, Sync, Quantize,
    Slip, LoopEnabled, LoopIn, LoopOut, LoopDouble, LoopHalve, LoopSize, LoopActive,
    JumpForward, JumpBackward, JumpSize, Touch, Move, Bend, Brake, Gain, Volume, Pfl,
    EqLow, EqMid, EqHigh, ColorAmount, ColorEnabled, Position, PositionSec,
    Duration, Bpm, FileBpm, Loaded, StemsReady, Level, Count
};
const char* controlNames[] = {
    "transport.playing", "transport.cue", "transport.reverseSlip", "tempo.fader",
    "tempo.range", "tempo.ratio", "key.lock", "key.shift", "sync.enabled", "grid.quantize",
    "slip.enabled", "loop.enabled", "loop.setIn", "loop.setOut", "loop.double",
    "loop.halve", "loop.size", "loop.active", "jump.forward", "jump.backward",
    "jump.size", "jog.touch", "jog.move", "pitchbend", "transport.brake",
    "level.gain", "level.volume", "level.pfl", "eq.low", "eq.mid", "eq.high",
    "colorfx.amount", "colorfx.enabled", "position", "positionSec", "durationSec",
    "bpm", "fileBpm", "trackLoaded", "stemsReady", "level"
};
struct DeckState {
    std::recursive_mutex mutex;
    std::array<QString, Count> keys;
    std::array<QString, kNumStems> stemGain, stemMute;
    std::array<QString, kNumHotcues> activate, set, clear, present;
    QString invert = QStringLiteral("app.tempoInvert");
    QString eqCurve = QStringLiteral("app.eqCurve");
    QString autoGain = QStringLiteral("app.autoGain");
    QString hotcueMode = QStringLiteral("app.hotcueMode");
    QString backspin = QStringLiteral("app.backspin");
    QString colorName;
    QString prefix;
    std::array<float, blockFrames * 2> scratch{};
    std::array<float, blockFrames * 4> pitchBuffer{}, stemBuffer{};
    std::array<int, blockFrames * 2> spliceOffsets{};
    std::array<double, 3> eqValues{{100, 100, 100}};
    int eqRate = 0;
    bool iso = true;
    bool cuePreview = false, hotcuePreview = false, hotcueWasPlaying = false, scratching = false;
    int heldHotcue = -1;
    double hotcueReturn = 0;
    float peak = 0;
    std::atomic<double> baseBpm{0};
};
// Deck indices are fixed by the engine; storage lives here to preserve Deck.h.
std::array<DeckState, kNumDecks> states;
DeckState& state(int index) { return states.at(index); }
double get(const DeckState& s, Control c, double def = 0) {
    return ControlRegistry::instance().get(s.keys[c], def);
}
void put(const DeckState& s, Control c, double value) {
    ControlRegistry::instance().setSilent(s.keys[c], value);
}
double bpmOf(const LoadedTrack& t) {
    return t.analysis.bpm > 0 ? t.analysis.bpm : t.info.bpm;
}
double tempoOf(const DeckState& s) {
    auto& registry = ControlRegistry::instance();
    if (get(s, Sync) && s.baseBpm > 0) {
        for (const auto& leader : states) {
            if (&leader != &s && !leader.keys[Playing].isEmpty() && get(leader, Playing)
                && !get(leader, Sync) && get(leader, Bpm) > 0)
                return get(leader, Bpm) / s.baseBpm.load();
        }
    }
    const double direction = registry.getBool(s.invert) ? -1 : 1;
    return std::max(0.0, 1 + direction * std::clamp(get(s, Fader), -1.0, 1.0)
                              * std::clamp(get(s, Range, 6), 0.0, 100.0) / 100);
}
double snap(const DeckState& s, const LoadedTrack& t, double sec) {
    const double bpm = bpmOf(t);
    if (get(s, Quantize) && bpm > 0) {
        const double beat = 60 / bpm;
        sec = t.analysis.firstBeatSec + std::round((sec - t.analysis.firstBeatSec) / beat) * beat;
    }
    return std::clamp(sec, 0.0, t.audio->durationSec());
}
float sample(const AudioBuffer& b, double position, int channel) {
    if (position < 0 || position >= b.frames()) return 0;
    const qint64 i = qint64(position);
    const qint64 j = std::min(i + 1, b.frames() - 1);
    const double fraction = position - i;
    return float(b.interleaved[i * 2 + channel] * (1 - fraction)
                 + b.interleaved[j * 2 + channel] * fraction);
}
void prepareColorFx(EffectSlot& fx, const QString& name, int rate, float* scratch) {
    fx.setSampleRate(rate); fx.setBpm(60); fx.setEnabled(true); fx.setMix(0); fx.setParameter(8);
    // Reserve the maximum delay and roll storage through the existing DSP API.
    for (const QString& warm : {QStringLiteral("echo"), QStringLiteral("roll"), QStringLiteral("reverb")}) {
        fx.setEffect(warm); fx.process(scratch, 1);
    }
    fx.setEffect(name); fx.setParameter(.5); fx.setBpm(120); fx.process(scratch, 1);
    fx.setEnabled(false);
}
}

Deck::Deck(int index, QObject* parent) : QObject(parent), m_index(index) {
    auto& s = state(index);
    s.prefix = QStringLiteral("deck.%1.").arg(index + 1);
    auto& registry = ControlRegistry::instance();
    for (int i = 0; i < Count; ++i) {
        s.keys[i] = key(controlNames[i]);
        double def = 0;
        if (i == Range) def = 6;
        if (i == Ratio || i == Gain || i == Volume || i == ColorEnabled) def = 1;
        if (i == LoopSize || i == JumpSize) def = 4;
        registry.setSilent(s.keys[i], registry.get(s.keys[i], def));
    }
    for (int i = 0; i < kNumStems; ++i) {
        s.stemGain[i] = key(QString("stems.%1.gain").arg(stemNames[i]).toUtf8().constData());
        s.stemMute[i] = key(QString("stems.%1.mute").arg(stemNames[i]).toUtf8().constData());
        registry.setSilent(s.stemGain[i], registry.get(s.stemGain[i], 1));
        registry.setSilent(s.stemMute[i], registry.get(s.stemMute[i]));
    }
    for (int i = 0; i < kNumHotcues; ++i) {
        const QString prefix = QStringLiteral("deck.%1.hotcue.%2.").arg(index + 1).arg(i + 1);
        s.activate[i] = prefix + QStringLiteral("activate");
        s.set[i] = prefix + QStringLiteral("set");
        s.clear[i] = prefix + QStringLiteral("clear");
        s.present[i] = prefix + QStringLiteral("set-out");
        registry.setSilent(s.present[i], 0);
    }
    s.colorName = key("colorfx.name");
    const int fxRate = std::clamp(AppSettings::instance().getInt(QStringLiteral("[Stembox]"),
        QStringLiteral("sound_sample_rate"), 44100), 8000, 192000);
    prepareColorFx(m_colorFx, registry.getString(s.colorName, QStringLiteral("filter")), fxRate, s.scratch.data());
    connect(&registry, &ControlRegistry::changed, this, &Deck::onControlChanged, Qt::DirectConnection);
    connect(&registry, &ControlRegistry::stringChanged, this,
            [this](const QString& name, const QString& value) {
        auto& s = state(m_index);
        if (name != s.colorName) return;
        std::lock_guard<std::recursive_mutex> lock(s.mutex);
        m_colorFx.setEffect(value);
        const bool enabled = m_colorFx.enabled();
        m_colorFx.setEnabled(true);
        std::fill(s.scratch.begin(), s.scratch.end(), 0);
        m_colorFx.process(s.scratch.data(), blockFrames);
        m_colorFx.setEnabled(enabled);
    }, Qt::DirectConnection);
    connect(&AppSettings::instance(), &AppSettings::changed, this,
            [this](const QString& group, const QString& name) {
        if (group != QStringLiteral("[Stembox]") || name != QStringLiteral("sound_sample_rate")) return;
        auto& s = state(m_index);
        std::lock_guard<std::recursive_mutex> lock(s.mutex);
        const int rate = std::clamp(AppSettings::instance().getInt(group, name, 44100), 8000, 192000);
        prepareColorFx(m_colorFx, m_colorFx.effect(), rate, s.scratch.data());
    }, Qt::DirectConnection);
    // Allocate DSP working storage before the audio callback ever runs.
    std::array<float, 4096> silence{};
    m_stretcher.process(silence.data(), 2048, 0, 1, s.scratch.data(), blockFrames);
    for (auto& stretcher : m_stemStretcher)
        stretcher.process(silence.data(), 2048, 0, 1, s.scratch.data(), blockFrames);
    m_colorFx.setEnabled(true);
    m_colorFx.process(s.scratch.data(), blockFrames);
    m_stretcher.reset();
    for (auto& stretcher : m_stemStretcher) stretcher.reset();
}

QString Deck::key(const char* k) const { return deckKey(m_index, k); }
double Deck::ctrl(const char* k, double def) const { return ControlRegistry::instance().get(key(k), def); }
LoadedTrackPtr Deck::track() const { QMutexLocker lock(&m_trackMutex); return m_track; }
double Deck::durationSec() const { auto t = track(); return t && t->audio ? t->audio->durationSec() : 0; }
double Deck::effectiveBpm() const { auto t = track(); return t ? bpmOf(*t) * tempoOf(state(m_index)) : 0; }

void Deck::loadTrack(LoadedTrackPtr t) {
    auto& s = state(m_index);
    std::unique_lock<std::recursive_mutex> lock(s.mutex);
    m_playing = false;
    { QMutexLocker trackLock(&m_trackMutex); m_track = t; }
    m_sampleRate = t && t->audio ? t->audio->sampleRate : 44100;
    s.baseBpm = t ? bpmOf(*t) : 0;
    m_positionFrames = 0;
    m_slipping = m_braking = m_cueHeld = false;
    s.cuePreview = s.hotcuePreview = s.scratching = false;
    s.heldHotcue = -1;
    m_stretcher.reset();
    for (auto& stretcher : m_stemStretcher) stretcher.reset();
    for (int ch = 0; ch < 2; ++ch) {
        m_eqLow[ch].reset(); m_eqMid[ch].reset(); m_eqHigh[ch].reset();
    }
    s.peak = 0;
    auto& registry = ControlRegistry::instance();
    registry.setString(key("track.title"), t ? t->info.title : QString());
    registry.setString(key("track.artist"), t ? t->info.artist : QString());
    registry.setString(key("track.path"), t ? t->info.path : QString());
    put(s, Playing, 0); put(s, Cue, 0); put(s, Reverse, 0); put(s, LoopEnabled, 0);
    put(s, Position, 0); put(s, PositionSec, 0); put(s, Level, 0);
    put(s, Duration, durationSec()); put(s, Loaded, t && t->audio && !t->audio->isEmpty());
    put(s, StemsReady, t && t->hasStems);
    put(s, FileBpm, t ? bpmOf(*t) : 0); put(s, Bpm, effectiveBpm());
    for (int i = 0; i < kNumHotcues; ++i)
        registry.setSilent(s.present[i], t && t->hotcues[i].positionSec >= 0);
    lock.unlock();
    if (t) emit trackLoaded(m_index);
}
void Deck::unload() { loadTrack(nullptr); }
void Deck::play() {
    auto& s = state(m_index);
    std::lock_guard<std::recursive_mutex> lock(s.mutex);
    auto t = track();
    if (!t || !t->audio || t->audio->isEmpty()) { put(s, Playing, 0); return; }
    if (m_positionFrames >= t->audio->frames()) seekSec(0);
    s.cuePreview = s.hotcuePreview = false;
    m_braking = false; m_brakeFactor = 1; m_playing = true;
    put(s, Playing, 1);
}
void Deck::pause() {
    auto& s = state(m_index);
    std::lock_guard<std::recursive_mutex> lock(s.mutex);
    m_playing = false; m_braking = false;
    s.cuePreview = s.hotcuePreview = false;
    put(s, Playing, 0);
}
void Deck::seekSec(double sec) {
    auto& s = state(m_index);
    std::lock_guard<std::recursive_mutex> lock(s.mutex);
    m_positionFrames = std::clamp(sec, 0.0, durationSec()) * m_sampleRate;
    m_stretcher.reset();
    for (auto& stretcher : m_stemStretcher) stretcher.reset();
    put(s, PositionSec, positionSec());
    const double duration = durationSec();
    put(s, Position, duration > 0 ? positionSec() / duration : 0);
}
void Deck::cuePress() {
    auto& s = state(m_index);
    std::lock_guard<std::recursive_mutex> lock(s.mutex);
    auto t = track();
    if (!t || !t->audio || m_cueHeld) return;
    m_cueHeld = true;
    put(s, Cue, 1);
    if (m_playing) { seekSec(t->cueSec); pause(); s.cuePreview = false; }
    else if (std::abs(positionSec() - t->cueSec) > 1.0 / m_sampleRate) {
        t->cueSec = snap(s, *t, positionSec());
        seekSec(t->cueSec);
        s.cuePreview = false;
    } else {
        seekSec(t->cueSec); s.cuePreview = true; m_playing = true; put(s, Playing, 1);
    }
}
void Deck::cueRelease() {
    auto& s = state(m_index);
    std::lock_guard<std::recursive_mutex> lock(s.mutex);
    auto t = track();
    if (m_cueHeld && s.cuePreview && t) { seekSec(t->cueSec); pause(); }
    m_cueHeld = false; s.cuePreview = false; put(s, Cue, 0);
}
void Deck::jogBy(double beats) {
    auto& s = state(m_index);
    std::lock_guard<std::recursive_mutex> lock(s.mutex);
    auto t = track();
    if (!t || !t->audio) return;
    if (m_playing && !get(s, Touch)) put(s, Bend, std::clamp(beats * .1, -.5, .5));
    else {
        if (m_playing && get(s, Slip) && !m_slipping) {
            m_slipPositionFrames = m_positionFrames; m_slipping = true;
        }
        const double bpm = bpmOf(*t);
        seekSec(positionSec() + beats * 60 / (bpm > 0 ? bpm : 120));
        s.scratching = m_playing;
    }
}
void Deck::scratchTo(double sec) {
    auto& s = state(m_index);
    std::lock_guard<std::recursive_mutex> lock(s.mutex);
    if (m_playing && get(s, Slip) && !m_slipping) {
        m_slipPositionFrames = m_positionFrames; m_slipping = true;
    }
    s.scratching = true;
    seekSec(sec);
}

void Deck::triggerPad(int pad, bool pressed) {
    auto& registry = ControlRegistry::instance();
    const QString& prefix = state(m_index).prefix;
    const auto t = track();
    auto pulse = [&](const QString& name) { registry.set(name, 0); registry.set(name, 1); };
    const double mode = registry.get(prefix + "pads.mode");
    if (mode == 0) {
        const QString base = prefix + QString("hotcue.%1.").arg(pad + 1);
        if (!pressed) registry.set(base + "activate", 0);
        else if (t && t->hotcues[pad].positionSec < 0) pulse(base + "set");
        else registry.set(base + "activate", 1);
    } else if (mode == 1 && pressed) {
        const double sizes[] = {.5, 1, 2, 4, 8, 16, 32};
        if (pad == 7) pulse(prefix + (t && t->loopInSec >= 0 && t->loopOutSec < 0 ? "loop.setOut" : "loop.setIn"));
        else {
            const bool same = registry.getBool(prefix + "loop.enabled") && registry.get(prefix + "loop.size") == sizes[pad];
            registry.set(prefix + "loop.size", sizes[pad]);
            if (same) registry.set(prefix + "loop.enabled", 0);
            else pulse(prefix + "loop.active");
        }
    } else if (mode == 2 && pressed) {
        registry.set(prefix + "jump.size", double(4 << (pad / 2)));
        pulse(prefix + (pad % 2 ? "jump.forward" : "jump.backward"));
    } else if (mode == 3 && pressed) {
        const int shifts[] = {-7, -5, -3, -1, 1, 3, 5, 7};
        registry.set(prefix + "key.shift", registry.get(prefix + "key.shift") == shifts[pad] ? 0 : shifts[pad]);
    } else if (mode == 4 && pressed) {
        const QStringList names{"vocals", "drums", "bass", "other"};
        if (pad < 4) registry.toggle(prefix + "stems." + names[pad] + ".mute");
        else {
            const int solo = pad - 4;
            bool alreadySolo = !registry.getBool(prefix + "stems." + names[solo] + ".mute");
            for (int i = 0; i < kNumStems; ++i)
                if (i != solo && !registry.getBool(prefix + "stems." + names[i] + ".mute")) alreadySolo = false;
            for (int i = 0; i < kNumStems; ++i)
                registry.set(prefix + "stems." + names[i] + ".mute", !alreadySolo && i != solo);
        }
    } else if (mode == 5 && pressed) {
        registry.toggle(QString("sampler.%1.play").arg(pad + 1));
    } else if (mode == 6) {
        auto& settings = AppSettings::instance();
        const QString group = QStringLiteral("[Stembox]");
        const int unitIndex = m_index % 2 + 1;
        const QString unit = QString("fx.unit.%1.").arg(unitIndex);
        const bool toggle = settings.getInt(group, "padfx_type") == 1;
        if (!pressed) { if (!toggle) registry.set(unit + "enabled", 0); return; }
        const QString effect = settings.get(group, QString("padfx_name_%1").arg(pad + 1), "ECHO")
            .toLower().remove(' ').remove('-');
        if (effect == "vinylbrake") { pulse(prefix + "transport.brake"); return; }
        registry.setString(unit + "name", effect); registry.setString(prefix + "fx.name", effect);
        registry.set(unit + "parameter", settings.getDouble(group, QString("padfx_bval_%1").arg(pad + 1), .5));
        registry.set(unit + "mix", 1); registry.set(prefix + QString("fx.unit%1").arg(unitIndex), 1);
        registry.set(prefix + "fx.enabled", 1);
        if (toggle) registry.toggle(unit + "enabled"); else registry.set(unit + "enabled", 1);
    }
}

void Deck::onControlChanged(const QString& name, double value) {
    auto& s = state(m_index);
    if (!name.startsWith(s.prefix) && name != s.invert && name != s.eqCurve && name != s.autoGain
        && name != s.hotcueMode && name != s.backspin) return;
    auto& registry = ControlRegistry::instance();
    const QString control = name.mid(s.prefix.size());
    const char* alias = nullptr;
    if (control == "cue.button") alias = "transport.cue";
    else if (control == "tempo.keyLock") alias = "key.lock";
    else if (control == "scratch.wheelTouch") alias = "jog.touch";
    else if (control == "colorFx.amount") alias = "colorfx.amount";
    if (alias) { registry.set(key(alias), value); return; }
    if (control.startsWith("pads.cell.") && control.endsWith(".press")) {
        bool ok = false;
        const int pad = control.mid(10, control.size() - 16).toInt(&ok) - 1;
        if (ok && pad >= 0 && pad < kNumHotcues) triggerPad(pad, value != 0);
        return;
    }
    if (control == "loop.activeLoop") {
        if (value) {
            if (get(s, LoopEnabled)) registry.set(s.keys[LoopEnabled], 0);
            else { registry.set(s.keys[LoopActive], 0); registry.set(s.keys[LoopActive], 1); }
        }
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(s.mutex);
    if (control == "scratch.wheelMove") {
        put(s, Move, get(s, Move) + value);
        registry.setSilent(name, 0);
        return;
    }
    if (name == s.keys[Playing]) { if (value) play(); else pause(); return; }
    if (name == s.keys[Cue]) { if (value) cuePress(); else cueRelease(); return; }
    if (name == s.keys[Position]) { seekSec(value * durationSec()); return; }
    if (name == s.keys[PositionSec]) { seekSec(value); return; }
    if (name == s.keys[Fader] || name == s.keys[Range] || name == s.keys[Sync] || name == s.invert) {
        put(s, Ratio, tempoOf(s)); put(s, Bpm, effectiveBpm()); return;
    }
    if (name == s.keys[Brake] && value) {
        m_braking = m_playing; m_brakeFactor = 1; put(s, Playing, 0); put(s, Brake, 0); return;
    }
    auto t = track();
    if (!t || !t->audio) return;
    const double bpm = bpmOf(*t);
    const double beat = 60 / (bpm > 0 ? bpm : 120);
    if (control == "loop.reloop" && value) {
        if (t->loopInSec >= 0 && t->loopOutSec > t->loopInSec) {
            seekSec(t->loopInSec); put(s, LoopEnabled, 1);
        }
        registry.setSilent(name, 0);
    } else if ((control == "jump.fourForward" || control == "jump.fourBack") && value) {
        seekSec(snap(s, *t, positionSec() + (control == "jump.fourForward" ? 4 : -4) * beat));
        registry.setSilent(name, 0);
    } else if (name == s.keys[LoopEnabled] && value) {
        if (t->loopInSec >= 0 && t->loopOutSec > t->loopInSec && positionSec() >= t->loopOutSec)
            seekSec(t->loopInSec);
    } else if (name == s.keys[LoopIn] && value) {
        t->loopInSec = snap(s, *t, positionSec()); put(s, LoopIn, 0);
    } else if (name == s.keys[LoopOut] && value) {
        t->loopOutSec = snap(s, *t, positionSec());
        put(s, LoopEnabled, t->loopOutSec > t->loopInSec && t->loopInSec >= 0); put(s, LoopOut, 0);
        if (get(s, LoopEnabled) && positionSec() >= t->loopOutSec) seekSec(t->loopInSec);
    } else if (name == s.keys[LoopActive] && value) {
        t->loopInSec = snap(s, *t, positionSec());
        t->loopOutSec = std::min(durationSec(), t->loopInSec + std::max(.03125, get(s, LoopSize, 4)) * beat);
        put(s, LoopEnabled, t->loopOutSec > t->loopInSec); put(s, LoopActive, 0);
    } else if ((name == s.keys[LoopDouble] || name == s.keys[LoopHalve]) && value) {
        if (t->loopInSec >= 0 && t->loopOutSec > t->loopInSec) {
            const double factor = name == s.keys[LoopDouble] ? 2 : .5;
            t->loopOutSec = std::min(durationSec(), t->loopInSec + (t->loopOutSec - t->loopInSec) * factor);
            put(s, LoopSize, (t->loopOutSec - t->loopInSec) / beat);
        }
        ControlRegistry::instance().setSilent(name, 0);
    } else if ((name == s.keys[JumpForward] || name == s.keys[JumpBackward]) && value) {
        seekSec(snap(s, *t, positionSec() + (name == s.keys[JumpForward] ? 1 : -1)
                     * std::max(.03125, get(s, JumpSize, 4)) * beat));
        ControlRegistry::instance().setSilent(name, 0);
    }
    for (int i = 0; i < kNumHotcues; ++i) {
        if (name == s.clear[i] && value) {
            t->hotcues[i].positionSec = -1; registry.setSilent(s.present[i], 0); registry.setSilent(name, 0);
        } else if (name == s.set[i] && value) {
            t->hotcues[i].positionSec = snap(s, *t, positionSec());
            registry.setSilent(s.present[i], 1); registry.setSilent(name, 0);
        } else if (name == s.activate[i]) {
            if (value && t->hotcues[i].positionSec >= 0) {
                const bool hold = registry.get(s.hotcueMode) == 1;
                const bool wasPlaying = m_playing;
                s.hotcueReturn = positionSec(); s.heldHotcue = i;
                seekSec(t->hotcues[i].positionSec); play();
                s.hotcuePreview = hold; s.hotcueWasPlaying = wasPlaying;
            } else if (!value && s.heldHotcue == i) {
                if (s.hotcuePreview) {
                    seekSec(s.hotcueReturn);
                    if (!s.hotcueWasPlaying) pause();
                }
                s.heldHotcue = -1; s.hotcuePreview = false;
            }
        }
    }
}

void Deck::render(float* out, int frames, int sampleRate) {
    if (!out || frames <= 0 || sampleRate <= 0) return;
    std::fill_n(out, frames * 2, 0.0f);
    auto& s = state(m_index);
    std::lock_guard<std::recursive_mutex> lock(s.mutex);
    auto t = track(); // Only the shared_ptr copy holds the track mutex.
    if (!t || !t->audio || t->audio->isEmpty()) return;
    auto& registry = ControlRegistry::instance();
    const double tempo = tempoOf(s);
    put(s, Ratio, tempo); put(s, Bpm, bpmOf(*t) * tempo);
    const double move = get(s, Move);
    const bool reverse = get(s, Reverse) != 0;
    const bool touch = get(s, Touch) != 0;
    const bool scrubbing = move != 0 && (!m_playing || touch);
    const double scrubStep = scrubbing ? move * 60 / (bpmOf(*t) > 0 ? bpmOf(*t) : 120)
        * m_sampleRate / frames : 0;
    if (move != 0) {
        if (!scrubbing) jogBy(move);
        put(s, Move, 0);
    }
    const bool slip = (get(s, Slip) && (touch || s.scratching)) || reverse;
    if (slip && !m_slipping && m_playing) { m_slipPositionFrames = m_positionFrames; m_slipping = true; }
    if (!slip && m_slipping) {
        seekSec(m_slipPositionFrames / m_sampleRate); m_slipping = false;
    }
    const double sourceRate = double(m_sampleRate) / sampleRate;
    const double bend = std::clamp(get(s, Bend), -.5, .5);
    if (move == 0 && !touch) put(s, Bend, bend * std::exp(-double(frames) / sampleRate / .08));
    const double rate = tempo * sourceRate * (1 + bend);
    const double pitchFactor = std::pow(2.0, std::clamp(get(s, KeyShift), -12.0, 12.0) / 12);
    const bool stretch = get(s, KeyLock) || get(s, KeyShift) != 0;
    const bool iso = registry.get(s.eqCurve) == 0;
    const double eq[] = {get(s, EqLow), get(s, EqMid), get(s, EqHigh)};
    if (s.eqRate != sampleRate || s.iso != iso || !std::equal(eq, eq + 3, s.eqValues.begin())) {
        double db[3];
        for (int i = 0; i < 3; ++i) {
            const double value = std::clamp(eq[i], -1.0, 1.0);
            db[i] = value <= -1 && iso ? -60 : value * (value < 0 ? (iso ? 26 : 24) : 6);
            s.eqValues[i] = eq[i];
        }
        for (int ch = 0; ch < 2; ++ch) {
            m_eqLow[ch].setLowShelf(sampleRate, 250, db[0]);
            m_eqMid[ch].setPeaking(sampleRate, 1000, .7, db[1]);
            m_eqHigh[ch].setHighShelf(sampleRate, 4000, db[2]);
        }
        s.eqRate = sampleRate; s.iso = iso;
    }
    const double gain = std::clamp(get(s, Gain, 1), 0.0, 2.0)
        * (registry.getBool(s.autoGain) ? std::pow(10.0, t->analysis.loudnessGainDb / 20) : 1);
    std::array<double, kNumStems> stemGain{};
    bool stems = t->hasStems;
    for (int i = 0; i < kNumStems; ++i) {
        stems = stems && t->stems[i] && !t->stems[i]->isEmpty();
        stemGain[i] = registry.getBool(s.stemMute[i]) ? 0 : std::clamp(registry.get(s.stemGain[i], 1), 0.0, 1.25);
    }
    put(s, StemsReady, stems);
    double position = m_positionFrames;
    const double end = t->audio->frames();
    const double loopIn = t->loopInSec * m_sampleRate, loopOut = t->loopOutSec * m_sampleRate;
    const bool loop = get(s, LoopEnabled) && loopIn >= 0 && loopOut > loopIn && loopOut <= end;
    bool ended = false;
    const bool audible = scrubbing || (m_playing && ((!touch && !s.scratching) || reverse));
    for (int offset = 0; offset < frames && audible;) {
        if (loop && position >= loopOut) {
            position = loopIn + std::fmod(position - loopIn, loopOut - loopIn);
            m_stretcher.reset(); for (auto& st : m_stemStretcher) st.reset();
        } else if (loop && reverse && position < loopIn) {
            position = loopOut - std::fmod(loopIn - position, loopOut - loopIn);
        }
        if (position >= end || position < 0) {
            position = std::clamp(position, 0.0, end);
            ended = m_playing && !reverse && !scrubbing;
            break;
        }
        int count = std::min(blockFrames, frames - offset);
        const double step = scrubbing ? scrubStep : rate * (reverse ? -1 : 1) * (m_braking ? m_brakeFactor : 1);
        const double boundary = step < 0 ? (loop ? loopIn : 0) : (loop ? loopOut : end);
        if (step != 0) count = int(std::min<double>(count, std::max(1.0, std::ceil(std::abs((boundary - position) / step)))));
        float* dst = out + offset * 2;
        if (stretch && !scrubbing && !reverse && !touch && !s.scratching && !m_braking && step > 0) {
            const int intermediateFrames = std::max(1, int(std::ceil(count * pitchFactor)));
            m_stretcher.process(t->audio->interleaved.constData(), t->audio->frames(), position,
                                step / pitchFactor, s.pitchBuffer.data(), intermediateFrames, s.spliceOffsets.data());
            if (stems) {
                std::fill_n(s.pitchBuffer.data(), intermediateFrames * 2, 0.0f);
                for (int i = 0; i < kNumStems; ++i) {
                    m_stemStretcher[i].processWithOffset(t->stems[i]->interleaved.constData(), t->stems[i]->frames(),
                        position, step / pitchFactor, s.stemBuffer.data(), intermediateFrames, s.spliceOffsets.data());
                    for (int f = 0; f < intermediateFrames * 2; ++f)
                        s.pitchBuffer[f] += float(s.stemBuffer[f] * stemGain[i]);
                }
            }
            for (int f = 0; f < count; ++f) {
                const double at = f * pitchFactor;
                const int a = std::min(int(at), intermediateFrames - 1), b = std::min(a + 1, intermediateFrames - 1);
                const double fraction = at - a;
                for (int ch = 0; ch < 2; ++ch)
                    dst[f * 2 + ch] = float((1 - fraction) * s.pitchBuffer[a * 2 + ch]
                                           + fraction * s.pitchBuffer[b * 2 + ch]);
            }
            position += step * count;
        } else {
            for (int f = 0; f < count; ++f) {
                for (int ch = 0; ch < 2; ++ch) {
                    if (stems) for (int i = 0; i < kNumStems; ++i) dst[f * 2 + ch] += float(sample(*t->stems[i], position, ch) * stemGain[i]);
                    else dst[f * 2 + ch] = sample(*t->audio, position, ch);
                }
                if (scrubbing) position += scrubStep;
                else if (reverse || (!touch && !s.scratching))
                    position += m_braking ? rate * (reverse ? -1 : 1) * m_brakeFactor : step;
                if (m_braking) {
                    const double seconds = std::max(.05, registry.get(s.backspin, 1));
                    m_brakeFactor = std::max(0.0, m_brakeFactor - 1 / (seconds * sampleRate));
                    if (m_brakeFactor == 0) { m_playing = false; m_braking = false; break; }
                }
            }
        }
        offset += count;
        if (!m_playing && !scrubbing) break;
    }
    if (m_slipping && m_playing) m_slipPositionFrames = std::min(end, m_slipPositionFrames + frames * tempo * sourceRate);
    s.scratching = false;
    position = std::clamp(position, 0.0, end);
    m_positionFrames = position;
    if (m_playing && !loop && !reverse && !scrubbing && position >= end) ended = true;
    if (ended) { pause(); emit trackEnded(m_index); }
    const double amount = std::clamp(get(s, ColorAmount), -1.0, 1.0);
    m_colorFx.setSampleRate(sampleRate); m_colorFx.setBpm(bpmOf(*t));
    m_colorFx.setEnabled(get(s, ColorEnabled, 1) && amount != 0);
    m_colorFx.setParameter((amount + 1) * .5); m_colorFx.setMix(std::abs(amount));
    float peak = 0;
    for (int f = 0; f < frames; ++f) for (int ch = 0; ch < 2; ++ch) {
        const int i = f * 2 + ch;
        out[i] = float(m_eqHigh[ch].process(m_eqMid[ch].process(m_eqLow[ch].process(out[i]))) * gain);
    }
    m_colorFx.process(out, frames);
    for (int i = 0; i < frames * 2; ++i) peak = std::max(peak, std::abs(out[i]));
    s.peak = std::max(peak, s.peak * float(std::exp(-double(frames) / sampleRate / .3)));
    put(s, Level, s.peak); put(s, PositionSec, positionSec()); put(s, Position, position / end);
}
