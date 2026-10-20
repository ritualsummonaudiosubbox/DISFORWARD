#pragma once
#include "engine.h"
namespace df {
// Friendly endpoint labels only. The original signed driver and interface
// identity remain VB-Audio's. No default-device or policy changes are made.
inline unsigned nameReceiveEndpoints(){
 Com<IMMDeviceEnumerator> en;
 if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)en.put())))return 0;
 unsigned named=0;
 for(auto flow:{eRender,eCapture}){
  Com<IMMDeviceCollection> collection;
  if(FAILED(en->EnumAudioEndpoints(flow,DEVICE_STATE_ACTIVE,collection.put())))continue;
  UINT count=0;collection->GetCount(&count);
  for(UINT i=0;i<count;++i){
   Com<IMMDevice> device;Com<IPropertyStore> properties;
   if(FAILED(collection->Item(i,device.put()))||FAILED(device->OpenPropertyStore(STGM_READWRITE,properties.put())))continue;
   PROPVARIANT vendor;PropVariantInit(&vendor);
   if(FAILED(properties->GetValue(PKEY_DeviceInterface_FriendlyName,&vendor)))continue;
   bool hifi=vendor.vt==VT_LPWSTR&&upperName(vendor.pwszVal)==L"VB-AUDIO HI-FI CABLE";
   PropVariantClear(&vendor);if(!hifi)continue;
   PROPVARIANT name;PropVariantInit(&name);name.vt=VT_LPWSTR;
   name.pwszVal=const_cast<LPWSTR>(flow==eRender?L"DISFORWARD Discord In (VB-Audio Hi-Fi Cable)":L"DISFORWARD Discord Audio (VB-Audio Hi-Fi Cable)");
   if(SUCCEEDED(properties->SetValue(PKEY_Device_FriendlyName,name))&&SUCCEEDED(properties->Commit()))++named;
  }
 }
 return named;
}
}
