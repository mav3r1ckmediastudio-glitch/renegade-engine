#include "renegade/bridge/NativeWindowSurface.h"

#include <iostream>
#include <stdexcept>

namespace
{
    void Require(bool value, const char* message)
    {
        if (!value) throw std::runtime_error(message);
    }

    struct CanvasBinding
    {
        unsigned count = 0;
        LONG width = 0;
        LONG height = 0;

        void SetWindow(HWND window)
        {
            RECT client = {};
            Require(GetClientRect(window, &client) != FALSE, "read bound window");
            ++count;
            width = client.right - client.left;
            height = client.bottom - client.top;
        }
    };
}

int main()
{
    HWND window = nullptr;
    try
    {
        CanvasBinding binding;
        Require(!renegade::bridge::TryBindDrawableNativeWindow(nullptr, binding),
            "null window must not bind");
        WNDCLASSW type = {};
        type.lpfnWndProc = DefWindowProcW;
        type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = L"RenegadeNativeWindowSurfaceRegression";
        Require(RegisterClassW(&type) != 0, "register regression window");
        window = CreateWindowExW(0, type.lpszClassName, L"Surface regression",
            WS_POPUP, 0, 0, 640, 480, nullptr, nullptr, type.hInstance, nullptr);
        Require(window != nullptr, "create hidden native window");
        Require(renegade::bridge::TryBindDrawableNativeWindow(window, binding),
            "initial positive client area must bind");
        Require(binding.count == 1 && binding.width == 640 && binding.height == 480,
            "positive binding updates the actual canvas dimensions");

        constexpr UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;
        const SIZE zeroAreas[] = {{0, 480}, {640, 0}, {0, 0}};
        for (const SIZE area : zeroAreas)
        {
            Require(SetWindowPos(window, nullptr, 0, 0, area.cx, area.cy, flags),
                "set transient native client extent");
            Require(!renegade::bridge::TryBindDrawableNativeWindow(window, binding),
                "zero width or height must not reach the graphics application");
            Require(binding.count == 1 && binding.width == 640 && binding.height == 480,
                "a rejected extent must preserve the previous canvas");
        }

        Require(SetWindowPos(window, nullptr, 0, 0, 1280, 720, flags),
            "restore a drawable extent");
        Require(renegade::bridge::TryBindDrawableNativeWindow(window, binding),
            "restoration must resume binding");
        Require(binding.count == 2 && binding.width == 1280 && binding.height == 720,
            "restored binding must use the new dimensions");
        ShowWindow(window, SW_SHOWMINNOACTIVE);
        Require(IsIconic(window), "native window must be minimized");
        Require(!renegade::bridge::TryBindDrawableNativeWindow(window, binding),
            "a minimized window must retain its previous graphics canvas");
        Require(binding.count == 2, "minimization must not call SetWindow");
        ShowWindow(window, SW_SHOWNOACTIVATE);
        Require(!IsIconic(window), "native window must be restored");
        Require(renegade::bridge::TryBindDrawableNativeWindow(window, binding),
            "binding must resume after native minimize/restore");
        Require(binding.count == 3 && binding.width > 0 && binding.height > 0,
            "positive control after minimization");
        DestroyWindow(window);
        Require(!renegade::bridge::TryBindDrawableNativeWindow(window, binding),
            "destroyed native window must not bind");
        window = nullptr;
        UnregisterClassW(type.lpszClassName, type.hInstance);
        std::cout << "NATIVE_WINDOW_ZERO_EXTENT_MINIMIZE_RESTORE_PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        if (window) DestroyWindow(window);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
