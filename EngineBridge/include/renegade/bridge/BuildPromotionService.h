#pragma once

#include <string>

namespace renegade::bridge
{
    enum class WindowsGameBuildPromotionCode
    {
        Success,
        InvalidRequest,
        CandidateRejected,
        ExistingBuildRejected,
        StaleTransaction,
        RecoveredPreviousBuild,
        PromotionFailed,
        RollbackFailed,
        FinalValidationFailed,
    };

    [[nodiscard]] const char* WindowsGameBuildPromotionCodeName(
        WindowsGameBuildPromotionCode code) noexcept;

    struct WindowsGameBuildPromotionRequest
    {
        std::string candidatePath;
        std::string finalOutputPath;
    };

    struct WindowsGameBuildPromotionResult
    {
        bool succeeded = false;
        WindowsGameBuildPromotionCode code =
            WindowsGameBuildPromotionCode::InvalidRequest;
        std::string message;
        std::string candidatePath;
        std::string finalOutputPath;
        std::string rollbackPath;
        bool previousBuildExisted = false;
        bool previousBuildPreserved = false;
        bool rollbackBackupRetained = false;
        std::string previousPackageManifestSha256;
        std::string finalPackageManifestSha256;
    };

    // LP06 Gate 5: promote only an exact Gate-4-verified candidate from the
    // governed .renegade-staging sibling into the owner-visible final build
    // directory. Existing final builds are moved to a hidden rollback directory
    // before the candidate commit and are restored on recoverable failure.
    // This function never performs file-by-file overwrite of the final build.
    [[nodiscard]] bool PromoteWindowsGameBuild(
        const WindowsGameBuildPromotionRequest& request,
        WindowsGameBuildPromotionResult& result,
        std::string& error);
}
