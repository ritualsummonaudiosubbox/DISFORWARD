#pragma once
#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>
namespace df {
struct Endpoint {std::wstring id,name,container,driver,identity;};
inline std::wstring upperName(std::wstring s){for(auto &c:s)c=static_cast<wchar_t>(std::towupper(c));return s;}
inline bool standardCable(const Endpoint &e){
 const auto vendor=upperName(e.driver),name=upperName(e.name);
 return e.identity!=L"DISFORWARD.Native.1"&&(vendor==L"VB-AUDIO VIRTUAL CABLE"||name.find(L"(VB-AUDIO VIRTUAL CABLE)")!=std::wstring::npos);
}
inline bool receiveCable(const Endpoint& e){
 const auto v=upperName(e.driver),n=upperName(e.name);
 return v==L"VB-AUDIO HI-FI CABLE"||n.find(L"(VB-AUDIO HI-FI CABLE)")!=std::wstring::npos;
}
inline bool receivePair(const std::vector<Endpoint>& outputs,const std::vector<Endpoint>& inputs,Endpoint& playback,Endpoint& capture){
 for(const auto& out:outputs)for(const auto& in:inputs){
  if(!receiveCable(out)||!receiveCable(in))continue;
  if(!out.container.empty()&&!in.container.empty()&&out.container!=in.container)continue;
  playback=out;capture=in;return true;
 }
 return false;
}
inline bool wideCable(const Endpoint& e){auto n=upperName(e.name);return n.find(L"16CH")!=std::wstring::npos||n.find(L"16 CH")!=std::wstring::npos;}
inline bool cablePair(const std::vector<Endpoint>& outputs,const std::vector<Endpoint>& inputs,Endpoint& send,Endpoint& mic){
 // Pack45 exposes a normal stereo input and a 16-channel input; prefer stereo.
 std::vector<Endpoint> ordered;
 for(const auto& e:outputs)if(standardCable(e))ordered.push_back(e);
 std::stable_sort(ordered.begin(),ordered.end(),[](const Endpoint& a,const Endpoint& b){return wideCable(a)<wideCable(b);});
 for(const auto& out:ordered)for(const auto& in:inputs){
  if(!standardCable(in))continue;
  if(!out.container.empty()&&!in.container.empty()&&out.container!=in.container)continue;
  send=out;mic=in;return true;
 }
 return false;
}
}
