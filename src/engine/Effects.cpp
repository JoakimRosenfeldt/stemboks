#include "Effects.h"
#include <algorithm>
#include <cmath>

namespace stemboks {

namespace {
constexpr double pi = 3.14159265358979323846;
double frequency(int sr, double f) { return std::clamp(f, 1.0, std::max(1.0, sr*0.49)); }
void coefficients(Biquad& b, double b0, double b1, double b2, double a0, double a1, double a2) {
    b.b0=b0/a0; b.b1=b1/a0; b.b2=b2/a0; b.a1=a1/a0; b.a2=a2/a0;
}
}
QStringList FxNames::beatFx() {
    return {"lowcutecho","echo","mtdelay","spiral","reverb","trans","enigmajet","flanger","phaser","stretch","sliproll","roll","mobiussaw","mobiustri"};
}
QStringList FxNames::colorFx() { return {"space","decho","crush","pitch","noise","filter"}; }
void Biquad::setLowpass(int sr, double freq, double q) {
    double w=2*pi*frequency(sr,freq)/std::max(1,sr), c=std::cos(w), alpha=std::sin(w)/(2*std::max(0.01,q));
    coefficients(*this,(1-c)/2,1-c,(1-c)/2,1+alpha,-2*c,1-alpha);
}
void Biquad::setHighpass(int sr, double freq, double q) {
    double w=2*pi*frequency(sr,freq)/std::max(1,sr), c=std::cos(w), alpha=std::sin(w)/(2*std::max(0.01,q));
    coefficients(*this,(1+c)/2,-(1+c),(1+c)/2,1+alpha,-2*c,1-alpha);
}
void Biquad::setPeaking(int sr, double freq, double q, double gainDb) {
    double a=std::pow(10,std::clamp(gainDb,-60.0,60.0)/40), w=2*pi*frequency(sr,freq)/std::max(1,sr);
    double c=std::cos(w), alpha=std::sin(w)/(2*std::max(0.01,q));
    coefficients(*this,1+alpha*a,-2*c,1-alpha*a,1+alpha/a,-2*c,1-alpha/a);
}
void Biquad::setLowShelf(int sr, double freq, double gainDb) {
    double a=std::pow(10,std::clamp(gainDb,-60.0,60.0)/40), w=2*pi*frequency(sr,freq)/std::max(1,sr);
    double c=std::cos(w), beta=std::sqrt(2*a)*std::sin(w);
    coefficients(*this,a*((a+1)-(a-1)*c+beta),2*a*((a-1)-(a+1)*c),a*((a+1)-(a-1)*c-beta),
                 (a+1)+(a-1)*c+beta,-2*((a-1)+(a+1)*c),(a+1)+(a-1)*c-beta);
}
void Biquad::setHighShelf(int sr, double freq, double gainDb) {
    double a=std::pow(10,std::clamp(gainDb,-60.0,60.0)/40), w=2*pi*frequency(sr,freq)/std::max(1,sr);
    double c=std::cos(w), beta=std::sqrt(2*a)*std::sin(w);
    coefficients(*this,a*((a+1)+(a-1)*c+beta),-2*a*((a-1)+(a+1)*c),a*((a+1)+(a-1)*c-beta),
                 (a+1)-(a-1)*c+beta,2*((a-1)-(a+1)*c),(a+1)-(a-1)*c-beta);
}

} // namespace stemboks
