#include "RuntimeApplication.h"

#include <Windows.h>

#include <cwchar>
#include <cstring>

namespace
{
    renegade::runtime::RuntimeApplication application;

    const wchar_t* GraphicsBackendTitle() noexcept
    {
        const auto* device = wi::graphics::GetDevice();
        if (device != nullptr &&
            std::strcmp(device->GetTag(), "[Vulkan]") == 0)
        {
            return L"Renegade Runtime - Phase 2 [Vulkan]";
        }
        return L"Renegade Runtime - Phase 2 [DX12]";
    }

    LRESULT CALLBACK RenegadeRuntimeWindowProc(
        const HWND window,
        const UINT message,
        const WPARAM wParam,
        const LPARAM lParam)
    {
        switch (message)
        {
        case WM_SIZE:
            if (application.is_window_active)
            {
                application.SetWindow(window);
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
            if (application.is_window_active)
            {
                application.SetWindow(window);
            }
            return 0;
        }

        case WM_INPUT:
            wi::input::rawinput::ParseMessage(reinterpret_cast<void*>(lParam));
            return 0;

        case WM_KILLFOCUS:
            application.is_window_active = false;
            return 0;

        case WM_SETFOCUS:
            application.is_window_active = true;
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
    windowClass.lpfnWndProc = RenegadeRuntimeWindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = L"RenegadeRuntimeWindow";

    if (RegisterClassExW(&windowClass) == 0)
    {
        return 1;
    }

    const HWND window = CreateWindowW(
        windowClass.lpszClassName,
        L"Renegade Runtime - Phase 2",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        0,
        1280,
        720,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (window == nullptr)
    {
        return 2;
    }

    ShowWindow(window, showCommand);
    application.SetWindow(window);
    SetWindowTextW(window, GraphicsBackendTitle());

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
            application.Run();
        }
    }

    wi::jobsystem::ShutDown();
    return static_cast<int>(message.wParam);
}
