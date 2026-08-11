#pragma once

#include "renegade/bridge/BuildIdentityService.h"
#include "renegade/bridge/BuildPromotionService.h"
#include "renegade/bridge/BuildService.h"
#include "renegade/bridge/BuildStageService.h"
#include "renegade/bridge/BuildVerificationService.h"

#include <functional>
#include <string>
#include <vector>

namespace renegade::bridge
{
    // UI-free LP06 Gate 5 composition boundary. Environment-specific launch
    // mechanics are supplied as a callback so Studio can run the real staged
    // executable without moving build semantics into the UI layer.
    using WindowsGameBuildSmokeRunner = std::function<bool(
        const WindowsGameBuildPlan& plan,
        const WindowsGameBuildStageResult& stage,
        std::string& runtimeEvidencePath,
        std::string& error)>;

    struct WindowsGameBuildWorkflowRequest
    {
        ProjectMetadata project;
        DependencyGraph dependencyGraph;
        AssetRegistry assetRegistry;
        WindowsGameBuildRequest build;
        std::vector<WindowsRuntimeSupportInput> runtimeSupport;
        WindowsGameBuildStagingRequest staging;
        WindowsGameExecutableIdentityRequest identity;
        WindowsGameBuildVerificationRequest verification;
    };

    struct WindowsGameBuildWorkflowResult
    {
        WindowsGameBuildPlan plan;
        WindowsGameBuildStageResult stage;
        WindowsGameExecutableIdentityResult identity;
        WindowsGameBuildVerificationResult verification;
        WindowsGameBuildPromotionResult promotion;
        std::string runtimeEvidencePath;
        std::string finalOutputPath;
    };

    // Composes the accepted LP06 services in their authoritative order:
    // Gate 1 plan -> Gate 2 staging -> Gate 3 executable identity -> Gate 4
    // real Runtime evidence/verification -> Gate 5 safe promotion.
    //
    // This function never creates an owner-visible final directory directly;
    // only PromoteWindowsGameBuild() may cross that boundary.
    [[nodiscard]] bool BuildWindowsGame(
        const WindowsGameBuildWorkflowRequest& request,
        const WindowsGameBuildSmokeRunner& smokeRunner,
        WindowsGameBuildWorkflowResult& result,
        std::string& error);
}
