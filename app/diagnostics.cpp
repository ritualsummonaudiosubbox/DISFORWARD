#include "engine.h"
#include "audio_core.h"
#include <cmath>
namespace df {
namespace {
HRESULT open(const Endpoint& endpoint,Com<IAudioClient>& client){
 Com<IMMDeviceEnumerator> en;Com<IMMDevice> device;
 HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)en.put());
 if(SUCCEEDED(hr))hr=en->GetDevice(endpoint.id.c_str(),device.put());
 if(SUCCEEDED(hr))hr=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,(void**)client.put());
 return hr;
}
}
std::wstring testVirtualLine(const Endpoint &playback,const Endpoint &microphone,HANDLE cancel){
    HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(co)&&co!=RPC_E_CHANGED_MODE)return L"FAIL: could not start Windows audio.";
    struct CoEnd{bool owns;~CoEnd(){if(owns)CoUninitialize();}} coEnd{SUCCEEDED(co)};
    Com<IAudioClient> in,out;Com<IAudioCaptureClient> capture;Com<IAudioRenderClient> render;
    auto fail=[](HRESULT hr){return L"FAIL: "+hrMessage(hr);};
    HRESULT hr=open(microphone,in);if(FAILED(hr))return fail(hr);
    hr=open(playback,out);if(FAILED(hr))return fail(hr);
    WAVEFORMATEX f{};f.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;f.nChannels=2;f.nSamplesPerSec=48000;f.wBitsPerSample=32;f.nBlockAlign=8;f.nAvgBytesPerSec=384000;
    constexpr DWORD flags=0x80000000u|0x08000000u;
    hr=in->Initialize(AUDCLNT_SHAREMODE_SHARED,flags,300000,0,&f,nullptr);if(FAILED(hr))return fail(hr);
    hr=out->Initialize(AUDCLNT_SHAREMODE_SHARED,flags,300000,0,&f,nullptr);if(FAILED(hr))return fail(hr);
    hr=in->GetService(__uuidof(IAudioCaptureClient),(void**)capture.put());if(FAILED(hr))return fail(hr);
    hr=out->GetService(__uuidof(IAudioRenderClient),(void**)render.put());if(FAILED(hr))return fail(hr);
    UINT32 capacity=0;hr=out->GetBufferSize(&capacity);if(FAILED(hr))return fail(hr);
    hr=in->Start();if(FAILED(hr))return fail(hr);
    struct Stop{IAudioClient* in;IAudioClient* out;~Stop(){in->Stop();out->Stop();out->Reset();}} stop{in.get(),out.get()};
    hr=out->Start();if(FAILED(hr))return fail(hr);
    const int tones[]={631,823,997,1103};double frequency=tones[GetTickCount64()%4],omega=6.283185307179586*frequency/48000.;
    ToneDetector detector(frequency);unsigned long long sent=0;ULONGLONG begin=GetTickCount64();
    while(GetTickCount64()-begin<2200){
        if(WaitForSingleObject(cancel,0)==WAIT_OBJECT_0)return L"CANCELLED: line test stopped.";
        UINT32 n=0;hr=capture->GetNextPacketSize(&n);if(FAILED(hr))return fail(hr);
        while(n){
            BYTE* data=nullptr;UINT32 frames=0;DWORD captureFlags=0;hr=capture->GetBuffer(&data,&frames,&captureFlags,nullptr,nullptr);if(FAILED(hr))return fail(hr);
            for(UINT32 i=0;i<frames;i++){
                double sample=captureFlags&AUDCLNT_BUFFERFLAGS_SILENT?0:((float*)data)[i*2];
                detector.sample(sample);
            }
            hr=capture->ReleaseBuffer(frames);if(FAILED(hr))return fail(hr);hr=capture->GetNextPacketSize(&n);if(FAILED(hr))return fail(hr);
        }
        UINT32 padding=0;hr=out->GetCurrentPadding(&padding);if(FAILED(hr))return fail(hr);UINT32 free=capacity-padding;
        if(free){BYTE* bytes=nullptr;hr=render->GetBuffer(free,&bytes);if(FAILED(hr))return fail(hr);
            for(UINT32 i=0;i<free;i++,sent++){float sample=.0316f*(float)std::sin(omega*static_cast<double>(sent));((float*)bytes)[i*2]=sample;((float*)bytes)[i*2+1]=sample;}
            hr=render->ReleaseBuffer(free,0);if(FAILED(hr))return fail(hr);
        }
        Sleep(2);
    }
    if(!detector.passed())return L"FAIL: the test tone did not reach the VB-CABLE microphone. Restart Windows, check VB-CABLE levels/mute and run this check again.";
    return L"PASS: test tone returned through VB-CABLE to the microphone endpoint. Select this microphone in your call app. This checks the cable path; finish with a short test call. Select My Mic or Discord Friend to resume.";
}
}
