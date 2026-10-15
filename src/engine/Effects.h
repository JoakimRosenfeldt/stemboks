#pragma once
#include "Types.h"
#include <QVector>
#include <QStringList>

namespace stemboks {

// Beat FX + Color FX vocabulary taken from Stembox's controller mappings.
namespace FxNames {
    QStringList beatFx();   // lowcutecho echo mtdelay spiral reverb trans enigmajet flanger phaser stretch sliproll roll mobiussaw mobiustri
    QStringList colorFx();  // space decho crush pitch noise filter
}

// Biquad filters for offline waveform analysis.
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

} // namespace stemboks
