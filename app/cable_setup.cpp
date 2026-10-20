#ifndef UNICODE
#define UNICODE
#endif
#include <initguid.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <cstring>
#include <urlmon.h>
#include "receive_setup.h"
namespace {
std::wstring literal(const std::wstring& s){std::wstring r=L"'";for(auto c:s){r+=c;if(c==L'\'')r+=c;}return r+L"'";}
bool original(const BYTE* bytes,DWORD length,bool hifi=false){
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;DWORD size=0,received=0;BYTE digest[32]{};
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 bool ok=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&received,0)>=0;
 std::vector<BYTE> object(size);
 if(ok)ok=BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0)>=0;
 if(ok)ok=BCryptHashData(hash,const_cast<PUCHAR>(bytes),length,0)>=0&&BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);
 constexpr BYTE expected[32]={0xb9,0x50,0xe3,0x9f,0x01,0xaf,0x1d,0x04,0xea,0x62,0x3c,0x8f,0x6d,0x8e,0xb9,0xb6,0xea,0x5c,0x47,0x7c,0x63,0x72,0x95,0xfa,0xbf,0x20,0x63,0x1c,0x85,0x11,0x6b,0xfb};
 constexpr BYTE receive[32]={0x3e,0xcf,0x20,0x4b,0xfd,0x85,0x79,0xd3,0x6b,0xb9,0x18,0xf9,0x85,0x6e,0xb1,0xea,0xdd,0xd4,0x9c,0x75,0x14,0x6f,0x5e,0x8a,0x8f,0x59,0xdc,0xb8,0x37,0x5a,0xe8,0x9a};
 return ok&&memcmp(digest,hifi?receive:expected,sizeof(digest))==0;
}
int error(const wchar_t* text){MessageBoxW(nullptr,text,L"DISFORWARD setup",MB_OK|MB_ICONERROR);return 1;}
}
int installSend(HINSTANCE instance){
 HANDLE lock=CreateMutexW(nullptr,TRUE,L"Global\\DISFORWARD_VBCABLE_SETUP");
 if(!lock||GetLastError()==ERROR_ALREADY_EXISTS){if(lock)CloseHandle(lock);return error(L"DISFORWARD audio setup is already running.");}
 struct Close{HANDLE h;~Close(){CloseHandle(h);}} close{lock};
 if(MessageBoxW(nullptr,L"DISFORWARD uses VB-CABLE by VB-Audio for its microphone output.\n\n1. In the vendor installer, choose Install Driver.\n2. Restart Windows.\n3. Open DISFORWARD and choose your mic in Setup.\n4. Choose CABLE Output as your call app microphone.\n\nKeep your existing call-app playback device. DISFORWARD does not require changing Windows default playback or communications devices.\n\nVB-CABLE: www.vb-cable.com\nDonationware; donations are welcome. Professional use requires the vendor's applicable license.\n\nOpen the original VB-CABLE installer?",L"DISFORWARD · Install audio line",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return 0;
 HRSRC resource=FindResourceW(instance,MAKEINTRESOURCEW(200),RT_RCDATA);if(!resource)return error(L"The bundled vendor package is missing. Download the complete package again.");
 HGLOBAL loaded=LoadResource(instance,resource);const BYTE* bytes=static_cast<const BYTE*>(LockResource(loaded));DWORD length=SizeofResource(instance,resource);
 if(!bytes||!length||!original(bytes,length))return error(L"The vendor archive did not match its official SHA256. Nothing was installed.");
 wchar_t folder[MAX_PATH];if(FAILED(SHGetFolderPathW(nullptr,CSIDL_PROGRAM_FILES,nullptr,0,folder)))return error(L"Could not locate Program Files.");
 std::wstring base=std::wstring(folder)+L"\\DISFORWARD";CreateDirectoryW(base.c_str(),nullptr);base+=L"\\VB-CABLE";CreateDirectoryW(base.c_str(),nullptr);
 std::wstring zip=base+L"\\VBCABLE_Driver_Pack45.zip";
 HANDLE file=CreateFileW(zip.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD written=0;
 bool ok=file!=INVALID_HANDLE_VALUE&&WriteFile(file,bytes,length,&written,nullptr)&&written==length;if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
 if(!ok)return error(L"Could not write the vendor package. Check administrator access and disk space.");
 wchar_t system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);std::wstring powershell=std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
 std::wstring cmd=L"\""+powershell+L"\" -NoLogo -NoProfile -NonInteractive -Command \"Expand-Archive -LiteralPath "+literal(zip)+L" -DestinationPath "+literal(base)+L" -Force\"";
 STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
 if(!CreateProcessW(powershell.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return error(L"Could not expand VB-CABLE. The original archive is in Program Files\\DISFORWARD\\VB-CABLE.");
 WaitForSingleObject(pi.hProcess,INFINITE);DWORD status=1;GetExitCodeProcess(pi.hProcess,&status);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 if(status)return error(L"Windows could not expand the vendor archive. No driver installer was launched.");
 std::wstring installer=base+L"\\VBCABLE_Setup_x64.exe";
 SHELLEXECUTEINFOW info{};info.cbSize=sizeof(info);info.fMask=SEE_MASK_NOCLOSEPROCESS;info.lpFile=installer.c_str();info.lpDirectory=base.c_str();info.nShow=SW_SHOWNORMAL;
 // Launch the original vendor setup visibly. Windows enforces driver signing.
 if(!ShellExecuteExW(&info)||!info.hProcess)return error(L"The VB-CABLE installer could not start. Open it from Program Files\\DISFORWARD\\VB-CABLE.");
 WaitForSingleObject(info.hProcess,INFINITE);CloseHandle(info.hProcess);
 MessageBoxW(nullptr,L"If you installed VB-CABLE, restart Windows before opening DISFORWARD.\n\nAfter restart: Setup → Refresh → choose your physical mic → Check audio line.\nIn your call app, choose CABLE Output as its microphone. The independent Discord receive installer is offered next.\n\nIf the vendor installer showed Remove Driver, VB-CABLE may already be installed. Cancel removal and restart Windows first. Do not repeatedly reinstall it.\n\nThis message does not confirm installation; DISFORWARD checks for the audio endpoints after restart.",L"DISFORWARD · Finish setup",MB_OK|MB_ICONINFORMATION);
 return 0;
}

int installReceive(){
 if(df::hasReceiveLine()){MessageBoxW(nullptr,L"The independent receive line is already installed. The original VB-Audio device names are preserved.\n\nOpen DISFORWARD → Setup → Refresh. In Discord choose Speakers (VB-Audio Hi-Fi Cable) as OUTPUT. Keep your physical mic as Discord INPUT and CABLE Output as the call app microphone.",L"DISFORWARD · Receive line ready",MB_OK|MB_ICONINFORMATION);return 0;}
 if(MessageBoxW(nullptr,L"Install a SECOND, independent VB-Audio Hi-Fi Cable for Discord?\n\nThis downloads the original installer from vb-audio.com. It creates a Hi-Fi Cable Input playback device for Discord and a paired recording device for DISFORWARD. Your existing CABLE Output remains the call app microphone.\n\nIn the original installer click Install, then restart Windows. DISFORWARD negotiates matching 48 kHz receive streams. If a format is locked, close Discord playback and Refresh. Do not enable Windows Listen to this device or start ASIO Bridge.\n\nVB-Audio donationware; professional use requires the applicable vendor license.",L"DISFORWARD · Discord receive line",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return 0;
 wchar_t folder[MAX_PATH];if(FAILED(SHGetFolderPathW(nullptr,CSIDL_PROGRAM_FILES,nullptr,0,folder)))return error(L"Could not locate Program Files.");
 std::wstring base=std::wstring(folder)+L"\\DISFORWARD";CreateDirectoryW(base.c_str(),nullptr);base+=L"\\Discord-Receive";CreateDirectoryW(base.c_str(),nullptr);
 std::wstring zip=base+L"\\HiFiCableAsioBridgeSetup_v1007.zip";
 if(FAILED(URLDownloadToFileW(nullptr,L"https://download.vb-audio.com/Download_CABLE/HiFiCableAsioBridgeSetup_v1007.zip",zip.c_str(),0,nullptr)))return error(L"Could not download the official Hi-Fi Cable installer. Check internet access and try again.");
 HANDLE file=CreateFileW(zip.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
 if(file==INVALID_HANDLE_VALUE)return error(L"Could not read the downloaded installer.");
 DWORD length=GetFileSize(file,nullptr),read=0;std::vector<BYTE> data(length<16000000?length:0);
 bool ok=!data.empty()&&ReadFile(file,data.data(),static_cast<DWORD>(data.size()),&read,nullptr)&&read==length;CloseHandle(file);
 if(!ok||!original(data.data(),read,true))return error(L"The official download did not match the expected SHA256. No installer was launched.");
 wchar_t system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);std::wstring ps=std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
 std::wstring cmd=L"\""+ps+L"\" -NoLogo -NoProfile -NonInteractive -Command \"Expand-Archive -LiteralPath "+literal(zip)+L" -DestinationPath "+literal(base)+L" -Force\"";
 STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
 if(!CreateProcessW(ps.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return error(L"Could not extract the original vendor package.");
 WaitForSingleObject(pi.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 if(code)return error(L"Could not extract the original vendor package.");
 std::wstring exe=base+L"\\HiFiCableAsioBridgeSetup.exe";SHELLEXECUTEINFOW info{};info.cbSize=sizeof(info);info.fMask=SEE_MASK_NOCLOSEPROCESS;info.lpFile=exe.c_str();info.lpDirectory=base.c_str();info.nShow=SW_SHOWNORMAL;
 if(!ShellExecuteExW(&info)||!info.hProcess)return error(L"Could not launch the original Hi-Fi Cable installer.");
 WaitForSingleObject(info.hProcess,INFINITE);CloseHandle(info.hProcess);
 MessageBoxW(nullptr,L"After a successful vendor installation, restart Windows and open DISFORWARD → Setup → Refresh.\n\nDiscord OUTPUT: Speakers (VB-Audio Hi-Fi Cable), also called Hi-Fi Cable Input.\nCall app MICROPHONE: CABLE Output.\nDISFORWARD SPEAKER: choose your headphones or physical speakers.\n\nKeep Discord INPUT on your physical microphone. Do not choose CABLE Output there, or your friend will hear their own forwarded voice.\n\nThis message does not confirm installation; DISFORWARD checks the actual endpoints.",L"DISFORWARD · Finish",MB_OK|MB_ICONINFORMATION);return 0;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int){
 HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 if(FAILED(initialized))return error(L"Could not initialize Windows audio setup.");
 struct CoEnd{~CoEnd(){CoUninitialize();}} coEnd;
 CoInitializeSecurity(nullptr,-1,nullptr,nullptr,RPC_C_AUTHN_LEVEL_DEFAULT,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE,nullptr);
 if(wcscmp(command,L"--receive")==0)return installReceive();
 int answer=MessageBoxW(nullptr,L"Install the VB-CABLE SEND line?\n\nChoose No if CABLE Output is already installed. The independent Discord receive line is offered next.",L"DISFORWARD · Audio setup",MB_YESNOCANCEL|MB_ICONQUESTION);
 if(answer==IDCANCEL)return 0;
 if(answer==IDYES&&installSend(instance))return 1;
 return installReceive();
}
