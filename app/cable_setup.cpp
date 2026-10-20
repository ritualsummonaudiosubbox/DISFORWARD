#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <cstring>
namespace {
std::wstring literal(const std::wstring& s){std::wstring r=L"'";for(auto c:s){r+=c;if(c==L'\'')r+=c;}return r+L"'";}
bool original(const BYTE* bytes,DWORD length){
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;DWORD size=0,received=0;BYTE digest[32]{};
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 bool ok=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&received,0)>=0;
 std::vector<BYTE> object(size);
 if(ok)ok=BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0)>=0;
 if(ok)ok=BCryptHashData(hash,const_cast<PUCHAR>(bytes),length,0)>=0&&BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);
 constexpr BYTE expected[32]={0xb9,0x50,0xe3,0x9f,0x01,0xaf,0x1d,0x04,0xea,0x62,0x3c,0x8f,0x6d,0x8e,0xb9,0xb6,0xea,0x5c,0x47,0x7c,0x63,0x72,0x95,0xfa,0xbf,0x20,0x63,0x1c,0x85,0x11,0x6b,0xfb};
 return ok&&memcmp(digest,expected,sizeof(digest))==0;
}
int error(const wchar_t* text){MessageBoxW(nullptr,text,L"DISFORWARD setup",MB_OK|MB_ICONERROR);return 1;}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
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
 MessageBoxW(nullptr,L"If you installed VB-CABLE, restart Windows before opening DISFORWARD.\n\nAfter restart: Setup → Refresh → choose your physical mic → Check audio line.\nIn your call app, choose CABLE Output as its microphone. Leave Discord output on headphones.\n\nIf the vendor installer showed Remove Driver, VB-CABLE may already be installed. Cancel removal and restart Windows first. Do not repeatedly reinstall it.\n\nThis message does not confirm installation; DISFORWARD checks for the audio endpoints after restart.",L"DISFORWARD · Finish setup",MB_OK|MB_ICONINFORMATION);
 return 0;
}
