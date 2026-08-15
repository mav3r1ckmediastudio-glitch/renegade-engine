#include "StudioApplication.h"
#include "StartupRevealRenderPath.h"

#include <Windows.h>

#include <cwchar>
#include <cstring>
#include <memory>

namespace
{
    renegade::studio::StudioApplication* application = nullptr;

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
            if (application != nullptr && application->is_window_active)
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
            if (application != nullptr && application->is_window_active)
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

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = RenegadeWindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // Gate 2A: if Windows exposes the native window before Wicked's first
    // present, the backing brush is black rather than the old white flash.
    windowClass.hbrBackground =
        reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = L"RenegadeStudioWindow";

    if (RegisterClassExW(&windowClass) == 0)
    {
        application = nullptr;
        return 1;
    }

    const HWND window = CreateWindowW(
        windowClass.lpszClassName,
        L"Renegade Studio - Phase 3",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        0,
        1600,
        900,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (window == nullptr)
    {
        application = nullptr;
        return 2;
    }

    // Initialize Studio while the native window is still hidden. This makes
    // the normal Studio path available without ever presenting it. Gate 2A's
    // reveal is then activated before ShowWindow(), preventing editor/Hub bleed.
    // Engine initialization and Studio's proving-ground setup complete before
    // the reveal starts; the movie is not used to disguise unrelated loading.
    application->SetWindow(window);
    application->Initialize();
    wi::initializer::WaitForInitializationsToFinish();

    wi::RenderPath* studioPath = application->GetActivePath();
    renegade::studio::StartupRevealRenderPath startupReveal;
    startupReveal.Configure(
        "Content/startup/renegade_logo_reveal_v2.mp4",
        "Content/startup/renegade_logo_reveal_v2.wav");
    application->ActivatePath(&startupReveal);

    // Media/decoder/audio failures are explicitly fail-open: enter Studio before
    // the window becomes visible instead of showing a broken or silent reveal.
    if (startupReveal.HasFailedOpen())
    {
        application->ActivatePath(studioPath);
    }

    SetWindowTextW(window, GraphicsBackendTitle());
    ShowWindow(window, showCommand);
    UpdateWindow(window);

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

            // The reveal owns its final fully-black frame. Switch paths only
            // after that frame has been presented so Studio never bleeds through
            // the fade. Gate 2B will later take over this black handoff point.
            if (application->GetActivePath() == &startupReveal &&
                startupReveal.IsFinished())
            {
                application->ActivatePath(studioPath);
            }
        }
    }

    wi::jobsystem::ShutDown();
    application = nullptr;
    return static_cast<int>(message.wParam);
}
