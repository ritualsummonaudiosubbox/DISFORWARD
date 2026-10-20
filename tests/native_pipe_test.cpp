#include "../driver/Source/Inc/disforward_pipe.h"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
#include <cstdint>
static std::vector<int32_t> signal(unsigned frames,int start=0){std::vector<int32_t> a(frames*2);for(unsigned i=0;i<frames;i++){a[2*i]=start+(int)i;a[2*i+1]=-(start+(int)i);}return a;}
int main(){
 auto pipe=std::make_unique<DisforwardPipe>();std::array<int32_t,96> out;out.fill(123);
 pipe->Read((unsigned char*)out.data(),sizeof(out));for(auto x:out)assert(x==0);
 auto first=signal(959,10);pipe->Write((unsigned char*)first.data(),(unsigned)first.size()*4);pipe->Read((unsigned char*)out.data(),sizeof(out));for(auto x:out)assert(x==0);assert(pipe->QueuedFrames()==959);
 int32_t last[2]={969,-969};pipe->Write((unsigned char*)last,8);pipe->Read((unsigned char*)out.data(),sizeof(out));assert(std::memcmp(out.data(),first.data(),sizeof(out))==0);
 pipe->Reset();auto longInput=signal(13000,30);pipe->Write((unsigned char*)longInput.data(),(unsigned)longInput.size()*4);assert(pipe->QueuedFrames()==4800);
 std::vector<int32_t> received(4800*2);pipe->Read((unsigned char*)received.data(),(unsigned)received.size()*4);assert(std::memcmp(received.data(),longInput.data()+(13000-4800)*2,4800*8)==0);
 pipe->Read((unsigned char*)out.data(),sizeof(out));for(auto x:out)assert(x==0);
 // Thousands of timer-sized packets wrap the ring without changing bytes.
 pipe->Reset();auto initial=signal(960,1);pipe->Write((unsigned char*)initial.data(),(unsigned)initial.size()*4);
 std::vector<int32_t> expected=initial;size_t consumed=0;
 for(int packet=0;packet<25000;packet++){
  auto next=signal(48,961+packet*48);pipe->Write((unsigned char*)next.data(),48*8);expected.insert(expected.end(),next.begin(),next.end());
  pipe->Read((unsigned char*)out.data(),sizeof(out));assert(std::memcmp(out.data(),expected.data()+consumed,48*8)==0);consumed+=96;
 }
 pipe->Reset();pipe->Read((unsigned char*)out.data(),sizeof(out));for(auto x:out)assert(x==0);
 // Separate adapters never share a ring.
 auto other=std::make_unique<DisforwardPipe>();other->Read((unsigned char*)out.data(),sizeof(out));for(auto x:out)assert(x==0);
 std::cout<<"PASS: native PCM pipe silence, preroll, exact bytes, bounded overflow, 25k timer packets, reset and adapter isolation.\n";
}
