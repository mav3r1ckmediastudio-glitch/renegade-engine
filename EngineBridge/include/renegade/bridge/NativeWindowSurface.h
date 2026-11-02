#pragma once

#ifdef _WIN32
#include <Windows.h>

namespace renegade::bridge
{
    // A restore/focus notification can arrive before the client area is drawable.
    // Keep the previous canvas and GPU allocations until a later valid size event.
    // Binding a zero canvas makes native RenderPath3D allocate invalid 0x0 targets.
    template <class Application>
    bool TryBindDrawableNativeWindow(HWND window, Application& application)
    {
        RECT client = {};
        if (window == nullptr || IsIconic(window) ||
            !GetClientRect(window, &client) ||
            client.right <= client.left || client.bottom <= client.top)
        {
            return false;
        }

        application.SetWindow(window);
        return true;
    }
}
#endif
