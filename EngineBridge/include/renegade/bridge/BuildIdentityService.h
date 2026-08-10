#pragma once

#include "renegade/bridge/BuildService.h"
#include "renegade/bridge/BuildStageService.h"

#include <string>

namespace renegade::bridge
{
    struct WindowsGameExecutableIdentityRequest
    {
        std::string developerPublisher;
        std::string description;
        std::string copyrightNotice;
        std::string internalBuildId;
        // Caller-owned deterministic UTC timestamp in YYYY-MM-DDTHH:MM:SSZ form.
        std::string buildTimestampUtc;
        // Absolute path to a governed .ico input. Gate 3 embeds the icon into
        // the staged named executable; it is not copied into the package.
        std::string iconSourcePath;
    };

    struct WindowsGameExecutableIdentityResult
    {
        std::string executableSha256;
        std::string applicationManifestPolicy;
    };

    // LP06 Gate 3: apply owner/game identity to the already staged named
    // Release Runtime, then regenerate every manifest that describes its bytes.
    // The source RenegadeRuntime executable and owner-visible final build path
    // are never modified by this operation.
    [[nodiscard]] bool ApplyWindowsGameExecutableIdentity(
        const WindowsGameBuildPlan& plan,
        const WindowsGameExecutableIdentityRequest& request,
        WindowsGameBuildStageResult& stage,
        WindowsGameExecutableIdentityResult& result,
        std::string& error);
}
