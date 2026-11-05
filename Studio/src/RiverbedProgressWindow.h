#pragma once
#include "renegade/bridge/TerrainOperationProgress.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <thread>
#include <mutex>
#include <memory>
#include <atomic>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace renegade::studio {
// Scene mutation stays on the scene-safe thread. The progress painter never
// calls into Studio and Studio NEVER waits for its Windows message loop.
class RiverbedProgressWindow {
    struct State {
        std::mutex mutex;
        std::atomic<bool> closing{false};
        RECT bounds{};
        std::string stage="Preparing riverbed";
        std::size_t completed=0,total=0;
        unsigned tick=0;
        static void Trace(const std::string& value) noexcept {
            try {
                const char* file=std::getenv("RENEGADE_CARVE_TRACE");
                if(file){std::ofstream out(file,std::ios::app);
                    out<<GetTickCount64()<<" "<<GetCurrentThreadId()<<" "<<value<<"\n";}
            } catch(...) {}
        }
        static LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM w,LPARAM l) {
            auto* self=reinterpret_cast<State*>(GetWindowLongPtrW(window,GWLP_USERDATA));
            if(message==WM_NCCREATE){
                self=static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
                SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
            }
            if(!self)return DefWindowProcW(window,message,w,l);
            if(message==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
            if(message==WM_CLOSE)return 0;
            if(message==WM_TIMER){
                if(self->closing.load()){PostQuitMessage(0);return 0;}
                ++self->tick;InvalidateRect(window,nullptr,FALSE);return 0;
            }
            if(message==WM_ERASEBKGND)return 1;
            if(message==WM_PAINT){self->Paint(window);return 0;}
            return DefWindowProcW(window,message,w,l);
        }
        void Paint(HWND window) {
            PAINTSTRUCT paint{};HDC dc=BeginPaint(window,&paint);
            RECT rect{};GetClientRect(window,&rect);
            const auto fill=[&](RECT box,COLORREF color){
                HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&box,brush);DeleteObject(brush);
            };
            fill(rect,RGB(25,29,32));
            std::string label;std::size_t count,maximum;
            {std::lock_guard<std::mutex> lock(mutex);
                label=stage;count=completed;maximum=total;}
            if(maximum)label+="  "+std::to_string(count)+"/"+std::to_string(maximum)+" chunks";
            const std::wstring text(label.begin(),label.end());
            HFONT font=CreateFontW(-16,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
            auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(237,240,241));
            RECT title={18,12,rect.right-18,37};
            DrawTextW(dc,text.c_str(),-1,&title,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);
            SelectObject(dc,old);DeleteObject(font);
            RECT track={18,47,rect.right-18,55};fill(track,RGB(49,57,60));
            RECT bar=track;const int width=track.right-track.left;
            if(maximum)bar.right=bar.left+int(width*std::min(1.0,double(count)/double(maximum)));
            else {bar.left+=int(tick*7%unsigned(std::max(1,width-60)));bar.right=bar.left+60;}
            fill(bar,RGB(148,199,189));EndPaint(window,&paint);
        }
        void Run() noexcept {
            try {
                Trace("ui-thread-start");
                WNDCLASSW type{};type.lpfnWndProc=Procedure;type.hInstance=GetModuleHandleW(nullptr);
                type.lpszClassName=L"RenegadeRiverbedProgress";type.hCursor=LoadCursor(nullptr,IDC_ARROW);
                RegisterClassW(&type);
                if(closing.load())return;
                const int width=std::max(240,std::min(440,int(bounds.right-bounds.left)-32));
                HWND window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,
                    type.lpszClassName,L"Carving riverbed",WS_POPUP|WS_VISIBLE,
                    (bounds.left+bounds.right-width)/2,bounds.bottom-120,width,72,
                    nullptr,nullptr,type.hInstance,this);
                Trace(window?"ui-window-created":"ui-window-failed");
                if(!window)return;
                SetTimer(window,1,40,nullptr);
                MSG message{};
                while(!closing.load() && GetMessageW(&message,nullptr,0,0)>0) {
                    TranslateMessage(&message);DispatchMessageW(&message);
                }
                KillTimer(window,1);DestroyWindow(window);Trace("ui-window-closed");
            } catch(...) {Trace("ui-thread-failed");}
        }
    };
    std::shared_ptr<State> state_;
    bridge::TerrainOperationProgress callback_;
    bridge::ScopedTerrainOperationProgress observe_;
    HWND owner_=nullptr;
    bool restoreEnabled_=false;
    bool wasForeground_=false;
    HWND fallbackForeground_=nullptr;
public:
    RiverbedProgressWindow():state_(std::make_shared<State>()),
        callback_([state=state_](const char* stage,std::size_t count,std::size_t maximum){
            {std::lock_guard<std::mutex> lock(state->mutex);
                state->stage=stage;state->completed=count;state->total=maximum;}
            State::Trace(std::string(stage)+" "+std::to_string(count)+"/"+std::to_string(maximum));
        }),observe_(callback_) {
        State::Trace("operation-start");
        owner_=GetActiveWindow();if(!owner_)return;
        DWORD pid=0;GetWindowThreadProcessId(owner_,&pid);
        if(pid!=GetCurrentProcessId()){owner_=nullptr;return;}
        GetWindowRect(owner_,&state_->bounds);
        restoreEnabled_=IsWindowEnabled(owner_)!=FALSE;
        wasForeground_=GetForegroundWindow()==owner_;
        if(restoreEnabled_)EnableWindow(owner_,FALSE);
        fallbackForeground_=GetForegroundWindow();
        State::Trace("input-locked");
        try {
            // The painter owns its shared state until it exits. No join or
            // readiness wait may block the engine's Windows message thread.
            std::thread([state=state_]{state->Run();}).detach();
        } catch(...) {State::Trace("ui-thread-launch-failed");}
        State::Trace("operation-ready");
    }
    ~RiverbedProgressWindow() {
        state_->closing.store(true);
        const HWND foreground=GetForegroundWindow();
        if(restoreEnabled_ && IsWindow(owner_)) {
            EnableWindow(owner_,TRUE);
            // EnableWindow alone does not undo WM_KILLFOCUS. Studio would
            // otherwise remain in its inactive update path after the carve.
            // Respect an explicit switch to another application during work.
            if(wasForeground_ && foreground==fallbackForeground_) {
                SetForegroundWindow(owner_);SetActiveWindow(owner_);SetFocus(owner_);
            }
        }
        State::Trace(GetFocus()==owner_?"operation-returned-focus-restored":"operation-returned-focus-elsewhere");
    }
    RiverbedProgressWindow(const RiverbedProgressWindow&)=delete;
    RiverbedProgressWindow& operator=(const RiverbedProgressWindow&)=delete;
};
}
