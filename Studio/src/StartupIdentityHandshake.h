#pragma once

#include <Windows.h>
#include <windowsx.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

namespace renegade::studio
{
    // Gate 2C final interactive identity screen.
    //
    // The generated motion plate remains purely visual. Once playback finishes,
    // this Renegade-owned window draws an exact extracted final frame and all
    // user-facing identity text/button natively so names and status copy are
    // deterministic and sharp at the current window resolution.
    class StartupIdentityHandshake final
    {
    public:
        StartupIdentityHandshake() = default;
        ~StartupIdentityHandshake()
        {
            Shutdown();
        }

        StartupIdentityHandshake(const StartupIdentityHandshake&) = delete;
        StartupIdentityHandshake& operator=(const StartupIdentityHandshake&) = delete;

        [[nodiscard]] bool Start(
            HWND parentWindow,
            const std::wstring& backgroundBitmapPath,
            std::wstring developerIdentity)
        {
            Shutdown();

            if (parentWindow == nullptr || !IsWindow(parentWindow) ||
                developerIdentity.empty() || !EnsureWindowClass())
            {
                return false;
            }

            parentWindow_ = parentWindow;
            identity_ = std::move(developerIdentity);
            backgroundBitmap_ = static_cast<HBITMAP>(LoadImageW(
                nullptr,
                backgroundBitmapPath.c_str(),
                IMAGE_BITMAP,
                0,
                0,
                LR_LOADFROMFILE | LR_CREATEDIBSECTION));

            if (backgroundBitmap_ != nullptr)
            {
                BITMAP bitmap = {};
                if (GetObjectW(backgroundBitmap_, sizeof(bitmap), &bitmap) == sizeof(bitmap))
                {
                    bitmapWidth_ = std::max(1, static_cast<int>(bitmap.bmWidth));
                    bitmapHeight_ = std::max(1, static_cast<int>(bitmap.bmHeight));
                }
            }

            window_ = CreateWindowExW(
                WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                WindowClassName,
                L"",
                WS_POPUP,
                0,
                0,
                1,
                1,
                parentWindow_,
                nullptr,
                GetModuleHandleW(nullptr),
                this);
            if (window_ == nullptr)
            {
                Shutdown();
                return false;
            }

            startedAt_ = std::chrono::steady_clock::now();
            active_ = true;
            completed_ = false;
            handoff_ = false;
            hoverButton_ = false;
            buttonEnabled_ = false;

            Resize();
            ShowWindow(window_, SW_SHOWNOACTIVATE);
            SetWindowPos(
                window_,
                HWND_TOP,
                0,
                0,
                0,
                0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            InvalidateRect(window_, nullptr, FALSE);
            UpdateWindow(window_);
            return true;
        }

        void Pump()
        {
            if (!active_ || window_ == nullptr)
                return;

            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startedAt_);
            const bool shouldEnable = elapsed.count() >= 850;
            if (shouldEnable != buttonEnabled_)
            {
                buttonEnabled_ = shouldEnable;
                InvalidateRect(window_, nullptr, FALSE);
            }
            else if (elapsed.count() < 1200)
            {
                // Animate the staged reveal of the native text/button without
                // introducing a second rendering backend.
                InvalidateRect(window_, nullptr, FALSE);
            }
        }

        void Resize()
        {
            if (window_ == nullptr || parentWindow_ == nullptr)
                return;

            RECT client = {};
            if (!GetClientRect(parentWindow_, &client))
                return;

            POINT topLeft = { client.left, client.top };
            POINT bottomRight = { client.right, client.bottom };
            if (!ClientToScreen(parentWindow_, &topLeft) ||
                !ClientToScreen(parentWindow_, &bottomRight))
            {
                return;
            }

            const int width = std::max(1, static_cast<int>(bottomRight.x - topLeft.x));
            const int height = std::max(1, static_cast<int>(bottomRight.y - topLeft.y));
            SetWindowPos(
                window_,
                HWND_TOP,
                topLeft.x,
                topLeft.y,
                width,
                height,
                SWP_NOACTIVATE | SWP_SHOWWINDOW);
            InvalidateRect(window_, nullptr, FALSE);
        }

        void BeginHandoff()
        {
            if (!active_ || window_ == nullptr)
                return;
            handoff_ = true;
            buttonEnabled_ = false;
            hoverButton_ = false;
            InvalidateRect(window_, nullptr, FALSE);
            UpdateWindow(window_);
        }

        void Shutdown()
        {
            active_ = false;
            buttonEnabled_ = false;
            hoverButton_ = false;

            if (window_ != nullptr)
            {
                DestroyWindow(window_);
                window_ = nullptr;
            }
            if (backgroundBitmap_ != nullptr)
            {
                DeleteObject(backgroundBitmap_);
                backgroundBitmap_ = nullptr;
            }

            parentWindow_ = nullptr;
            identity_.clear();
            bitmapWidth_ = 0;
            bitmapHeight_ = 0;
            buttonRect_ = {};
        }

        [[nodiscard]] bool IsActive() const noexcept { return active_; }
        [[nodiscard]] bool IsCompleted() const noexcept { return completed_; }
        [[nodiscard]] bool HasBackgroundBitmap() const noexcept
        {
            return backgroundBitmap_ != nullptr;
        }

    private:
        static constexpr wchar_t WindowClassName[] = L"RenegadeStartupIdentityHandshake";

        static bool EnsureWindowClass() noexcept
        {
            static const bool registered = []() {
                WNDCLASSEXW windowClass = {};
                windowClass.cbSize = sizeof(windowClass);
                windowClass.style = CS_HREDRAW | CS_VREDRAW;
                windowClass.lpfnWndProc = WindowProc;
                windowClass.hInstance = GetModuleHandleW(nullptr);
                windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
                windowClass.hbrBackground =
                    reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
                windowClass.lpszClassName = WindowClassName;

                if (RegisterClassExW(&windowClass) != 0)
                    return true;
                return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
            }();
            return registered;
        }

        static LRESULT CALLBACK WindowProc(
            HWND window,
            UINT message,
            WPARAM wParam,
            LPARAM lParam)
        {
            StartupIdentityHandshake* self = reinterpret_cast<StartupIdentityHandshake*>(
                GetWindowLongPtrW(window, GWLP_USERDATA));

            if (message == WM_NCCREATE)
            {
                const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
                self = static_cast<StartupIdentityHandshake*>(create->lpCreateParams);
                SetWindowLongPtrW(
                    window,
                    GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(self));
            }

            if (self == nullptr)
                return DefWindowProcW(window, message, wParam, lParam);

            switch (message)
            {
            case WM_PAINT:
                self->Paint();
                return 0;

            case WM_ERASEBKGND:
                return 1;

            case WM_NCHITTEST:
            {
                if (!self->buttonEnabled_ || self->handoff_)
                    return HTTRANSPARENT;
                POINT point = {
                    GET_X_LPARAM(lParam),
                    GET_Y_LPARAM(lParam)
                };
                ScreenToClient(window, &point);
                return PtInRect(&self->buttonRect_, point) ? HTCLIENT : HTTRANSPARENT;
            }

            case WM_MOUSEMOVE:
            {
                POINT point = {
                    GET_X_LPARAM(lParam),
                    GET_Y_LPARAM(lParam)
                };
                const bool hovering =
                    self->buttonEnabled_ && PtInRect(&self->buttonRect_, point);
                if (hovering != self->hoverButton_)
                {
                    self->hoverButton_ = hovering;
                    InvalidateRect(window, &self->buttonRect_, FALSE);
                }
                TRACKMOUSEEVENT track = {};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = window;
                TrackMouseEvent(&track);
                return 0;
            }

            case WM_MOUSELEAVE:
                if (self->hoverButton_)
                {
                    self->hoverButton_ = false;
                    InvalidateRect(window, &self->buttonRect_, FALSE);
                }
                return 0;

            case WM_LBUTTONUP:
            {
                if (!self->buttonEnabled_ || self->handoff_)
                    return 0;
                POINT point = {
                    GET_X_LPARAM(lParam),
                    GET_Y_LPARAM(lParam)
                };
                if (PtInRect(&self->buttonRect_, point))
                    self->completed_ = true;
                return 0;
            }

            case WM_SETCURSOR:
                if (self->hoverButton_ && self->buttonEnabled_)
                {
                    SetCursor(LoadCursor(nullptr, IDC_HAND));
                    return TRUE;
                }
                break;

            default:
                break;
            }

            return DefWindowProcW(window, message, wParam, lParam);
        }

        static HFONT CreateUiFont(int pixelHeight, int weight)
        {
            return CreateFontW(
                -std::max(10, pixelHeight),
                0,
                0,
                0,
                weight,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                FF_DONTCARE,
                L"Segoe UI");
        }

        void Paint()
        {
            if (window_ == nullptr)
                return;

            PAINTSTRUCT paint = {};
            HDC target = BeginPaint(window_, &paint);
            if (target == nullptr)
                return;

            RECT client = {};
            GetClientRect(window_, &client);
            const int clientWidth = std::max(1, static_cast<int>(client.right - client.left));
            const int clientHeight = std::max(1, static_cast<int>(client.bottom - client.top));

            HBRUSH blackBrush = CreateSolidBrush(RGB(2, 3, 4));
            FillRect(target, &client, blackBrush);
            DeleteObject(blackBrush);

            if (handoff_)
            {
                EndPaint(window_, &paint);
                return;
            }

            RECT imageRect = client;
            if (backgroundBitmap_ != nullptr && bitmapWidth_ > 0 && bitmapHeight_ > 0)
            {
                const double scale = std::min(
                    static_cast<double>(clientWidth) / static_cast<double>(bitmapWidth_),
                    static_cast<double>(clientHeight) / static_cast<double>(bitmapHeight_));
                const int width = std::max(1, static_cast<int>(bitmapWidth_ * scale));
                const int height = std::max(1, static_cast<int>(bitmapHeight_ * scale));
                imageRect.left = (clientWidth - width) / 2;
                imageRect.top = (clientHeight - height) / 2;
                imageRect.right = imageRect.left + width;
                imageRect.bottom = imageRect.top + height;

                HDC source = CreateCompatibleDC(target);
                HGDIOBJ previousBitmap = SelectObject(source, backgroundBitmap_);
                const int previousMode = SetStretchBltMode(target, HALFTONE);
                StretchBlt(
                    target,
                    imageRect.left,
                    imageRect.top,
                    width,
                    height,
                    source,
                    0,
                    0,
                    bitmapWidth_,
                    bitmapHeight_,
                    SRCCOPY);
                SetStretchBltMode(target, previousMode);
                SelectObject(source, previousBitmap);
                DeleteDC(source);
            }

            const int imageWidth = std::max(1, static_cast<int>(imageRect.right - imageRect.left));
            const int imageHeight = std::max(1, static_cast<int>(imageRect.bottom - imageRect.top));

            // Hide the generated plate's non-authoritative bottom progress copy.
            // All meaningful identity/status copy below is Renegade-rendered.
            RECT identityPanel = {
                imageRect.left + static_cast<int>(imageWidth * 0.235),
                imageRect.top + static_cast<int>(imageHeight * 0.720),
                imageRect.left + static_cast<int>(imageWidth * 0.765),
                imageRect.top + static_cast<int>(imageHeight * 0.975)
            };
            HBRUSH panelBrush = CreateSolidBrush(RGB(5, 7, 9));
            FillRect(target, &identityPanel, panelBrush);
            DeleteObject(panelBrush);

            HPEN accentPen = CreatePen(PS_SOLID, std::max(1, clientHeight / 720), RGB(210, 28, 38));
            HGDIOBJ oldPen = SelectObject(target, accentPen);
            MoveToEx(target, identityPanel.left + 24, identityPanel.top + 1, nullptr);
            LineTo(target, identityPanel.right - 24, identityPanel.top + 1);
            SelectObject(target, oldPen);
            DeleteObject(accentPen);

            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startedAt_).count();

            SetBkMode(target, TRANSPARENT);
            SetTextColor(target, RGB(238, 241, 244));

            if (elapsed >= 180)
            {
                HFONT welcomeFont = CreateUiFont(std::max(24, clientHeight / 17), FW_SEMIBOLD);
                HGDIOBJ oldFont = SelectObject(target, welcomeFont);
                RECT welcomeRect = identityPanel;
                welcomeRect.top += std::max(10, imageHeight / 42);
                welcomeRect.bottom = welcomeRect.top + std::max(38, imageHeight / 10);
                const std::wstring welcome = L"WELCOME, " + identity_;
                DrawTextW(
                    target,
                    welcome.c_str(),
                    static_cast<int>(welcome.size()),
                    &welcomeRect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(target, oldFont);
                DeleteObject(welcomeFont);
            }

            if (elapsed >= 430)
            {
                HFONT statusFont = CreateUiFont(std::max(14, clientHeight / 38), FW_MEDIUM);
                HGDIOBJ oldFont = SelectObject(target, statusFont);
                SetTextColor(target, RGB(211, 34, 44));
                RECT statusRect = identityPanel;
                statusRect.top += std::max(54, imageHeight / 11);
                statusRect.bottom = statusRect.top + std::max(26, imageHeight / 20);
                const std::wstring status = L"RENEGADE  //  IDENTITY ACCEPTED";
                DrawTextW(
                    target,
                    status.c_str(),
                    static_cast<int>(status.size()),
                    &statusRect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(target, oldFont);
                DeleteObject(statusFont);
            }

            const int buttonWidth = std::max(260, static_cast<int>(imageWidth * 0.30));
            const int buttonHeight = std::max(52, static_cast<int>(imageHeight * 0.075));
            const int buttonLeft = (clientWidth - buttonWidth) / 2;
            const int buttonTop = identityPanel.bottom - buttonHeight - std::max(12, imageHeight / 34);
            buttonRect_ = {
                buttonLeft,
                buttonTop,
                buttonLeft + buttonWidth,
                buttonTop + buttonHeight
            };

            if (elapsed >= 700)
            {
                const COLORREF fill = hoverButton_
                    ? RGB(28, 8, 11)
                    : RGB(9, 11, 14);
                HBRUSH buttonBrush = CreateSolidBrush(fill);
                FillRect(target, &buttonRect_, buttonBrush);
                DeleteObject(buttonBrush);

                HPEN buttonPen = CreatePen(
                    PS_SOLID,
                    std::max(1, clientHeight / 480),
                    buttonEnabled_ ? RGB(226, 35, 47) : RGB(105, 31, 37));
                oldPen = SelectObject(target, buttonPen);
                HGDIOBJ oldBrush = SelectObject(target, GetStockObject(NULL_BRUSH));
                Rectangle(
                    target,
                    buttonRect_.left,
                    buttonRect_.top,
                    buttonRect_.right,
                    buttonRect_.bottom);
                SelectObject(target, oldBrush);
                SelectObject(target, oldPen);
                DeleteObject(buttonPen);

                HFONT buttonFont = CreateUiFont(std::max(16, clientHeight / 32), FW_MEDIUM);
                HGDIOBJ oldFont = SelectObject(target, buttonFont);
                SetTextColor(
                    target,
                    buttonEnabled_ ? RGB(244, 246, 248) : RGB(128, 132, 136));
                const std::wstring buttonText = L"ENTER HUB";
                DrawTextW(
                    target,
                    buttonText.c_str(),
                    static_cast<int>(buttonText.size()),
                    &buttonRect_,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(target, oldFont);
                DeleteObject(buttonFont);
            }

            EndPaint(window_, &paint);
        }

        HWND parentWindow_ = nullptr;
        HWND window_ = nullptr;
        HBITMAP backgroundBitmap_ = nullptr;
        std::wstring identity_;
        int bitmapWidth_ = 0;
        int bitmapHeight_ = 0;
        RECT buttonRect_ = {};
        bool active_ = false;
        bool completed_ = false;
        bool handoff_ = false;
        bool hoverButton_ = false;
        bool buttonEnabled_ = false;
        std::chrono::steady_clock::time_point startedAt_{};
    };
}
