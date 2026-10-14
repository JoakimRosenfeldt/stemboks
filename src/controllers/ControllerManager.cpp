#include <QHash>
#include "ControllerManager.h"
#include "../control/AppSettings.h"
#include "../control/ControlRegistry.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QMetaObject>
#include <QRegularExpression>
#include <QTimer>
#include <QThread>
#include <RtMidi.h>
#if __has_include(<hidapi/hidapi.h>)
#include <hidapi/hidapi.h>
#else
#include <hidapi.h>
#endif
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace {
bool tokenize(const QString& line, QStringList* tokens) {
    QString token; bool quoted = false, escaped = false, started = false;
    for (const QChar c : line) {
        if (escaped) { token += c == 'n' ? QChar('\n') : c == 't' ? QChar('\t') : c; escaped = false; continue; }
        if (quoted && c == '\\') { escaped = true; continue; }
        if (c == '"') { quoted = !quoted; started = true; continue; }
        if (!quoted && c == '#') break;
        if (!quoted && c.isSpace()) {
            if (started) { tokens->append(token); token.clear(); started = false; }
        } else { token += c; started = true; }
    }
    if (quoted || escaped) return false;
    if (started) tokens->append(token);
    return true;
}
bool integer(const QString& token, int* out) {
    bool ok; *out = token.toInt(&ok, token.startsWith("0x", Qt::CaseInsensitive) ? 16 : 10); return ok;
}
bool number(const QString& token, double* out) { bool ok; *out = token.toDouble(&ok); return ok && std::isfinite(*out); }
bool source(const QStringList& t, int at, MappingBinding* b) {
    if (at < 0 || at + 2 >= t.size()) return false;
    const QString type = t[at];
    if (type == "midi-note") b->source = MappingBinding::MidiNote;
    else if (type == "midi-cc") b->source = MappingBinding::MidiCC;
    else if (type == "midi-pitchbend" || type == "midi-pitch-bend") b->source = MappingBinding::MidiPitchBend;
    else if (type == "hid-report") b->source = MappingBinding::HidReport;
    else return false;
    if (!integer(t[at + 1], &b->channel) || !integer(t[at + 2], &b->number)) return false;
    if (b->source == MappingBinding::HidReport) {
        b->bit = -1;
        if (at + 3 < t.size() && !integer(t[at + 3], &b->bit)) return false;
        return b->channel >= 0 && b->channel <= 255 && b->number >= 0 && b->number < 4096 && b->bit >= -1 && b->bit < 8;
    }
    return b->channel >= 0 && b->channel < 16 && b->number >= 0 && b->number < 128;
}
int sourceIndex(const QStringList& t) {
    for (int i = 0; i < t.size(); ++i)
        if (t[i].startsWith("midi-") || t[i] == "hid-report") return i;
    return -1;
}
}

struct ControllerManager::Impl {
    struct MidiPort {
        ControllerManager* owner;
        int mapping;
        std::unique_ptr<RtMidiIn> input;
        std::unique_ptr<RtMidiOut> output;
        QHash<int, double> lastSent;
    };
    struct HidPort { hid_device* device; int mapping; QByteArray previous; };
    QVector<ControllerMapping> mappings;
    std::vector<std::unique_ptr<MidiPort>> midi;
    QVector<HidPort> hid;
    int dispatchMapping = -1;
    QThread* hidThread = nullptr;
    QTimer* feedbackTimer = nullptr;
    ~Impl() {
        delete feedbackTimer;
        if (hidThread) { hidThread->quit(); hidThread->wait(); delete hidThread; }
        for (auto& p : midi) if (p->input) p->input->cancelCallback();
        for (const auto& p : hid) hid_close(p.device);
        hid_exit();
    }
};

ControllerManager& ControllerManager::instance() { static ControllerManager manager; return manager; }

bool ControllerManager::parseMapping(const QString& text, ControllerMapping* out, QString* error) {
    if (error) error->clear();
    if (!out) { if (error) *error = "Missing mapping destination"; return false; }
    ControllerMapping mapping; bool header = false, device = false;
    int lineNo = 0;
    auto fail = [&](const QString& message) { if (error) *error = QString("Line %1: %2").arg(lineNo).arg(message); return false; };
    for (const auto& line : text.split('\n')) {
        ++lineNo; QStringList t;
        if (!tokenize(line, &t)) return fail("Unbalanced quoted string");
        if (t.isEmpty()) continue;
        if (!header) {
            if (t != QStringList{"SBX_CONTROLLER_MAPPING", "6"}) return fail("Expected SBX_CONTROLLER_MAPPING 6");
            header = true; continue;
        }
        const QString keyword = t[0];
        if (keyword == "id" || keyword == "name") {
            if (t.size() != 2) return fail("Expected one quoted value");
            if (keyword == "id") mapping.id = t[1]; else mapping.name = t[1];
            continue;
        }
        if (keyword == "device") {
            if (t.size() != 5 || (t[1] != "midi" && t[1] != "hid") || !integer(t[2], &mapping.vid) || !integer(t[3], &mapping.pid)) return fail("Invalid device");
            mapping.hid = t[1] == "hid"; mapping.deviceName = t[4]; device = true;
            if (mapping.vid < 0 || mapping.vid > 65535 || mapping.pid < 0 || mapping.pid > 65535) return fail("Invalid VID/PID");
            continue;
        }
        if (keyword != "input" && keyword != "output" && keyword != "modifier" && keyword != "effect-selection") {
            qWarning().noquote() << "Skipping unsupported mapping record at line" << lineNo << keyword;
            continue;
        }
        if (t.size() < 4) return fail("Incomplete binding");
        MappingBinding b; b.id = t[1]; b.control = t[2]; b.rawLine = line;
        if (keyword == "output") {
            b.kind = MappingBinding::Output;
            // v6 byte templates: transport, modifier, scale, ranges, byte count, three byte expressions.
            if (t.size() == 16 && t[3] == "midi" && (t[5] == "scale" || t[5] == "threshold") && t[9] == "3" && t[10] == "literal" && t[12] == "literal" && t[14] == "value7") {
                int status;
                if (!integer(t[11], &status) || !integer(t[13], &b.number) || !number(t[6], &b.inMin) || !number(t[7], &b.inMax) || !number(t[8], &b.outMin) || !number(t[15], &b.outMax)) return fail("Invalid output byte template");
                // Template values are status, number, scaled value; the last value7 argument is a flag.
                b.outMin = 0; b.outMax = 127;
                b.mode = t[5] == "threshold" ? MappingBinding::Toggle : MappingBinding::Absolute;
                if ((status & 0xf0) == 0x90 || (status & 0xf0) == 0x80) b.source = MappingBinding::MidiNote;
                else if ((status & 0xf0) == 0xb0) b.source = MappingBinding::MidiCC;
                else return fail("Unsupported output MIDI status");
                b.channel = status & 15;
                if (t[4] != "none") b.modifier = t[4];
                if (b.number < 0 || b.number > 127) return fail("Invalid output number");
            } else {
                if (!source(t, sourceIndex(t), &b)) return fail("Invalid output source");
            }
        } else if (keyword == "modifier") {
            b.kind = MappingBinding::Modifier; b.mode = MappingBinding::Momentary;
            if (t[3] != "held" || !source(t, sourceIndex(t), &b)) return fail("Invalid held modifier");
        } else if (keyword == "effect-selection") {
            b.kind = MappingBinding::EffectSelection; b.effectName = t[3]; b.offOnRelease = t.contains("off-on-release");
            if (!source(t, sourceIndex(t), &b)) return fail("Invalid effect source");
            if (!QRegularExpression("^fx\\.unit\\.[1-2]\\.(name|slot\\.[1-4])$").match(b.control).hasMatch()) return fail("Invalid effect unit");
        } else {
            b.kind = MappingBinding::Input;
            if (t.size() < 11 || t[5] != "linear") return fail("Invalid input fields");
            const QString action = t[3], mode = t[4];
            if (action != "write" && action != "toggle" && action != "increment" && action != "trigger" && action != "write-literal") {
                qWarning() << "Skipping unsupported input action" << action << "at line" << lineNo; continue;
            }
            if (mode == "absolute") b.mode = MappingBinding::Absolute;
            else if (mode == "relative" || mode == "binary-offset") b.mode = MappingBinding::Relative;
            else if (mode == "toggle") b.mode = MappingBinding::Toggle;
            else if (mode == "momentary") b.mode = MappingBinding::Momentary;
            else if (mode == "trigger") b.mode = MappingBinding::Trigger;
            else return fail("Unsupported input mode");
            if (action == "toggle") b.mode = MappingBinding::Toggle;
            else if (action == "increment") b.mode = MappingBinding::Relative;
            else if (action == "trigger") b.mode = MappingBinding::Trigger;
            // v6 has an enabled field at 6; four range fields begin at 7.
            const int range = t.size() >= 28 ? 7 : 6;
            if (!number(t[range], &b.inMin) || !number(t[range + 1], &b.inMax) || !number(t[range + 2], &b.outMin) || !number(t[range + 3], &b.outMax) || !source(t, sourceIndex(t), &b)) return fail("Invalid input range/source");
            if (b.inMin == b.inMax) return fail("Zero input range");
            if (range == 7) {
                int enabled;
                if (!integer(t[6], &enabled)) return fail("Invalid enabled flag");
                if (!enabled) continue;
                if (b.mode == MappingBinding::Relative) {
                    double sensitivity;
                    if (!number(t[11], &sensitivity)) return fail("Invalid relative sensitivity");
                    b.outMax = b.outMin + (b.outMax - b.outMin) * sensitivity;
                }
            }
            if (action == "write-literal") {
                if (range != 7 || !number(t[19], &b.outMax)) return fail("Missing literal value");
                b.mode = MappingBinding::Trigger;
            }
        }
        const int mod = t.indexOf("modifier");
        if (mod >= 0) { if (mod + 1 >= t.size()) return fail("Missing modifier name"); b.modifier = t[mod + 1]; }
        mapping.bindings.append(b);
    }
    if (!header || !device || mapping.id.isEmpty() || mapping.deviceName.isEmpty()) return fail("Missing mapping identity/device");
    *out = mapping; return true;
}
bool ControllerManager::loadMappingFile(const QString& path, ControllerMapping* out, QString* error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (error) *error = f.errorString(); return false; }
    return parseMapping(QString::fromUtf8(f.readAll()), out, error);
}
QStringList ControllerManager::availableMappings() const {
    QStringList result;
    if (m_impl) for (const auto& mapping : m_impl->mappings) result.append(mapping.name);
    result.removeDuplicates(); return result;
}

void ControllerManager::start() {
    if (m_impl) return;
    m_impl = new Impl;
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { delete m_impl; m_impl = nullptr; m_active.clear(); m_connected.clear(); });
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList directories = {QStringLiteral(":/mappings"), appDir + "/mappings", appDir + "/../share/stemboks/mappings", QDir::current().filePath("res/mappings"), AppSettings::dataDir() + "/mappings"};
    QHash<QString, QString> selected;
    QFile selection(AppSettings::dataDir() + "/mappings-actifs.conf");
    if (selection.open(QIODevice::ReadOnly)) for (const auto& line : QString::fromUtf8(selection.readAll()).split('\n')) {
        if (line.trimmed().isEmpty() || line.trimmed().startsWith('#')) continue;
        const auto values = line.trimmed().split('\t');
        if (values.size() == 2) selected[values[0]] = values[1];
    }
    struct Candidate { ControllerMapping mapping; QString file; QDateTime modified; int priority; };
    QVector<Candidate> candidates;
    for (int i = 0; i < directories.size(); ++i) for (const auto& file : QDir(directories[i]).entryInfoList({"*.mapping"}, QDir::Files, QDir::Name)) {
        ControllerMapping mapping; QString error;
        if (loadMappingFile(file.absoluteFilePath(), &mapping, &error)) candidates.append({mapping, file.fileName(), file.lastModified(), i});
        else qWarning() << file.fileName() << error;
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.priority == b.priority ? a.modified < b.modified : a.priority < b.priority;
    });
    for (const auto& candidate : candidates) {
        auto it = std::find_if(m_impl->mappings.begin(), m_impl->mappings.end(), [&](const ControllerMapping& m) { return m.id == candidate.mapping.id; });
        if (it == m_impl->mappings.end()) m_impl->mappings.append(candidate.mapping); else *it = candidate.mapping;
    }
    auto choose = [&](const QString& port, bool hid, int vid = 0, int pid = 0) -> ControllerMapping {
        QString wanted = selected.value(port);
        if (wanted.isEmpty()) for (const auto& c : candidates)
            if (port.contains(c.mapping.deviceName, Qt::CaseInsensitive)) {
                const QString id = selected.value(c.file, selected.value(c.mapping.deviceName));
                if (!id.isEmpty()) wanted = id;
            }
        for (auto it = candidates.crbegin(); it != candidates.crend(); ++it) {
            const auto& m = it->mapping;
            if (m.hid != hid || (hid && (m.vid != vid || m.pid != pid))) continue;
            if (!wanted.isEmpty() ? m.id == wanted : port.contains(m.deviceName, Qt::CaseInsensitive)) return m;
        }
        return {};
    };
    try {
        RtMidiIn enumerator;
        for (unsigned int i = 0; i < enumerator.getPortCount(); ++i) {
            const QString name = QString::fromStdString(enumerator.getPortName(i));
            const auto mapping = choose(name, false);
            if (mapping.id.isEmpty()) continue;
            auto port = std::make_unique<Impl::MidiPort>(); port->owner = this; port->mapping = m_active.size();
            port->input = std::make_unique<RtMidiIn>();
            port->input->ignoreTypes(true, true, true);
            port->input->setCallback([](double, std::vector<unsigned char>* message, void* context) {
                if (!message || message->size() < 2) return;
                auto* p = static_cast<Impl::MidiPort*>(context);
                const int status = (*message)[0], data1 = (*message)[1], data2 = message->size() > 2 ? (*message)[2] : 0;
                ControllerManager* owner = p->owner; const int index = p->mapping;
                QMetaObject::invokeMethod(owner, [owner, index, status, data1, data2] {
                    if (!owner->m_impl) return;
                    owner->m_impl->dispatchMapping = index;
                    owner->handleMidi(status, data1, data2);
                    owner->m_impl->dispatchMapping = -1;
                }, Qt::QueuedConnection);
            }, port.get());
            try { port->input->openPort(i, "Stemboks input"); }
            catch (const RtMidiError& e) { qWarning() << e.what(); continue; }
            m_active.append(mapping); m_connected.append(name);
            try {
                port->output = std::make_unique<RtMidiOut>();
                for (unsigned int j = 0; j < port->output->getPortCount(); ++j)
                    if (QString::fromStdString(port->output->getPortName(j)).contains(mapping.deviceName, Qt::CaseInsensitive)) { port->output->openPort(j, "Stemboks feedback"); break; }
            } catch (const RtMidiError& e) { qWarning() << e.what(); port->output.reset(); }
            m_impl->midi.push_back(std::move(port));
        }
    } catch (const RtMidiError& e) { qWarning() << "MIDI enumeration:" << e.what(); }
    if (hid_init() == 0) {
        hid_device_info* devices = hid_enumerate(0, 0);
        for (auto* info = devices; info; info = info->next) {
            const QString name = info->product_string ? QString::fromWCharArray(info->product_string) : QString::fromLocal8Bit(info->path);
            const auto mapping = choose(name, true, info->vendor_id, info->product_id);
            if (mapping.id.isEmpty()) continue;
            if (auto* handle = hid_open_path(info->path)) {
                m_impl->hid.append({handle, int(m_active.size()), {}}); m_active.append(mapping); m_connected.append(name);
            }
        }
        hid_free_enumeration(devices);
    }
    if (!m_impl->hid.isEmpty()) {
        m_impl->hidThread = new QThread;
        auto* reader = new QObject;
        auto* timer = new QTimer(reader); timer->setInterval(10);
        reader->moveToThread(m_impl->hidThread);
        connect(m_impl->hidThread, &QThread::finished, reader, &QObject::deleteLater);
        connect(m_impl->hidThread, &QThread::started, timer, qOverload<>(&QTimer::start));
        const auto ports = m_impl->hid;
        connect(timer, &QTimer::timeout, reader, [this, ports] {
            for (int index = 0; index < ports.size(); ++index) {
                unsigned char report[4096];
                const int count = hid_read_timeout(ports[index].device, report, sizeof(report), 5);
                if (count < 0) { qWarning() << "HID read failed"; continue; }
                if (count == 0) continue;
                const QByteArray current(reinterpret_cast<const char*>(report), count);
                QMetaObject::invokeMethod(this, [this, index, current] {
                    if (!m_impl || index >= m_impl->hid.size()) return;
                    auto& p = m_impl->hid[index];
                    const auto* bytes = reinterpret_cast<const unsigned char*>(current.constData());
                    // Modifier edges precede controls from the same report.
                    for (int pass = 0; pass < 2; ++pass) for (const auto& binding : m_active[p.mapping].bindings) {
                        if (binding.kind == MappingBinding::Output || (binding.kind == MappingBinding::Modifier) != (pass == 0)
                            || binding.source != MappingBinding::HidReport || binding.number >= current.size()
                            || (binding.channel != 0 && bytes[0] != binding.channel)) continue;
                        const int value = binding.bit < 0 ? bytes[binding.number] : (bytes[binding.number] >> binding.bit) & 1;
                        const int old = binding.number < p.previous.size() ? static_cast<unsigned char>(p.previous[binding.number]) : 0;
                        const int previous = binding.bit < 0 ? old : (old >> binding.bit) & 1;
                        if (value != previous || binding.mode == MappingBinding::Relative) applyBinding(binding, value, value > 0);
                    }
                    p.previous = current;
                }, Qt::QueuedConnection);
            }
        });
        m_impl->hidThread->start();
    }
    m_impl->feedbackTimer = new QTimer(this);
    m_impl->feedbackTimer->setInterval(50);
    connect(m_impl->feedbackTimer, &QTimer::timeout, this, [this] {
        if (!m_impl) return;
        for (const auto& port : m_impl->midi) {
            if (!port->output || !port->output->isPortOpen()) continue;
            const auto& bindings = m_active[port->mapping].bindings;
            for (int i = 0; i < bindings.size(); ++i) {
                const auto& b = bindings[i];
                if (b.kind != MappingBinding::Output) continue;
                if (!b.modifier.isEmpty()) {
                    const bool inverted = b.modifier.startsWith('!');
                    if (m_modifiers.value(inverted ? b.modifier.mid(1) : b.modifier) == inverted) {
                        port->lastSent.remove(i); continue;
                    }
                }
                const double value = ControlRegistry::instance().get(b.control);
                if (!std::isfinite(value) || (port->lastSent.contains(i) && port->lastSent.value(i) == value)) continue;
                const double fraction = b.mode == MappingBinding::Toggle ? (value >= b.inMin && value <= b.inMax ? 1.0 : 0.0)
                    : b.inMax != b.inMin ? std::clamp((value - b.inMin) / (b.inMax - b.inMin), 0.0, 1.0) : 0;
                const int velocity = std::clamp(int(std::lround(b.outMin + fraction * (b.outMax - b.outMin))), 0, 127);
                const unsigned char status = (b.source == MappingBinding::MidiCC ? 0xb0 : velocity ? 0x90 : 0x80) | b.channel;
                std::vector<unsigned char> message{status, static_cast<unsigned char>(b.number), static_cast<unsigned char>(velocity)};
                try { port->output->sendMessage(&message); port->lastSent[i] = value; }
                catch (const RtMidiError& e) { qWarning() << e.what(); }
            }
        }
    });
    m_impl->feedbackTimer->start();
    emit devicesChanged();
}

void ControllerManager::handleMidi(int status, int data1, int data2) {
    const int type = status & 0xf0, channel = status & 15;
    if (type != 0x80 && type != 0x90 && type != 0xb0 && type != 0xe0) return;
    const auto sourceType = type == 0xb0 ? MappingBinding::MidiCC : type == 0xe0 ? MappingBinding::MidiPitchBend : MappingBinding::MidiNote;
    const double raw = type == 0xe0 ? data1 + (data2 << 7) : type == 0x80 ? 0 : data2;
    const bool pressed = type != 0x80 && raw > 0;
    for (int pass = 0; pass < 2; ++pass) for (int i = 0; i < m_active.size(); ++i) {
        if (m_impl && m_impl->dispatchMapping >= 0 && i != m_impl->dispatchMapping) continue;
        for (const auto& b : m_active[i].bindings) {
            if (b.kind == MappingBinding::Output || (b.kind == MappingBinding::Modifier) != (pass == 0) || b.source != sourceType || b.channel != channel || (sourceType != MappingBinding::MidiPitchBend && b.number != data1)) continue;
            applyBinding(b, raw, pressed);
        }
    }
}
void ControllerManager::applyBinding(const MappingBinding& b, double raw, bool pressed) {
    if (!std::isfinite(raw)) return;
    if (!b.modifier.isEmpty()) {
        const bool inverted = b.modifier.startsWith('!');
        if (m_modifiers.value(inverted ? b.modifier.mid(1) : b.modifier) == inverted) return;
    }
    if (b.kind == MappingBinding::Modifier) { m_modifiers[b.control] = pressed; return; }
    auto& registry = ControlRegistry::instance();
    if (b.kind == MappingBinding::EffectSelection) {
        const QString unit = b.control.section('.', 0, 2);
        if (pressed) { registry.setString(unit + ".name", b.effectName); if (b.offOnRelease) registry.set(unit + ".enabled", 1); }
        else if (b.offOnRelease) registry.set(unit + ".enabled", 0);
        return;
    }
    switch (b.mode) {
    case MappingBinding::Absolute: {
        if (b.source == MappingBinding::MidiNote && !pressed) break;
        const double fraction = std::clamp((raw - b.inMin) / (b.inMax - b.inMin), 0.0, 1.0);
        registry.set(b.control, b.outMin + fraction * (b.outMax - b.outMin)); break;
    }
    case MappingBinding::Relative: {
        // Pioneer binary-offset encoders idle at 64, clockwise at 65, counterclockwise at 63.
        if (b.source == MappingBinding::MidiNote && !pressed) break;
        double step = b.source == MappingBinding::MidiNote ? 1 : raw - 64;
        if (b.rawLine.contains("twos-complement")) step = raw >= 64 ? raw - 128 : raw;
        registry.set(b.control, registry.get(b.control) + step * (b.outMax - b.outMin)); break;
    }
    case MappingBinding::Toggle: if (pressed) registry.toggle(b.control); break;
    case MappingBinding::Momentary: registry.set(b.control, pressed ? b.outMax : b.outMin); break;
    case MappingBinding::Trigger: if (pressed) registry.set(b.control, b.outMax); break;
    }
}
