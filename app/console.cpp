#include "engine.h"
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <cwctype>
#include <cmath>
using namespace Gdiplus;
namespace {
HWND window,microphone,listening;HFONT uiFont;HBRUSH field;
df::Engine engine,monitor;
bool speaker=false,receive=false;
bool pinned=false,setup=false,preview=false;int source=0;float scale=1;
std::wstring message=L"Muted",ini;
std::vector<df::Endpoint> microphones,speakers;
df::Endpoint cableSend,cableMic,discordPlayback,discordCapture;
bool cable=false;
int px(double n){return (int)std::lround(n*scale);}
Color c(int r,int g,int b){return Color(255,static_cast<BYTE>(r),static_cast<BYTE>(g),static_cast<BYTE>(b));}
void label(Graphics &g,const std::wstring& s,float x,float y,float w,float h,float size,Color color,bool bold=false,bool centered=false){
 FontFamily family(L"Segoe UI");Font font(&family,size,bold?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush brush(color);StringFormat f;f.SetTrimming(StringTrimmingEllipsisCharacter);f.SetFormatFlags(StringFormatFlagsNoWrap);f.SetLineAlignment(StringAlignmentCenter);if(centered)f.SetAlignment(StringAlignmentCenter);g.DrawString(s.c_str(),-1,&font,RectF(x,y,w,h),&f,&brush);
}
void box(Graphics &g,float x,float y,float w,float h,Color color){GraphicsPath path;const float d=std::min(12.f,std::min(w,h));path.AddArc(x,y,d,d,180,90);path.AddArc(x+w-d,y,d,d,270,90);path.AddArc(x+w-d,y+h-d,d,d,0,90);path.AddArc(x,y+h-d,d,d,90,90);path.CloseFigure();SolidBrush b(color);g.FillPath(&b,&path);}
void redraw(){InvalidateRect(window,nullptr,FALSE);for(int id:{101,102,103,104,110})InvalidateRect(GetDlgItem(window,id),nullptr,FALSE);}
std::wstring selectedMic(){int i=(int)SendMessageW(microphone,CB_GETCURSEL,0,0);return i>=0&&i<(int)microphones.size()?microphones[i].id:L"";}
std::wstring selectedSpeaker(){int i=(int)SendMessageW(listening,CB_GETCURSEL,0,0);return i>=0&&i<(int)speakers.size()?speakers[i].id:L"";}
void save(){if(!preview){WritePrivateProfileStringW(L"Devices",L"Mic",selectedMic().c_str(),ini.c_str());WritePrivateProfileStringW(L"Devices",L"Speaker",selectedSpeaker().c_str(),ini.c_str());}}
void listen(bool enabled){
 monitor.stop();speaker=false;
 if(preview){speaker=enabled;redraw();return;}
 if(enabled){
  auto out=selectedSpeaker();
  if(!receive||out.empty()){MessageBoxW(window,L"Install the Discord receive line and choose your headphones or speakers in Setup.",L"DISFORWARD · Speaker",MB_OK|MB_ICONINFORMATION);setup=true;return;}
  df::RouteConfig route;route.process=false;route.feed=discordCapture.id;route.output=out;
  speaker=true;monitor.start(nullptr,route,2);
 }
 redraw();
}
void layout(){for(int id:{120,121,130,131,132,133})ShowWindow(GetDlgItem(window,id),setup?SW_SHOW:SW_HIDE);SetWindowPos(window,nullptr,0,0,px(360),px(setup?462:174),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);redraw();}
void refresh(){
 save();engine.stop();monitor.stop();speaker=false;source=0;cable=false;receive=false;microphones.clear();speakers.clear();SendMessageW(listening,CB_RESETCONTENT,0,0);SendMessageW(microphone,CB_RESETCONTENT,0,0);
 auto inputs=df::endpoints(eCapture),outputs=df::endpoints(eRender);
 cable=df::cablePair(outputs,inputs,cableSend,cableMic);
 receive=df::receivePair(outputs,inputs,discordPlayback,discordCapture);
 wchar_t saved[4096]{};GetPrivateProfileStringW(L"Devices",L"Mic",L"",saved,4096,ini.c_str());int current=-1;
 for(auto &e:inputs){if(e.identity==L"DISFORWARD.Native.1"||df::virtualCable(e))continue;SendMessageW(microphone,CB_ADDSTRING,0,(LPARAM)e.name.c_str());if(e.id==saved)current=(int)microphones.size();microphones.push_back(e);}
 if(current<0&&!microphones.empty())current=0;
 SendMessageW(microphone,CB_SETCURSEL,current,0);
 wchar_t savedSpeaker[4096]{};GetPrivateProfileStringW(L"Devices",L"Speaker",L"",savedSpeaker,4096,ini.c_str());current=-1;
 for(auto &e:outputs){if(df::virtualCable(e))continue;SendMessageW(listening,CB_ADDSTRING,0,(LPARAM)e.name.c_str());if(e.id==savedSpeaker)current=(int)speakers.size();speakers.push_back(e);}
 if(current<0&&!speakers.empty())current=0;
 SendMessageW(listening,CB_SETCURSEL,current,0);
 message=cable&&receive?L"Muted · choose who speaks":L"Setup needed · install audio lines";redraw();
}
void choose(int wanted){
 if(preview){source=wanted;message=wanted==1?L"Your microphone":wanted==2?L"Discord friend":L"Muted";redraw();return;}
 if(!wanted){engine.stop();source=0;message=L"Muted";redraw();return;}
 engine.stop();source=0;
 if(!cable){message=L"Setup needed · install VB-CABLE";setup=true;layout();return;}
 df::RouteConfig route;route.output=cableSend.id;route.process=false;
 if(wanted==1){route.mic=selectedMic();if(route.mic.empty()){message=L"Choose your microphone in Setup";setup=true;layout();return;}}
 else{
  if(!receive){message=L"Install the Discord receive line";setup=true;layout();return;}
  route.feed=discordCapture.id;
 }
 save();source=wanted;message=L"Connecting…";engine.start(window,route,wanted);redraw();
}
void install(){
 wchar_t path[32768];GetModuleFileNameW(nullptr,path,32768);std::wstring file=path;file.erase(file.find_last_of(L"\\/")+1);file+=L"DISFORWARD-Setup.exe";
 if(GetFileAttributesW(file.c_str())==INVALID_FILE_ATTRIBUTES){MessageBoxW(window,L"Extract the complete download first. DISFORWARD-Setup.exe must be beside DISFORWARD.exe.",L"DISFORWARD setup",MB_OK|MB_ICONERROR);return;}
 if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"runas",file.c_str(),cable?L"--receive":nullptr,nullptr,SW_SHOWNORMAL))<=32)MessageBoxW(window,L"Setup could not start. Administrator approval is needed to install VB-CABLE.",L"DISFORWARD setup",MB_OK|MB_ICONINFORMATION);
}
HWND button(const wchar_t* s,int id,int x,int y,int w,int h){return CreateWindowW(L"BUTTON",s,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,px(x),px(y),px(w),px(h),window,(HMENU)(INT_PTR)id,nullptr,nullptr);}
void paint(){
 PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);RECT r;GetClientRect(window,&r);HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,r.right,r.bottom);auto prior=SelectObject(mem,bmp);
 {Graphics g(mem);g.Clear(c(18,21,26));g.ScaleTransform(scale,scale);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
 label(g,L"DISFORWARD",14,9,200,25,14,c(236,241,246),true);
 label(g,message,14,42,332,27,13,c(182,198,209),true);
 label(g,L"Call hears the selected source. Speaker is separate.",14,66,332,17,10,c(131,145,161));
 float p=static_cast<float>(engine.peak.load())/10000.f;float db=p>0?20*std::log10(p):-90;
 box(g,14,153,55,3,c(38,45,56));float amount=std::clamp((db+60)/60,0.f,1.f);if(amount>0)box(g,14,153,55*amount,3,db>=0?c(252,135,124):c(104,221,187));
 label(g,cable?L"Mic: "+cableMic.name.substr(0,cableMic.name.find(L" (")):L"Mic: not connected",194,141,152,22,10,c(131,145,161));
 if(setup){label(g,L"Your microphone",14,184,332,18,10,c(158,174,189));
 label(g,L"SPEAKER plays Discord through",14,242,332,18,10,c(158,174,189));
 label(g,L"Set Discord OUTPUT to:",14,301,332,18,10,c(158,174,189));
 label(g,receive?discordPlayback.name:L"Install Hi-Fi Cable receive line",14,320,332,20,11,c(234,239,245),true);
 label(g,L"Set call app MICROPHONE to:",14,345,332,18,10,c(158,174,189));
 label(g,cable?cableMic.name:L"Install VB-CABLE send line",14,364,332,20,11,c(234,239,245),true); }
 }
 BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,prior);DeleteObject(bmp);DeleteDC(mem);EndPaint(window,&ps);
}
void draw(DRAWITEMSTRUCT* d){Graphics g(d->hDC);g.Clear(c(18,21,26));g.ScaleTransform(scale,scale);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);float w=static_cast<float>(d->rcItem.right-d->rcItem.left)/scale,h=static_cast<float>(d->rcItem.bottom-d->rcItem.top)/scale;
 int id=(int)d->CtlID;bool on=(id==101&&source==1)||(id==102&&source==2)||(id==103&&source==0)||(id==110&&pinned)||(id==104&&speaker);Color bg=on&&id!=103?c(104,221,187):c(35,42,52),ink=on&&id!=103?c(18,21,26):c(234,239,245);
 if(d->itemState&ODS_SELECTED)bg=c(68,105,98);
 box(g,0,0,w,h,bg);
 wchar_t s[80];GetWindowTextW(d->hwndItem,s,80);label(g,s,2,0,w-4,h,id>=101&&id<=103?12.f:9.f,ink,true,true);
 if(d->itemState&ODS_FOCUS){Pen p(ink);g.DrawRectangle(&p,2.f,2.f,w-5,h-5);}
}
LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){
 case WM_PAINT:paint();return 0;case WM_ERASEBKGND:return 1;
 case WM_NCHITTEST:{POINT p{(short)LOWORD(l),(short)HIWORD(l)};ScreenToClient(h,&p);return p.y<px(37)&&p.x<px(210)?HTCAPTION:HTCLIENT;}
 case WM_DRAWITEM:draw((DRAWITEMSTRUCT*)l);return TRUE;
 case WM_COMMAND:{int id=LOWORD(w);if(id==120&&HIWORD(w)==CBN_SELCHANGE){engine.stop();source=0;message=L"Muted · microphone changed";save();redraw();return 0;}
 if(id==121&&HIWORD(w)==CBN_SELCHANGE){bool was=speaker;listen(false);save();if(was)listen(true);layout();return 0;}
 switch(id){case 104:listen(!speaker);layout();break;case 101:choose(1);break;case 102:choose(2);break;case 103:choose(0);break;case 110:pinned=!pinned;SetWindowPos(h,pinned?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);setup=false;layout();break;case 111:setup=!setup;layout();break;case 112:SendMessageW(h,WM_CLOSE,0,0);break;case 130:refresh();break;case 131:install();break;case 132:{
  if(preview)break;
  engine.stop();monitor.stop();speaker=false;source=0;
  if(!cable){message=L"Install VB-CABLE, then Refresh";redraw();break;}
  if(MessageBoxW(h,L"Check the VB-CABLE playback-to-microphone path with a quiet 2-second test tone? Run this before joining a live call.\n\nThis test does not verify Discord or the call app.",L"DISFORWARD · Check audio line",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)break;
  HANDLE cancel=CreateEventW(nullptr,TRUE,FALSE,nullptr);
  auto result=df::testVirtualLine(cableSend,cableMic,cancel);if(receive)result+=L"\n\nDiscord receive line:\n"+df::testVirtualLine(discordPlayback,discordCapture,cancel);if(cancel)CloseHandle(cancel);
  message=L"Muted · line check finished";MessageBoxW(h,result.c_str(),L"DISFORWARD · Audio line result",MB_OK|MB_ICONINFORMATION);break;
 }case 133:ShellExecuteW(h,L"open",L"control.exe",L"mmsys.cpl",nullptr,SW_SHOWNORMAL);break;}redraw();return 0;}
 case df::Engine::StateMessage:if(engine.state.load()==3){source=0;message=engine.message();MessageBoxW(h,message.c_str(),L"DISFORWARD audio error",MB_OK|MB_ICONERROR);}else if(engine.state.load()==2)message=source==1?L"Your microphone is live":L"Discord friend is live";redraw();return 0;
 case WM_TIMER:{if(speaker&&monitor.state.load()==3){speaker=false;auto detail=monitor.message();monitor.stop();MessageBoxW(h,detail.c_str(),L"DISFORWARD · Speaker audio error",MB_OK|MB_ICONERROR);redraw();}RECT r{px(14),px(144),px(176),px(163)};InvalidateRect(h,&r,FALSE);return 0;}
 case WM_CLOSE:save();monitor.stop();engine.stop();DestroyWindow(h);return 0;case WM_DESTROY:PostQuitMessage(0);return 0;}
 return DefWindowProcW(h,m,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int show){
 preview=wcscmp(command,L"--ui-preview")==0;
 HANDLE lock=CreateMutexW(nullptr,TRUE,L"Local\\DISFORWARD_NATIVE_CONSOLE");if(GetLastError()==ERROR_ALREADY_EXISTS){auto h=FindWindowW(L"DisforwardNativeConsole",nullptr);if(h)SetForegroundWindow(h);if(lock)CloseHandle(lock);return 0;}
 SetProcessDPIAware();HDC dc=GetDC(nullptr);scale=static_cast<float>(GetDeviceCaps(dc,LOGPIXELSX))/96.f;ReleaseDC(nullptr,dc);
 HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(co))return 1;
 GdiplusStartupInput start;ULONG_PTR token;if(GdiplusStartup(&token,&start,nullptr)!=Ok){CoUninitialize();return 1;}
 wchar_t folder[MAX_PATH];SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,0,folder);ini=std::wstring(folder)+L"\\DISFORWARD";CreateDirectoryW(ini.c_str(),nullptr);ini+=L"\\settings.ini";
 field=CreateSolidBrush(RGB(18,21,26));uiFont=CreateFontW(-px(12),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 WNDCLASSW wc{};wc.hInstance=instance;wc.lpszClassName=L"DisforwardNativeConsole";wc.lpfnWndProc=proc;wc.hbrBackground=field;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(100));RegisterClassW(&wc);
 window=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,L"DISFORWARD",WS_POPUP|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,px(360),px(174),nullptr,nullptr,instance,nullptr);
 button(L"PIN",110,222,10,40,24);button(L"SETUP",111,268,10,45,24);button(L"×",112,320,10,26,24);
 button(L"My Mic",101,14,94,104,39);button(L"Discord Friend",102,128,94,104,39);button(L"Mute",103,242,94,104,39);
 microphone=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,px(14),px(208),px(332),px(200),window,(HMENU)120,nullptr,nullptr);SendMessageW(microphone,WM_SETFONT,(WPARAM)uiFont,TRUE);
 button(L"SPEAKER",104,82,141,100,24);
 listening=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,px(14),px(266),px(332),px(200),window,(HMENU)121,nullptr,nullptr);SendMessageW(listening,WM_SETFONT,(WPARAM)uiFont,TRUE);
 button(L"Check audio lines",132,14,393,160,26);button(L"Install audio lines",131,186,393,160,26);
 button(L"Refresh",130,14,429,160,26);button(L"Sound settings",133,186,429,160,26);
 if(preview){message=L"Interface preview · audio disabled";}else refresh();layout();ShowWindow(window,show);SetTimer(window,1,50,nullptr);
 MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
 DeleteObject(uiFont);DeleteObject(field);GdiplusShutdown(token);CoUninitialize();if(lock)CloseHandle(lock);return 0;
}
