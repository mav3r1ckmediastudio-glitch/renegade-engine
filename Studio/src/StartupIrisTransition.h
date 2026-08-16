#pragma once

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <string>

namespace renegade::studio
{
    // PR #58 Gate 2D: deterministic transition from the accepted identity
    // handshake final frame into the already-rendered live Project Hub.
    //
    // The surrounding handshake plate fades away while two clipped halves of
    // the central mechanical iris travel left/right. The transition owns no Hub
    // content and never substitutes a generated video for the live Studio UI.
    class StartupIrisTransition final
    {
    public:
        StartupIrisTransition() = default;
        ~StartupIrisTransition()
        {
            Shutdown();
        }

        StartupIrisTransition(const StartupIrisTransition&) = delete;
        StartupIrisTransition& operator=(const StartupIrisTransition&) = delete;

        [[nodiscard]] bool Start(
            HWND parentWindow,
            const std::wstring& backgroundBitmapPath)
        {
            Shutdown();

            if (parentWindow == nullptr || !IsWindow(parentWindow) ||
                !EnsureWindowClasses())
            {
                return false;
            }

            parentWindow_ = parentWindow;
            backgroundBitmap_ = static_cast<HBITMAP>(LoadImageW(
                nullptr,
                backgroundBitmapPath.c_str(),
                IMAGE_BITMAP,
                0,
                0,
                LR_LOADFROMFILE | LR_CREATEDIBSECTION));
            if (backgroundBitmap_ == nullptr)
            {
                Shutdown();
                return false;
            }

            BITMAP bitmap = {};
            if (GetObjectW(backgroundBitmap_, sizeof(bitmap), &bitmap) != sizeof(bitmap))
            {
                Shutdown();
                return false;
            }
            bitmapWidth_ = std::max(1, static_cast<int>(bitmap.bmWidth));
            bitmapHeight_ = std::max(1, static_cast<int>(bitmap.bmHeight));

            overlayWindow_ = CreateWindowExW(
                WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                    WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                OverlayWindowClassName,
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
            if (overlayWindow_ == nullptr)
            {
                Shutdown();
                return false;
            }

            leftIrisWindow_ = CreateWindowExW(
                WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                    WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                IrisWindowClassName,
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
            rightIrisWindow_ = CreateWindowExW(
                WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                    WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                IrisWindowClassName,
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
            if (leftIrisWindow_ == nullptr || rightIrisWindow_ == nullptr)
            {
                Shutdown();
                return false;
            }

            (void)SetLayeredWindowAttributes(overlayWindow_, 0, 255, LWA_ALPHA);
            (void)SetLayeredWindowAttributes(leftIrisWindow_, 0, 255, LWA_ALPHA);
            (void)SetLayeredWindowAttributes(rightIrisWindow_, 0, 255, LWA_ALPHA);

            active_ = true;
            completed_ = false;
            progress_ = 0.0;
            startedAt_ = std::chrono::steady_clock::now();

            Resize();

            ShowWindow(overlayWindow_, SW_SHOWNOACTIVATE);
            SetWindowPos(
                overlayWindow_,
                HWND_TOP,
                0,
                0,
                0,
                0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            InvalidateRect(overlayWindow_, nullptr, FALSE);
            UpdateWindow(overlayWindow_);

            ShowWindow(leftIrisWindow_, SW_SHOWNOACTIVATE);
            ShowWindow(rightIrisWindow_, SW_SHOWNOACTIVATE);
            SetWindowPos(
                leftIrisWindow_,
                HWND_TOP,
                0,
                0,
                0,
                0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            SetWindowPos(
                rightIrisWindow_,
                HWND_TOP,
                0,
                0,
                0,
                0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            InvalidateRect(leftIrisWindow_, nullptr, FALSE);
            InvalidateRect(rightIrisWindow_, nullptr, FALSE);
            UpdateWindow(leftIrisWindow_);
            UpdateWindow(rightIrisWindow_);
            return true;
        }

        void Pump()
        {
            if (!active_)
                return;

            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startedAt_);
            const auto animatedElapsed = std::max(
                std::chrono::milliseconds::zero(),
                elapsed - HoldDuration);

            const double plateProgress = std::clamp(
                static_cast<double>(animatedElapsed.count()) /
                    static_cast<double>(PlateFadeDuration.count()),
                0.0,
                1.0);
            const double irisProgress = std::clamp(
                static_cast<double>(animatedElapsed.count()) /
                    static_cast<double>(IrisTravelDuration.count()),
                0.0,
                1.0);
            progress_ = irisProgress;

            if (overlayWindow_ != nullptr)
            {
                const double visible = 1.0 - SmoothStep(plateProgress);
                const BYTE alpha = static_cast<BYTE>(
                    std::clamp(visible * 255.0, 0.0, 255.0) + 0.5);
                (void)SetLayeredWindowAttributes(
                    overlayWindow_,
                    0,
                    alpha,
                    LWA_ALPHA);
                if (plateProgress >= 1.0)
                    ShowWindow(overlayWindow_, SW_HIDE);
            }

            UpdateIrisGeometry(irisProgress);

            // Keep the iris fully solid for the majority of the separation so
            // the mechanical split reads clearly, then soften it as the halves
            // leave the viewport.
            double irisAlpha = 1.0;
            if (irisProgress > IrisFadeStart)
            {
                irisAlpha = 1.0 -
                    ((irisProgress - IrisFadeStart) /
                     (1.0 - IrisFadeStart));
            }
            const BYTE irisByte = static_cast<BYTE>(
                std::clamp(irisAlpha * 255.0, 0.0, 255.0) + 0.5);
            if (leftIrisWindow_ != nullptr)
            {
                (void)SetLayeredWindowAttributes(
                    leftIrisWindow_, 0, irisByte, LWA_ALPHA);
            }
            if (rightIrisWindow_ != nullptr)
            {
                (void)SetLayeredWindowAttributes(
                    rightIrisWindow_, 0, irisByte, LWA_ALPHA);
            }

            if (irisProgress >= 1.0)
            {
                active_ = false;
                completed_ = true;
                ShowWindow(overlayWindow_, SW_HIDE);
                DestroyIrisWindows();
            }
        }

        void Resize()
        {
            if (overlayWindow_ == nullptr || parentWindow_ == nullptr)
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

            const int width = std::max(
                1,
                static_cast<int>(bottomRight.x - topLeft.x));
            const int height = std::max(
                1,
                static_cast<int>(bottomRight.y - topLeft.y));
            SetWindowPos(
                overlayWindow_,
                HWND_TOP,
                topLeft.x,
                topLeft.y,
                width,
                height,
                SWP_NOACTIVATE | SWP_SHOWWINDOW);
            InvalidateRect(overlayWindow_, nullptr, FALSE);

            UpdateIrisGeometry(progress_);
        }

        void Shutdown()
        {
            active_ = false;
            completed_ = false;
            progress_ = 0.0;

            DestroyIrisWindows();

            if (overlayWindow_ != nullptr)
            {
                DestroyWindow(overlayWindow_);
                overlayWindow_ = nullptr;
            }
            if (backgroundBitmap_ != nullptr)
            {
                DeleteObject(backgroundBitmap_);
                backgroundBitmap_ = nullptr;
            }

            parentWindow_ = nullptr;
            bitmapWidth_ = 0;
            bitmapHeight_ = 0;
            irisHalfWidth_ = 0;
            irisHeight_ = 0;
        }

        [[nodiscard]] bool IsActive() const noexcept { return active_; }
        [[nodiscard]] bool IsCompleted() const noexcept { return completed_; }
        [[nodiscard]] bool HasBackgroundBitmap() const noexcept
        {
            return backgroundBitmap_ != nullptr;
        }

    private:
        static constexpr wchar_t OverlayWindowClassName[] =
            L"RenegadeStartupIrisTransitionOverlay";
        static constexpr wchar_t IrisWindowClassName[] =
            L"RenegadeStartupIrisTransitionHalf";

        // Normalized from the accepted Gate 2C final frame. The iris outer
        // housing is approximately a 600 px circle centered at (640,315) in the
        // accepted 1280x720 still.
        static constexpr double IrisCenterXNormalized = 0.500000;
        static constexpr double IrisCenterYNormalized = 0.437500;
        static constexpr double IrisRadiusXNormalized = 0.234375;
        static constexpr double IrisRadiusYNormalized = 0.416667;

        static constexpr auto HoldDuration = std::chrono::milliseconds(70);
        static constexpr auto PlateFadeDuration = std::chrono::milliseconds(280);
        static constexpr auto IrisTravelDuration = std::chrono::milliseconds(920);
        static constexpr double IrisFadeStart = 0.78;

        static double SmoothStep(const double value) noexcept
        {
            const double t = std::clamp(value, 0.0, 1.0);
            return t * t * (3.0 - (2.0 * t));
        }

        static bool EnsureWindowClasses() noexcept
        {
            static const bool registered = []() {
                WNDCLASSEXW overlayClass = {};
                overlayClass.cbSize = sizeof(overlayClass);
                overlayClass.style = CS_HREDRAW | CS_VREDRAW;
                overlayClass.lpfnWndProc = OverlayWindowProc;
                overlayClass.hInstance = GetModuleHandleW(nullptr);
                overlayClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
                overlayClass.hbrBackground =
                    reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
                overlayClass.lpszClassName = OverlayWindowClassName;
                if (RegisterClassExW(&overlayClass) == 0 &&
                    GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                {
                    return false;
                }

                WNDCLASSEXW irisClass = {};
                irisClass.cbSize = sizeof(irisClass);
                irisClass.style = CS_HREDRAW | CS_VREDRAW;
                irisClass.lpfnWndProc = IrisWindowProc;
                irisClass.hInstance = GetModuleHandleW(nullptr);
                irisClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
                irisClass.hbrBackground =
                    reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
                irisClass.lpszClassName = IrisWindowClassName;
                if (RegisterClassExW(&irisClass) == 0 &&
                    GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                {
                    return false;
                }
                return true;
            }();
            return registered;
        }

        static LRESULT CALLBACK OverlayWindowProc(
            HWND window,
            UINT message,
            WPARAM wParam,
            LPARAM lParam)
        {
            StartupIrisTransition* self = reinterpret_cast<StartupIrisTransition*>(
                GetWindowLongPtrW(window, GWLP_USERDATA));
            if (message == WM_NCCREATE)
            {
                const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
                self = static_cast<StartupIrisTransition*>(create->lpCreateParams);
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
                self->PaintOverlay();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_NCHITTEST:
                return HTTRANSPARENT;
            default:
                break;
            }
            return DefWindowProcW(window, message, wParam, lParam);
        }

        static LRESULT CALLBACK IrisWindowProc(
            HWND window,
            UINT message,
            WPARAM wParam,
            LPARAM lParam)
        {
            StartupIrisTransition* self = reinterpret_cast<StartupIrisTransition*>(
                GetWindowLongPtrW(window, GWLP_USERDATA));
            if (message == WM_NCCREATE)
            {
                const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
                self = static_cast<StartupIrisTransition*>(create->lpCreateParams);
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
                self->PaintIrisHalf(window);
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_NCHITTEST:
                return HTTRANSPARENT;
            default:
                break;
            }
            return DefWindowProcW(window, message, wParam, lParam);
        }

        [[nodiscard]] RECT ResolveImageRect(
            const int clientWidth,
            const int clientHeight) const noexcept
        {
            RECT imageRect = {
                0,
                0,
                std::max(1, clientWidth),
                std::max(1, clientHeight)
            };
            if (bitmapWidth_ <= 0 || bitmapHeight_ <= 0)
                return imageRect;

            const double scale = std::min(
                static_cast<double>(clientWidth) /
                    static_cast<double>(bitmapWidth_),
                static_cast<double>(clientHeight) /
                    static_cast<double>(bitmapHeight_));
            const int width = std::max(
                1,
                static_cast<int>(bitmapWidth_ * scale + 0.5));
            const int height = std::max(
                1,
                static_cast<int>(bitmapHeight_ * scale + 0.5));
            imageRect.left = (clientWidth - width) / 2;
            imageRect.top = (clientHeight - height) / 2;
            imageRect.right = imageRect.left + width;
            imageRect.bottom = imageRect.top + height;
            return imageRect;
        }

        void PaintOverlay()
        {
            if (overlayWindow_ == nullptr)
                return;

            PAINTSTRUCT paint = {};
            HDC target = BeginPaint(overlayWindow_, &paint);
            if (target == nullptr)
                return;

            RECT client = {};
            GetClientRect(overlayWindow_, &client);
            const int clientWidth = std::max(
                1,
                static_cast<int>(client.right - client.left));
            const int clientHeight = std::max(
                1,
                static_cast<int>(client.bottom - client.top));

            HBRUSH black = CreateSolidBrush(RGB(2, 3, 4));
            FillRect(target, &client, black);
            DeleteObject(black);

            const RECT imageRect = ResolveImageRect(clientWidth, clientHeight);
            const int imageWidth = std::max(
                1,
                static_cast<int>(imageRect.right - imageRect.left));
            const int imageHeight = std::max(
                1,
                static_cast<int>(imageRect.bottom - imageRect.top));

            if (backgroundBitmap_ != nullptr)
            {
                HDC source = CreateCompatibleDC(target);
                HGDIOBJ oldBitmap = SelectObject(source, backgroundBitmap_);
                const int oldMode = SetStretchBltMode(target, HALFTONE);
                StretchBlt(
                    target,
                    imageRect.left,
                    imageRect.top,
                    imageWidth,
                    imageHeight,
                    source,
                    0,
                    0,
                    bitmapWidth_,
                    bitmapHeight_,
                    SRCCOPY);
                SetStretchBltMode(target, oldMode);
                SelectObject(source, oldBitmap);
                DeleteDC(source);
            }

            // Match Gate 2C's authoritative lower-panel cover, but deliberately
            // omit the live identity copy/button: those disappear as soon as
            // ENTER HUB begins the transition.
            RECT lowerCover = {
                imageRect.left + static_cast<int>(imageWidth * 0.235),
                imageRect.top + static_cast<int>(imageHeight * 0.720),
                imageRect.left + static_cast<int>(imageWidth * 0.765),
                imageRect.top + static_cast<int>(imageHeight * 0.975)
            };
            HBRUSH panel = CreateSolidBrush(RGB(5, 7, 9));
            FillRect(target, &lowerCover, panel);
            DeleteObject(panel);

            EndPaint(overlayWindow_, &paint);
        }

        void PaintIrisHalf(HWND irisWindow)
        {
            if (irisWindow == nullptr || backgroundBitmap_ == nullptr)
                return;

            PAINTSTRUCT paint = {};
            HDC target = BeginPaint(irisWindow, &paint);
            if (target == nullptr)
                return;

            RECT client = {};
            GetClientRect(irisWindow, &client);
            const int width = std::max(
                1,
                static_cast<int>(client.right - client.left));
            const int height = std::max(
                1,
                static_cast<int>(client.bottom - client.top));

            const int sourceCenterX = std::clamp(
                static_cast<int>(bitmapWidth_ * IrisCenterXNormalized + 0.5),
                1,
                bitmapWidth_ - 1);
            const int sourceCenterY = std::clamp(
                static_cast<int>(bitmapHeight_ * IrisCenterYNormalized + 0.5),
                1,
                bitmapHeight_ - 1);
            const int sourceRadius = std::max(
                1,
                static_cast<int>(
                    std::min(
                        bitmapWidth_ * IrisRadiusXNormalized,
                        bitmapHeight_ * IrisRadiusYNormalized) + 0.5));

            const bool rightHalf = irisWindow == rightIrisWindow_;
            const int sourceLeft = rightHalf
                ? sourceCenterX
                : sourceCenterX - sourceRadius;
            const int sourceTop = sourceCenterY - sourceRadius;

            HDC source = CreateCompatibleDC(target);
            HGDIOBJ oldBitmap = SelectObject(source, backgroundBitmap_);
            const int oldMode = SetStretchBltMode(target, HALFTONE);
            StretchBlt(
                target,
                0,
                0,
                width,
                height,
                source,
                sourceLeft,
                sourceTop,
                sourceRadius,
                sourceRadius * 2,
                SRCCOPY);
            SetStretchBltMode(target, oldMode);
            SelectObject(source, oldBitmap);
            DeleteDC(source);

            // A fine red seam gives the split a deliberate mechanical read.
            HPEN seam = CreatePen(
                PS_SOLID,
                std::max(1, height / 360),
                RGB(190, 24, 34));
            HGDIOBJ oldPen = SelectObject(target, seam);
            const int seamX = rightHalf ? 0 : width - 1;
            MoveToEx(target, seamX, height / 5, nullptr);
            LineTo(target, seamX, (height * 4) / 5);
            SelectObject(target, oldPen);
            DeleteObject(seam);

            EndPaint(irisWindow, &paint);
        }

        void UpdateIrisGeometry(const double progress)
        {
            if (leftIrisWindow_ == nullptr ||
                rightIrisWindow_ == nullptr ||
                overlayWindow_ == nullptr ||
                bitmapWidth_ <= 0 ||
                bitmapHeight_ <= 0)
            {
                return;
            }

            RECT client = {};
            if (!GetClientRect(overlayWindow_, &client))
                return;
            const int clientWidth = std::max(
                1,
                static_cast<int>(client.right - client.left));
            const int clientHeight = std::max(
                1,
                static_cast<int>(client.bottom - client.top));
            const RECT imageRect = ResolveImageRect(clientWidth, clientHeight);
            const int imageWidth = std::max(
                1,
                static_cast<int>(imageRect.right - imageRect.left));
            const int imageHeight = std::max(
                1,
                static_cast<int>(imageRect.bottom - imageRect.top));

            const double imageScale = std::min(
                static_cast<double>(imageWidth) /
                    static_cast<double>(bitmapWidth_),
                static_cast<double>(imageHeight) /
                    static_cast<double>(bitmapHeight_));
            const double sourceRadius = std::min(
                bitmapWidth_ * IrisRadiusXNormalized,
                bitmapHeight_ * IrisRadiusYNormalized);
            const int diameter = std::max(
                8,
                static_cast<int>(sourceRadius * 2.0 * imageScale + 0.5));
            const int halfWidth = std::max(4, diameter / 2);
            const int irisHeight = diameter;

            const int centerX = imageRect.left + static_cast<int>(
                imageWidth * IrisCenterXNormalized + 0.5);
            const int centerY = imageRect.top + static_cast<int>(
                imageHeight * IrisCenterYNormalized + 0.5);

            RECT overlayScreen = {};
            if (!GetWindowRect(overlayWindow_, &overlayScreen))
                return;

            const double eased = SmoothStep(progress);
            const int travelDistance =
                std::max(centerX, clientWidth - centerX) + 24;
            const int travel = static_cast<int>(
                travelDistance * eased + 0.5);

            const int leftX =
                overlayScreen.left + centerX - halfWidth - travel;
            const int rightX =
                overlayScreen.left + centerX + travel;
            const int topY =
                overlayScreen.top + centerY - (irisHeight / 2);

            const bool sizeChanged =
                halfWidth != irisHalfWidth_ || irisHeight != irisHeight_;
            irisHalfWidth_ = halfWidth;
            irisHeight_ = irisHeight;

            SetWindowPos(
                leftIrisWindow_,
                HWND_TOP,
                leftX,
                topY,
                halfWidth,
                irisHeight,
                SWP_NOACTIVATE | SWP_SHOWWINDOW);
            SetWindowPos(
                rightIrisWindow_,
                HWND_TOP,
                rightX,
                topY,
                halfWidth,
                irisHeight,
                SWP_NOACTIVATE | SWP_SHOWWINDOW);

            if (sizeChanged)
                ConfigureIrisRegions(halfWidth, irisHeight);
        }

        void ConfigureIrisRegions(
            const int halfWidth,
            const int height)
        {
            HRGN leftEllipse = CreateEllipticRgn(
                0,
                0,
                halfWidth * 2,
                height);
            HRGN leftClip = CreateRectRgn(0, 0, halfWidth, height);
            if (leftEllipse != nullptr && leftClip != nullptr)
            {
                CombineRgn(leftEllipse, leftEllipse, leftClip, RGN_AND);
                if (SetWindowRgn(leftIrisWindow_, leftEllipse, TRUE) == 0)
                    DeleteObject(leftEllipse);
            }
            else if (leftEllipse != nullptr)
            {
                DeleteObject(leftEllipse);
            }
            if (leftClip != nullptr)
                DeleteObject(leftClip);

            HRGN rightEllipse = CreateEllipticRgn(
                0,
                0,
                halfWidth * 2,
                height);
            HRGN rightClip = CreateRectRgn(
                halfWidth,
                0,
                halfWidth * 2,
                height);
            if (rightEllipse != nullptr && rightClip != nullptr)
            {
                CombineRgn(rightEllipse, rightEllipse, rightClip, RGN_AND);
                OffsetRgn(rightEllipse, -halfWidth, 0);
                if (SetWindowRgn(rightIrisWindow_, rightEllipse, TRUE) == 0)
                    DeleteObject(rightEllipse);
            }
            else if (rightEllipse != nullptr)
            {
                DeleteObject(rightEllipse);
            }
            if (rightClip != nullptr)
                DeleteObject(rightClip);
        }

        void DestroyIrisWindows() noexcept
        {
            if (leftIrisWindow_ != nullptr)
            {
                DestroyWindow(leftIrisWindow_);
                leftIrisWindow_ = nullptr;
            }
            if (rightIrisWindow_ != nullptr)
            {
                DestroyWindow(rightIrisWindow_);
                rightIrisWindow_ = nullptr;
            }
        }

        HWND parentWindow_ = nullptr;
        HWND overlayWindow_ = nullptr;
        HWND leftIrisWindow_ = nullptr;
        HWND rightIrisWindow_ = nullptr;
        HBITMAP backgroundBitmap_ = nullptr;
        int bitmapWidth_ = 0;
        int bitmapHeight_ = 0;
        int irisHalfWidth_ = 0;
        int irisHeight_ = 0;
        bool active_ = false;
        bool completed_ = false;
        double progress_ = 0.0;
        std::chrono::steady_clock::time_point startedAt_{};
    };
}
