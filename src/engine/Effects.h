#pragma once
#include "Types.h"
#include <QVector>
#include <QStringList>

// Beat FX + Color FX vocabulary taken from Stembox's controller mappings.
namespace FxNames {
    QStringList beatFx();   // lowcutecho echo mtdelay spiral reverb trans enigmajet flanger phaser stretch sliproll roll mobiussaw mobiustri
    QStringList colorFx();  // space decho crush pitch noise filter
}

// One stereo effect slot with a single beat-fraction/amount parameter, DJM style.
class EffectSlot {
public:
    void setSampleRate(int sr) { m_sr = sr; }
    void setEffect(const QString& name);       // resets state
    QString effect() const { return m_name; }
    void setEnabled(bool on) {
        if (on && !m_enabled) {
            m_latchedBpm = m_bpm; m_rollCaptured = false; m_rollPos = 0; m_gatePhase = 0;
        }
        m_enabled = on;
    }
    bool enabled() const { return m_enabled; }
    void setParameter(double v) {
        if (v != m_param) {
            m_latchedBpm = m_bpm; m_rollCaptured = false; m_rollPos = 0; m_gatePhase = 0;
        }
        m_param = v;
    }   // meaning depends on effect (beat fraction or 0..1)
    void setMix(double v) { m_mix = v; }           // 0..1 wet
    void setBpm(double bpm) { m_bpm = bpm; }
    void process(float* interleaved, int frames);  // in-place
private:
    QString m_name = QStringLiteral("echo");
    bool m_enabled = false;
    double m_param = 0.5, m_mix = 1.0, m_bpm = 124.0;
    double m_latchedBpm = 124.0;
    int m_sr = 44100;
    QVector<float> m_delay; int m_delayPos = 0;
    QVector<float> m_roll;  int m_rollPos = 0; bool m_rollCaptured = false;
    double m_phase = 0.0;           // LFO
    float m_lp[2] = {0,0}, m_hp[2] = {0,0};
    float m_allpass[4][2] = {};     // phaser/reverb state
    QVector<float> m_comb[4]; int m_combPos[4] = {0,0,0,0};
    float m_gateEnv = 1.0f; double m_gatePhase = 0.0;
    quint32 m_noiseSeed = 22222;
};

// Simple one-pole biquad helpers used by EQ/filter code.
struct Biquad {
    double b0=1,b1=0,b2=0,a1=0,a2=0;
    double x1=0,x2=0,y1=0,y2=0;
    void setLowShelf(int sr, double freq, double gainDb);
    void setHighShelf(int sr, double freq, double gainDb);
    void setPeaking(int sr, double freq, double q, double gainDb);
    void setLowpass(int sr, double freq, double q = 0.707);
    void setHighpass(int sr, double freq, double q = 0.707);
    inline float process(float x) {
        double y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        x2=x1; x1=x; y2=y1; y1=y; return float(y);
    }
    void reset() { x1=x2=y1=y2=0; }
};
