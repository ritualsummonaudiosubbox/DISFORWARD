#include "../app/route_devices.h"
#include <cassert>
#include <iostream>
int main(){
 using df::Endpoint;
 Endpoint stereo{L"s",L"CABLE Input (VB-Audio Virtual Cable)",L"vb",L"VB-Audio Virtual Cable",L""};
 Endpoint wide{L"w",L"CABLE In 16 Ch (VB-Audio Virtual Cable)",L"vb",L"VB-Audio Virtual Cable",L""};
 Endpoint mic{L"m",L"CABLE Output (VB-Audio Virtual Cable)",L"vb",L"VB-Audio Virtual Cable",L""};
 Endpoint native{L"n",L"DISFORWARD",L"native",L"DISFORWARD",L"DISFORWARD.Native.1"};
 Endpoint hardware{L"h",L"USB Mic",L"usb",L"USB Audio",L""},out,in;
 assert(df::cablePair({wide,stereo},{native,mic,hardware},out,in)&&out.id==L"s"&&in.id==L"m");
 assert(!df::cablePair({stereo},{native,hardware},out,in));
 Endpoint mismatch=mic;mismatch.container=L"other";assert(!df::cablePair({stereo},{mismatch},out,in));
 Endpoint renamed=stereo;renamed.name=L"DISFORWARD Send";Endpoint renamedMic=mic;renamedMic.name=L"DISFORWARD";
 assert(df::cablePair({renamed},{renamedMic},out,in)&&out.id==L"s");
 Endpoint impostor=renamed;impostor.driver=L"Unrelated";assert(!df::cablePair({impostor},{mic},out,in));
 assert(!df::standardCable(native)&&!df::standardCable(hardware));
 Endpoint hiOut{L"hi-out",L"Hi-Fi Cable Input (VB-Audio Hi-Fi Cable)",L"hifi",L"VB-Audio Hi-Fi Cable",L""};
 Endpoint hiIn{L"hi-in",L"Hi-Fi Cable Output (VB-Audio Hi-Fi Cable)",L"hifi",L"VB-Audio Hi-Fi Cable",L""};
 assert(df::receivePair({stereo,hiOut},{mic,hiIn},out,in)&&out.id==hiOut.id&&in.id==hiIn.id);
 assert(!df::receivePair({stereo,wide},{mic},out,in));
 auto wrong=hiIn;wrong.container=L"unrelated";assert(!df::receivePair({hiOut},{wrong},out,in));
 assert(!df::standardCable(hiOut)&&!df::receiveCable(stereo));
 std::cout<<"PASS: standard VB-CABLE identification, stereo preference, recording pair, container mismatch, renamed legacy pair and impostor rejection.\n";
}
