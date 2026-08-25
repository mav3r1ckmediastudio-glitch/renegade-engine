#pragma once

#include <algorithm>
#include <string>

namespace renegade::studio
{
    struct WindowsGameBuildUiResult
    {
        bool succeeded = false;
        std::string message;
        std::string finalOutputPath;
    };

    // Thin Studio environment adapter for the UI-free LP06 build workflow.
    // All build planning, staging, verification and promotion semantics remain
    // owned by EngineBridge services.
    [[nodiscard]] WindowsGameBuildUiResult BuildActiveWindowsGame();
}
