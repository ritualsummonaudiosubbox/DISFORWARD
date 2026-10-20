#pragma once
#include "engine.h"
#include <cfgmgr32.h>
#include <devpkey.h>
namespace df {
// Friendly endpoint labels only. The original signed driver and interface
// identity remain VB-Audio's. No default-device or policy changes are made.
inline unsigned nameReceiveEndpoints(HRESULT* lastError=nullptr){
 if(lastError)*lastError=S_FALSE;
 Com<IMMDeviceEnumerator> en;
 if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)en.put())))return 0;
 unsigned named=0;
 for(auto flow:{eRender,eCapture}){
  Com<IMMDeviceCollection> collection;
  if(FAILED(en->EnumAudioEndpoints(flow,DEVICE_STATE_ACTIVE,collection.put())))continue;
  UINT count=0;collection->GetCount(&count);
  for(UINT i=0;i<count;++i){
   Com<IMMDevice> device;Com<IPropertyStore> properties;
   if(FAILED(collection->Item(i,device.put()))||FAILED(device->OpenPropertyStore(STGM_READ,properties.put())))continue;
   PROPVARIANT vendor;PropVariantInit(&vendor);
   if(FAILED(properties->GetValue(PKEY_DeviceInterface_FriendlyName,&vendor)))continue;
   bool hifi=vendor.vt==VT_LPWSTR&&upperName(vendor.pwszVal)==L"VB-AUDIO HI-FI CABLE";
   PropVariantClear(&vendor);if(!hifi)continue;
   HRESULT result=device->OpenPropertyStore(STGM_READWRITE,properties.put());
   if(lastError)*lastError=result;
   PROPVARIANT name;PropVariantInit(&name);name.vt=VT_LPWSTR;
   name.pwszVal=const_cast<LPWSTR>(flow==eRender?L"DISFORWARD Discord In (VB-Audio Hi-Fi Cable)":L"DISFORWARD Discord Audio (VB-Audio Hi-Fi Cable)");
   if(SUCCEEDED(result))result=properties->SetValue(PKEY_Device_FriendlyName,name);
   if(SUCCEEDED(result))result=properties->Commit();
   if(FAILED(result)){
    // The endpoint manager can protect its store on newer Windows builds.
    // Try the documented PnP friendly-name property, without changing ACLs.
    LPWSTR id=nullptr;
    if(SUCCEEDED(device->GetId(&id))){
     std::wstring instance=L"SWD\\MMDEVAPI\\"+std::wstring(id);CoTaskMemFree(id);
     DEVINST node=0;CONFIGRET cr=CM_Locate_DevNodeW(&node,instance.data(),CM_LOCATE_DEVNODE_NORMAL);
     if(cr==CR_SUCCESS)cr=CM_Set_DevNode_PropertyW(node,&DEVPKEY_Device_FriendlyName,DEVPROP_TYPE_STRING,reinterpret_cast<const BYTE*>(name.pwszVal),static_cast<ULONG>((wcslen(name.pwszVal)+1)*sizeof(wchar_t)),0);
     result=cr==CR_SUCCESS?S_OK:HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr,ERROR_ACCESS_DENIED));
    }
   }
   if(lastError)*lastError=result;
   if(SUCCEEDED(result))++named;
  }
 }
 return named;
}
}
