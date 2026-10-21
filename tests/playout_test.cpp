#include "../app/audio_core.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

static void constant(df::StereoQueue& queue,size_t count,float value){
 for(size_t i=0;i<count;++i){float pair[]={value,-value*.5f};queue.push(pair,1);}
}
int main(){
 auto queue=std::make_unique<df::StereoQueue>();df::PlayoutBuffer output;
 float l=0,r=0;
 constant(*queue,1440,.25f);output.beginBlock(*queue,1440);
 assert(!output.active());
 for(int i=0;i<1440;++i){output.frame(*queue,l,r);assert(l==0&&r==0);}
 assert(queue->size()==1440);
 constant(*queue,1696,.25f);output.beginBlock(*queue,1440);assert(output.active());
 float previous=0;
 for(int i=0;i<1440;++i){output.frame(*queue,l,r);assert(std::abs(l-previous)<.0011f);assert(std::abs(r+l*.5f)<1e-7);previous=l;}
 assert(queue->size()>=df::PlayoutBuffer::reserveFrames+64);
 assert(output.recoveries()==0);
 // Exhaust input completely: a real signal fades to silence rather than
 // being replaced by alternating zero packets or repeated samples.
 for(int i=0;i<4000;++i){output.frame(*queue,l,r);assert(std::isfinite(l)&&std::abs(l-previous)<.0012f);previous=l;}
 assert(!output.active()&&output.recoveries()==1&&queue->size()==0);
 for(int i=0;i<4000;++i){output.beginBlock(*queue,480);output.frame(*queue,l,r);assert(l==0&&r==0);}
 constant(*queue,1984,-.25f);output.beginBlock(*queue,480);assert(output.active());
 for(int i=0;i<480;++i){output.frame(*queue,l,r);assert(std::abs(l-previous)<.0012f);assert(std::abs(r+l*.5f)<1e-7);previous=l;}
 assert(std::abs(l+.25f)<1e-6);

 // Packet delivery jitter, followed by one 110 ms capture outage.
 for(bool stall:{false,true}){
  auto packets=std::make_unique<df::StereoQueue>();df::PlayoutBuffer playout;
  float last=0;bool heard=false,recovered=false;int nextCapture=0,packet=0;
  for(int ms=0;ms<2000;++ms){
   if(ms>=nextCapture){
    if(!(stall&&ms>=700&&ms<810))constant(*packets,480,.2f);
    ++packet;nextCapture=packet*10+(packet%7==0?2:0);
   }
   if(ms%10==0){
    playout.beginBlock(*packets,480);
    for(int i=0;i<480;++i){
     playout.frame(*packets,l,r);assert(std::isfinite(l)&&std::isfinite(r));
     assert(std::abs(l-last)<.0015f);assert(std::abs(r+l*.5f)<1e-7);last=l;
     if(ms>150&&ms<600&&std::abs(l-.2f)<1e-6)heard=true;
     if(ms>1100&&std::abs(l-.2f)<1e-6)recovered=true;
    }
   }
  }
  assert(heard&&recovered);assert(playout.recoveries()==(stall?1ul:0ul));
 }
 std::cout<<"PASS: render-buffer reserve, smooth underrun fade, silence while refilling, stereo-safe recovery, packet jitter and 110 ms capture outage.\n";
}
