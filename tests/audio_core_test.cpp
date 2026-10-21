#include "../app/audio_core.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
int main(){
 auto q=std::make_unique<df::StereoQueue>();
 float in[]={.125f,-.75f,1.f,-1.f};q->push(in,2);float l,r;
 assert(q->pop(l,r)&&l==.125f&&r==-.75f);assert(q->pop(l,r)&&l==1.f&&r==-1.f);assert(!q->pop(l,r)&&l==0&&r==0);
 for(int i=0;i<100000;i++){float x[2]={(float)i,-(float)i};q->push(x,1);}assert(q->size()==48000);
 for(int i=52000;i<100000;i++){assert(q->pop(l,r));assert(l==(float)i&&r==-(float)i);}
 float bad[]={std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()};q->push(bad,1);assert(q->pop(l,r)&&l==0&&r==0);
 q->push(nullptr,100,true);assert(q->size()==100);while(q->pop(l,r))assert(l==0&&r==0);
 df::SourceGate gate;
 for(int i=0;i<1000;i++){gate.frame(1,.25f,-.25f,.75f,-.75f,l,r);assert(l<=.25f&&r>=-.25f);}assert(std::abs(l-.25f)<1e-6);
 bool sawZero=false,sawNew=false;
 for(int i=0;i<1000;i++){
  gate.frame(2,.25f,0,0,.75f,l,r);
  assert(!(l!=0&&r!=0)); // distinct-channel sentinels prove no voice overlap
  if(l==0&&r==0)sawZero=true;
  if(r>0){assert(sawZero);sawNew=true;}
 }assert(sawNew&&std::abs(r-.75f)<1e-6);
 for(int i=0;i<300;i++){gate.frame(0,.25f,0,0,.75f,l,r);}assert(l==0&&r==0);
 std::mt19937 random(73);
 for(int i=0;i<500000;i++){
  int want=(random()%17==0)?(int)(random()%3):gate.source();
  gate.frame(want,1,0,0,1,l,r);assert(std::isfinite(l)&&std::isfinite(r));assert(l>=0&&r>=0&&l<=1&&r<=1);assert(!(l>0&&r>0));
 }
 // Transparent float path after the brief source-selection ramp.
 for(int i=0;i<600;i++)gate.frame(1,0,0,0,0,l,r);
 for(int i=0;i<48000;i++){
  float sample=.9f*std::sin((float)i*.131f);gate.frame(1,sample,-sample,0,0,l,r);assert(std::abs(l-sample)<1e-6&&std::abs(r+sample)<1e-6);
 }

 // Compare clock-corrected output with analytic full-band sine references.
 // This catches the treble loss of a linear variable-rate interpolator.
 for(double ratio:{.999,1.,1.001})for(double hz:{80.,997.,8000.,16000.,20000.}){
  auto stream=std::make_unique<df::StereoQueue>();df::DriftReader reader;reader.setRatio(ratio);
  unsigned long long inputFrame=0;double error=0,energy=0;
  for(int i=0;i<16000;++i){
   while(stream->size()<4096){float v=.25f*static_cast<float>(std::sin(6.283185307179586*hz*static_cast<double>(inputFrame++)/48000.));float pair[]={v,-v*.5f};stream->push(pair,1);}
   assert(reader.frame(*stream,l,r));
   double expected=.25*std::sin(6.283185307179586*hz*(31+i*ratio)/48000.);
   error+=(l-expected)*(l-expected);energy+=expected*expected;assert(std::abs(r+l*.5f)<1e-7);
  }
  assert(std::sqrt(error/energy)<.001);
 }
 std::cout<<"PASS: 64-tap drift correction at +/-1000 ppm, 80 Hz-20 kHz, stereo independence and better than -60 dB error against analytic references.\n";

 df::ToneDetector correct(997),wrong(997),silent(997),noise(997),tooShort(997);
 for(int i=0;i<86400;i++){
  correct.sample(.0316*std::sin(6.283185307179586*997*(i-317)/48000.));
  wrong.sample(.0316*std::sin(6.283185307179586*631*i/48000.));
  silent.sample(0);noise.sample((int(random()%2001)-1000)/10000.);
  if(i<25000)tooShort.sample(.0316*std::sin(6.283185307179586*997*i/48000.));
 }
 assert(correct.passed());assert(!wrong.passed()&&!silent.passed()&&!noise.passed()&&!tooShort.passed());
 std::cout<<"PASS: tone detector accepts a delayed target tone; rejects wrong tone, silence, random noise and insufficient capture.\n";
 std::cout<<"PASS: FIFO order/wrap/overflow; silence; NaN/Inf sanitization; exclusive-source ramp; 500k rapid-switch frames; float pass-through.\n";
}
