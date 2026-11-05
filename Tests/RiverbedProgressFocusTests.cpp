// Interactive Windows regression. Build with Studio/src and
// EngineBridge/include on the include path; link user32 and gdi32.
#include "RiverbedProgressWindow.h"
#include <cstdio>
int main(){
 HWND owner=CreateWindowExW(0,L"STATIC",L"Riverbed focus regression",
 WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 SetForegroundWindow(owner);SetActiveWindow(owner);SetFocus(owner);
 const bool before=GetFocus()==owner;
 std::printf("focus_before=%d\n",before);
 {
  renegade::studio::RiverbedProgressWindow progress;
  renegade::bridge::ReportTerrainOperation("Focus regression",0,1);
  Sleep(500);
 }
 const bool enabled=IsWindowEnabled(owner)!=FALSE;
 const bool focused=GetFocus()==owner;
 std::printf("enabled_after=%d focus_after=%d\n",enabled,focused);
 HWND other=CreateWindowExW(0,L"STATIC",L"Explicit focus switch",
 WS_OVERLAPPEDWINDOW,200,200,300,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 {
  renegade::studio::RiverbedProgressWindow progress;
  ShowWindow(other,SW_SHOW);SetForegroundWindow(other);SetActiveWindow(other);SetFocus(other);
  Sleep(100);
 }
 const bool respectedSwitch=GetFocus()==other;
 std::printf("explicit_focus_switch_preserved=%d\n",respectedSwitch);
 MSG message{};const auto end=GetTickCount64()+250;
 while(GetTickCount64()<end){while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
  TranslateMessage(&message);DispatchMessageW(&message);}Sleep(5);}
 DestroyWindow(other);DestroyWindow(owner);
 return before && enabled && focused && respectedSwitch?0:5;
}
