#include "StudioApplication.h"
#include "StartupIdentityHandshake.h"
#include "StartupIdentityPrompt.h"
#include "StartupMediaFoundationPlayer.h"
#include "StudioUserPreferences.h"

#include <Windows.h>

#include <algorithm>
#include <cwchar>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

namespace
{
    renegade::studio::StudioApplication* application = nullptr;
    renegade::studio::StartupMediaFoundationPlayer* startupPlayer = nullptr;
    renegade::studio::StartupIdentityPrompt* startupIdentityPrompt = nullptr;
    renegade::studio::StartupIdentityHandshake* startupIdentityHandshake = nullptr;
    bool windowReadyForWicked = false;
    bool startupMediaActive = false;
    bool startupIdentityActive = false;
    bool startupHandshakeActive = false;

    void ResetGate2ALog() noexcept
    {
        std::error_code error;
        std::filesystem::create_directories("Saved/Diagnostics", error);
        std::ofstream stream(
            "Saved/Diagnostics/PR58Gate2AStartup.log",
            std::ios::out | std::ios::trunc);
    }

    void LogGate2A(std::string_view message) noexcept
    {
        std::error_code error;
        std::filesystem::create_directories("Saved/Diagnostics", error);
        std::ofstream stream(
            "Saved/Diagnostics/PR58Gate2AStartup.log",
            std::ios::out | std::ios::app);
        if (stream)
            stream << "[PR58-GATE2A] " << message << '\n';
    }

    void ResetGate2BLog() noexcept
    {
        std::error_code error;
        std::filesystem::create_directories("Saved/Diagnostics", error);
        std::ofstream stream(
            "Saved/Diagnostics/PR58Gate2BStartup.log",
            std::ios::out | std::ios::trunc);
    }

    void LogGate2B(std::string_view message) noexcept
    {
        std::error_code error;
        std::filesystem::create_directories("Saved/Diagnostics", error);
        std::ofstream stream(
            "Saved/Diagnostics/PR58Gate2BStartup.log",
            std::ios::out | std::ios::app);
        if (stream)
            stream << "[PR58-GATE2B] " << message << '\n';
    }

    void ResetGate2CLog() noexcept
    {
        std::error_code error;
        std::filesystem::create_directories("Saved/Diagnostics", error);
        std::ofstream stream(
            "Saved/Diagnostics/PR58Gate2CStartup.log",
            std::ios::out | std::ios::trunc);
    }

    void LogGate2C(std::string_view message) noexcept
    {
        std::error_code error;
        std::filesystem::create_directories("Saved/Diagnostics", error);
        std::ofstream stream(
            "Saved/Diagnostics/PR58Gate2CStartup.log",
            std::ios::out | std::ios::app);
        if (stream)
            stream << "[PR58-GATE2C] " << message << '\n';
    }

    std::string DescribeFile(const std::filesystem::path& path) noexcept
    {
        std::error_code error;
        const bool exists = std::filesystem::exists(path, error);
        const auto bytes = exists ? std::filesystem::file_size(path, error) : 0;
        return "exists=" + std::string(exists ? "1" : "0") +
               " // bytes=" + std::to_string(bytes);
    }

    RECT ResolveLaunchMonitorWorkRect() noexcept
    {
        POINT cursor = {};
        if (!GetCursorPos(&cursor))
        {
            cursor.x = 0;
            cursor.y = 0;
        }

        const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (monitor != nullptr && GetMonitorInfoW(monitor, &monitorInfo))
            return monitorInfo.rcWork;

        RECT fallback = {};
        fallback.right = GetSystemMetrics(SM_CXSCREEN);
        fallback.bottom = GetSystemMetrics(SM_CYSCREEN);
        return fallback;
    }

    const wchar_t* GraphicsBackendTitle() noexcept
    {
        const auto* device = wi::graphics::GetDevice();
        if (device != nullptr && std::strcmp(device->GetTag(), "[Vulkan]") == 0)
            return L"Renegade Studio - Phase 3 [Vulkan]";
        return L"Renegade Studio - Phase 3 [DX12]";
    }

    LRESULT CALLBACK RenegadeWindowProc(
        const HWND window,
        const UINT message,
        const WPARAM wParam,
        const LPARAM lParam)
    {
        switch (message)
        {
        case WM_SIZE:
            if (startupHandshakeActive && startupIdentityHandshake != nullptr)
            {
                startupIdentityHandshake->Resize();
            }
            else if (startupIdentityActive && startupIdentityPrompt != nullptr)
            {
                startupIdentityPrompt->Resize();
            }
            else if (startupMediaActive && startupPlayer != nullptr)
            {
                startupPlayer->Resize();
            }
            else if (application != nullptr &&
                     windowReadyForWicked &&
                     application->is_window_active)
            {
                application->SetWindow(window);
            }
            return 0;

        case WM_DPICHANGED:
        {
            const auto* suggested = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(
                window,
                nullptr,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOACTIVATE | SWP_NOZORDER);
            if (startupHandshakeActive && startupIdentityHandshake != nullptr)
            {
                startupIdentityHandshake->Resize();
            }
            else if (startupIdentityActive && startupIdentityPrompt != nullptr)
            {
                startupIdentityPrompt->Resize();
            }
            else if (startupMediaActive && startupPlayer != nullptr)
            {
                startupPlayer->Resize();
            }
            else if (application != nullptr &&
                     windowReadyForWicked &&
                     application->is_window_active)
            {
                application->SetWindow(window);
            }
            return 0;
        }

        case WM_PAINT:
            if (startupMediaActive && startupPlayer != nullptr)
            {
                PAINTSTRUCT paint = {};
                BeginPaint(window, &paint);
                startupPlayer->Repaint();
                EndPaint(window, &paint);
                return 0;
            }
            break;

        case WM_ERASEBKGND:
            if (startupMediaActive || startupIdentityActive || startupHandshakeActive)
                return 1;
            break;

        case WM_CHAR:
            if (startupIdentityActive || startupHandshakeActive)
                return 0;
            if (wParam == VK_BACK)
            {
                wi::gui::TextInputField::DeleteFromInput();
            }
            else if (wParam != VK_RETURN)
            {
                wi::gui::TextInputField::AddInput(static_cast<wchar_t>(wParam));
            }
            return 0;

        case WM_INPUT:
            wi::input::rawinput::ParseMessage(reinterpret_cast<void*>(lParam));
            return 0;

        case WM_KILLFOCUS:
            if (application != nullptr)
                application->is_window_active = false;
            return 0;

        case WM_SETFOCUS:
            if (application != nullptr)
                application->is_window_active = true;
            if (startupIdentityActive && startupIdentityPrompt != nullptr)
                startupIdentityPrompt->Focus();
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
}

int APIENTRY wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE,
    _In_ LPWSTR commandLine,
    _In_ int showCommand)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    wi::arguments::Parse(commandLine);

    auto localApplication = std::make_unique<renegade::studio::StudioApplication>();
    application = localApplication.get();

    if (wi::arguments::HasArgument("startup-smoke"))
    {
        application = nullptr;
        return 0;
    }

    std::filesystem::path executableDirectory;
    wchar_t executablePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, executablePath, MAX_PATH) > 0)
    {
        executableDirectory = std::filesystem::path(executablePath).parent_path();
        if (!executableDirectory.empty())
            SetCurrentDirectoryW(executableDirectory.c_str());
    }

    ResetGate2ALog();
    ResetGate2BLog();
    ResetGate2CLog();
    LogGate2A("PROCESS_START");
    LogGate2B("PROCESS_START");
    LogGate2C("PROCESS_START");

    if (wi::arguments::HasArgument("reset-developer-identity"))
    {
        const bool cleared = renegade::studio::StudioUserPreferences::ClearDeveloperIdentity();
        LogGate2B(std::string("IDENTITY_RESET_REQUESTED // success=") + (cleared ? "1" : "0"));
    }

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = RenegadeWindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = L"RenegadeStudioWindow";

    if (RegisterClassExW(&windowClass) == 0)
    {
        LogGate2A("REGISTER_WINDOW_CLASS_FAILED");
        application = nullptr;
        return 1;
    }

    const RECT workRect = ResolveLaunchMonitorWorkRect();
    const int workWidth = std::max(640, static_cast<int>(workRect.right - workRect.left));
    const int workHeight = std::max(480, static_cast<int>(workRect.bottom - workRect.top));
    const int initialWidth = std::min(1600, workWidth);
    const int initialHeight = std::min(900, workHeight);
    const int initialX = static_cast<int>(workRect.left) + (workWidth - initialWidth) / 2;
    const int initialY = static_cast<int>(workRect.top) + (workHeight - initialHeight) / 2;

    const HWND window = CreateWindowExW(
        WS_EX_APPWINDOW,
        windowClass.lpszClassName,
        L"Renegade Studio - Phase 3",
        WS_OVERLAPPEDWINDOW,
        initialX,
        initialY,
        initialWidth,
        initialHeight,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (window == nullptr)
    {
        LogGate2A("CREATE_WINDOW_FAILED");
        application = nullptr;
        return 2;
    }

    ShowWindow(window, showCommand == SW_HIDE ? SW_HIDE : SW_MAXIMIZE);
    UpdateWindow(window);
    LogGate2A(
        "WINDOW_VISIBLE_MAXIMIZED // workarea=" +
        std::to_string(workWidth) + "x" + std::to_string(workHeight));

    windowReadyForWicked = true;
    application->SetWindow(window);
    SetWindowTextW(window, GraphicsBackendTitle());
    LogGate2A("WICKED_WINDOW_BOUND");

    LogGate2A("STUDIO_INIT_BEGIN");
    application->Initialize();
    wi::initializer::WaitForInitializationsToFinish();
    LogGate2A("STUDIO_INIT_END");

    if (application->GetActivePath() == nullptr)
    {
        LogGate2A("STUDIO_PATH_MISSING");
        application = nullptr;
        return 3;
    }

    const std::filesystem::path revealPath =
        executableDirectory / L"Content" / L"startup" / L"renegade_logo_reveal_v2.mp4";
    LogGate2A("MEDIA_FILE // " + DescribeFile(revealPath));

    renegade::studio::StartupMediaFoundationPlayer reveal;
    startupPlayer = &reveal;
    LogGate2A("MEDIA_FOUNDATION_REVEAL_BEGIN");
    const bool revealStarted = reveal.Start(window, revealPath.wstring());

    MSG message = {};
    bool revealAcceptedHandoff = false;
    if (revealStarted)
    {
        startupMediaActive = true;
        windowReadyForWicked = false;
        LogGate2A("MEDIA_FOUNDATION_REVEAL_ACTIVE");

        while (message.message != WM_QUIT && reveal.IsActive())
        {
            if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            else
            {
                reveal.Pump();
                Sleep(1);
            }
        }

        startupMediaActive = false;
        revealAcceptedHandoff = reveal.IsFinished() && !reveal.HasFailed();
        if (reveal.HasFailed())
        {
            LogGate2A("MEDIA_FOUNDATION_REVEAL_FAILED // " + reveal.FailureReason());
        }
        else if (revealAcceptedHandoff)
        {
            LogGate2A("MEDIA_FOUNDATION_REVEAL_FINISHED");
        }

        reveal.Shutdown(revealAcceptedHandoff);
    }
    else
    {
        LogGate2A("MEDIA_FOUNDATION_REVEAL_FAILED // " + reveal.FailureReason());
        reveal.Shutdown(false);
    }

    startupPlayer = nullptr;
    startupMediaActive = false;
    windowReadyForWicked = true;

    auto developerIdentity = renegade::studio::StudioUserPreferences::LoadDeveloperIdentity();
    LogGate2B(
        std::string("IDENTITY_PREFERENCES // found=") +
        (developerIdentity.has_value() ? "1" : "0"));

    renegade::studio::StartupIdentityPrompt identityPrompt;
    bool identityPromptHeld = false;

    if (message.message != WM_QUIT && !developerIdentity.has_value())
    {
        startupIdentityPrompt = &identityPrompt;
        if (identityPrompt.Start(window))
        {
            startupIdentityActive = true;
            windowReadyForWicked = false;
            LogGate2B("FIRST_RUN_PROMPT_ACTIVE");

            // The prompt is completely opaque before Gate 2A's final-black
            // overlay is removed, preserving the accepted no-Hub-flash behavior.
            if (revealAcceptedHandoff)
                reveal.ReleaseFadeOverlay();

            while (message.message != WM_QUIT && identityPrompt.IsActive())
            {
                if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                else
                {
                    identityPrompt.Pump();
                    Sleep(8);
                }
            }

            startupIdentityActive = false;
            windowReadyForWicked = true;

            if (message.message != WM_QUIT && identityPrompt.IsCompleted())
            {
                developerIdentity = identityPrompt.Identity();
                identityPromptHeld = true;
                LogGate2B(
                    "FIRST_RUN_IDENTITY_ACCEPTED // characters=" +
                    std::to_string(developerIdentity->size()));
                LogGate2B("IDENTITY_PROMPT_HELD_FOR_GATE2C_HANDOFF");
            }
            else if (message.message == WM_QUIT)
            {
                LogGate2B("FIRST_RUN_PROMPT_ABORTED_BY_WINDOW_CLOSE");
            }
        }
        else
        {
            LogGate2B("FIRST_RUN_PROMPT_CREATE_FAILED // fail_open=1");
        }

        startupIdentityPrompt = nullptr;
        startupIdentityActive = false;
        windowReadyForWicked = true;
    }
    else if (developerIdentity.has_value())
    {
        LogGate2B(
            "FIRST_RUN_PROMPT_SKIPPED // saved_identity_characters=" +
            std::to_string(developerIdentity->size()));
    }

    bool firstStudioFrameReady = false;

    if (message.message != WM_QUIT && developerIdentity.has_value())
    {
        const std::filesystem::path handshakeVideoPath =
            executableDirectory / L"Content" / L"startup" / L"renegade_identity_handshake_v1.mp4";
        const std::filesystem::path handshakeFinalFramePath =
            executableDirectory / L"Content" / L"startup" / L"renegade_identity_handshake_final.bmp";

        LogGate2C("MOTION_PLATE_FILE // " + DescribeFile(handshakeVideoPath));
        LogGate2C("FINAL_FRAME_FILE // " + DescribeFile(handshakeFinalFramePath));

        renegade::studio::StartupMediaFoundationPlayer handshakeVideo;
        startupPlayer = &handshakeVideo;
        LogGate2C("HANDSHAKE_MOTION_BEGIN");
        const bool handshakeVideoStarted =
            handshakeVideo.Start(window, handshakeVideoPath.wstring());

        bool handshakeMotionSucceeded = false;
        if (handshakeVideoStarted)
        {
            startupMediaActive = true;
            windowReadyForWicked = false;
            LogGate2C("HANDSHAKE_MOTION_ACTIVE");

            // Keep the previous opaque startup surface visible until Media
            // Foundation has constructed the EVR video path. This prevents a
            // transient Hub/editor frame between Gate 2B and the motion plate.
            while (message.message != WM_QUIT &&
                   handshakeVideo.IsActive() &&
                   !handshakeVideo.HasVideo())
            {
                if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                else
                {
                    handshakeVideo.Pump();
                    Sleep(1);
                }
            }

            if (message.message != WM_QUIT && handshakeVideo.HasVideo())
            {
                if (identityPromptHeld)
                {
                    identityPrompt.Shutdown();
                    identityPromptHeld = false;
                    LogGate2B("IDENTITY_PROMPT_RELEASED_TO_GATE2C");
                }
                if (revealAcceptedHandoff)
                    reveal.ReleaseFadeOverlay();
                LogGate2C("HANDSHAKE_MOTION_VISIBLE");
            }

            while (message.message != WM_QUIT && handshakeVideo.IsActive())
            {
                if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                else
                {
                    handshakeVideo.Pump();
                    Sleep(1);
                }
            }

            startupMediaActive = false;
            handshakeMotionSucceeded =
                handshakeVideo.IsFinished() && !handshakeVideo.HasFailed();
            if (handshakeVideo.HasFailed())
            {
                LogGate2C("HANDSHAKE_MOTION_FAILED // " + handshakeVideo.FailureReason());
            }
            else if (handshakeMotionSucceeded)
            {
                LogGate2C("HANDSHAKE_MOTION_FINISHED");
            }

            // Keep the player's final-black fade overlay until the exact final
            // still + native Renegade UI is fully painted on top of it.
            handshakeVideo.Shutdown(handshakeMotionSucceeded);
        }
        else
        {
            LogGate2C("HANDSHAKE_MOTION_FAILED // " + handshakeVideo.FailureReason());
            handshakeVideo.Shutdown(false);
        }

        startupPlayer = nullptr;
        startupMediaActive = false;

        renegade::studio::StartupIdentityHandshake handshakeUi;
        startupIdentityHandshake = &handshakeUi;
        const bool handshakeUiStarted = handshakeUi.Start(
            window,
            handshakeFinalFramePath.wstring(),
            *developerIdentity);

        if (handshakeUiStarted)
        {
            startupHandshakeActive = true;
            windowReadyForWicked = false;
            LogGate2C(
                std::string("FINAL_IDENTITY_UI_ACTIVE // background=") +
                (handshakeUi.HasBackgroundBitmap() ? "1" : "0") +
                " // identity_characters=" +
                std::to_string(developerIdentity->size()));

            // The final UI is already painted before either previous cover is
            // released, so the generated plate never exposes the Studio/Hub.
            if (identityPromptHeld)
            {
                identityPrompt.Shutdown();
                identityPromptHeld = false;
                LogGate2B("IDENTITY_PROMPT_RELEASED_TO_GATE2C_FALLBACK");
            }
            if (handshakeMotionSucceeded)
                handshakeVideo.ReleaseFadeOverlay();
            if (revealAcceptedHandoff)
                reveal.ReleaseFadeOverlay();

            while (message.message != WM_QUIT && !handshakeUi.IsCompleted())
            {
                if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                else
                {
                    handshakeUi.Pump();
                    Sleep(8);
                }
            }

            if (message.message != WM_QUIT && handshakeUi.IsCompleted())
            {
                LogGate2C("ENTER_HUB_ACCEPTED");

                // Gate 2C ends with an intentionally simple black handoff. Gate
                // 2D will replace this with the approved vertical iris split.
                handshakeUi.BeginHandoff();
                application->SetWindow(window);
                application->Run();
                firstStudioFrameReady = true;
                LogGate2C("FIRST_STUDIO_FRAME_READY_BEHIND_HANDSHAKE");

                handshakeUi.Shutdown();
                startupHandshakeActive = false;
                windowReadyForWicked = true;
                application->SetWindow(window);
                application->Run();
                LogGate2C("HANDOFF_TO_STUDIO");
            }
            else if (message.message == WM_QUIT)
            {
                LogGate2C("HANDSHAKE_ABORTED_BY_WINDOW_CLOSE");
            }
        }
        else
        {
            LogGate2C("FINAL_IDENTITY_UI_CREATE_FAILED // fail_open=1");
            handshakeVideo.ReleaseFadeOverlay();
        }

        startupIdentityHandshake = nullptr;
        startupHandshakeActive = false;
        windowReadyForWicked = true;

        if (!handshakeMotionSucceeded)
            handshakeVideo.ReleaseFadeOverlay();
    }

    if (message.message != WM_QUIT && identityPromptHeld)
    {
        // Gate 2C could not establish an owner surface. Keep the old Gate 2B
        // fail-open contract: render Studio first, then remove the opaque prompt.
        application->SetWindow(window);
        application->Run();
        firstStudioFrameReady = true;
        identityPrompt.Shutdown();
        identityPromptHeld = false;
        LogGate2B("FIRST_STUDIO_FRAME_READY_BEHIND_PROMPT // Gate2C_fail_open=1");
    }

    if (message.message != WM_QUIT && !firstStudioFrameReady)
    {
        application->SetWindow(window);
        application->Run();
        firstStudioFrameReady = true;
        if (revealAcceptedHandoff)
            reveal.ReleaseFadeOverlay();
        LogGate2C("FAIL_OPEN_HANDOFF_TO_STUDIO");
    }

    while (message.message != WM_QUIT)
    {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        else
        {
            application->Run();
        }
    }

    LogGate2C("PROCESS_EXIT");
    LogGate2B("PROCESS_EXIT");
    LogGate2A("PROCESS_EXIT");
    wi::jobsystem::ShutDown();
    application = nullptr;
    return static_cast<int>(message.wParam);
}
