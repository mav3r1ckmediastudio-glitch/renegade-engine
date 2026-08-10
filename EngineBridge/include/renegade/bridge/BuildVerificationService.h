#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* WindowsVcRuntimePrerequisitePolicy =
        "system-vc-redist-2015-2022-x64";

    struct WindowsGameBuildVerificationRequest
    {
        std::string packageRootPath;
        std::string runtimeEvidencePath;
        std::string expectedGraphicsBackend = "DX12";
        std::string windowsPrerequisitePolicy =
            WindowsVcRuntimePrerequisitePolicy;
        std::vector<std::string> expectedFlowTrace;
    };

    struct WindowsGameBuildVerificationResult
    {
        bool succeeded = false;
        std::string message;
        std::string packageRootPath;
        std::string runtimeEvidenceSha256;
        std::uint64_t runtimeEvidenceBytes = 0;
        std::string buildReportSha256;
        std::string packageManifestSha256;
    };

    // Consumes evidence emitted by a real isolated packaged Runtime launch.
    // On success it advances only the staged build report to Gate 4
    // "smoke passed, not promoted" state, refreshes the build-report record in
    // package-manifest.json, and revalidates the entire package. It never
    // creates, replaces or promotes the owner-visible final build directory.
    [[nodiscard]] bool RecordWindowsGameBuildVerification(
        const WindowsGameBuildVerificationRequest& request,
        WindowsGameBuildVerificationResult& result,
        std::string& error);
}
