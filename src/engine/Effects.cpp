#include "Effects.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr double pi = 3.14159265358979323846;
float clean(double x) { return std::isfinite(x) && std::abs(x) >= 1e-20 ? float(x) : 0.0f; }
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
void EffectSlot::setEffect(const QString& name) {
    m_name=name.trimmed().toLower(); m_delay.clear(); m_delayPos=0;
    m_roll.clear(); m_rollPos=0; m_rollCaptured=false; m_phase=0;
    for (int c=0; c<2; ++c) { m_lp[c]=m_hp[c]=0; for (auto& state : m_allpass) state[c]=0; }
    for (int i=0; i<4; ++i) { m_comb[i].clear(); m_combPos[i]=0; }
    m_gateEnv=1; m_gatePhase=0; m_noiseSeed=22222;
}
void EffectSlot::process(float* audio, int frames) {
    if (!audio || frames <= 0 || m_sr <= 0) return;
    if (!m_enabled) { m_rollCaptured=false; m_rollPos=0; m_gatePhase=0; return; }
    enum Fx { Echo, Lowcut, Multitap, Spiral, Reverb, Gate, Jet, Flanger, Phaser, Stretch, Roll, Saw, Triangle, Dub, Crush, Pitch, Noise, Filter };
    Fx fx;
    if (m_name=="echo") fx=Echo; else if (m_name=="lowcutecho") fx=Lowcut;
    else if (m_name=="mtdelay") fx=Multitap; else if (m_name=="spiral") fx=Spiral;
    else if (m_name=="reverb" || m_name=="space") fx=Reverb; else if (m_name=="trans") fx=Gate;
    else if (m_name=="enigmajet") fx=Jet; else if (m_name=="flanger") fx=Flanger;
    else if (m_name=="phaser") fx=Phaser; else if (m_name=="stretch") fx=Stretch;
    else if (m_name=="roll" || m_name=="sliproll") fx=Roll; else if (m_name=="mobiussaw") fx=Saw;
    else if (m_name=="mobiustri") fx=Triangle; else if (m_name=="decho") fx=Dub;
    else if (m_name=="crush") fx=Crush; else if (m_name=="pitch") fx=Pitch;
    else if (m_name=="noise") fx=Noise; else if (m_name=="filter") fx=Filter; else return;
    double param=std::isfinite(m_param) ? m_param : 0.5, amount=std::clamp(param,0.0,1.0);
    double mix=std::isfinite(m_mix) ? std::clamp(m_mix,0.0,1.0) : 0;
    double beat=60/(std::isfinite(m_bpm) && m_bpm>0 ? m_bpm : 124);
    double fraction=std::clamp(param,1.0/64,8.0), period=std::max(1.0/m_sr,beat*fraction);
    bool delayFx=fx==Echo || fx==Lowcut || fx==Multitap || fx==Spiral || fx==Dub || fx==Jet || fx==Flanger || fx==Pitch;
    int delayFrames=std::max(4,m_sr*8);
    if (delayFx && m_delay.size()!=2*delayFrames) { m_delay.fill(0,2*delayFrames); m_delayPos=0; }
    int apLength[2]={std::max(1,int(m_sr*0.005)),std::max(1,int(m_sr*0.0017))};
    if (fx==Reverb) {
        static const double lengths[]={0.0297,0.0371,0.0411,0.0437};
        for (int i=0; i<4; ++i) {
            int len=std::max(1,int(m_sr*lengths[i]));
            if (m_comb[i].size()!=2*len) { m_comb[i].fill(0,2*len); m_combPos[i]=0; }
        }
        int size=2*(apLength[0]+apLength[1]);
        if (m_delay.size()!=size) { m_delay.fill(0,size); m_delayPos=0; }
    }
    int loopFrames=std::clamp(int(std::min(8.0,period)*m_sr),1,delayFrames);
    if (fx==Stretch) loopFrames=std::max(16,int(m_sr*(0.03+0.12*amount)));
    if ((fx==Roll || fx==Stretch) && m_roll.size()!=2*loopFrames) {
        m_roll.fill(0,2*loopFrames); m_rollPos=0; m_rollCaptured=false; m_gatePhase=0;
    }
    auto delayed=[&](double delay,int c) {
        double at=m_delayPos-std::clamp(delay,1.0,double(delayFrames-2));
        if (at<0) at+=delayFrames;
        int i=int(at); double t=at-i;
        return float((1-t)*m_delay[2*i+c]+t*m_delay[2*((i+1)%delayFrames)+c]);
    };
    double lowAlpha=1-std::exp(-2*pi*frequency(m_sr,200)/m_sr);
    for (int f=0; f<frames; ++f) {
        double lfo=std::sin(2*pi*m_phase);
        if (fx==Gate) {
            double target=m_gatePhase<0.5 ? 1 : 0;
            m_gateEnv=clean(m_gateEnv+(target-m_gateEnv)*(1-std::exp(-1.0/(m_sr*0.002))));
        }
        bool captured=m_rollCaptured;
        for (int c=0; c<2; ++c) {
            float x=clean(audio[2*f+c]), wet=x, feedback=0;
            switch (fx) {
            case Echo: case Lowcut: case Multitap: case Spiral: case Dub: {
                double delay=std::clamp(period*m_sr,1.0,double(delayFrames-2));
                wet=delayed(delay,c); feedback=wet;
                if (fx==Multitap) wet=0.5f*wet+0.3f*delayed(std::min(2*delay,double(delayFrames-2)),c)+0.2f*delayed(std::min(3*delay,double(delayFrames-2)),c);
                if (fx==Lowcut) { m_lp[c]=clean(m_lp[c]+lowAlpha*(wet-m_lp[c])); wet-=m_lp[c]; feedback=wet; }
                if (fx==Spiral || fx==Dub) {
                    double cutoff=fx==Spiral ? 300*std::pow(40.0,m_phase) : 2500;
                    double a=1-std::exp(-2*pi*frequency(m_sr,cutoff)/m_sr);
                    m_lp[c]=clean(m_lp[c]+a*(wet-m_lp[c])); wet=m_lp[c]; feedback=wet;
                }
                m_delay[2*m_delayPos+c]=clean(std::tanh(x+(fx==Dub ? 0.72 : 0.55)*feedback));
                break;
            }
            case Reverb: {
                float sum=0;
                for (int i=0; i<4; ++i) {
                    int n=int(m_comb[i].size()/2), at=(m_combPos[i]+c*(i+1)*7)%n;
                    float y=m_comb[i][2*at+c]; sum+=y*0.25f;
                    m_allpass[i][c]=clean(m_allpass[i][c]+0.35*(y-m_allpass[i][c]));
                    m_comb[i][2*at+c]=clean(x+(0.65+0.25*amount)*m_allpass[i][c]);
                }
                wet=sum; int base=0;
                for (int i=0; i<2; ++i) {
                    int at=2*(base+m_delayPos%apLength[i])+c; float old=m_delay[at];
                    float y=old-0.5f*wet; m_delay[at]=clean(wet+0.5f*y); wet=y; base+=apLength[i];
                }
                wet=float((1-amount)*x+amount*wet); break;
            }
            case Gate: wet=x*m_gateEnv; break;
            case Jet: case Flanger: case Pitch: {
                double milliseconds=fx==Pitch ? 15+7*std::sin(2*pi*m_phase+c*pi/2) : 2+1.8*lfo;
                wet=delayed(milliseconds*m_sr/1000,c);
                if (fx==Jet) {
                    double a=1-std::exp(-2*pi*frequency(m_sr,300+3700*(lfo+1)/2)/m_sr);
                    m_lp[c]=clean(m_lp[c]+a*(wet-m_lp[c])); wet=m_lp[c];
                    m_allpass[0][c]=clean(m_allpass[0][c]+lowAlpha*(wet-m_allpass[0][c])); wet-=m_allpass[0][c];
                }
                m_delay[2*m_delayPos+c]=clean(std::tanh(x+(fx==Pitch ? 0.15 : 0.65)*wet));
                wet=0.5f*(x+wet); break;
            }
            case Phaser: {
                double cutoff=200*std::pow(20.0,(lfo+1)/2), t=std::tan(pi*frequency(m_sr,cutoff)/m_sr), a=(1-t)/(1+t);
                wet=x+0.25f*m_hp[c];
                for (int i=0; i<4; ++i) { float y=clean(-a*wet+m_allpass[i][c]); m_allpass[i][c]=clean(wet+a*y); wet=y; }
                m_hp[c]=clean(wet); wet=0.5f*(x+wet); break;
            }
            case Roll: case Stretch: {
                if (!captured) m_roll[2*m_rollPos+c]=x;
                else {
                    double at=fx==Roll ? m_rollPos : m_gatePhase; int i=int(at)%loopFrames;
                    double t=at-std::floor(at); wet=float((1-t)*m_roll[2*i+c]+t*m_roll[2*((i+1)%loopFrames)+c]);
                    int fade=std::min(loopFrames/4,std::max(1,m_sr/200));
                    if (fade>0 && at<fade) {
                        float tail=m_roll[2*(loopFrames-fade+i)+c]; wet=float(at/fade)*wet+float(1-at/fade)*tail;
                    }
                    if (fx==Stretch) m_roll[2*m_rollPos+c]=x;
                }
                break;
            }
            case Saw: case Triangle: {
                double oscillator=fx==Saw ? 2*m_gatePhase-1 : 1-4*std::abs(m_gatePhase-0.5);
                wet=float(x*oscillator); break;
            }
            case Crush: {
                int step=1+int(amount*63), bits=std::max(2,16-int(amount*14));
                if (m_delayPos%step==0) { double levels=std::ldexp(1.0,bits-1); m_lp[c]=clean(std::round(std::clamp(double(x),-1.0,1.0)*levels)/levels); }
                wet=m_lp[c]; break;
            }
            case Noise: {
                m_noiseSeed^=m_noiseSeed<<13; m_noiseSeed^=m_noiseSeed>>17; m_noiseSeed^=m_noiseSeed<<5;
                float noise=float(double(m_noiseSeed)/4294967295.0*2-1);
                double a=1-std::exp(-2*pi*frequency(m_sr,200*std::pow(60.0,amount))/m_sr);
                m_lp[c]=clean(m_lp[c]+a*(noise-m_lp[c])); wet=x+float(0.25*amount)*m_lp[c]; break;
            }
            case Filter: {
                double distance=std::abs(amount-0.5)*2;
                double cutoff=amount>=0.5 ? std::min(20000.0,m_sr*0.49)*std::pow(0.0025,distance) : 20*std::pow(500.0,distance);
                double a=1-std::exp(-2*pi*frequency(m_sr,cutoff)/m_sr);
                m_lp[c]=clean(m_lp[c]+a*(x-m_lp[c]));
                float filtered=amount>=0.5 ? m_lp[c] : x-m_lp[c]; wet=float((1-distance)*x+distance*filtered); break;
            }
            }
            audio[2*f+c]=clean((1-mix)*x+mix*wet);
        }
        if (delayFx) m_delayPos=(m_delayPos+1)%delayFrames;
        if (fx==Reverb) {
            for (int i=0; i<4; ++i) m_combPos[i]=(m_combPos[i]+1)%int(m_comb[i].size()/2);
            m_delayPos=(m_delayPos+1)%(apLength[0]*apLength[1]);
        }
        if (fx==Crush) m_delayPos=(m_delayPos+1)%1000000;
        if (fx==Roll || fx==Stretch) { m_rollPos=(m_rollPos+1)%loopFrames; if (m_rollPos==0) m_rollCaptured=true; }
        if (fx==Stretch) { m_gatePhase+=0.5; if (m_gatePhase>=loopFrames) m_gatePhase-=loopFrames; }
        else {
            double speed=(fx==Saw || fx==Triangle) ? 30+970*(0.5+0.5*lfo)*amount : 1/period;
            m_gatePhase+=speed/m_sr; m_gatePhase-=std::floor(m_gatePhase);
        }
        double speed=fx==Pitch ? 0.2+3*amount : fx==Phaser || fx==Flanger || fx==Jet ? 0.05+0.95*amount : 1/(4*beat);
        m_phase+=speed/m_sr; m_phase-=std::floor(m_phase);
    }
}
