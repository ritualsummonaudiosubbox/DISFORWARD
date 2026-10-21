#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include "route_devices.h"

namespace df {
template<class T> class Com {
    T* p=nullptr;
public:
    ~Com(){if(p)p->Release();}
    Com()=default;Com(const Com&)=delete;Com& operator=(const Com&)=delete;
    T* get()const{return p;} T** put(){if(p){p->Release();p=nullptr;}return &p;}
    T* operator->()const{return p;} explicit operator bool()const{return p!=nullptr;}
};
struct Program {DWORD pid;std::wstring label,exe;};
std::vector<Endpoint> endpoints(EDataFlow flow);
std::vector<Program> programs();
bool sameCable(const Endpoint &input,const Endpoint &output);
bool virtualCable(const Endpoint &e);
bool supportsProcessCapture();
std::wstring receiveFormatIssue(const Endpoint& playback,const Endpoint& capture);
std::wstring hrMessage(HRESULT hr);
std::wstring testVirtualLine(const Endpoint& playback,const Endpoint& microphone,HANDLE cancel);
struct RouteConfig {std::wstring mic,feed,output;DWORD pid=0;bool process=true,monitor=false;
#ifdef DISFORWARD_AUDIO_TEST
 unsigned capturePauseMs=0;
#endif
};
class Engine {
    HWND target=nullptr;
    HANDLE stopEvent=nullptr;
    std::thread thread;
    std::mutex messageMutex;
    std::wstring detail;
    void run(RouteConfig config);
    void fail(HRESULT hr,const std::wstring& context);
public:
    static constexpr UINT StateMessage=WM_APP+40;
    std::atomic<int> wanted{0},state{0},peak{0},bufferMs{0};
    std::atomic<unsigned long> glitches{0},underflows{0};
    Engine();~Engine();
    void start(HWND hwnd,const RouteConfig &config,int source);
    void stop();
    std::wstring message();
};
}
