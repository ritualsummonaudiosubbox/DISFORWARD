#include "../app/engine.h"
#include "../app/receive_session.h"
#include <iostream>
#include <cmath>
namespace df {int probeProcessTone(DWORD pid,double frequency,unsigned duration);int probeEndpointTone(const std::wstring&,double,unsigned);}
int tone(const df::Endpoint& out,unsigned duration=12000){
 df::Com<IMMDeviceEnumerator> en;df::Com<IMMDevice> device;df::Com<IAudioClient> client;df::Com<IAudioRenderClient> render;
 HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)en.put());
 if(SUCCEEDED(hr))hr=en->GetDevice(out.id.c_str(),device.put());
 if(SUCCEEDED(hr))hr=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,(void**)client.put());
 WAVEFORMATEX f{};f.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;f.nChannels=2;f.nSamplesPerSec=48000;f.wBitsPerSample=32;f.nBlockAlign=8;f.nAvgBytesPerSec=384000;
 if(SUCCEEDED(hr))hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,0x88000000u,200000,0,&f,nullptr);
 if(SUCCEEDED(hr))hr=client->GetService(__uuidof(IAudioRenderClient),(void**)render.put());
 UINT32 size=0;if(SUCCEEDED(hr))hr=client->GetBufferSize(&size);
 if(SUCCEEDED(hr))hr=client->Start();if(FAILED(hr))return 1;
 auto begin=GetTickCount64();unsigned long long frame=0;
 while(GetTickCount64()-begin<duration){
  UINT32 padding=0;hr=client->GetCurrentPadding(&padding);if(FAILED(hr))break;
  UINT32 n=size-padding;if(n){BYTE* bytes=nullptr;hr=render->GetBuffer(n,&bytes);if(FAILED(hr))break;
   for(UINT32 i=0;i<n;++i,++frame){float v=.0316f*static_cast<float>(std::sin(6.283185307179586*997*static_cast<double>(frame)/48000.));reinterpret_cast<float*>(bytes)[i*2]=v;reinterpret_cast<float*>(bytes)[i*2+1]=v;}
   hr=render->ReleaseBuffer(n,0);if(FAILED(hr))break;
  }Sleep(2);
 }
 client->Stop();return FAILED(hr)?1:0;
}
std::wstring defaults(){
 df::Com<IMMDeviceEnumerator> en;
 if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)en.put())))return L"error";
 std::wstring value;
 for(auto flow:{eRender,eCapture})for(auto role:{eConsole,eMultimedia,eCommunications}){
  df::Com<IMMDevice> d;LPWSTR id=nullptr;
  if(SUCCEEDED(en->GetDefaultAudioEndpoint(flow,role,d.put()))&&SUCCEEDED(d->GetId(&id))){value+=id;CoTaskMemFree(id);}value+=L"|";
 }return value;
}
int split(const df::Endpoint& send,const df::Endpoint& mic){
 df::Endpoint receivePlayback,receiveCapture;
 bool ready=false;for(int i=0;i<40&&!ready;++i){ready=df::receivePair(df::endpoints(eRender),df::endpoints(eCapture),receivePlayback,receiveCapture);if(!ready)Sleep(250);}
 if(!ready){
  for(auto flow:{eRender,eCapture})for(const auto& e:df::endpoints(flow))std::wcout<<L"Endpoint "<<static_cast<int>(flow)<<L": "<<e.name<<L" | "<<e.driver<<L" | "<<e.container<<L"\n";
  std::wcerr<<L"FAIL: independent receive pair missing.\n";return 1;
 }
 auto before=defaults();
 auto issue=df::receiveFormatIssue(receivePlayback,receiveCapture);std::wcout<<L"Initial receive format check: "<<(issue.empty()?L"equal rates":issue)<<L"\n";
 df::ReceiveSession session;issue=session.start(receivePlayback,receiveCapture);
 if(!issue.empty()){std::wcerr<<issue<<L"\n";return 1;}
 std::wcout<<L"Active receive engines verified at 48,000 Hz through GetCurrentSharedModeEnginePeriod. Persistent default formats remain unchanged.\n";
 HANDLE cancel=CreateEventW(nullptr,TRUE,FALSE,nullptr);auto result=df::testVirtualLine(receivePlayback,receiveCapture,cancel);if(cancel)CloseHandle(cancel);
 std::wcout<<L"Receive playback: "<<receivePlayback.name<<L" | "<<receivePlayback.driver<<L"\nReceive recording: "<<receiveCapture.name<<L" | "<<receiveCapture.driver<<L"\n"<<result<<L"\n";
 if(result.rfind(L"PASS:",0)!=0)return 1;
 wchar_t path[32768];GetModuleFileNameW(nullptr,path,32768);std::wstring cmd=L"\""+std::wstring(path)+L"\" --tone-receive";
 STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
 if(!CreateProcessW(path,cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return 1;
 CloseHandle(pi.hThread);
 struct Child{HANDLE h;~Child(){WaitForSingleObject(h,22000);CloseHandle(h);}} child{pi.hProcess};
 bool ok=true;
 auto check=[&](const wchar_t* label,const std::wstring& id,int expected){int got=df::probeEndpointTone(id,997,1000);std::wcout<<label<<L": "<<got<<L" (expected "<<expected<<L")\n";ok=ok&&got==expected;};
 check(L"Discord receive signal",receiveCapture.id,1);
 check(L"Muted send has no receive bypass",mic.id,0);
 df::RouteConfig route;route.process=false;route.feed=receiveCapture.id;route.output=send.id;
 df::Engine call,monitor;
 call.start(nullptr,route,2);Sleep(400);
 check(L"Friend forwarded to send",mic.id,1);
 call.stop();Sleep(400);
 check(L"Mute removes friend from send",mic.id,0);
 // CI has no physical speakers. A second shared stream into the test cable
 // proves independent engine lifetimes; it is not an acoustic-echo test.
 monitor.start(nullptr,route,2);call.start(nullptr,route,2);Sleep(400);
 call.stop();Sleep(300);
 check(L"Listening stream survives phone mute (test sink)",mic.id,1);
 ok=ok&&monitor.state.load()==2;
 call.start(nullptr,route,2);Sleep(300);monitor.stop();Sleep(300);
 check(L"Phone stream survives speaker off (test sink)",mic.id,1);
 ok=ok&&call.state.load()==2;call.stop();Sleep(400);
 check(L"Both streams off are silent",mic.id,0);
 bool unchanged=before==defaults();std::wcout<<L"Windows default endpoints unchanged: "<<unchanged<<L"\n";
 if(!ok||!unchanged){std::wcerr<<L"FAIL: split cable route or independent engine lifecycle.\n";return 1;}
 std::wcout<<L"PASS: independent receive/send buses, routed friend forwarding and mute isolation, independent listening/send stream lifetimes, unchanged default endpoints. Physical speakers and actual calls not tested.\n";return 0;
}
int wmain(int argc,wchar_t** argv){
 if(argc>1&&wcscmp(argv[1],L"--silent")==0){Sleep(14000);return 0;}
 HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(co))return 2;
 CoInitializeSecurity(nullptr,-1,nullptr,nullptr,RPC_C_AUTHN_LEVEL_DEFAULT,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE,nullptr);
 struct End{~End(){CoUninitialize();}} end;
 df::Endpoint send,mic;bool found=false;
 for(int i=0;i<40&&!found;++i){found=df::cablePair(df::endpoints(eRender),df::endpoints(eCapture),send,mic);if(!found)Sleep(250);}
 if(!found){std::wcerr<<L"FAIL: no active VB-CABLE endpoint pair.\n";return 1;}
 if(argc>1&&wcscmp(argv[1],L"--tone")==0)return tone(send);
 if(argc>1&&wcscmp(argv[1],L"--tone-receive")==0){df::Endpoint out,in;if(!df::receivePair(df::endpoints(eRender),df::endpoints(eCapture),out,in))return 1;return tone(out,18000);}
 if(argc>1&&wcscmp(argv[1],L"--split")==0)return split(send,mic);
 std::wcout<<L"Playback: "<<send.name<<L" | interface: "<<send.driver<<L"\nCapture: "<<mic.name<<L" | interface: "<<mic.driver<<L"\n";
 HANDLE cancel=CreateEventW(nullptr,TRUE,FALSE,nullptr);auto result=df::testVirtualLine(send,mic,cancel);if(cancel)CloseHandle(cancel);std::wcout<<result<<L"\n";
 if(result.rfind(L"PASS:",0)!=0)return 1;
 wchar_t path[32768];GetModuleFileNameW(nullptr,path,32768);std::wstring cmd=L"\""+std::wstring(path)+L"\" --tone";
 STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
 if(!CreateProcessW(path,cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return 1;
 CloseHandle(pi.hThread);
 // Capture an unrelated sibling, not the parent: include-tree correctly
 // includes the parent's tone-playing child, so the parent is not a negative.
 std::wstring silentCmd=L"\""+std::wstring(path)+L"\" --silent";PROCESS_INFORMATION silent{};
 if(!CreateProcessW(path,silentCmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&silent)){WaitForSingleObject(pi.hProcess,15000);CloseHandle(pi.hProcess);return 1;}
 CloseHandle(silent.hThread);
 int excluded=df::probeProcessTone(silent.dwProcessId,997,1400);
 int included=df::probeProcessTone(pi.dwProcessId,997,2400);
 std::wcout<<L"Process capture: excluded source="<<excluded<<L", selected source="<<included<<L"\n";
 bool processEngine=false;
 {
  df::Engine engine;df::RouteConfig config;config.pid=pi.dwProcessId;config.output=send.id;
  engine.start(nullptr,config,2);
  auto begin=GetTickCount64();while(GetTickCount64()-begin<2500&&engine.state.load()!=3){if(engine.state.load()==2&&engine.peak.load()>10){processEngine=true;break;}Sleep(10);}
  if(!processEngine)std::wcerr<<L"Engine process route failed: "<<engine.message()<<L"\n";
  engine.stop();
 }
 WaitForSingleObject(pi.hProcess,15000);CloseHandle(pi.hProcess);WaitForSingleObject(silent.hProcess,15000);CloseHandle(silent.hProcess);Sleep(300);
 bool micEngine=false;
 {
  df::Engine engine;df::RouteConfig config;config.mic=mic.id;config.output=send.id;
  engine.start(nullptr,config,1);
  auto begin=GetTickCount64();while(GetTickCount64()-begin<2500&&engine.state.load()!=3){if(engine.state.load()==2){micEngine=true;break;}Sleep(10);}
  if(!micEngine)std::wcerr<<L"Engine capture-device route failed: "<<engine.message()<<L"\n";
  engine.stop();
 }
 std::wcout<<L"Engine routes: process signal="<<processEngine<<L", capture-device start="<<micEngine<<L"\n";
 if(excluded!=0||included!=1||!processEngine||!micEngine){std::wcerr<<L"FAIL: application audio capture/isolation or routing engine check.\n";return 1;}
 std::wcout<<L"PASS: signed-driver tone return, process-loopback isolation, engine process signal and engine capture-device start/stop. This is not a Discord/physical-microphone/call-app test.\n";return 0;
}
