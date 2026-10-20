// CI-only: install the original signed INF on the disposable Windows runner.
#include <windows.h>
#include <setupapi.h>
#include <newdev.h>
#include <iostream>
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 2;
 SP_INF_SIGNER_INFO_W signer{};signer.cbSize=sizeof(signer);
 if(!SetupVerifyInfFileW(argv[1],nullptr,&signer)){std::wcerr<<L"Catalog verification failed: "<<GetLastError()<<L"\n";return 1;}
 std::wcout<<L"Catalog verified. Signer: "<<signer.DigitalSigner<<L"\n";
 const GUID media={0x4d36e96c,0xe325,0x11ce,{0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18}};
 constexpr wchar_t hardware[]=L"VBAudioVACWDM\0";
 HDEVINFO set=SetupDiCreateDeviceInfoList(&media,nullptr);SP_DEVINFO_DATA dev{};dev.cbSize=sizeof(dev);
 if(set==INVALID_HANDLE_VALUE)return 1;
 if(!SetupDiCreateDeviceInfoW(set,L"VBAudioVACWDM",&media,L"VB-Audio Virtual Cable",nullptr,DICD_GENERATE_ID,&dev)||!SetupDiSetDeviceRegistryPropertyW(set,&dev,SPDRP_HARDWAREID,reinterpret_cast<const BYTE*>(hardware),sizeof(hardware))||!SetupDiCallClassInstaller(DIF_REGISTERDEVICE,set,&dev)){
  std::wcerr<<L"Device registration failed: "<<GetLastError()<<L"\n";SetupDiDestroyDeviceInfoList(set);return 1;
 }
 BOOL reboot=FALSE;
 BOOL ok=UpdateDriverForPlugAndPlayDevicesW(nullptr,hardware,argv[1],INSTALLFLAG_FORCE|INSTALLFLAG_NONINTERACTIVE,&reboot);
 if(!ok){std::wcerr<<L"Driver install failed: "<<GetLastError()<<L"\n";SetupDiCallClassInstaller(DIF_REMOVE,set,&dev);}
 SetupDiDestroyDeviceInfoList(set);
 if(!ok)return 1;
 std::wcout<<L"Vendor driver installed; restart requested: "<<reboot<<L"\n";return 0;
}
