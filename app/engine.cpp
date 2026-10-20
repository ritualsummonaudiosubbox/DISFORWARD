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
// Newer SDK ABI declarations; kept local to build with MinGW-w64 11.
struct ProcessParams {int type=1;DWORD pid=0;int mode=0;};
static_assert(sizeof(ProcessParams)==12,"Windows process capture ABI");
class Activation final : public IActivateAudioInterfaceCompletionHandler {
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
    StereoQueue queue;
    HRESULT init(const std::wstring &id,bool process,DWORD pid,HANDLE cancel){
        auto hr=process?processClient(pid,cancel,client):clientFor(id,client);if(FAILED(hr))return hr;
        auto f=format();DWORD flags=Convert|(process?AUDCLNT_STREAMFLAGS_LOOPBACK:0);
        hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,flags,process?0:200000,0,&f,nullptr);
        if(SUCCEEDED(hr))hr=client->GetService(__uuidof(IAudioCaptureClient),(void**)service.put());
        return hr;
    }
    HRESULT start(){auto hr=client->Start();started=SUCCEEDED(hr);return hr;}
    ~Capture(){if(started)client->Stop();}
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
bool virtualCable(const Endpoint &e){auto n=upper(e.name);return n.find(L"CABLE")!=std::wstring::npos||n.find(L"DISFORWARD")!=std::wstring::npos||n.find(L"VIRTUAL")!=std::wstring::npos;}
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
    default:return std::wstring(L"Audio could not start (")+code+L"). Check devices; check the native driver and Discord version.";
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
        const bool isProgram=active==2;
        auto input=std::make_unique<Capture>();
        Com<IAudioClient> output;Com<IAudioRenderClient> render;Com<IAudioClockAdjustment> clock;
        HANDLE program=nullptr;
        if(isProgram){program=OpenProcess(SYNCHRONIZE,FALSE,config.pid);if(!program){fail(HRESULT_FROM_WIN32(GetLastError()),L"Discord closed");return;}}
        struct Close{HANDLE h;~Close(){if(h)CloseHandle(h);}} close{program};
        auto checked=[&](HRESULT hr,const wchar_t* context){if(FAILED(hr)){if(WaitForSingleObject(stopEvent,0)!=WAIT_OBJECT_0)fail(hr,context);return false;}return true;};
        if(!checked(input->init(config.mic,isProgram,config.pid,stopEvent),isProgram?L"Discord playback":L"Your microphone"))return;
        if(!checked(clientFor(config.output,output),L"DISFORWARD native device"))return;
        auto f=format();
        if(!checked(output->Initialize(AUDCLNT_SHAREMODE_SHARED,Convert|AUDCLNT_STREAMFLAGS_RATEADJUST,300000,0,&f,nullptr),L"DISFORWARD native output"))return;
        if(!checked(output->GetService(__uuidof(IAudioRenderClient),(void**)render.put()),L"Output buffer"))return;
        output->GetService(__uuidof(IAudioClockAdjustment),(void**)clock.put());
        UINT32 frames=0;if(!checked(output->GetBufferSize(&frames),L"Output capacity"))return;
        bufferMs=(int)(frames*1000/48000);
        if(!checked(input->start(),isProgram?L"Discord playback":L"Your microphone"))return;
        if(!checked(output->Start(),L"DISFORWARD native output"))return;
        struct Stop{IAudioClient* client;~Stop(){client->Stop();client->Reset();}} outputStop{output.get()};
        Mmcss priority;SourceGate gate;
        state=2;PostMessageW(target,StateMessage,0,0);
        bool primed=false;float correction=0;ULONGLONG adjusted=GetTickCount64(),closeAt=0;
        constexpr size_t targetFrames=1440;
        while(true){
            bool stopping=WaitForSingleObject(stopEvent,0)==WAIT_OBJECT_0;
            if(stopping&&!closeAt){wanted=0;closeAt=GetTickCount64()+45;}
            if(closeAt&&GetTickCount64()>=closeAt)break;
            if(program&&WaitForSingleObject(program,0)==WAIT_OBJECT_0){fail(HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED),L"Discord closed. Open Discord and select Friend again");break;}
            if(!checked(input->drain(glitches),isProgram?L"Discord stream":L"Microphone stream"))break;
            UINT32 padding=0;if(!checked(output->GetCurrentPadding(&padding),L"DISFORWARD device"))break;
            UINT32 available=frames-padding;
            if(available){
                BYTE* bytes=nullptr;if(!checked(render->GetBuffer(available,&bytes),L"Output buffer"))break;
                auto samples=(float*)bytes;float maxSample=0;
                int requested=stopping?0:wanted.load();
                if(!primed&&input->queue.size()>=targetFrames)primed=true;
                bool missing=false;
                for(UINT32 i=0;i<available;i++){
                    float l=0,r=0;bool has=primed&&input->queue.pop(l,r);
                    if(primed&&requested&&!has)missing=true;
                    gate.frame(primed?requested:0,active==1?l:0,active==1?r:0,active==2?l:0,active==2?r:0,samples[i*2],samples[i*2+1]);
                    maxSample=std::max({maxSample,std::abs(samples[i*2]),std::abs(samples[i*2+1])});
                }
                if(missing)++underflows;
                peak=(int)(std::min(maxSample,2.f)*10000);
                if(!checked(render->ReleaseBuffer(available,0),L"Output stream"))break;
            }
            auto now=GetTickCount64();if(clock&&now-adjusted>=250){
                float desired=std::clamp(((float)input->queue.size()-(float)targetFrames)*.0000003f,-.001f,.001f);
                correction+=std::clamp(desired-correction,-.00001f,.00001f);
                if(!checked(clock->SetSampleRate(48000.f*(1+correction)),L"Output clock"))break;
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
