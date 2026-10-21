#pragma once
#include "engine.h"
namespace df {
// Keep the TWO dedicated Hi-Fi engines at the same 48 kHz stream format.
// MATCH_FORMAT is the documented Windows 10+ stream negotiation API. This
// does not edit persistent device formats, defaults, or physical sound cards.
class ReceiveSession {
 Com<IAudioClient3> playback,capture;
 static HRESULT open(const Endpoint& endpoint,Com<IAudioClient3>& client){
  Com<IMMDeviceEnumerator> en;Com<IMMDevice> device;
  HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)en.put());
  if(SUCCEEDED(hr))hr=en->GetDevice(endpoint.id.c_str(),device.put());
  if(SUCCEEDED(hr))hr=device->Activate(__uuidof(IAudioClient3),CLSCTX_ALL,nullptr,(void**)client.put());
  AudioClientProperties properties{};properties.cbSize=sizeof(properties);properties.eCategory=AudioCategory_Media;properties.Options=AUDCLNT_STREAMOPTIONS_MATCH_FORMAT;
  if(SUCCEEDED(hr))hr=client->SetClientProperties(&properties);
  WAVEFORMATEX f{};f.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;f.nChannels=2;f.nSamplesPerSec=48000;f.wBitsPerSample=32;f.nBlockAlign=8;f.nAvgBytesPerSec=384000;
  UINT32 period=0,fundamental=0,minimum=0,maximum=0;
  if(SUCCEEDED(hr))hr=client->GetSharedModeEnginePeriod(&f,&period,&fundamental,&minimum,&maximum);
  if(SUCCEEDED(hr))hr=client->InitializeSharedAudioStream(0,period,&f,nullptr);
  return hr;
 }
public:
 void stop(){if(playback)playback->Stop();if(capture)capture->Stop();playback.put();capture.put();}
 ~ReceiveSession(){stop();}
 std::wstring start(const Endpoint& out,const Endpoint& in,const std::wstring& label=L"Discord receive"){
  stop();HRESULT hr=open(out,playback);
  if(SUCCEEDED(hr))hr=open(in,capture);
  if(SUCCEEDED(hr))hr=playback->Start();
  if(SUCCEEDED(hr))hr=capture->Start();
  for(auto client:{playback.get(),capture.get()}){
   if(FAILED(hr))break;
   WAVEFORMATEX* current=nullptr;UINT32 period=0;
   hr=client->GetCurrentSharedModeEnginePeriod(&current,&period);
   if(SUCCEEDED(hr)&&(!current||current->nSamplesPerSec!=48000))hr=AUDCLNT_E_UNSUPPORTED_FORMAT;
   if(current)CoTaskMemFree(current);
  }
  // The capture anchor keeps the format active. Its bounded Windows buffer
  // is intentionally unread; send/monitor clients read independent streams.
  if(FAILED(hr)){stop();return label+L" format: "+hrMessage(hr)+L" Close applications using this cable and ASIO Bridge, then Refresh. If needed set both cable formats to 48,000 Hz in Sound settings.";}
  return L"";
 }
};
}
