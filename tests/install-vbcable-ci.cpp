// CI-only: verify/install an original signed vendor INF on a disposable runner.
#include <windows.h>
#include <setupapi.h>
#include <newdev.h>
#include <iostream>
#include <string>
int wmain(int argc,wchar_t** argv){
 if(argc<2||argc>3)return 2;
 SP_INF_SIGNER_INFO_W signer{};signer.cbSize=sizeof(signer);
 if(!SetupVerifyInfFileW(argv[1],nullptr,&signer)){
  DWORD error=GetLastError();
  // Documented SetupAPI success category for an Authenticode (non-WHQL)
  // signed package whose publisher is already trusted. Other errors fail.
  if(error!=ERROR_AUTHENTICODE_TRUSTED_PUBLISHER){std::wcerr<<L"Catalog verification failed: "<<error<<L"\n";return 1;}
 }
 std::wcout<<L"Catalog verified. Signer: "<<signer.DigitalSigner<<L"\n";
 const GUID media={0x4d36e96c,0xe325,0x11ce,{0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18}};
 std::wstring hardware=argc==3?argv[2]:L"VBAudioVACWDM";
 HDEVINFO set=SetupDiGetClassDevsW(&media,nullptr,nullptr,DIGCF_PRESENT);
 if(set==INVALID_HANDLE_VALUE)return 1;
 bool exists=false;SP_DEVINFO_DATA dev{};dev.cbSize=sizeof(dev);
 for(DWORD i=0;SetupDiEnumDeviceInfo(set,i,&dev);++i){
  wchar_t ids[2048]{};DWORD type=0;
  if(SetupDiGetDeviceRegistryPropertyW(set,&dev,SPDRP_HARDWAREID,&type,reinterpret_cast<BYTE*>(ids),sizeof(ids)-sizeof(wchar_t),nullptr)){
   for(const wchar_t* id=ids;*id;id+=wcslen(id)+1)if(_wcsicmp(id,hardware.c_str())==0)exists=true;
  }
 }
 SetupDiDestroyDeviceInfoList(set);
 bool created=false;
 set=SetupDiCreateDeviceInfoList(&media,nullptr);dev={};dev.cbSize=sizeof(dev);
 if(set==INVALID_HANDLE_VALUE)return 1;
 if(!exists){
  std::wstring multi=hardware;multi.push_back(L'\0');multi.push_back(L'\0');
  if(!SetupDiCreateDeviceInfoW(set,hardware.c_str(),&media,L"Original VB-Audio driver",nullptr,DICD_GENERATE_ID,&dev)||!SetupDiSetDeviceRegistryPropertyW(set,&dev,SPDRP_HARDWAREID,reinterpret_cast<const BYTE*>(multi.data()),static_cast<DWORD>(multi.size()*sizeof(wchar_t)))||!SetupDiCallClassInstaller(DIF_REGISTERDEVICE,set,&dev)){
   std::wcerr<<L"Device registration failed: "<<GetLastError()<<L"\n";SetupDiDestroyDeviceInfoList(set);return 1;
  }created=true;
 }
 BOOL reboot=FALSE;
 BOOL ok=UpdateDriverForPlugAndPlayDevicesW(nullptr,hardware.c_str(),argv[1],INSTALLFLAG_FORCE|INSTALLFLAG_NONINTERACTIVE,&reboot);
 if(!ok){std::wcerr<<L"Driver install failed: "<<GetLastError()<<L"\n";if(created)SetupDiCallClassInstaller(DIF_REMOVE,set,&dev);}
 SetupDiDestroyDeviceInfoList(set);
 if(!ok)return 1;
 std::wcout<<L"Vendor driver installed; restart requested: "<<reboot<<L"\n";return 0;
}
