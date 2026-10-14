#pragma once
#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVector>

// Parser for Stembox's SBX_CONTROLLER_MAPPING v6 text format + MIDI (RtMidi)
// and HID (hidapi) backends driving the ControlRegistry.
//
// Grammar (from shipped files):
//   SBX_CONTROLLER_MAPPING 6
//   id "DDJ-FLX10"
//   name "Pioneer DDJ-FLX10"
//   device midi 0 0 "DDJ-FLX10"          (also: device hid <vid> <pid> "name")
//   modifier "<id>" "<name>" held 0 midi midi-note <ch> <note> 0 0 0
//   modifier-feedback "<id>" midi ...
//   effect-selection "<id>" "fx.unit.1.name" "<fxname>" midi midi-note <ch> <note> ... [off-on-release]
//   input "<id>" "<dotted.control>" write absolute linear <...> midi midi-cc <ch> <cc> ...
//   input "<id>" "<dotted.control>" write toggle|momentary|relative ... midi midi-note ...
//   output "<id>" "<dotted.control>" ... midi ...   (LED feedback)
// Unknown lines are logged and skipped (never approximate-matched).

struct MappingBinding {
    enum Kind { Input, Output, Modifier, EffectSelection } kind = Input;
    enum Source { MidiNote, MidiCC, MidiPitchBend, HidReport } source = MidiNote;
    QString id;
    QString control;        // dotted registry name (or modifier name)
    QString effectName;     // for EffectSelection
    QString modifier;       // required modifier name ("" = none)
    int channel = 0;        // 0-based MIDI channel
    int number = 0;         // note/cc number, or HID byte index
    int bit = -1;           // HID bit (-1 = whole byte)
    enum Mode { Absolute, Relative, Toggle, Momentary, Trigger } mode = Absolute;
    double inMin = 0, inMax = 127, outMin = 0, outMax = 1;
    bool offOnRelease = false;
    QString rawLine;
};

struct ControllerMapping {
    QString id, name, deviceName;
    bool hid = false; int vid = 0, pid = 0;
    QVector<MappingBinding> bindings;
};

class ControllerManager : public QObject {
    Q_OBJECT
public:
    static ControllerManager& instance();
    void start();   // scan devices, load mappings from res + user dir
    QStringList availableMappings() const;
    QStringList connectedDevices() const { return m_connected; }
    bool loadMappingFile(const QString& path, ControllerMapping* out, QString* error = nullptr);
    static bool parseMapping(const QString& text, ControllerMapping* out, QString* error = nullptr);

signals:
    void devicesChanged();

private:
    ControllerManager() = default;
    struct Impl; Impl* m_impl = nullptr;
    QStringList m_connected;
    QVector<ControllerMapping> m_active;
    void handleMidi(int status, int data1, int data2);
    void applyBinding(const MappingBinding& b, double rawValue, bool pressed);
    QHash<QString, bool> m_modifiers;
};
