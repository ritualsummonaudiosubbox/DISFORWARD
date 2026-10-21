#include <initguid.h>
#include "engine.h"
#include "audio_core.h"
#include <avrt.h>
#include <ks.h>
#include <ksmedia.h>
#include <algorithm>
#include <cwctype>
#include <memory>
#include <set>
#include <map>
#include <new>
#include <cmath>
#include <cstdio>

namespace df {
namespace {
constexpr DWORD Convert=0x80000000u|0x08000000u; // auto PCM conversion + quality SRC
std::wstring property(IMMDevice* device,REFPROPERTYKEY key){
    Com<IPropertyStore> store;PROPVARIANT v;PropVariantInit(&v);
    if(FAILED(device->OpenPropertyStore(STGM_READ,store.put())))return L"";
    std::wstring result;
    if(SUCCEEDED(store->GetValue(key,&v))){
        if(v.vt==VT_LPWSTR&&v.pwszVal)result=v.pwszVal;
        else if(v.vt==VT_CLSID&&v.puuid){wchar_t b[64];StringFromGUID2(*v.puuid,b,64);result=b;}
    }
    PropVariantClear(&v);return result;
}
std::wstring upper(std::wstring s){for(auto &c:s)c=towupper(c);return s;}
std::wstring cableKey(std::wstring s){
    s=upper(s);
    for(auto word:{L" OUTPUT",L" INPUT"}){auto p=s.find(word);if(p!=std::wstring::npos)s.erase(p,wcslen(word));}
    return s;
}
WAVEFORMATEX format(){WAVEFORMATEX f{};f.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;f.nChannels=2;f.nSamplesPerSec=48000;f.wBitsPerSample=32;f.nBlockAlign=8;f.nAvgBytesPerSec=384000;return f;}
HRESULT clientFor(const std::wstring &id,Com<IAudioClient>& client){
    Com<IMMDeviceEnumerator> enumerator;Com<IMMDevice> device;
    HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)enumerator.put());
    if(SUCCEEDED(hr))hr=enumerator->GetDevice(id.c_str(),device.put());
    if(SUCCEEDED(hr))hr=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,(void**)client.put());
    return hr;
}
HRESULT monitorClient(const std::wstring& id,Com<IAudioClient>& client,bool allowRaw=true){
    auto hr=clientFor(id,client);if(FAILED(hr))return hr;
    Com<IAudioClient2> media;
    hr=client->QueryInterface(__uuidof(IAudioClient2),(void**)media.put());if(FAILED(hr))return hr;
    // A listening stream is media, never a communications/voice-processing stream.
    // Request RAW only when the device explicitly advertises support.
    Com<IMMDeviceEnumerator> en;Com<IMMDevice> device;Com<IPropertyStore> properties;
    bool raw=false;
    const PROPERTYKEY rawKey={{0x8943b373,0x388c,0x4395,{0xb5,0x57,0xbc,0x6d,0xba,0xff,0xaf,0xdb}},2};
    if(SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)en.put()))&&
       SUCCEEDED(en->GetDevice(id.c_str(),device.put()))&&SUCCEEDED(device->OpenPropertyStore(STGM_READ,properties.put()))){
        PROPVARIANT value;PropVariantInit(&value);
        if(SUCCEEDED(properties->GetValue(rawKey,&value)))raw=allowRaw&&value.vt==VT_BOOL&&value.boolVal==VARIANT_TRUE;
        PropVariantClear(&value);
    }
    AudioClientProperties options{};options.cbSize=sizeof(options);options.eCategory=AudioCategory_Media;
    options.Options=raw?AUDCLNT_STREAMOPTIONS_RAW:AUDCLNT_STREAMOPTIONS_NONE;
    hr=media->SetClientProperties(&options);
    if(FAILED(hr)&&raw){raw=false;options.Options=AUDCLNT_STREAMOPTIONS_NONE;hr=media->SetClientProperties(&options);}
    if(FAILED(hr))return hr;
    WAVEFORMATEX* mix=nullptr;hr=client->GetMixFormat(&mix);
    if(SUCCEEDED(hr)&&(!mix||mix->nSamplesPerSec<44100))hr=AUDCLNT_E_UNSUPPORTED_FORMAT;
#ifdef DISFORWARD_AUDIO_TEST
    std::wprintf(L"SPEAKER media output: %lu Hz, RAW supported/accepted=%d\n",mix?mix->nSamplesPerSec:0,raw?1:0);
#endif
    if(mix)CoTaskMemFree(mix);
    return hr;
}
// Newer SDK ABI declarations; kept local to build with MinGW-w64 11.
struct ProcessParams {int type=1;DWORD pid=0;int mode=0;};
static_assert(sizeof(ProcessParams)==12,"Windows process capture ABI");
class Activation final : public IActivateAudioInterfaceCompletionHandler, public IAgileObject {
    std::atomic<ULONG> refs{1};
    Com<IUnknown> marshaler;
public:
    HANDLE done=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    HRESULT result=E_PENDING;
    Com<IAudioClient> client;
    ProcessParams params;
    PROPVARIANT variant{};
    explicit Activation(DWORD pid){
        params.pid=pid;variant.vt=VT_BLOB;variant.blob.cbSize=sizeof(params);variant.blob.pBlobData=(BYTE*)&params;
        result=CoCreateFreeThreadedMarshaler(static_cast<IActivateAudioInterfaceCompletionHandler*>(this),marshaler.put());
    }
    ~Activation(){if(done)CloseHandle(done);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,void** obj)override{
        if(!obj)return E_POINTER;
        *obj=nullptr;
        if(riid==__uuidof(IUnknown)||riid==__uuidof(IActivateAudioInterfaceCompletionHandler)){*obj=static_cast<IActivateAudioInterfaceCompletionHandler*>(this);AddRef();return S_OK;}
        if(riid==__uuidof(IAgileObject)){*obj=static_cast<IAgileObject*>(this);AddRef();return S_OK;}
        if(riid==IID_IMarshal&&marshaler)return marshaler->QueryInterface(riid,obj);
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation* op)override{
        Com<IUnknown> object;HRESULT activation=E_FAIL;result=op->GetActivateResult(&activation,object.put());
        if(SUCCEEDED(result))result=activation;
        if(SUCCEEDED(result)&&object)result=object->QueryInterface(__uuidof(IAudioClient),(void**)client.put());
        SetEvent(done);return S_OK;
    }
};
HRESULT processClient(DWORD pid,HANDLE cancel,Com<IAudioClient>& client){
    // Imported API exists on supported Windows. Activation fails cleanly on older builds.
    auto a=new(std::nothrow) Activation(pid);if(!a)return E_OUTOFMEMORY;
    if(FAILED(a->result)||!a->done){HRESULT hr=FAILED(a->result)?a->result:HRESULT_FROM_WIN32(GetLastError());a->Release();return hr;}
    Com<IActivateAudioInterfaceAsyncOperation> operation;
    HRESULT hr=ActivateAudioInterfaceAsync(L"VAD\\Process_Loopback",__uuidof(IAudioClient),&a->variant,a,operation.put());
    if(SUCCEEDED(hr)){
        HANDLE events[]={cancel,a->done};DWORD wait=WaitForMultipleObjects(2,events,FALSE,10000);
        if(wait==WAIT_OBJECT_0)hr=HRESULT_FROM_WIN32(ERROR_CANCELLED);
        else if(wait==WAIT_OBJECT_0+1){hr=a->result;if(SUCCEEDED(hr))hr=a->client->QueryInterface(__uuidof(IAudioClient),(void**)client.put());}
        else hr=HRESULT_FROM_WIN32(wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError());
    }
    a->Release();return hr;
}
struct Capture {
    Com<IAudioClient> client;Com<IAudioCaptureClient> service;
    bool started=false;
    HANDLE event=nullptr;
    StereoQueue queue;
    HRESULT init(const std::wstring &id,bool process,DWORD pid,HANDLE cancel){
        auto hr=process?processClient(pid,cancel,client):clientFor(id,client);if(FAILED(hr))return hr;
#ifdef DISFORWARD_AUDIO_TEST
        if(process)std::wprintf(L"Activation succeeded for PID %lu\n",static_cast<unsigned long>(pid));
#endif
        auto f=format();DWORD flags=Convert|(process?(AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_EVENTCALLBACK):0);
        if(process){event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event)return HRESULT_FROM_WIN32(GetLastError());}
        hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,flags,process?0:200000,0,&f,nullptr);
#ifdef DISFORWARD_AUDIO_TEST
        if(process)std::wprintf(L"Process Initialize: 0x%08lX\n",static_cast<unsigned long>(hr));
#endif
        if(SUCCEEDED(hr)&&process)hr=client->SetEventHandle(event);
        if(SUCCEEDED(hr))hr=client->GetService(__uuidof(IAudioCaptureClient),(void**)service.put());
        return hr;
    }
    HRESULT start(){auto hr=client->Start();started=SUCCEEDED(hr);return hr;}
    ~Capture(){if(started)client->Stop();if(event)CloseHandle(event);}
    HRESULT drain(std::atomic<unsigned long>& faults){
        UINT32 n=0;HRESULT hr=service->GetNextPacketSize(&n);
        // Bound a drain pass so a stalled producer cannot starve rendering.
        unsigned packets=0;
        while(SUCCEEDED(hr)&&n&&packets++<64){
            BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;
            hr=service->GetBuffer(&data,&frames,&flags,nullptr,nullptr);if(FAILED(hr))return hr;
            if(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)++faults;
            if(queue.push((const float*)data,frames,(flags&AUDCLNT_BUFFERFLAGS_SILENT)!=0))++faults;
            hr=service->ReleaseBuffer(frames);if(FAILED(hr))return hr;
            hr=service->GetNextPacketSize(&n);
        }
        return hr;
    }
};
struct Mmcss {HANDLE handle=nullptr;Mmcss(){DWORD id=0;handle=AvSetMmThreadCharacteristicsW(L"Pro Audio",&id);}~Mmcss(){if(handle)AvRevertMmThreadCharacteristics(handle);}};
}
std::vector<Endpoint> endpoints(EDataFlow flow){
    std::vector<Endpoint> result;Com<IMMDeviceEnumerator> en;Com<IMMDeviceCollection> devices;
    if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)en.put())))return result;
    if(FAILED(en->EnumAudioEndpoints(flow,DEVICE_STATE_ACTIVE,devices.put())))return result;
    UINT n=0;devices->GetCount(&n);
    const PROPERTYKEY containerKey={{0x8c7ed206,0x3f8a,0x4827,{0xb3,0xab,0xae,0x9e,0x1f,0xae,0xfc,0x6c}},2};
    const PROPERTYKEY nativeKey={{0xd16f1100,0x13a4,0x4ea5,{0x9d,0x06,0xb1,0x2b,0x91,0xb5,0x89,0x00}},1};
    for(UINT i=0;i<n;i++){
        Com<IMMDevice> d;LPWSTR id=nullptr;
        if(SUCCEEDED(devices->Item(i,d.put()))&&SUCCEEDED(d->GetId(&id))){result.push_back({id,property(d.get(),PKEY_Device_FriendlyName),property(d.get(),containerKey),property(d.get(),PKEY_DeviceInterface_FriendlyName),property(d.get(),nativeKey)});CoTaskMemFree(id);}
    }
    return result;
}
#ifdef DISFORWARD_AUDIO_TEST
int probeEndpointQuality(const std::wstring& id,std::array<double,8>* levels){
 HANDLE cancel=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!cancel)return 0;
 struct Close{HANDLE h;~Close(){CloseHandle(h);}} close{cancel};
 auto input=std::make_unique<Capture>();
 if(FAILED(input->init(id,false,0,cancel))||FAILED(input->start()))return 0;
 std::vector<float> left,right;auto begin=GetTickCount64();std::atomic<unsigned long> faults{0};
 while(GetTickCount64()-begin<1800){
  if(FAILED(input->drain(faults)))return 0;
  float l=0,r=0;while(input->queue.pop(l,r)){left.push_back(l);right.push_back(r);}Sleep(2);
 }
 constexpr size_t count=48000;
 if(left.size()<count)return 0;
 const size_t start=left.size()-count;
 std::vector<double> window(count);double total=0;
 for(size_t i=0;i<count;++i){window[i]=.5-.5*std::cos(6.283185307179586*static_cast<double>(i)/(count-1));total+=window[i];}
 auto amplitude=[&](const std::vector<float>& samples,double hz){
  double best=0;
  // Clock compensation is allowed; search a narrow neighborhood rather
  // than mistaking a sub-Hz device-clock difference for lost treble.
  for(int offset=-16;offset<=16;++offset){
   double omega=6.283185307179586*(hz+offset*.25)/48000.;
   double cosine=std::cos(omega),sine=std::sin(omega),c=1,s=0,real=0,imag=0;
   for(size_t i=0;i<count;++i){double v=samples[start+i]*window[i];real+=v*c;imag+=v*s;double next=c*cosine-s*sine;s=s*cosine+c*sine;c=next;}
   best=std::max(best,2*std::hypot(real,imag)/total);
  }
  return best;
 };
 bool ok=true;size_t band=0;
 for(double hz:{80.,997.,8000.,16000.}){
  double l=amplitude(left,hz),r=amplitude(right,hz);
  if(levels){(*levels)[band*2]=l;(*levels)[band*2+1]=r;}++band;
  std::wprintf(L"SPEAKER full-band %5.0f Hz: L=%.6f R=%.6f (target .0125/.00625)\n",hz,l,r);
  ok=ok&&std::abs(l/.0125-1)<.15&&std::abs(r/.00625-1)<.15;
 }
 std::wprintf(L"%ls: 80 Hz-16 kHz transfer and independent stereo channel levels on virtual test sink.\n",ok?L"PASS":L"FAIL");
 return ok?1:0;
}
int probeEndpointTone(const std::wstring& id,double frequency,unsigned duration){
 HANDLE cancel=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!cancel)return -1;
 struct Close{HANDLE h;~Close(){CloseHandle(h);}} close{cancel};
 auto input=std::make_unique<Capture>();
 if(FAILED(input->init(id,false,0,cancel))||FAILED(input->start()))return -1;
 ToneDetector detector(frequency);std::atomic<unsigned long> faults{0};auto begin=GetTickCount64();
 while(GetTickCount64()-begin<duration){
  if(FAILED(input->drain(faults)))return -1;
  float l=0,r=0;while(input->queue.pop(l,r))detector.sample(l);Sleep(2);
 }
 return detector.passed()?1:0;
}
int probeProcessTone(DWORD pid,double frequency,unsigned duration){
    HANDLE cancel=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!cancel)return -1;
    struct Close{HANDLE h;~Close(){CloseHandle(h);}} close{cancel};
    auto input=std::make_unique<Capture>();
    HRESULT hr=input->init(L"",true,pid,cancel);
    std::wprintf(L"Process-loopback init: 0x%08lX\n",static_cast<unsigned long>(hr));
    if(FAILED(hr))return -1;
    hr=input->start();std::wprintf(L"Process-loopback start: 0x%08lX\n",static_cast<unsigned long>(hr));
    if(FAILED(hr))return -1;
    ToneDetector detector(frequency);std::atomic<unsigned long> faults{0};auto begin=GetTickCount64();
    while(GetTickCount64()-begin<duration){
        if(FAILED(input->drain(faults)))return -1;
        float l=0,r=0;while(input->queue.pop(l,r))detector.sample(l);
        Sleep(2);
    }
    return detector.passed()?1:0;
}
#endif
std::wstring receiveFormatIssue(const Endpoint& playback,const Endpoint& capture){
 Com<IAudioClient> out,in;
 if(FAILED(clientFor(playback.id,out))||FAILED(clientFor(capture.id,in)))return L"Receive line unavailable. Restart Windows, then Refresh.";
 WAVEFORMATEX *a=nullptr,*b=nullptr;HRESULT x=out->GetMixFormat(&a),y=in->GetMixFormat(&b);
 bool valid=SUCCEEDED(x)&&SUCCEEDED(y)&&a&&b;
 bool equal=valid&&a->nSamplesPerSec==b->nSamplesPerSec;
 std::wstring rates=valid?L" (playback "+std::to_wstring(a->nSamplesPerSec)+L", recording "+std::to_wstring(b->nSamplesPerSec)+L" Hz)":L"";
 if(a)CoTaskMemFree(a);if(b)CoTaskMemFree(b);
 if(!valid)return L"Could not read the Discord receive formats.";
 return equal?L"":L"Hi-Fi Cable rates differ"+rates+L". In Sound settings, set both Hi-Fi Cable playback and recording devices to 48,000 Hz, then Refresh.";
}
bool virtualCable(const Endpoint &e){auto n=upper(e.name);return standardCable(e)||receiveCable(e)||e.identity==L"DISFORWARD.Native.1"||n.find(L"CABLE")!=std::wstring::npos||n.find(L"DISFORWARD")!=std::wstring::npos||n.find(L"VIRTUAL")!=std::wstring::npos;}
bool sameCable(const Endpoint &a,const Endpoint &b){
    if(a.id==b.id)return true;
    if(upper(a.name)==L"DISFORWARD"&&upper(b.name)==L"DISFORWARD SEND")return true;
    if(!virtualCable(a)||!virtualCable(b))return false;
    return cableKey(a.name)==cableKey(b.name);
}
bool supportsProcessCapture(){
    using RtlVersion=LONG(WINAPI*)(OSVERSIONINFOW*);
    HMODULE ntdll=GetModuleHandleW(L"ntdll.dll");auto fn=reinterpret_cast<RtlVersion>(reinterpret_cast<void*>(GetProcAddress(ntdll,"RtlGetVersion")));
    OSVERSIONINFOW v{};v.dwOSVersionInfoSize=sizeof(v);return fn&&fn(&v)==0&&v.dwBuildNumber>=20348;
}
std::vector<Program> programs(){
    std::vector<Program> result;std::map<DWORD,PROCESSENTRY32W> all;
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snap==INVALID_HANDLE_VALUE)return result;
    PROCESSENTRY32W p{};p.dwSize=sizeof(p);
    if(Process32FirstW(snap,&p)){do{all[p.th32ProcessID]=p;}while(Process32NextW(snap,&p));}
    CloseHandle(snap);
    std::set<DWORD> visible;
    EnumWindows([](HWND h,LPARAM arg)->BOOL{if(IsWindowVisible(h)&&GetWindowTextLengthW(h)>0){DWORD pid=0;GetWindowThreadProcessId(h,&pid);reinterpret_cast<std::set<DWORD>*>(arg)->insert(pid);}return TRUE;},(LPARAM)&visible);
    std::set<DWORD> added;
    for(auto &[pid,entry]:all){
        std::wstring exe=entry.szExeFile;auto u=upper(exe);bool discord=u.find(L"DISCORD")!=std::wstring::npos;
        if(!discord&&!visible.count(pid))continue;
        if(pid==GetCurrentProcessId())continue;
        DWORD root=pid;std::set<DWORD> visited;
        while(all.count(root)&&visited.insert(root).second){auto parent=all[root].th32ParentProcessID;
            if(!all.count(parent)||upper(all[parent].szExeFile)!=u)break;
            root=parent;
        }
        if(!added.insert(root).second)continue;
        result.push_back({root,exe+L"  ·  "+std::to_wstring(root),exe});
    }
    std::sort(result.begin(),result.end(),[](auto &a,auto &b){bool ad=upper(a.exe).find(L"DISCORD")!=std::wstring::npos,bd=upper(b.exe).find(L"DISCORD")!=std::wstring::npos;return ad!=bd?ad>bd:a.label<b.label;});
    return result;
}
std::wstring hrMessage(HRESULT hr){
    wchar_t code[24];swprintf(code,24,L"0x%08lX",(unsigned long)hr);
    switch(hr){
    case E_ACCESSDENIED:return L"Microphone access was denied. Check Windows microphone privacy permissions.";
    case AUDCLNT_E_DEVICE_INVALIDATED:return L"An audio device disconnected or changed. Reconnect it, then refresh.";
    case AUDCLNT_E_UNSUPPORTED_FORMAT:return L"This device rejected 48 kHz float audio. Use a compatible shared-mode device.";
    case AUDCLNT_E_DEVICE_IN_USE:return L"Another app is using this device exclusively. Close its exclusive audio stream.";
    default:return std::wstring(L"Audio could not start (")+code+L"). Check devices; check VB-CABLE installation and Discord.";
    }
}
Engine::Engine(){stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);}
Engine::~Engine(){stop();if(stopEvent)CloseHandle(stopEvent);}
void Engine::stop(){wanted=0;if(stopEvent)SetEvent(stopEvent);if(thread.joinable())thread.join();state=0;peak=0;}
std::wstring Engine::message(){std::lock_guard<std::mutex> guard(messageMutex);return detail;}
void Engine::fail(HRESULT hr,const std::wstring& context){
    {std::lock_guard<std::mutex> guard(messageMutex);detail=context+L": "+hrMessage(hr);}
    wanted=0;state=3;peak=0;PostMessageW(target,StateMessage,0,0);
}
void Engine::start(HWND hwnd,const RouteConfig &config,int source){
    stop();target=hwnd;
    if(!stopEvent){fail(E_OUTOFMEMORY,L"Engine");return;}
    ResetEvent(stopEvent);wanted=source;state=1;glitches=0;underflows=0;
    thread=std::thread(&Engine::run,this,config);
}
void Engine::run(RouteConfig config){
    HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(co)){fail(co,L"Windows audio");return;}
    {
    auto work=[&](){
        const int active=wanted.load();
        const bool isProgram=active==2&&config.process;
        const auto inputId=active==2&&!config.process?config.feed:config.mic;
        const wchar_t* inputContext=active==2?L"Discord receive line":L"Your microphone";
        auto input=std::make_unique<Capture>();
        Com<IAudioClient> output;Com<IAudioRenderClient> render;
        HANDLE program=nullptr;
        if(isProgram){program=OpenProcess(SYNCHRONIZE,FALSE,config.pid);if(!program){fail(HRESULT_FROM_WIN32(GetLastError()),L"Discord closed");return;}}
        struct Close{HANDLE h;~Close(){if(h)CloseHandle(h);}} close{program};
        auto checked=[&](HRESULT hr,const wchar_t* context){if(FAILED(hr)){if(WaitForSingleObject(stopEvent,0)!=WAIT_OBJECT_0)fail(hr,context);return false;}return true;};
        if(!checked(input->init(inputId,isProgram,config.pid,stopEvent),inputContext))return;
        if(!checked(config.monitor?monitorClient(config.output,output):clientFor(config.output,output),config.monitor?L"SPEAKER output (use a device at 44.1 kHz or higher)":L"Selected audio output"))return;
        auto f=format();
        HRESULT outputInit=output->Initialize(AUDCLNT_SHAREMODE_SHARED,Convert,300000,0,&f,nullptr);
        if(FAILED(outputInit)&&config.monitor){
            // Some drivers advertise RAW but reject its stream/format combination.
            // A fresh media client is required; never reuse a failed Initialize.
            output.put();
            if(!checked(monitorClient(config.output,output,false),L"SPEAKER compatible media output"))return;
            outputInit=output->Initialize(AUDCLNT_SHAREMODE_SHARED,Convert,300000,0,&f,nullptr);
        }
        if(!checked(outputInit,L"Selected audio output"))return;
        if(!checked(output->GetService(__uuidof(IAudioRenderClient),(void**)render.put()),L"Output buffer"))return;
        if(config.monitor){
            Com<IAudioSessionControl> baseSession;
            Com<IAudioSessionControl2> session;
            if(!checked(output->GetService(__uuidof(IAudioSessionControl),(void**)baseSession.put()),L"SPEAKER audio session"))return;
            if(!checked(baseSession->QueryInterface(__uuidof(IAudioSessionControl2),(void**)session.put()),L"SPEAKER session control"))return;
            if(!checked(session->SetDuckingPreference(TRUE),L"SPEAKER volume stability"))return;
        }
        PlayoutBuffer playout;
        UINT32 frames=0;if(!checked(output->GetBufferSize(&frames),L"Output capacity"))return;
        bufferMs=(int)(frames*1000/48000);
        if(!checked(input->start(),inputContext))return;
        if(!checked(output->Start(),L"Selected audio output"))return;
        struct Stop{IAudioClient* client;~Stop(){client->Stop();client->Reset();}} outputStop{output.get()};
        Mmcss priority;SourceGate gate;
        state=2;PostMessageW(target,StateMessage,0,0);
        float correction=0;ULONGLONG adjusted=GetTickCount64(),closeAt=0;
        constexpr size_t targetFrames=PlayoutBuffer::reserveFrames;
#ifdef DISFORWARD_AUDIO_TEST
        const auto testBegin=GetTickCount64();
#endif
        while(true){
            bool stopping=WaitForSingleObject(stopEvent,0)==WAIT_OBJECT_0;
            if(stopping&&!closeAt){wanted=0;closeAt=GetTickCount64()+45;}
            if(closeAt&&GetTickCount64()>=closeAt)break;
            if(program&&WaitForSingleObject(program,0)==WAIT_OBJECT_0){fail(HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED),L"Discord closed. Open Discord and select Friend again");break;}
            bool drainCapture=true;
#ifdef DISFORWARD_AUDIO_TEST
            const auto testElapsed=GetTickCount64()-testBegin;
            drainCapture=!config.capturePauseMs||testElapsed<1000||testElapsed>=1000+config.capturePauseMs;
#endif
            if(drainCapture&&!checked(input->drain(glitches),inputContext))break;
            UINT32 padding=0;if(!checked(output->GetCurrentPadding(&padding),L"DISFORWARD device"))break;
            UINT32 available=frames-padding;
            if(available){
                BYTE* bytes=nullptr;if(!checked(render->GetBuffer(available,&bytes),L"Output buffer"))break;
                auto samples=(float*)bytes;float maxSample=0;
                int requested=stopping?0:wanted.load();
                playout.beginBlock(input->queue,available);
                for(UINT32 i=0;i<available;i++){
                    float l=0,r=0;playout.frame(input->queue,l,r);
                    gate.frame(requested,active==1?l:0,active==1?r:0,active==2?l:0,active==2?r:0,samples[i*2],samples[i*2+1]);
                    maxSample=std::max({maxSample,std::abs(samples[i*2]),std::abs(samples[i*2+1])});
                }
                underflows=playout.recoveries();
                peak=(int)(std::min(maxSample,2.f)*10000);
                if(!checked(render->ReleaseBuffer(available,0),L"Output stream"))break;
            }
            auto now=GetTickCount64();if(now-adjusted>=250){
                float desired=std::clamp(((float)input->queue.size()-(float)targetFrames)*.0000003f,-.001f,.001f);
                correction+=std::clamp(desired-correction,-.00001f,.00001f);
                playout.setRatio(1+correction);
                adjusted=now;
            }
            Sleep(2);
        }
    };
    try{work();}catch(...){fail(E_OUTOFMEMORY,L"Audio engine");}
    }
    if(state.load()!=3)state=0;
    peak=0;CoUninitialize();PostMessageW(target,StateMessage,0,0);
}
}
