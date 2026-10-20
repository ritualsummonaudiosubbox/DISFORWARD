#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <setupapi.h>
#include <newdev.h>
#include <string>
#include <vector>
#include <cwchar>
namespace {
const GUID MediaClass={0x4d36e96c,0xe325,0x11ce,{0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18}};
constexpr wchar_t HardwareId[]=L"ROOT\\DISFORWARD\0";
std::wstring errorText(DWORD error){
 wchar_t code[32];swprintf(code,32,L"0x%08lX",static_cast<unsigned long>(error));
 LPWSTR description=nullptr;
 FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,error,0,reinterpret_cast<LPWSTR>(&description),0,nullptr);
 std::wstring text=code;if(description){text+=L": ";text+=description;LocalFree(description);}return text;
}
bool present(){
 HDEVINFO set=SetupDiGetClassDevsW(&MediaClass,nullptr,nullptr,DIGCF_PRESENT);if(set==INVALID_HANDLE_VALUE)return false;
 SP_DEVINFO_DATA d{};d.cbSize=sizeof(d);bool found=false;
 for(DWORD i=0;SetupDiEnumDeviceInfo(set,i,&d);i++){
  DWORD bytes=0,type=0;SetupDiGetDeviceRegistryPropertyW(set,&d,SPDRP_HARDWAREID,&type,nullptr,0,&bytes);
  if(!bytes)continue;
  std::vector<BYTE> data(bytes+sizeof(wchar_t)*2);
  if(SetupDiGetDeviceRegistryPropertyW(set,&d,SPDRP_HARDWAREID,&type,data.data(),bytes,nullptr)&&type==REG_MULTI_SZ){
   for(const wchar_t* id=(wchar_t*)data.data();*id;id+=wcslen(id)+1)if(_wcsicmp(id,HardwareId)==0){found=true;break;}
  }
  if(found)break;
 }
 SetupDiDestroyDeviceInfoList(set);return found;
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int){
 wchar_t path[32768];if(!GetModuleFileNameW(nullptr,path,32768))return 1;
 std::wstring folder=path;folder.erase(folder.find_last_of(L"\\/")+1);folder+=L"driver\\";
 auto inf=folder+L"DisforwardAudio.inf",sys=folder+L"DisforwardAudio.sys",cat=folder+L"DisforwardAudio.cat";
 for(auto &file:{inf,sys,cat})if(GetFileAttributesW(file.c_str())==INVALID_FILE_ATTRIBUTES){MessageBoxW(nullptr,L"The native signed driver package is incomplete. DisforwardAudio.inf, .sys and .cat must be in the driver folder beside this installer. No device was created.",L"DISFORWARD · Driver missing",MB_OK|MB_ICONERROR);return 1;}
 SP_INF_SIGNER_INFO_W signer{};signer.cbSize=sizeof(signer);
 if(!SetupVerifyInfFileW(inf.c_str(),nullptr,&signer)){
  DWORD error=GetLastError();
  auto report=L"The DISFORWARD driver catalog could not be verified. No device was created.\n\nWindows error "+errorText(error)+L"\n\nThe package may be unsigned, altered or signed by an untrusted publisher. A Microsoft-signed driver package is required for normal Windows installation.";
  MessageBoxW(nullptr,report.c_str(),L"DISFORWARD · Driver signature required",MB_OK|MB_ICONERROR);return 1;
 }
 if(MessageBoxW(nullptr,L"Install or update the native DISFORWARD microphone and send endpoint?\n\nThis driver has its own ROOT\\DISFORWARD hardware identity. It does not rename or reuse third-party audio devices. Windows will enforce its normal driver-signing policy. No signing/security setting or default audio device will be changed by this installer.",L"DISFORWARD native driver",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return 0;
 HDEVINFO set=INVALID_HANDLE_VALUE;SP_DEVINFO_DATA device{};device.cbSize=sizeof(device);bool created=false;
 if(!present()){
  set=SetupDiCreateDeviceInfoList(&MediaClass,nullptr);
  if(set==INVALID_HANDLE_VALUE||!SetupDiCreateDeviceInfoW(set,L"DISFORWARD",&MediaClass,L"DISFORWARD Audio",nullptr,DICD_GENERATE_ID,&device)||!SetupDiSetDeviceRegistryPropertyW(set,&device,SPDRP_HARDWAREID,(const BYTE*)HardwareId,sizeof(HardwareId))||!SetupDiCallClassInstaller(DIF_REGISTERDEVICE,set,&device)){
   if(set!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(set);
   MessageBoxW(nullptr,L"Windows could not register the native device. Run this installer as administrator.",L"DISFORWARD driver",MB_OK|MB_ICONERROR);return 1;
  }
  created=true;
 }
 BOOL reboot=FALSE;BOOL installed=UpdateDriverForPlugAndPlayDevicesW(nullptr,HardwareId,inf.c_str(),INSTALLFLAG_FORCE,&reboot);
 DWORD error=GetLastError();
 if(!installed&&created)SetupDiCallClassInstaller(DIF_REMOVE,set,&device);
 if(set!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(set);
 if(!installed){auto report=L"Windows rejected the native driver installation.\n\n"+errorText(error)+L"\n\nDetails: %WINDIR%\\inf\\setupapi.dev.log\nThe package must be signed for this Windows system.";MessageBoxW(nullptr,report.c_str(),L"DISFORWARD driver",MB_OK|MB_ICONERROR);return 1;}
 MessageBoxW(nullptr,reboot?L"Windows reports the driver installed. Restart Windows before testing DISFORWARD.":L"Windows reports the driver installed. Open DISFORWARD and Refresh. Verify the microphone using a test call before customer use.",L"DISFORWARD driver",MB_OK|MB_ICONINFORMATION);return 0;
}
