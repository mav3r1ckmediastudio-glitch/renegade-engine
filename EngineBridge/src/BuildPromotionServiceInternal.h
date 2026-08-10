#pragma once

#include "renegade/bridge/BuildPromotionService.h"

#include <functional>
#include <string>

namespace renegade::bridge::detail
{
    enum class WindowsGameBuildPromotionHookPoint
    {
        AfterPreviousBuildMoved,
        BeforeFinalValidation,
        BeforePreviousBackupCleanup,
    };

    using WindowsGameBuildPromotionHook = std::function<bool(
        WindowsGameBuildPromotionHookPoint point,
        const std::string& candidatePath,
        const std::string& finalOutputPath,
        const std::string& rollbackPath,
        std::string& error)>;

    // Private deterministic fault seam for Gate 5 regression tests. Production
    // callers use PromoteWindowsGameBuild(), which supplies no hook.
    [[nodiscard]] bool PromoteWindowsGameBuildWithHook(
        const WindowsGameBuildPromotionRequest& request,
        WindowsGameBuildPromotionResult& result,
        std::string& error,
        const WindowsGameBuildPromotionHook& hook);
}
