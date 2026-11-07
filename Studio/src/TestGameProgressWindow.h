#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace renegade::studio {
// Independent OS painter: keeps animating while the scene-safe thread captures.
// It owns no scene, renderer or launch-process references and never waits on them.
class TestGameProgressWindow {
    struct State {
        HWND owner = nullptr;
        std::atomic<bool> closing{false};
        std::mutex mutex;
        std::string stage = "Capturing scene...";
        unsigned tick = 0;
        static LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM w, LPARAM l) {
            auto* self = reinterpret_cast<State*>(GetWindowLongPtrW(window, GWLP_USERDATA));
            if (message == WM_NCCREATE) {
                self = static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
            if (!self) return DefWindowProcW(window, message, w, l);
            if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
            if (message == WM_CLOSE) return 0;
            if (message == WM_ERASEBKGND) return 1;
            if (message == WM_TIMER) {
                if (self->closing.load() || !IsWindow(self->owner)) { PostQuitMessage(0); return 0; }
                DWORD foregroundPid = 0;
                GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
                const bool visible = foregroundPid == GetCurrentProcessId() && !IsIconic(self->owner);
                RECT bounds{}; GetWindowRect(self->owner, &bounds);
                const int width = std::max(240, std::min(440, int(bounds.right - bounds.left) - 32));
                SetWindowPos(window, HWND_TOPMOST, (bounds.left + bounds.right - width) / 2,
                    (bounds.top + bounds.bottom - 112) / 2, width, 112,
                    SWP_NOACTIVATE | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
                ++self->tick; InvalidateRect(window, nullptr, FALSE); return 0;
            }
            if (message == WM_PAINT) { self->Paint(window); return 0; }
            return DefWindowProcW(window, message, w, l);
        }
        void Paint(HWND window) {
            PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint);
            RECT rect{}; GetClientRect(window, &rect);
            HDC buffer = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, rect.right, rect.bottom);
            const auto oldBitmap = SelectObject(buffer, bitmap);
            const auto fill = [&](RECT box, COLORREF color) {
                HBRUSH brush = CreateSolidBrush(color); FillRect(buffer, &box, brush); DeleteObject(brush);
            };
            fill(rect, RGB(25, 29, 32));
            std::string label; { std::lock_guard<std::mutex> lock(mutex); label = stage; }
            HFONT font = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            const auto oldFont = SelectObject(buffer, font);
            SetBkMode(buffer, TRANSPARENT); SetTextColor(buffer, RGB(237, 240, 241));
            RECT title{20, 15, rect.right - 20, 38};
            DrawTextW(buffer, L"PREPARING TEST GAME", -1, &title, DT_LEFT | DT_SINGLELINE);
            SetTextColor(buffer, RGB(175, 184, 187));
            RECT subtitle{20, 43, rect.right - 20, 66};
            const std::wstring text(label.begin(), label.end());
            DrawTextW(buffer, text.c_str(), -1, &subtitle, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT track{20, 83, rect.right - 20, 91}; fill(track, RGB(49, 57, 60));
            const int travel = std::max(1, int(track.right - track.left) - 72);
            const unsigned phase = tick * 6 % unsigned(travel * 2);
            RECT bar = track;
            bar.left += phase <= unsigned(travel) ? int(phase) : travel * 2 - int(phase);
            bar.right = bar.left + 72; fill(bar, RGB(148, 199, 189));
            BitBlt(dc, 0, 0, rect.right, rect.bottom, buffer, 0, 0, SRCCOPY);
            SelectObject(buffer, oldFont); DeleteObject(font);
            SelectObject(buffer, oldBitmap); DeleteObject(bitmap); DeleteDC(buffer); EndPaint(window, &paint);
        }
        void Run() noexcept {
            WNDCLASSW type{}; type.lpfnWndProc = Procedure; type.hInstance = GetModuleHandleW(nullptr);
            type.lpszClassName = L"RenegadeTestGameProgress"; type.hCursor = LoadCursor(nullptr, IDC_ARROW);
            RegisterClassW(&type);
            if (closing.load()) return;
            HWND window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                type.lpszClassName, L"Preparing Test Game", WS_POPUP, 0, 0, 440, 112,
                nullptr, nullptr, type.hInstance, this);
            if (!window) return;
            SetTimer(window, 1, 40, nullptr);
            MSG message{};
            while (!closing.load() && GetMessageW(&message, nullptr, 0, 0) > 0) {
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            KillTimer(window, 1); DestroyWindow(window);
        }
    };
    std::shared_ptr<State> state_;
public:
    TestGameProgressWindow() : state_(std::make_shared<State>()) {
        state_->owner = GetActiveWindow();
        if (!state_->owner) return;
        try { std::thread([state = state_] { state->Run(); }).detach(); } catch (...) {}
    }
    ~TestGameProgressWindow() { state_->closing.store(true); }
    void SetStage(const std::string& stage) {
        std::lock_guard<std::mutex> lock(state_->mutex); state_->stage = stage;
    }
    TestGameProgressWindow(const TestGameProgressWindow&) = delete;
    TestGameProgressWindow& operator=(const TestGameProgressWindow&) = delete;
};
}
