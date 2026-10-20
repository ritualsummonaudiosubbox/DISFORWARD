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
 std::cout<<"PASS: standard VB-CABLE identification, stereo preference, recording pair, container mismatch, renamed legacy pair and impostor rejection.\n";
}
