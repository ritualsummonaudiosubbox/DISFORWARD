#include "engine.h"
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <cwctype>
#include <cmath>
using namespace Gdiplus;
namespace {
HWND window,microphone;HFONT uiFont;HBRUSH field;
df::Engine engine;
bool pinned=false,setup=false,preview=false;int source=0;float scale=1;
std::wstring message=L"Muted",ini;
std::vector<df::Endpoint> microphones;
df::Endpoint nativeSend,nativeMic;
bool native=false;
int px(double n){return (int)std::lround(n*scale);}
Color c(int r,int g,int b){return Color(255,static_cast<BYTE>(r),static_cast<BYTE>(g),static_cast<BYTE>(b));}
void label(Graphics &g,const std::wstring& s,float x,float y,float w,float h,float size,Color color,bool bold=false,bool centered=false){
 FontFamily family(L"Segoe UI");Font font(&family,size,bold?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush brush(color);StringFormat f;f.SetTrimming(StringTrimmingEllipsisCharacter);f.SetFormatFlags(StringFormatFlagsNoWrap);f.SetLineAlignment(StringAlignmentCenter);if(centered)f.SetAlignment(StringAlignmentCenter);g.DrawString(s.c_str(),-1,&font,RectF(x,y,w,h),&f,&brush);
}
void box(Graphics &g,float x,float y,float w,float h,Color color){GraphicsPath path;const float d=std::min(12.f,std::min(w,h));path.AddArc(x,y,d,d,180,90);path.AddArc(x+w-d,y,d,d,270,90);path.AddArc(x+w-d,y+h-d,d,d,0,90);path.AddArc(x,y+h-d,d,d,90,90);path.CloseFigure();SolidBrush b(color);g.FillPath(&b,&path);}
void redraw(){InvalidateRect(window,nullptr,FALSE);for(int id:{101,102,103,110})InvalidateRect(GetDlgItem(window,id),nullptr,FALSE);}
std::wstring selectedMic(){int i=(int)SendMessageW(microphone,CB_GETCURSEL,0,0);return i>=0&&i<(int)microphones.size()?microphones[i].id:L"";}
void save(){if(!preview)WritePrivateProfileStringW(L"Devices",L"Mic",selectedMic().c_str(),ini.c_str());}
void layout(){for(int id:{120,130,131})ShowWindow(GetDlgItem(window,id),setup?SW_SHOW:SW_HIDE);SetWindowPos(window,nullptr,0,0,px(360),px(setup?282:174),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);redraw();}
void refresh(){
 save();engine.stop();source=0;native=false;microphones.clear();SendMessageW(microphone,CB_RESETCONTENT,0,0);
 auto inputs=df::endpoints(eCapture),outputs=df::endpoints(eRender);bool out=false,in=false;
 for(auto &e:outputs)if(e.identity==L"DISFORWARD.Native.1"){nativeSend=e;out=true;break;}
 for(auto &e:inputs)if(e.identity==L"DISFORWARD.Native.1"){nativeMic=e;in=true;break;}
 native=out&&in;
 wchar_t saved[4096]{};GetPrivateProfileStringW(L"Devices",L"Mic",L"",saved,4096,ini.c_str());int current=-1;
 for(auto &e:inputs){if(e.identity==L"DISFORWARD.Native.1"||df::virtualCable(e))continue;SendMessageW(microphone,CB_ADDSTRING,0,(LPARAM)e.name.c_str());if(e.id==saved)current=(int)microphones.size();microphones.push_back(e);}
 if(current<0&&!microphones.empty())current=0;
 SendMessageW(microphone,CB_SETCURSEL,current,0);
 message=native?L"Ready · choose who speaks":L"Native DISFORWARD driver is missing";redraw();
}
void choose(int wanted){
 if(preview){source=wanted;message=wanted==1?L"Your microphone":wanted==2?L"Discord friend":L"Muted";redraw();return;}
 if(!wanted){engine.stop();source=0;message=L"Muted";redraw();return;}
 engine.stop();source=0;
 if(!native){message=L"Native DISFORWARD driver is missing";setup=true;layout();return;}
 df::RouteConfig route;route.output=nativeSend.id;route.process=wanted==2;
 if(wanted==1){route.mic=selectedMic();if(route.mic.empty()){message=L"Choose your microphone in Setup";setup=true;layout();return;}}
 else{
  if(!df::supportsProcessCapture()){message=L"Discord capture needs Windows 11";redraw();return;}
  auto apps=df::programs();std::vector<DWORD> matches;
  for(auto &app:apps){auto name=app.exe;for(auto &ch:name)ch=towupper(ch);if(name==L"DISCORD.EXE"||name==L"DISCORDPTB.EXE"||name==L"DISCORDCANARY.EXE")matches.push_back(app.pid);}
  if(matches.empty()){message=L"Open Discord, join your friend's call";redraw();return;}
  if(matches.size()>1){message=L"Close extra Discord instances first";redraw();return;}
  route.pid=matches[0];
 }
 save();source=wanted;message=L"Connecting…";engine.start(window,route,wanted);redraw();
}
void install(){
 wchar_t path[32768];GetModuleFileNameW(nullptr,path,32768);std::wstring file=path;file.erase(file.find_last_of(L"\\/")+1);file+=L"DISFORWARD-Driver-Setup.exe";
 if(GetFileAttributesW(file.c_str())==INVALID_FILE_ATTRIBUTES){MessageBoxW(window,L"The native driver installer has not been built and signed in this development package. A production driver package must be built, Microsoft-signed and tested on Windows before this console can forward audio.\n\nNo third-party cable or renamed endpoint is used.",L"DISFORWARD · Native driver required",MB_OK|MB_ICONINFORMATION);return;}
 ShellExecuteW(window,L"runas",file.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
}
HWND button(const wchar_t* s,int id,int x,int y,int w,int h){return CreateWindowW(L"BUTTON",s,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,px(x),px(y),px(w),px(h),window,(HMENU)(INT_PTR)id,nullptr,nullptr);}
void paint(){
 PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);RECT r;GetClientRect(window,&r);HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,r.right,r.bottom);auto prior=SelectObject(mem,bmp);
 {Graphics g(mem);g.Clear(c(18,21,26));g.ScaleTransform(scale,scale);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
 label(g,L"DISFORWARD",14,9,200,25,14,c(236,241,246),true);
 label(g,message,14,42,332,27,13,c(182,198,209),true);
 label(g,L"One microphone input. One person at a time.",14,66,332,17,10,c(131,145,161));
 float p=static_cast<float>(engine.peak.load())/10000.f;float db=p>0?20*std::log10(p):-90;
 box(g,14,153,160,3,c(38,45,56));float amount=std::clamp((db+60)/60,0.f,1.f);if(amount>0)box(g,14,153,160*amount,3,db>=0?c(252,135,124):c(104,221,187));
 label(g,L"Output: DISFORWARD",194,141,152,22,10,c(131,145,161));
 if(setup){label(g,L"Your microphone",14,184,332,18,10,c(158,174,189));}
 }
 BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,prior);DeleteObject(bmp);DeleteDC(mem);EndPaint(window,&ps);
}
void draw(DRAWITEMSTRUCT* d){Graphics g(d->hDC);g.Clear(c(18,21,26));g.ScaleTransform(scale,scale);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);float w=static_cast<float>(d->rcItem.right-d->rcItem.left)/scale,h=static_cast<float>(d->rcItem.bottom-d->rcItem.top)/scale;
 int id=(int)d->CtlID;bool on=(id==101&&source==1)||(id==102&&source==2)||(id==103&&source==0)||(id==110&&pinned);Color bg=on&&id!=103?c(104,221,187):c(35,42,52),ink=on&&id!=103?c(18,21,26):c(234,239,245);
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
 switch(id){case 101:choose(1);break;case 102:choose(2);break;case 103:choose(0);break;case 110:pinned=!pinned;SetWindowPos(h,pinned?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);setup=false;layout();break;case 111:setup=!setup;layout();break;case 112:SendMessageW(h,WM_CLOSE,0,0);break;case 130:refresh();break;case 131:install();break;}redraw();return 0;}
 case df::Engine::StateMessage:if(engine.state.load()==3){source=0;message=engine.message();MessageBoxW(h,message.c_str(),L"DISFORWARD audio error",MB_OK|MB_ICONERROR);}else if(engine.state.load()==2)message=source==1?L"Your microphone is live":L"Discord friend is live";redraw();return 0;
 case WM_TIMER:{RECT r{px(14),px(144),px(176),px(163)};InvalidateRect(h,&r,FALSE);return 0;}
 case WM_CLOSE:save();engine.stop();DestroyWindow(h);return 0;case WM_DESTROY:PostQuitMessage(0);return 0;}
 return DefWindowProcW(h,m,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int show){
 preview=wcscmp(command,L"--ui-preview")==0;
 HANDLE lock=CreateMutexW(nullptr,TRUE,L"Local\\DISFORWARD_NATIVE_CONSOLE");if(GetLastError()==ERROR_ALREADY_EXISTS){auto h=FindWindowW(L"DisforwardNativeConsole",nullptr);if(h)SetForegroundWindow(h);if(lock)CloseHandle(lock);return 0;}
 SetProcessDPIAware();HDC dc=GetDC(nullptr);scale=static_cast<float>(GetDeviceCaps(dc,LOGPIXELSX))/96.f;ReleaseDC(nullptr,dc);
 HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(co))return 1;
 GdiplusStartupInput start;ULONG_PTR token;if(GdiplusStartup(&token,&start,nullptr)!=Ok){CoUninitialize();return 1;}
 wchar_t folder[MAX_PATH];SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,0,folder);ini=std::wstring(folder)+L"\\DISFORWARD-native";CreateDirectoryW(ini.c_str(),nullptr);ini+=L"\\settings.ini";
 field=CreateSolidBrush(RGB(18,21,26));uiFont=CreateFontW(-px(12),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 WNDCLASSW wc{};wc.hInstance=instance;wc.lpszClassName=L"DisforwardNativeConsole";wc.lpfnWndProc=proc;wc.hbrBackground=field;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(100));RegisterClassW(&wc);
 window=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,L"DISFORWARD",WS_POPUP|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,px(360),px(174),nullptr,nullptr,instance,nullptr);
 button(L"PIN",110,222,10,40,24);button(L"SETUP",111,268,10,45,24);button(L"×",112,320,10,26,24);
 button(L"My Mic",101,14,94,104,39);button(L"Discord Friend",102,128,94,104,39);button(L"Mute",103,242,94,104,39);
 microphone=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,px(14),px(208),px(332),px(200),window,(HMENU)120,nullptr,nullptr);SendMessageW(microphone,WM_SETFONT,(WPARAM)uiFont,TRUE);
 button(L"Refresh",130,14,245,104,26);button(L"Native driver setup",131,128,245,218,26);
 if(preview){message=L"Interface preview · audio disabled";}else refresh();layout();ShowWindow(window,show);SetTimer(window,1,50,nullptr);
 MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
 DeleteObject(uiFont);DeleteObject(field);GdiplusShutdown(token);CoUninitialize();if(lock)CloseHandle(lock);return 0;
}
