#pragma once
#include "engine.h"
namespace df {
inline bool hasReceiveLine(){
 Com<IMMDeviceEnumerator> en;
 if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)en.put())))return false;
 for(auto flow:{eRender,eCapture}){
  bool found=false;Com<IMMDeviceCollection> collection;
  if(FAILED(en->EnumAudioEndpoints(flow,DEVICE_STATE_ACTIVE,collection.put())))return false;
  UINT count=0;collection->GetCount(&count);
  for(UINT i=0;i<count;++i){
   Com<IMMDevice> device;Com<IPropertyStore> properties;
   if(FAILED(collection->Item(i,device.put()))||FAILED(device->OpenPropertyStore(STGM_READ,properties.put())))continue;
   PROPVARIANT vendor;PropVariantInit(&vendor);
   if(SUCCEEDED(properties->GetValue(PKEY_DeviceInterface_FriendlyName,&vendor)))found=found||(vendor.vt==VT_LPWSTR&&vendor.pwszVal&&upperName(vendor.pwszVal)==L"VB-AUDIO HI-FI CABLE");
   PropVariantClear(&vendor);
  }
  if(!found)return false;
 }
 return true;
}
}
