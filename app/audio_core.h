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
    bool at(size_t offset,float& l,float& r)const{if(offset>=count){l=r=0;return false;}size_t i=(read+offset)%capacity;l=data[i*2];r=data[i*2+1];return true;}
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
// Fractional clock matching stays in the app. A 64-tap Blackman-windowed
// sinc preserves treble that linear variable-rate interpolation attenuates.
// The phase table is shared, immutable, and built before audio starts.
class DriftReader {
    static constexpr int half=32,phases=1024,taps=64;
    using Kernel=std::array<float,taps>;
    using Table=std::array<Kernel,phases+1>;
    const Table& table;
    double position=31,ratio=1;
    static const Table& coefficients(){
        static const Table values=[](){
            Table result{};constexpr double pi=3.14159265358979323846;
            for(int phase=0;phase<=phases;++phase){
                double fraction=static_cast<double>(phase)/phases,total=0;
                for(int j=0;j<taps;++j){
                    double distance=j-31-fraction;
                    double sinc=std::abs(distance)<1e-12?1:std::sin(pi*distance)/(pi*distance);
                    double window=.42+.5*std::cos(pi*distance/half)+.08*std::cos(2*pi*distance/half);
                    result[phase][j]=static_cast<float>(sinc*window);total+=sinc*window;
                }
                for(auto& value:result[phase])value=static_cast<float>(value/total);
            }
            return result;
        }();
        return values;
    }
public:
    DriftReader():table(coefficients()){}
    void reset(){position=31;ratio=1;}
    void setRatio(double value){ratio=std::isfinite(value)?std::clamp(value,.999,1.001):1;}
    bool frame(StereoQueue& queue,float& left,float& right){
        const auto base=static_cast<size_t>(position);
        if(queue.size()<=base+32){left=right=0;return false;}
        double phase=(position-base)*phases;int index=std::min(static_cast<int>(phase),phases-1);
        float blend=static_cast<float>(phase-index);double l=0,r=0;
        for(int j=0;j<taps;++j){
            float a=0,b=0;queue.at(base-31+static_cast<size_t>(j),a,b);
            float weight=table[index][j]+blend*(table[index+1][j]-table[index][j]);l+=a*weight;r+=b*weight;
        }
        left=static_cast<float>(l);right=static_cast<float>(r);
        position+=ratio;size_t used=static_cast<size_t>(position)-31;queue.discard(used);position-=used;
        return true;
    }
};
// Keep a queue reserve AFTER filling the render buffer. If capture stalls,
// fade the remaining real audio out, discard its stale tail, and refill the
// reserve before fading in. Never repeat a packet or hold a last sample.
class PlayoutBuffer {
    DriftReader reader;
    bool running=false,draining=false;
    float gain=0;
    size_t fadeRemaining=0;
    double correction=1;
    unsigned long gaps=0;
    static constexpr size_t guard=64,fadeFrames=240;
public:
    static constexpr size_t reserveFrames=1440;
    void setRatio(double ratio){correction=ratio;reader.setRatio(ratio);}
    unsigned long recoveries()const{return gaps;}
    bool active()const{return running;}
    void beginBlock(const StereoQueue& queue,size_t renderFrames){
        if(!running&&queue.size()>=reserveFrames+renderFrames+guard){
            reader.reset();reader.setRatio(correction);running=true;draining=false;gain=0;
        }
    }
    void frame(StereoQueue& queue,float& l,float& r){
        l=r=0;if(!running)return;
        const size_t available=queue.size()>guard?queue.size()-guard:0;
        if(!draining&&available<=fadeFrames+8){draining=true;fadeRemaining=available>8?available-8:0;++gaps;}
        if((draining&&!fadeRemaining)||!reader.frame(queue,l,r)){
            if(!draining)++gaps;
            queue.clear();reader.reset();running=draining=false;gain=0;l=r=0;return;
        }
        if(draining){gain*=static_cast<float>(fadeRemaining-1)/static_cast<float>(fadeRemaining);--fadeRemaining;}
        else gain=std::min(1.f,gain+1.f/static_cast<float>(fadeFrames));
        l*=gain;r*=gain;
    }
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
