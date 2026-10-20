#pragma once
// DISFORWARD native transport. Shared by kernel code and portable tests.
// Fixed 48 kHz / stereo / signed 32-bit PCM. The caller supplies synchronization.
// No allocation, floating point, file/network I/O or user pointers in this class.
class DisforwardPipe {
    static constexpr unsigned FrameBytes=8;
    static constexpr unsigned Capacity=4800; // at most 100 ms of recent audio
    static constexpr unsigned PrimeFrames=960; // 20 ms scheduling margin
    unsigned char audio[Capacity][FrameBytes];
    unsigned read=0,count=0;
    bool primed=false;
public:
    void Reset(){read=0;count=0;primed=false;}
    unsigned QueuedFrames()const{return count;}
    void Write(const unsigned char* source,unsigned bytes){
        unsigned frames=bytes/FrameBytes;
        if(frames>Capacity){source+=(frames-Capacity)*FrameBytes;frames=Capacity;Reset();}
        for(unsigned i=0;i<frames;++i){
            if(count==Capacity){read=(read+1)%Capacity;--count;}
            unsigned at=(read+count)%Capacity;
            for(unsigned c=0;c<FrameBytes;++c)audio[at][c]=source[i*FrameBytes+c];
            ++count;
        }
    }
    void Read(unsigned char* destination,unsigned bytes){
        // Underflow is defined silence, never uninitialized/stale ring memory.
        for(unsigned i=0;i<bytes;++i)destination[i]=0;
        if(!primed){if(count<PrimeFrames)return;primed=true;}
        unsigned frames=bytes/FrameBytes;
        unsigned available=frames<count?frames:count;
        for(unsigned i=0;i<available;++i){
            for(unsigned c=0;c<FrameBytes;++c)destination[i*FrameBytes+c]=audio[read][c];
            read=(read+1)%Capacity;--count;
        }
        if(available<frames)primed=false;
    }
};
