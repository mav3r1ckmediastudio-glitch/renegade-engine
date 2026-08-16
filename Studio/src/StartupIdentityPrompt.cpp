#include "StartupIdentityPrompt.h"

#include "StudioUserPreferences.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace
{
    constexpr wchar_t IdentityPromptClassName[] = L"RenegadeStartupIdentityPrompt";
    constexpr std::wstring_view PromptText = L"Welcome developer, what is your name?";
    constexpr auto TypewriterInterval = std::chrono::milliseconds(32);
    constexpr auto CursorBlinkInterval = std::chrono::milliseconds(500);

    COLORREF PromptTextColor() noexcept { return RGB(210, 216, 220); }
    COLORREF IdentityTextColor() noexcept { return RGB(245, 247, 248); }
    COLORREF ErrorTextColor() noexcept { return RGB(205, 92, 92); }
}

namespace renegade::studio
{
    StartupIdentityPrompt::~StartupIdentityPrompt()
    {
        Shutdown();
    }

    bool StartupIdentityPrompt::EnsureWindowClass() noexcept
    {
        static const bool registered = []() {
            WNDCLASSEXW windowClass = {};
            windowClass.cbSize = sizeof(windowClass);
            windowClass.lpfnWndProc = StartupIdentityPrompt::WindowProc;
            windowClass.hInstance = GetModuleHandleW(nullptr);
            windowClass.hCursor = LoadCursor(nullptr, IDC_IBEAM);
            windowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            windowClass.lpszClassName = IdentityPromptClassName;

            if (RegisterClassExW(&windowClass) != 0)
                return true;
            return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        }();
        return registered;
    }

    bool StartupIdentityPrompt::Start(const HWND parentWindow)
    {
        Shutdown();
        if (parentWindow == nullptr || !IsWindow(parentWindow) || !EnsureWindowClass())
            return false;

        parentWindow_ = parentWindow;
        RECT client = {};
        if (!GetClientRect(parentWindow_, &client))
            return false;

        window_ = CreateWindowExW(
            0,
            IdentityPromptClassName,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0,
            0,
            std::max<LONG>(1, client.right - client.left),
            std::max<LONG>(1, client.bottom - client.top),
            parentWindow_,
            nullptr,
            GetModuleHandleW(nullptr),
            this);
        if (window_ == nullptr)
        {
            parentWindow_ = nullptr;
            return false;
        }

        const UINT dpi = GetDpiForWindow(parentWindow_);
        font_ = CreateFontW(
            -MulDiv(20, static_cast<int>(dpi == 0 ? 96 : dpi), 72),
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            FIXED_PITCH | FF_MODERN,
            L"Cascadia Mono");

        identity_.clear();
        lastError_.clear();
        cursorIndex_ = 0;
        promptVisibleCharacters_ = 0;
        active_ = true;
        completed_ = false;
        cursorVisible_ = true;
        startedAt_ = std::chrono::steady_clock::now();
        lastCursorToggle_ = startedAt_;

        Resize();
        Focus();
        InvalidateRect(window_, nullptr, TRUE);
        UpdateWindow(window_);
        return true;
    }

    void StartupIdentityPrompt::Pump()
    {
        if (window_ == nullptr)
            return;

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = now - startedAt_;
        const auto typed = static_cast<std::size_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() /
            TypewriterInterval.count());
        const std::size_t nextVisible = std::min<std::size_t>(PromptText.size(), typed);

        bool redraw = nextVisible != promptVisibleCharacters_;
        promptVisibleCharacters_ = nextVisible;

        if (now - lastCursorToggle_ >= CursorBlinkInterval)
        {
            cursorVisible_ = !cursorVisible_;
            lastCursorToggle_ = now;
            redraw = true;
        }

        if (redraw)
            InvalidateRect(window_, nullptr, FALSE);
    }

    void StartupIdentityPrompt::Resize()
    {
        if (window_ == nullptr || parentWindow_ == nullptr)
            return;

        RECT client = {};
        if (!GetClientRect(parentWindow_, &client))
            return;

        SetWindowPos(
            window_,
            HWND_TOP,
            0,
            0,
            std::max<LONG>(1, client.right - client.left),
            std::max<LONG>(1, client.bottom - client.top),
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    void StartupIdentityPrompt::Focus()
    {
        if (window_ != nullptr && active_)
            SetFocus(window_);
    }

    void StartupIdentityPrompt::Shutdown()
    {
        active_ = false;
        if (window_ != nullptr)
        {
            DestroyWindow(window_);
            window_ = nullptr;
        }
        if (font_ != nullptr)
        {
            DeleteObject(font_);
            font_ = nullptr;
        }
        parentWindow_ = nullptr;
    }

    bool StartupIdentityPrompt::PromptFinishedTyping() const noexcept
    {
        return promptVisibleCharacters_ >= PromptText.size();
    }

    void StartupIdentityPrompt::HandleCharacter(const wchar_t character)
    {
        if (!active_ || !PromptFinishedTyping())
            return;

        if (character == L'\b')
        {
            if (cursorIndex_ > 0 && !identity_.empty())
            {
                identity_.erase(cursorIndex_ - 1, 1);
                --cursorIndex_;
                lastError_.clear();
            }
        }
        else if (character == L'\r' || character == L'\n')
        {
            TryConfirm();
            return;
        }
        else if (character >= 0x20 && character != 0x7F)
        {
            if (identity_.size() < StudioUserPreferences::MaxDeveloperIdentityCharacters)
            {
                identity_.insert(identity_.begin() + static_cast<std::ptrdiff_t>(cursorIndex_), character);
                ++cursorIndex_;
                lastError_.clear();
            }
            else
            {
                lastError_ = L"IDENTITY MUST BE 32 CHARACTERS OR FEWER";
            }
        }

        cursorVisible_ = true;
        lastCursorToggle_ = std::chrono::steady_clock::now();
        InvalidateRect(window_, nullptr, FALSE);
    }

    void StartupIdentityPrompt::HandleKeyDown(const WPARAM key)
    {
        if (!active_ || !PromptFinishedTyping())
            return;

        switch (key)
        {
        case VK_DELETE:
            if (cursorIndex_ < identity_.size())
            {
                identity_.erase(cursorIndex_, 1);
                lastError_.clear();
            }
            break;
        case VK_LEFT:
            if (cursorIndex_ > 0)
                --cursorIndex_;
            break;
        case VK_RIGHT:
            if (cursorIndex_ < identity_.size())
                ++cursorIndex_;
            break;
        case VK_HOME:
            cursorIndex_ = 0;
            break;
        case VK_END:
            cursorIndex_ = identity_.size();
            break;
        default:
            return;
        }

        cursorVisible_ = true;
        lastCursorToggle_ = std::chrono::steady_clock::now();
        InvalidateRect(window_, nullptr, FALSE);
    }

    void StartupIdentityPrompt::TryConfirm()
    {
        std::wstring normalized;
        if (!StudioUserPreferences::NormalizeDeveloperIdentity(identity_, normalized))
        {
            if (identity_.size() > StudioUserPreferences::MaxDeveloperIdentityCharacters)
                lastError_ = L"IDENTITY MUST BE 32 CHARACTERS OR FEWER";
            else
                lastError_ = L"IDENTITY REQUIRED";
            InvalidateRect(window_, nullptr, FALSE);
            return;
        }

        if (!StudioUserPreferences::SaveDeveloperIdentity(normalized))
        {
            lastError_ = L"UNABLE TO SAVE USER PREFERENCES";
            InvalidateRect(window_, nullptr, FALSE);
            return;
        }

        identity_ = std::move(normalized);
        cursorIndex_ = identity_.size();
        lastError_.clear();
        completed_ = true;
        active_ = false;
        InvalidateRect(window_, nullptr, FALSE);
        UpdateWindow(window_);
    }

    void StartupIdentityPrompt::Paint()
    {
        if (window_ == nullptr)
            return;

        PAINTSTRUCT paint = {};
        HDC dc = BeginPaint(window_, &paint);
        if (dc == nullptr)
            return;

        RECT client = {};
        GetClientRect(window_, &client);
        FillRect(dc, &client, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        SetBkMode(dc, TRANSPARENT);

        HFONT previousFont = nullptr;
        if (font_ != nullptr)
            previousFont = static_cast<HFONT>(SelectObject(dc, font_));

        TEXTMETRICW metrics = {};
        GetTextMetricsW(dc, &metrics);
        const int lineHeight = std::max(30, metrics.tmHeight + metrics.tmExternalLeading + 8);
        const int left = std::max(48L, (client.right - client.left) / 10);
        const int top = std::max(72L, (client.bottom - client.top) / 3);

        const std::wstring prompt(PromptText.substr(0, promptVisibleCharacters_));
        SetTextColor(dc, PromptTextColor());
        TextOutW(dc, left, top, prompt.data(), static_cast<int>(prompt.size()));

        if (PromptFinishedTyping())
        {
            const int inputTop = top + (lineHeight * 2);
            constexpr std::wstring_view prefix = L"> ";
            SetTextColor(dc, IdentityTextColor());
            TextOutW(dc, left, inputTop, prefix.data(), static_cast<int>(prefix.size()));
            TextOutW(
                dc,
                left,
                inputTop + lineHeight,
                identity_.data(),
                static_cast<int>(identity_.size()));

            if (cursorVisible_ && active_)
            {
                const std::wstring beforeCursor = identity_.substr(0, cursorIndex_);
                SIZE extent = {};
                if (!beforeCursor.empty())
                {
                    GetTextExtentPoint32W(
                        dc,
                        beforeCursor.data(),
                        static_cast<int>(beforeCursor.size()),
                        &extent);
                }
                RECT cursor = {
                    left + extent.cx,
                    inputTop + lineHeight + metrics.tmAscent + 3,
                    left + extent.cx + std::max(8, metrics.tmAveCharWidth),
                    inputTop + lineHeight + metrics.tmAscent + 6
                };
                HBRUSH cursorBrush = CreateSolidBrush(IdentityTextColor());
                FillRect(dc, &cursor, cursorBrush);
                DeleteObject(cursorBrush);
            }

            if (!lastError_.empty())
            {
                SetTextColor(dc, ErrorTextColor());
                TextOutW(
                    dc,
                    left,
                    inputTop + (lineHeight * 3),
                    lastError_.data(),
                    static_cast<int>(lastError_.size()));
            }
        }

        if (previousFont != nullptr)
            SelectObject(dc, previousFont);
        EndPaint(window_, &paint);
    }

    LRESULT CALLBACK StartupIdentityPrompt::WindowProc(
        const HWND window,
        const UINT message,
        const WPARAM wParam,
        const LPARAM lParam)
    {
        StartupIdentityPrompt* self = reinterpret_cast<StartupIdentityPrompt*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));

        if (message == WM_NCCREATE)
        {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            self = static_cast<StartupIdentityPrompt*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }

        if (self != nullptr)
        {
            switch (message)
            {
            case WM_PAINT:
                self->Paint();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_CHAR:
                self->HandleCharacter(static_cast<wchar_t>(wParam));
                return 0;
            case WM_KEYDOWN:
                self->HandleKeyDown(wParam);
                return 0;
            case WM_GETDLGCODE:
                return DLGC_WANTALLKEYS | DLGC_WANTCHARS | DLGC_WANTARROWS;
            case WM_LBUTTONDOWN:
                self->Focus();
                return 0;
            default:
                break;
            }
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
}
