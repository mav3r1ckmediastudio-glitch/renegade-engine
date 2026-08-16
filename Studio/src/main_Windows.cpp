#include "StudioApplication.h"
#include "StartupRevealRenderPath.h"

#include <Windows.h>

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
    bool windowReadyForWicked = false;

    void LogGate2A(std::string_view message) noexcept
    {
        std::error_code error;
        std::filesystem::create_directories("Saved/Diagnostics", error);
        std::ofstream stream(
            "Saved/Diagnostics/PR58Gate2AStartup.log",
            std::ios::out | std::ios::app);
        if (stream)
        {
            stream << "[PR58-GATE2A] " << message << '\n';
        }
    }

    RECT ResolveLaunchMonitorRect() noexcept
    {
        POINT cursor = {};
        if (!GetCursorPos(&cursor))
        {
            cursor.x = 0;
            cursor.y = 0;
        }

        const HMONITOR monitor =
            MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (monitor != nullptr && GetMonitorInfoW(monitor, &monitorInfo))
        {
            return monitorInfo.rcMonitor;
        }

        RECT fallback = {};
        fallback.right = GetSystemMetrics(SM_CXSCREEN);
        fallback.bottom = GetSystemMetrics(SM_CYSCREEN);
        return fallback;
    }

    const wchar_t* GraphicsBackendTitle() noexcept
    {
        const auto* device = wi::graphics::GetDevice();
        if (device != nullptr &&
            std::strcmp(device->GetTag(), "[Vulkan]") == 0)
        {
            return L"Renegade Studio - Phase 3 [Vulkan]";
        }
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
            if (application != nullptr &&
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
            if (application != nullptr &&
                windowReadyForWicked &&
                application->is_window_active)
            {
                application->SetWindow(window);
            }
            return 0;
        }

        case WM_CHAR:
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
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(window, message, wParam, lParam);
        }
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

    // Construct Studio after all C++ static initializers have completed so
    // creator chrome may safely subscribe to Wicked services. Studio is a very
    // large aggregate, so keep it off the Windows thread stack.
    auto localApplication =
        std::make_unique<renegade::studio::StudioApplication>();
    application = localApplication.get();

    // CI startup proof: reaching this point proves all process/static and Studio
    // object construction completed successfully, including creator-chrome event
    // registration, without requiring a graphics adapter or interactive window.
    if (wi::arguments::HasArgument("startup-smoke"))
    {
        application = nullptr;
        return 0;
    }

    wchar_t executablePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, executablePath, MAX_PATH) > 0)
    {
        wchar_t* lastSeparator = std::wcsrchr(executablePath, L'\\');
        if (lastSeparator != nullptr)
        {
            *lastSeparator = L'\0';
            SetCurrentDirectoryW(executablePath);
        }
    }
    LogGate2A("PROCESS_START");

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = RenegadeWindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // Windows owns the first visible pixels. Keep them black until Wicked has a
    // reveal frame to present; never expose the editor underneath.
    windowClass.hbrBackground =
        reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = L"RenegadeStudioWindow";

    if (RegisterClassExW(&windowClass) == 0)
    {
        LogGate2A("REGISTER_WINDOW_CLASS_FAILED");
        application = nullptr;
        return 1;
    }

    // Gate 2A is a full-screen startup experience. Create a borderless window
    // covering the monitor under the launch cursor rather than the legacy
    // 1600x900 overlapped editor window.
    const RECT monitorRect = ResolveLaunchMonitorRect();
    const int monitorWidth = monitorRect.right - monitorRect.left;
    const int monitorHeight = monitorRect.bottom - monitorRect.top;

    const HWND window = CreateWindowExW(
        WS_EX_APPWINDOW,
        windowClass.lpszClassName,
        L"Renegade Studio - Phase 3",
        WS_POPUP,
        monitorRect.left,
        monitorRect.top,
        monitorWidth,
        monitorHeight,
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

    ShowWindow(window, showCommand == SW_HIDE ? SW_SHOW : showCommand);
    UpdateWindow(window);
    LogGate2A(
        "WINDOW_VISIBLE_FULLSCREEN // " +
        std::to_string(monitorWidth) + "x" +
        std::to_string(monitorHeight));

    windowReadyForWicked = true;
    application->SetWindow(window);
    SetWindowTextW(window, GraphicsBackendTitle());
    LogGate2A("WICKED_WINDOW_BOUND");

    // Initialize Studio while the already-visible full-screen window remains
    // black. This intentionally happens BEFORE the movie, so the cinematic does
    // not disguise editor/project loading. It also guarantees the normal Studio
    // virtual frame loop is fully initialized before Application::Run() begins.
    LogGate2A("STUDIO_INIT_BEGIN");
    application->Initialize();
    wi::initializer::WaitForInitializationsToFinish();
    LogGate2A("STUDIO_INIT_END");

    wi::RenderPath* studioPath = application->GetActivePath();
    if (studioPath == nullptr)
    {
        LogGate2A("STUDIO_PATH_MISSING");
        application = nullptr;
        return 3;
    }

    renegade::studio::StartupRevealRenderPath startupReveal;
    startupReveal.Configure(
        "Content/startup/renegade_logo_reveal_v2.mp4",
        "Content/startup/renegade_logo_reveal_v2.wav");

    LogGate2A("REVEAL_ACTIVATE_BEGIN");
    application->ActivatePath(&startupReveal);
    if (startupReveal.HasFailedOpen())
    {
        LogGate2A(
            "REVEAL_FAIL_OPEN // " + startupReveal.FailureReason());
        application->ActivatePath(studioPath);
    }
    else
    {
        LogGate2A("REVEAL_ACTIVE");
    }

    MSG message = {};
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

            // StartupRevealRenderPath owns the final fully-black frame. The
            // Studio path is already initialized and ready, so switching here
            // cannot expose editor loading behind the reveal.
            if (application->GetActivePath() == &startupReveal &&
                startupReveal.IsFinished())
            {
                LogGate2A(
                    startupReveal.HasFailedOpen()
                        ? "REVEAL_FINISHED_FAIL_OPEN"
                        : "REVEAL_FINISHED");
                application->ActivatePath(studioPath);
                LogGate2A("STUDIO_PATH_RESTORED");
            }
        }
    }

    LogGate2A("PROCESS_EXIT");
    wi::jobsystem::ShutDown();
    application = nullptr;
    return static_cast<int>(message.wParam);
}
