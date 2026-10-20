#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace df {
// One worker owns these queues. No heap allocation in the audio loop.
class StereoQueue {
    static constexpr size_t capacity = 48000;
    std::array<float,capacity*2> data{};
    size_t read=0, count=0;
public:
    size_t size() const {return count;}
    void clear(){read=0;count=0;}
    void discard(size_t frames){frames=std::min(frames,count);read=(read+frames)%capacity;count-=frames;}
    size_t push(const float* in,size_t frames,bool silent=false){
        size_t dropped=0;
        for(size_t i=0;i<frames;i++){
            if(count==capacity){discard(1);++dropped;}
            size_t at=(read+count)%capacity;
            for(size_t c=0;c<2;c++){
                float v=silent?0.f:in[2*i+c];
                data[at*2+c]=std::isfinite(v)?v:0.f;
            }
            ++count;
        }
        return dropped;
    }
    bool pop(float &l,float &r){if(!count){l=r=0;return false;}l=data[read*2];r=data[read*2+1];discard(1);return true;}
};
// Break-before-make: the old source ramps completely to zero before the
// new source is permitted. Never sum two voices, including mid-ramp changes.
class SourceGate {
    int current=0;
    float gain=0;
    static constexpr float step=1.f/240.f; // 5 ms at 48 kHz
public:
    void reset(){current=0;gain=0;}
    int source() const{return current;}
    float level() const{return gain;}
    void frame(int wanted,float ml,float mr,float dl,float dr,float &l,float &r){
        wanted=std::clamp(wanted,0,2);
        if(current!=wanted){gain=std::max(0.f,gain-step);if(gain<=step/2){gain=0;current=wanted;}}
        else gain=std::min(1.f,gain+step);
        if(current==0){gain=0;l=r=0;return;}
        l=(current==1?ml:dl)*gain;r=(current==1?mr:dr)*gain;
        if(!std::isfinite(l))l=0;
        if(!std::isfinite(r))r=0;
    }
};
// Tone-return check is independent of signal phase, so cable latency does not
// require an arbitrary alignment. Diagnostic use only, not voice processing.
class ToneDetector {
    double omega,energy=0,real=0,imag=0;
    unsigned long long position=0,measured=0;
public:
    explicit ToneDetector(double frequency):omega(6.283185307179586*frequency/48000.){}
    void sample(double value){
        if(position>=24000&&std::isfinite(value)){
            energy+=value*value;real+=value*std::cos(omega*position);imag+=value*std::sin(omega*position);++measured;
        }
        ++position;
    }
    bool passed()const{
        if(measured<12000||energy<=0)return false;
        double rms=std::sqrt(energy/measured);
        double coherence=2*(real*real+imag*imag)/(energy*measured);
        return rms>=.001&&coherence>=.35;
    }
};

}
