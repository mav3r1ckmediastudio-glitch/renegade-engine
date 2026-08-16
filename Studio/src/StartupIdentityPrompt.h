#pragma once

#include <Windows.h>

#include <chrono>
#include <string>

namespace renegade::studio
{
    class StartupIdentityPrompt final
    {
    public:
        StartupIdentityPrompt() = default;
        ~StartupIdentityPrompt();

        StartupIdentityPrompt(const StartupIdentityPrompt&) = delete;
        StartupIdentityPrompt& operator=(const StartupIdentityPrompt&) = delete;

        [[nodiscard]] bool Start(HWND parentWindow);
        void Pump();
        void Resize();
        void Focus();
        void Shutdown();

        [[nodiscard]] bool IsActive() const noexcept { return active_; }
        [[nodiscard]] bool IsCompleted() const noexcept { return completed_; }
        [[nodiscard]] const std::wstring& Identity() const noexcept { return identity_; }
        [[nodiscard]] const std::wstring& LastError() const noexcept { return lastError_; }

    private:
        static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
        static bool EnsureWindowClass() noexcept;

        void Paint();
        void HandleCharacter(wchar_t character);
        void HandleKeyDown(WPARAM key);
        void TryConfirm();
        [[nodiscard]] bool PromptFinishedTyping() const noexcept;

        HWND parentWindow_ = nullptr;
        HWND window_ = nullptr;
        HFONT font_ = nullptr;
        bool active_ = false;
        bool completed_ = false;
        bool cursorVisible_ = true;
        std::size_t cursorIndex_ = 0;
        std::size_t promptVisibleCharacters_ = 0;
        std::wstring identity_;
        std::wstring lastError_;
        std::chrono::steady_clock::time_point startedAt_{};
        std::chrono::steady_clock::time_point lastCursorToggle_{};
    };
}
