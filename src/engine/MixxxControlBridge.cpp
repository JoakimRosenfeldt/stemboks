#include "MixxxControlBridge.h"
#include "../control/ControlRegistry.h"
#include "../control/AppSettings.h"
#include <coreservices.h>
#include <control/controlproxy.h>
#include <control/controlobject.h>
#include <controllers/scripting/legacy/controllerscriptinterfacelegacy.h>
#include <effects/effectsmanager.h>
#include <effects/effectchain.h>
#include <effects/effectslot.h>
#include <effects/effectparameter.h>
#include <effects/chains/quickeffectchain.h>
#include <effects/chains/equalizereffectchain.h>
#include <effects/backends/effectmanifest.h>
#include <effects/backends/effectsbackendmanager.h>
#include <mixer/basetrackplayer.h>
#include <mixer/playermanager.h>
#include <track/track.h>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QSet>
#include <limits>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <functional>
#include <array>
#include <vector>

namespace stemboks {
namespace {
QString deckGroup(int index) { return QStringLiteral("[Channel%1]").arg(index + 1); }
QString normalizedEffect(QString value) { return value.toLower().remove(QRegularExpression("[^a-z0-9]")); }
}
struct MixxxControlBridge::Impl {
    struct Binding {
        QString name;
        std::unique_ptr<::ControlProxy> proxy;
        double scale = 1;
        double offset = 0;
        bool parameter = false;
        bool event = false;
        bool writable = true;
    };
    ::mixxx::CoreServices* core;
    MixxxControlBridge* owner;
    std::vector<Binding> bindings;
    QHash<QString, int> byName;
    QTimer poll;
    std::vector<std::unique_ptr<::ControlObject>> compatibility;
    std::vector<std::unique_ptr<::ControlProxy>> navigation;
    bool publishing = false;
    QSet<int> rollingUnits;
    std::array<QSet<int>, 2> rollingDecks;
    std::array<double, 4> hotcueReturn{};
    std::array<bool, 4> hotcueWasPlaying{};
    std::array<int, 4> heldHotcue{{-1, -1, -1, -1}};
    ::RuntimeLoggingCategory logger{QStringLiteral("stemboks.controls")};
    ::ControllerScriptInterfaceLegacy scratch{nullptr, logger};

    Impl(::mixxx::CoreServices* services, MixxxControlBridge* parent) : core(services), owner(parent) {}
    void bind(const QString& name, const QString& group, const QString& item,
            double scale = 1, double offset = 0, bool parameter = false, bool event = false, bool writable = true) {
        auto proxy = std::make_unique<::ControlProxy>(::ConfigKey(group, item), owner, ::ControlFlag::AllowMissingOrInvalid);
        if (!proxy->valid()) {
            qWarning() << "Mixxx control unavailable:" << name << group << item;
            return;
        }
        byName.insert(name, int(bindings.size()));
        bindings.push_back({name, std::move(proxy), scale, offset, parameter, event, writable});
    }
    void pulse(const QString& name) {
        auto& registry = ControlRegistry::instance(); registry.set(name, 0); registry.set(name, 1); registry.set(name, 0);
    }
    void setNative(const QString& group, const QString& item, double value) {
        ::ControlProxy proxy(::ConfigKey(group, item), nullptr, ::ControlFlag::AllowMissingOrInvalid);
        if (proxy.valid()) proxy.set(value);
    }
    void loadEffect(const ::EffectChainPointer& chain, const QString& requested) {
        if (!chain) return;
        QString wanted = normalizedEffect(requested);
        static const QHash<QString, QString> aliases{{"noise", "whitenoise"}, {"crush", "bitcrusher"}, {"pitch", "pitchshift"},
                {"space", "reverb"}, {"dub", "echo"}, {"decho", "echo"}, {"trans", "tremolo"}};
        wanted = aliases.value(wanted, wanted);
        if (wanted == "roll") {
            for (const auto& slot : chain->getEffectSlots()) slot->setEnabled(false);
            return;
        }
        ::EffectManifestPointer manifest;
        for (const auto& candidate : core->getEffectsManager()->getBackendManager()->getManifests()) {
            const QString id = normalizedEffect(candidate->id().section('.', -1));
            if (id == wanted || normalizedEffect(candidate->name()) == wanted) { manifest = candidate; break; }
        }
        if (!manifest) {
            chain->getEffectSlot(0)->setEnabled(false);
            qWarning() << "No Mixxx effect for" << requested;
            ControlRegistry::instance().setString(QStringLiteral("app.engineStatus"), QStringLiteral("Effect unavailable: ") + requested);
            return;
        }
        ControlRegistry::instance().setString(QStringLiteral("app.engineStatus"),
                wanted == normalizedEffect(requested) ? QString() : QStringLiteral("%1 uses Mixxx %2").arg(requested.toUpper(), manifest->name()));
        if (auto slot = chain->getEffectSlot(0)) { slot->loadEffectWithDefaults(manifest); slot->setEnabled(true); }
        for (int i = 1; i < chain->getEffectSlots().size(); ++i) chain->getEffectSlot(i)->setEnabled(false);
    }
    void pad(int deck, int pad, bool pressed) {
        auto& r = ControlRegistry::instance(); const QString prefix = QStringLiteral("deck.%1.").arg(deck + 1);
        const int mode = int(r.get(prefix + "pads.mode"));
        if (mode == 0) {
            const QString cue = prefix + QStringLiteral("hotcue.%1.").arg(pad + 1);
            if (!pressed) r.set(cue + "activate", 0);
            else if (!r.getBool(cue + "present")) pulse(cue + "set");
            else r.set(cue + "activate", 1);
        } else if (mode == 1 && pressed) {
            const double sizes[]{.5, 1, 2, 4, 8, 16, 32};
            const QString group = deckGroup(deck);
            if (pad == 7) {
                const double start = ::ControlObject::get(::ConfigKey(group, "loop_start_position"));
                const double end = ::ControlObject::get(::ConfigKey(group, "loop_end_position"));
                pulse(prefix + (start >= 0 && end < 0 ? "loop.setOut" : "loop.setIn"));
            } else {
                const bool same = ::ControlObject::get(::ConfigKey(group, "loop_enabled")) != 0
                        && ::ControlObject::get(::ConfigKey(group, "beatloop_size")) == sizes[pad];
                setNative(group, "beatloop_size", sizes[pad]);
                r.setSilent(prefix + "loop.size", sizes[pad]);
                if (same) {
                    setNative(group, "loop_enabled", 0);
                    r.setSilent(prefix + "loop.enabled", 0);
                } else pulse(prefix + "loop.active");
            }
        } else if (mode == 2 && pressed) {
            r.set(prefix + "jump.size", 4 << (pad / 2)); pulse(prefix + (pad % 2 ? "jump.forward" : "jump.backward"));
        } else if (mode == 3 && pressed) {
            const int shifts[]{-7, -5, -3, -1, 1, 3, 5, 7};
            r.set(prefix + "key.shift", r.get(prefix + "key.shift") == shifts[pad] ? 0 : shifts[pad]);
        } else if (mode == 4 && pressed) {
            const QStringList names{"vocals", "drums", "bass", "other"};
            if (pad < 4) r.toggle(prefix + "stems." + names[pad] + ".mute");
            else {
                const int solo = pad - 4; bool restore = !r.getBool(prefix + "stems." + names[solo] + ".mute");
                for (int i = 0; i < 4; ++i) if (i != solo && !r.getBool(prefix + "stems." + names[i] + ".mute")) restore = false;
                for (int i = 0; i < 4; ++i) r.set(prefix + "stems." + names[i] + ".mute", !restore && i != solo);
            }
        } else if (mode == 5 && pressed) r.toggle(QStringLiteral("sampler.%1.play").arg(pad + 1));
        else if (mode == 6) {
            const int unit = deck % 2 + 1; const QString effectPrefix = QStringLiteral("fx.unit.%1.").arg(unit);
            const bool toggle = r.getBool("app.padfxType");
            if (!pressed) { if (!toggle) r.set(effectPrefix + "enabled", 0); return; }
            auto& settings = AppSettings::instance();
            const QString effect = settings.get("[Stembox]", QStringLiteral("padfx_name_%1").arg(pad + 1), "ECHO");
            if (normalizedEffect(effect) == "vinylbrake") { pulse(prefix + "transport.brake"); return; }
            r.setString(effectPrefix + "name", effect);
            r.set(effectPrefix + "parameter", settings.getDouble("[Stembox]", QStringLiteral("padfx_bval_%1").arg(pad + 1), .5));
            r.set(effectPrefix + "mix", 1); r.set(prefix + QStringLiteral("fx.unit%1").arg(unit), 1);
            r.set(prefix + "fx.enabled", 1);
            if (toggle) r.toggle(effectPrefix + "enabled"); else r.set(effectPrefix + "enabled", 1);
        }
    }
    void updateRoll(int unit) {
        auto& registry = ControlRegistry::instance();
        const QString prefix = QStringLiteral("fx.unit.%1.").arg(unit);
        const bool rolling = normalizedEffect(registry.getString(prefix + "name")) == "roll" && registry.getBool(prefix + "enabled");
        const double beats = std::clamp(registry.get(prefix + "parameter", .5), .03125, 16.0);
        QSet<int> next;
        for (int deck = 0; deck < 4; ++deck) if (rolling && registry.getBool(deckKey(deck, unit == 1 ? "fx.unit1" : "fx.unit2"))) next.insert(deck);
        auto& previous = rollingDecks[unit - 1];
        for (int deck : previous) if (!next.contains(deck)) setNative(deckGroup(deck), "beatlooproll_activate", 0);
        for (int deck : next) {
            setNative(deckGroup(deck), "beatloop_size", beats);
            if (!previous.contains(deck)) setNative(deckGroup(deck), "beatlooproll_activate", 1);
        }
        previous = next;
        if (rolling) rollingUnits.insert(unit); else rollingUnits.remove(unit);
    }
    void changed(const QString& name, double value) {
        auto& registry = ControlRegistry::instance();
        static const QRegularExpression transport("^deck\\.([1-4])\\.transport\\.playing$");
        const auto play = transport.match(name);
        if (play.hasMatch() && registry.getBool("app.vinylStartStop")) {
            const int deck = play.captured(1).toInt();
            if (value != 0) scratch.softStart(deck, true);
            else scratch.brake(deck, true, std::max(.1, registry.get("app.backspin", 4)));
            return;
        }
        static const QRegularExpression effectSelect("^fx\\.unit\\.([12])\\.select\\.(.+)$");
        const auto selection = effectSelect.match(name);
        if (selection.hasMatch() && value != 0) {
            registry.setString(QStringLiteral("fx.unit.%1.name").arg(selection.captured(1)), selection.captured(2));
            registry.setSilent(name, 0); return;
        }
        static const QRegularExpression hotcueActivate("^deck\\.([1-4])\\.hotcue\\.([1-8])\\.activate$");
        const auto hotcue = hotcueActivate.match(name);
        if (hotcue.hasMatch()) {
            const int deck = hotcue.captured(1).toInt() - 1, cue = hotcue.captured(2).toInt();
            const QString group = deckGroup(deck);
            if (value != 0) {
                if (registry.getBool("app.hotcueMode")) {
                    hotcueReturn[deck] = ::ControlObject::get(::ConfigKey(group, "playposition"));
                    hotcueWasPlaying[deck] = ::ControlObject::get(::ConfigKey(group, "play")) != 0;
                    heldHotcue[deck] = cue;
                }
                const QString item = QStringLiteral("hotcue_%1_gotoandplay").arg(cue);
                setNative(group, item, 0); setNative(group, item, 1); setNative(group, item, 0);
            } else if (heldHotcue[deck] == cue) {
                setNative(group, "playposition", hotcueReturn[deck]);
                setNative(group, "play", hotcueWasPlaying[deck]); heldHotcue[deck] = -1;
            }
            return;
        }
        static const QRegularExpression effectUnit("^fx\\.unit\\.([12])\\.(enabled|parameter)$");
        const auto unitMatch = effectUnit.match(name);
        if (unitMatch.hasMatch()) {
            const int unit = unitMatch.captured(1).toInt();
            if (normalizedEffect(registry.getString(QStringLiteral("fx.unit.%1.name").arg(unit))) == "roll") {
                setNative(QStringLiteral("[EffectRack1_EffectUnit%1]").arg(unit), "enabled", 0);
                updateRoll(unit);
                return;
            }
        }
        if (name == "app.eqSensitivity") {
            for (int deck = 0; deck < 4; ++deck) for (const auto* band : {"low", "mid", "high"}) {
                const QString key = QStringLiteral("deck.%1.eq.%2").arg(deck + 1).arg(QString::fromLatin1(band));
                changed(key, registry.get(key));
            }
            return;
        }
        auto it = byName.constFind(name);
        if (it != byName.constEnd()) {
            auto& binding = bindings[size_t(it.value())];
            if (!binding.writable) return;
            if (binding.event) {
                if (value != 0) { binding.proxy->set(0); binding.proxy->set(value); binding.proxy->set(0); }
                registry.setSilent(name, 0);
            } else {
                const double sensitivity = name.startsWith("deck.") && name.contains(".eq.")
                        ? std::clamp(registry.get("app.eqSensitivity", 100) / 100, .01, 2.0) : 1.0;
                const double native = value * sensitivity * binding.scale + binding.offset;
                if (binding.parameter) binding.proxy->setParameter(native); else binding.proxy->set(native);
            }
            return;
        }
        if (name == "app.tempoInvert") {
            for (int i = 0; i < 4; ++i) setNative(deckGroup(i), "rate_dir", value != 0 ? -1 : 1);
        } else if (name == "app.xfaderDisable") {
            for (int i = 0; i < 4; ++i) setNative(deckGroup(i), "orientation", value != 0 ? 1 : (i % 2 ? 2 : 0));
        } else if (name == "app.loadLock") {
            core->getSettings()->set(::ConfigKey("[Controls]", "AllowTrackLoadToPlayingDeck"), ::ConfigValue(value != 0 ? 0 : 1));
            core->getSettings()->set(::ConfigKey("[Controls]", "LoadWhenDeckPlaying"), ::ConfigValue(value != 0 ? 0 : 1));
        } else if (name == "app.eqCurve") {
            const QString effect = value == 0 ? "org.mixxx.effects.biquadfullkilleq" : "org.mixxx.effects.bessel4lvmixeq";
            const auto manifest = core->getEffectsManager()->getBackendManager()->getManifest(effect, ::EffectBackendType::BuiltIn);
            if (manifest) for (int i = 0; i < 4; ++i)
                core->getEffectsManager()->getEqualizerEffectChain(deckGroup(i))->getEffectSlot(0)->loadEffectWithDefaults(manifest);
        } else {
            static const QRegularExpression deckPattern("^deck\\.([1-4])\\.(.+)$");
            const auto match = deckPattern.match(name); if (!match.hasMatch()) return;
            const int deck = match.captured(1).toInt() - 1; const QString suffix = match.captured(2), group = deckGroup(deck);
            static const QHash<QString, QString> aliases{{"cue.button", "transport.cue"}, {"tempo.keyLock", "key.lock"},
                    {"scratch.wheelTouch", "jog.touch"}, {"scratch.wheelMove", "jog.move"}, {"colorFx.amount", "colorfx.amount"}};
            if (aliases.contains(suffix)) { registry.set(deckKey(deck, qPrintable(aliases.value(suffix))), value); return; }
            if (suffix.startsWith("pads.cell.") && suffix.endsWith(".press")) {
                const int index = suffix.section('.', 2, 2).toInt() - 1; if (index >= 0 && index < 8) pad(deck, index, value != 0);
            } else if (suffix == "positionSec") {
                if (auto player = core->getPlayerManager()->getDeckBase(deck); player && player->getLoadedTrack()) {
                    const double duration = player->getLoadedTrack()->getDuration();
                    if (duration > 0) setNative(group, "playposition", std::clamp(value / duration, 0.0, 1.0));
                }
            } else if (suffix == "loop.activeLoop" && value != 0) {
                if (registry.getBool(deckKey(deck, "loop.enabled"))) registry.set(deckKey(deck, "loop.enabled"), 0);
                else pulse(deckKey(deck, "loop.active"));
            } else if ((suffix == "jump.fourForward" || suffix == "jump.fourBack") && value != 0)
                setNative(group, "beatjump", suffix == "jump.fourForward" ? 4 : -4);
            else if (suffix == "jog.touch") {
                if (value != 0) scratch.scratchEnable(deck + 1, 128, 33.333333, .125, .125 / 32);
                else scratch.scratchDisable(deck + 1);
            } else if (suffix == "jog.move") {
                if (scratch.isScratching(deck + 1)) scratch.scratchTick(deck + 1, int(std::round(value * 128)));
                else setNative(group, "jog", value);
                registry.setSilent(name, 0);
            } else if (suffix == "pitchbend") {
                setNative(group, "rate_temp_up", value > 0); setNative(group, "rate_temp_down", value < 0);
            } else if (suffix == "transport.brake" && value != 0) {
                scratch.brake(deck + 1, true, std::max(.1, registry.get("app.backspin", 4)));
                registry.setSilent(name, 0);
            } else if (suffix == "fx.enabled") {
                for (int unit = 0; unit < 2; ++unit)
                    setNative(QStringLiteral("[EffectRack1_EffectUnit%1]").arg(unit + 1),
                            QStringLiteral("group_%1_enable").arg(group), value != 0 && registry.getBool(deckKey(deck, unit == 0 ? "fx.unit1" : "fx.unit2")));
            } else if (suffix.startsWith("stems.")) {
                const QString stemName = suffix.section('.', 1, 1); const QString parameter = suffix.section('.', 2, 2);
                auto player = core->getPlayerManager()->getDeckBase(deck); auto track = player ? player->getLoadedTrack() : ::TrackPointer();
#ifdef __STEM__
                if (track) {
                    const auto stems = track->getStemInfo();
                    for (int i = 0; i < stems.size(); ++i) if (normalizedEffect(stems[i].getLabel()) == stemName) {
                        setNative(QStringLiteral("[Channel%1_Stem%2]").arg(deck + 1).arg(i + 1), parameter == "gain" ? "volume" : "mute", value); return;
                    }
                    registry.setString(deckKey(deck, "load.status"), QStringLiteral("Stem label unavailable: ") + stemName);
                }
#endif
            }
        }
    }
};

MixxxControlBridge::MixxxControlBridge(::mixxx::CoreServices* core, QObject* parent)
        : QObject(parent), m_impl(std::make_unique<Impl>(core, this)) {
    auto& impl = *m_impl;
    auto& registry = ControlRegistry::instance();
    auto* effects = core->getEffectsManager().get();
    double rangePercent = core->getSettings()->getValue<double>(::ConfigKey("[Controls]", "RateRangePercent"), 6);
    if (!std::isfinite(rangePercent) || rangePercent < 1 || rangePercent > 100) rangePercent = 6;
    core->getSettings()->set(::ConfigKey("[Controls]", "RateRangePercent"), ::ConfigValue(rangePercent));
    for (int i = 0; i < 4; ++i) {
        const QString group = deckGroup(i), prefix = QStringLiteral("deck.%1.").arg(i + 1);
        const std::pair<const char*, const char*> direct[]{
            {"transport.playing", "play"}, {"transport.cue", "cue_default"}, {"transport.reverseSlip", "reverseroll"},
            {"tempo.fader", "rate"}, {"key.lock", "keylock"}, {"key.shift", "pitch"},
            {"sync.enabled", "sync_enabled"}, {"grid.quantize", "quantize"}, {"slip.enabled", "slip_enabled"},
            {"loop.enabled", "loop_enabled"}, {"loop.size", "beatloop_size"}, {"jump.size", "beatjump_size"},
            {"position", "playposition"}, {"level.gain", "pregain"}, {"level.volume", "volume"}, {"level.pfl", "pfl"}};
        for (const auto& [name, item] : direct) impl.bind(prefix + name, group, item);
        impl.bind(prefix + "tempo.range", group, "rateRange", .01);
        impl.bind(prefix + "tempo.ratio", group, "rate_ratio", 1, 0, false, false, false);
        impl.bind(prefix + "bpm", group, "bpm", 1, 0, false, false, false);
        impl.bind(prefix + "level", group, "vu_meter", 1, 0, false, false, false);
        const std::pair<const char*, const char*> events[]{
            {"loop.setIn", "loop_in"}, {"loop.setOut", "loop_out"}, {"loop.double", "loop_double"},
            {"loop.halve", "loop_halve"}, {"loop.active", "beatloop_activate"}, {"loop.reloop", "reloop_toggle"},
            {"jump.forward", "beatjump_forward"}, {"jump.backward", "beatjump_backward"}};
        for (const auto& [name, item] : events) impl.bind(prefix + name, group, item, 1, 0, false, true);
        const char* bands[]{"low", "mid", "high"};
        for (int band = 0; band < 3; ++band) impl.bind(prefix + "eq." + bands[band],
                ::EqualizerEffectChain::formatEffectSlotGroup(group), QStringLiteral("parameter%1").arg(band + 1), .5, .5, true);
        const QString quick = ::QuickEffectChain::formatEffectChainGroup(group);
        impl.bind(prefix + "colorfx.amount", quick, "super1", .5, .5);
        impl.bind(prefix + "colorfx.enabled", quick, "enabled");
        for (int cue = 1; cue <= 8; ++cue) {
            const QString cuePrefix = prefix + QStringLiteral("hotcue.%1.").arg(cue);
            impl.bind(cuePrefix + "set", group, QStringLiteral("hotcue_%1_set").arg(cue), 1, 0, false, true);
            impl.bind(cuePrefix + "clear", group, QStringLiteral("hotcue_%1_clear").arg(cue), 1, 0, false, true);
            impl.bind(cuePrefix + "activate", group, QStringLiteral("hotcue_%1_activate").arg(cue));
            impl.bind(cuePrefix + "present", group, QStringLiteral("hotcue_%1_status").arg(cue), 1, 0, false, false, false);
        }
        for (int unit = 1; unit <= 2; ++unit) impl.bind(prefix + QStringLiteral("fx.unit%1").arg(unit),
                QStringLiteral("[EffectRack1_EffectUnit%1]").arg(unit), QStringLiteral("group_%1_enable").arg(group));
        impl.setNative(group, "rate_dir", registry.getBool("app.tempoInvert") ? -1 : 1);
        // Native Mixxx initializes these in its deck preferences constructor.
        impl.setNative(group, "rateRange", rangePercent / 100);
        impl.setNative(group, "rate_ratio", 1);
        impl.setNative(group, "rate", 0);
        registry.setSilent(prefix + "tempo.range", rangePercent);
        registry.setSilent(prefix + "tempo.ratio", 1);
        registry.setSilent(prefix + "tempo.fader", 0);
        impl.setNative(group, "orientation", registry.getBool("app.xfaderDisable") ? 1 : (i % 2 ? 2 : 0));
    }
    impl.bind("mixer.crossfader.position", "[Master]", "crossfader");
    impl.bind("mixer.master.gain", "[Master]", "gain");
    impl.bind("mixer.head.gain", "[Master]", "headGain");
    impl.bind("mixer.head.mix", "[Master]", "headMix", 2, -1);
    impl.bind("app.autoGain", "[ReplayGain]", "ReplayGainEnabled");
    impl.bind("app.xfaderMode", "[Mixer Profile]", "xFaderMode");
    impl.bind("app.xfaderCurve", "[Mixer Profile]", "xFaderCurve");
    impl.bind("app.xfaderReverse", "[Mixer Profile]", "xFaderReverse");
    for (int slot = 1; slot <= 8; ++slot) {
        const QString group = QStringLiteral("[Sampler%1]").arg(slot), prefix = QStringLiteral("sampler.%1.").arg(slot);
        impl.bind(prefix + "play", group, "play"); impl.bind(prefix + "gain", group, "pregain");
    }
    for (int unit = 1; unit <= 2; ++unit) {
        const QString group = QStringLiteral("[EffectRack1_EffectUnit%1]").arg(unit), prefix = QStringLiteral("fx.unit.%1.").arg(unit);
        impl.loadEffect(effects->getStandardEffectChain(unit - 1), registry.getString(prefix + "name", "echo"));
        impl.bind(prefix + "enabled", group, "enabled"); impl.bind(prefix + "mix", group, "mix");
        impl.bind(prefix + "parameter", group, "super1");
    }
    if (const auto output = effects->getOutputEffectChain()) {
        impl.loadEffect(output, "graphiceq");
        for (int band = 1; band <= 8; ++band) impl.bind(QStringLiteral("mixer.eq.%1").arg(band),
                output->getEffectSlot(0)->getGroup(), QStringLiteral("parameter%1").arg(band), 1.0 / 24, .5, true);
        if (output->getEffectSlots().size() > 1) {
            const auto limiter = output->getEffectSlot(1);
            const auto compressor = effects->getBackendManager()->getManifest("org.mixxx.effects.compressor", ::EffectBackendType::BuiltIn);
            if (compressor) {
                limiter->loadEffectWithDefaults(compressor);
                const QHash<QString, double> settings{{"threshold", -.2}, {"ratio", 1000}, {"knee", 0},
                        {"attack", 0}, {"release", 100}, {"level", 0}, {"automakeup", 0}};
                for (const auto& parameters : limiter->getLoadedParameters()) for (const auto& parameter : parameters) {
                    const auto id = parameter->manifest()->id();
                    if (settings.contains(id)) parameter->setValue(settings.value(id));
                }
                limiter->setEnabled(registry.getBool("app.masterLimiter", true));
                impl.bind("app.masterLimiter", limiter->getGroup(), "enabled");
            }
        }
    }
    QSet<QString> customNames;
    const QRegularExpression mappingControl(R"rx(^(?:input|output)\s+"[^"]*"\s+"([^"]+)")rx");
    const QRegularExpression effectControl(R"rx(^effect-selection\s+"[^"]*"\s+"fx\.unit\.([12])\.name"\s+"([^"]+)")rx");
    for (const auto& fileName : QDir(":/mappings").entryList({"*.mapping"}, QDir::Files)) {
        QFile file(QStringLiteral(":/mappings/") + fileName);
        if (!file.open(QIODevice::ReadOnly)) continue;
        for (const auto& line : QString::fromUtf8(file.readAll()).split('\n')) {
            auto match = mappingControl.match(line);
            if (match.hasMatch()) customNames.insert(match.captured(1));
            match = effectControl.match(line);
            if (match.hasMatch()) customNames.insert(QStringLiteral("fx.unit.%1.select.%2").arg(match.captured(1), match.captured(2)));
        }
    }
    for (const auto& name : customNames) {
        auto control = std::make_unique<::ControlObject>(::ConfigKey("[Stemboks]", name), false);
        control->set(registry.get(name));
        connect(control.get(), &::ControlObject::valueChanged, this, [this, name](double value) {
            if (m_impl->publishing) return;
            auto& registry = ControlRegistry::instance();
            registry.setSilent(name, std::numeric_limits<double>::quiet_NaN());
            registry.set(name, value);
        });
        impl.compatibility.push_back(std::move(control));
    }
    auto navigation = [this](const QString& group, const QString& native, const QString& dotted) {
        auto proxy = std::make_unique<::ControlProxy>(::ConfigKey(group, native), this, ::ControlFlag::AllowMissingOrInvalid);
        if (!proxy->valid()) return;
        proxy->connectValueChanged(this, [dotted](double value) {
            if (value == 0) return;
            auto& registry = ControlRegistry::instance();
            registry.setSilent(dotted, 0); registry.set(dotted, value); registry.setSilent(dotted, 0);
        });
        m_impl->navigation.push_back(std::move(proxy));
    };
    navigation("[Library]", "MoveVertical", "library.moveVertical");
    navigation("[Library]", "MoveFocusForward", "library.moveFocusForward");
    navigation("[Library]", "MoveFocusBackward", "library.moveFocusBackward");
    for (int i = 0; i < 4; ++i) {
        navigation(deckGroup(i), "LoadSelectedTrack", QStringLiteral("library.loadDeck.%1").arg(i + 1));
        navigation(deckGroup(i), "LoadSelectedTrackAndPlay", QStringLiteral("library.loadPlayDeck.%1").arg(i + 1));
    }
    connect(&registry, &ControlRegistry::changed, this, [this](const QString& name, double value) { m_impl->changed(name, value); });
    connect(&registry, &ControlRegistry::stringChanged, this, [this](const QString& name, const QString& value) {
        static const QRegularExpression standard("^fx\\.unit\\.([12])\\.name$");
        static const QRegularExpression quick("^deck\\.([1-4])\\.colorfx\\.name$");
        auto match = standard.match(name);
        if (match.hasMatch()) {
            const int unit = match.captured(1).toInt();
            if (m_impl->rollingUnits.remove(unit)) for (int deck : m_impl->rollingDecks[unit - 1])
                m_impl->setNative(deckGroup(deck), "beatlooproll_activate", 0);
            m_impl->rollingDecks[unit - 1].clear();
            const auto chain = m_impl->core->getEffectsManager()->getStandardEffectChain(unit - 1);
            if (normalizedEffect(value) == "roll") {
                for (const auto& slot : chain->getEffectSlots()) slot->setEnabled(false);
            } else m_impl->loadEffect(chain, value);
        }
        else if ((match = quick.match(name)).hasMatch()) m_impl->loadEffect(m_impl->core->getEffectsManager()->getQuickEffectChain(deckGroup(match.captured(1).toInt() - 1)), value);
    });
    impl.poll.setInterval(20);
    connect(&impl.poll, &QTimer::timeout, this, [this] {
        auto& registry = ControlRegistry::instance();
        for (const auto& binding : m_impl->bindings) if (!binding.event) {
            if ((binding.name == "fx.unit.1.enabled" || binding.name == "fx.unit.1.parameter")
                    && normalizedEffect(registry.getString("fx.unit.1.name")) == "roll") continue;
            if ((binding.name == "fx.unit.2.enabled" || binding.name == "fx.unit.2.parameter")
                    && normalizedEffect(registry.getString("fx.unit.2.name")) == "roll") continue;
            double value = binding.parameter ? binding.proxy->getParameter() : binding.proxy->get();
            if (binding.name.endsWith(".present")) value = value > 0 ? 1 : 0;
            const double sensitivity = binding.name.startsWith("deck.") && binding.name.contains(".eq.")
                    ? std::clamp(registry.get("app.eqSensitivity", 100) / 100, .01, 2.0) : 1.0;
            registry.setSilent(binding.name, (value - binding.offset) / (binding.scale * sensitivity));
        }
        m_impl->updateRoll(1); m_impl->updateRoll(2);
        m_impl->publishing = true;
        for (const auto& control : m_impl->compatibility) {
            const double value = registry.get(control->getKey().item);
            if (control->get() != value) control->set(value);
        }
        m_impl->publishing = false;
#ifdef __STEM__
        for (int deck = 0; deck < 4; ++deck) {
            auto* player = m_impl->core->getPlayerManager()->getDeckBase(deck);
            const auto track = player ? player->getLoadedTrack() : ::TrackPointer();
            if (!track) continue;
            const auto stems = track->getStemInfo();
            for (int stem = 0; stem < stems.size(); ++stem) {
                const QString name = normalizedEffect(stems[stem].getLabel());
                if (name != "drums" && name != "bass" && name != "other" && name != "vocals") continue;
                for (const auto& parameter : {QStringLiteral("volume"), QStringLiteral("mute")}) {
                    ::ControlProxy proxy(::ConfigKey(QStringLiteral("[Channel%1_Stem%2]").arg(deck + 1).arg(stem + 1), parameter));
                    registry.setSilent(QStringLiteral("deck.%1.stems.%2.%3").arg(deck + 1).arg(name, parameter == "volume" ? "gain" : "mute"), proxy.get());
                }
            }
        }
#endif
    });
    impl.poll.start();
}
MixxxControlBridge::~MixxxControlBridge() = default;
} // namespace stemboks
