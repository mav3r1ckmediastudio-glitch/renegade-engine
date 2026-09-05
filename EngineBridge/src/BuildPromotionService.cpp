#include "renegade/bridge/BuildPromotionService.h"

#include "BuildPromotionServiceInternal.h"
#include "renegade/bridge/PackageIntegrityService.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "json.hpp"

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr const char* Gate4Status =
            "isolated_smoke_passed_not_promoted";
        constexpr const char* Gate5Status =
            "gate5_validated_for_final_path";
        constexpr const char* Gate5PromotionMode =
            "commit_by_directory_rename";
        constexpr const char* Gate5ProjectPromotionState =
            "gate5_final_path_commit";

        bool ComponentEqual(const fs::path& left, const fs::path& right)
        {
            const std::wstring leftText = left.native();
            const std::wstring rightText = right.native();
            return CompareStringOrdinal(
                leftText.c_str(), static_cast<int>(leftText.size()),
                rightText.c_str(), static_cast<int>(rightText.size()),
                TRUE) == CSTR_EQUAL;
        }

        bool QueryExists(
            const fs::path& path,
            bool& exists,
            std::string& error)
        {
            std::error_code ec;
            exists = fs::exists(path, ec);
            if (ec)
            {
                error = "Gate 5 could not inspect path state: " +
                    path.generic_u8string() + " (" + ec.message() + ")";
                return false;
            }
            error.clear();
            return true;
        }

        bool RequireDeclaredDirectory(
            const fs::path& path,
            const bool mustExist,
            std::string& error)
        {
            std::error_code ec;
            const fs::file_status status = fs::symlink_status(path, ec);
            if (ec)
            {
                error = "Gate 5 could not inspect declared directory: " +
                    path.generic_u8string() + " (" + ec.message() + ")";
                return false;
            }

            if (status.type() == fs::file_type::not_found)
            {
                if (!mustExist)
                {
                    error.clear();
                    return true;
                }
                error = "Gate 5 declared directory is missing: " +
                    path.generic_u8string();
                return false;
            }

            if (fs::is_symlink(status) || !fs::is_directory(status))
            {
                error = "Gate 5 declared directory is symlinked or not a directory: " +
                    path.generic_u8string();
                return false;
            }

            error.clear();
            return true;
        }

        bool IsDirectoryNonSymlink(const fs::path& path)
        {
            std::error_code ec;
            const fs::file_status status = fs::symlink_status(path, ec);
            return !ec && !fs::is_symlink(status) && fs::is_directory(status);
        }

        bool IsRegularNonSymlink(const fs::path& path)
        {
            std::error_code ec;
            const fs::file_status status = fs::symlink_status(path, ec);
            return !ec && !fs::is_symlink(status) && fs::is_regular_file(status);
        }

        bool ReadJson(
            const fs::path& path,
            nlohmann::json& value,
            std::string& error)
        {
            if (!IsRegularNonSymlink(path))
            {
                error = "Gate 5 metadata is missing or symlinked: " +
                    path.generic_u8string();
                return false;
            }

            try
            {
                std::ifstream input(path, std::ios::binary);
                if (!input)
                    throw std::runtime_error("could not open file");
                input >> value;
            }
            catch (const std::exception& exception)
            {
                error = std::string("Gate 5 metadata is invalid JSON: ") +
                    exception.what();
                return false;
            }

            if (!value.is_object())
            {
                error = "Gate 5 metadata root must be a JSON object.";
                return false;
            }
            error.clear();
            return true;
        }

        bool WriteJson(
            const fs::path& path,
            const nlohmann::json& value,
            std::string& error)
        {
            if (!IsRegularNonSymlink(path))
            {
                error = "Gate 5 refuses to rewrite missing or symlinked metadata: " +
                    path.generic_u8string();
                return false;
            }

            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                error = "Gate 5 could not rewrite metadata: " +
                    path.generic_u8string();
                return false;
            }

            const std::string text = value.dump();
            output.write(text.data(), static_cast<std::streamsize>(text.size()));
            output.close();
            if (!output)
            {
                error = "Gate 5 could not complete metadata rewrite: " +
                    path.generic_u8string();
                return false;
            }
            error.clear();
            return true;
        }

        bool Gate4ReportAccepted(const nlohmann::json& report)
        {
            return report.value("format", std::string{}) ==
                    "renegade-build-report" &&
                report.value("schema_version", 0) == 3 &&
                report.value("status", std::string{}) == Gate4Status &&
                report.value("stage_only", false) == true &&
                report.value("distribution_ready", true) == false &&
                report.value("smoke_test", std::string{}) == "passed_gate4" &&
                report.value("package_isolation", std::string{}) ==
                    "passed_gate4" &&
                report.value("test_all_parity", std::string{}) ==
                    "passed_gate4" &&
                report.value("promotion", std::string{}) ==
                    "not_attempted_gate4";
        }

        bool Gate4ProjectManifestAccepted(const nlohmann::json& manifest)
        {
            return manifest.value("format", std::string{}) ==
                    "renegade-project-package-manifest" &&
                manifest.value("schema_version", 0) == 2 &&
                manifest.value("stage_only", false) == true;
        }

        bool Gate4PackageManifestAccepted(const nlohmann::json& manifest)
        {
            return manifest.value("format", std::string{}) ==
                    "renegade-package-manifest" &&
                manifest.value("schema_version", 0) == 2 &&
                manifest.value("stage_only", false) == true &&
                manifest.value("distribution_ready", true) == false;
        }

        bool Gate5ReportAccepted(const nlohmann::json& report)
        {
            return report.value("format", std::string{}) ==
                    "renegade-build-report" &&
                report.value("status", std::string{}) == Gate5Status &&
                report.value("schema_version", 0) == 4 &&
                report.value("stage_only", true) == false &&
                report.value("distribution_ready", true) == false &&
                report.value("smoke_test", std::string{}) == "passed_gate4" &&
                report.value("package_isolation", std::string{}) ==
                    "passed_gate4" &&
                report.value("test_all_parity", std::string{}) ==
                    "passed_gate4" &&
                report.value("promotion", std::string{}) ==
                    Gate5PromotionMode &&
                report.value("safe_rebuild", std::string{}) ==
                    "passed_gate5" &&
                report.value("last_good_preservation", std::string{}) ==
                    "rollback_before_replace";
        }

        bool Gate5ProjectManifestAccepted(const nlohmann::json& manifest)
        {
            return manifest.value("format", std::string{}) ==
                    "renegade-project-package-manifest" &&
                manifest.value("schema_version", 0) == 2 &&
                manifest.value("stage_only", true) == false &&
                manifest.value("promotion_state", std::string{}) ==
                    Gate5ProjectPromotionState;
        }

        bool Gate5PackageManifestAccepted(const nlohmann::json& manifest)
        {
            return manifest.value("format", std::string{}) ==
                    "renegade-package-manifest" &&
                manifest.value("schema_version", 0) == 2 &&
                manifest.value("stage_only", true) == false &&
                manifest.value("distribution_ready", true) == false &&
                manifest.value("promotion", std::string{}) ==
                    Gate5PromotionMode;
        }

        bool RefreshPackageRecord(
            nlohmann::json& packageManifest,
            const fs::path& packageRoot,
            const std::string& relativePath,
            std::string& error)
        {
            if (!packageManifest.contains("files") ||
                !packageManifest["files"].is_array())
            {
                error = "Gate 5 package manifest has no file array.";
                return false;
            }

            WindowsGamePackageFileDigest digest;
            if (!DigestWindowsGamePackageFile(
                    (packageRoot / fs::u8path(relativePath)).generic_u8string(),
                    digest,
                    error))
            {
                return false;
            }

            bool refreshed = false;
            for (auto& item : packageManifest["files"])
            {
                if (!item.is_object() ||
                    item.value("path", std::string{}) != relativePath)
                {
                    continue;
                }
                if (refreshed)
                {
                    error = "Gate 5 package manifest contains duplicate metadata record: " +
                        relativePath;
                    return false;
                }
                item["bytes"] = digest.byteCount;
                item["sha256"] = digest.sha256;
                refreshed = true;
            }

            if (!refreshed)
            {
                error = "Gate 5 package manifest lost required metadata record: " +
                    relativePath;
                return false;
            }
            error.clear();
            return true;
        }

        bool PrepareCandidateMetadata(
            const fs::path& candidate,
            WindowsGamePackageIntegrityResult& integrity,
            std::string& error)
        {
            if (!ValidateWindowsGamePackage(
                    candidate.generic_u8string(), integrity, error))
            {
                return false;
            }

            const fs::path projectManifestPath =
                candidate / "GameData" / "project.manifest.json";
            const fs::path buildReportPath = candidate / "build-report.json";
            const fs::path packageManifestPath =
                candidate / "package-manifest.json";

            nlohmann::json projectManifest;
            nlohmann::json buildReport;
            nlohmann::json packageManifest;
            if (!ReadJson(projectManifestPath, projectManifest, error) ||
                !ReadJson(buildReportPath, buildReport, error) ||
                !ReadJson(packageManifestPath, packageManifest, error))
            {
                return false;
            }

            if (Gate5ReportAccepted(buildReport))
            {
                if (!Gate5ProjectManifestAccepted(projectManifest) ||
                    !Gate5PackageManifestAccepted(packageManifest))
                {
                    error = "Gate 5 prepared candidate metadata is internally inconsistent.";
                    return false;
                }
                error.clear();
                return true;
            }

            if (!Gate4ReportAccepted(buildReport) ||
                !Gate4ProjectManifestAccepted(projectManifest) ||
                !Gate4PackageManifestAccepted(packageManifest))
            {
                error = "Gate 5 candidate does not exactly match the accepted Gate 4 staged metadata state.";
                return false;
            }

            projectManifest["stage_only"] = false;
            projectManifest["promotion_state"] = Gate5ProjectPromotionState;

            buildReport["schema_version"] = 4;
            buildReport["status"] = Gate5Status;
            buildReport["stage_only"] = false;
            buildReport["distribution_ready"] = false;
            buildReport["promotion"] = Gate5PromotionMode;
            buildReport["safe_rebuild"] = "passed_gate5";
            buildReport["last_good_preservation"] =
                "rollback_before_replace";

            if (!WriteJson(projectManifestPath, projectManifest, error) ||
                !WriteJson(buildReportPath, buildReport, error))
            {
                return false;
            }

            packageManifest["stage_only"] = false;
            packageManifest["distribution_ready"] = false;
            packageManifest["promotion"] = Gate5PromotionMode;
            if (!RefreshPackageRecord(
                    packageManifest,
                    candidate,
                    "GameData/project.manifest.json",
                    error) ||
                !RefreshPackageRecord(
                    packageManifest,
                    candidate,
                    "build-report.json",
                    error) ||
                !WriteJson(packageManifestPath, packageManifest, error))
            {
                return false;
            }

            if (!ValidateWindowsGamePackage(
                    candidate.generic_u8string(), integrity, error))
            {
                return false;
            }
            error.clear();
            return true;
        }

        bool ValidateGate5Build(
            const fs::path& root,
            WindowsGamePackageIntegrityResult& integrity,
            std::string& error)
        {
            if (!ValidateWindowsGamePackage(
                    root.generic_u8string(), integrity, error))
            {
                return false;
            }

            nlohmann::json projectManifest;
            nlohmann::json buildReport;
            nlohmann::json packageManifest;
            if (!ReadJson(
                    root / "GameData" / "project.manifest.json",
                    projectManifest,
                    error) ||
                !ReadJson(root / "build-report.json", buildReport, error) ||
                !ReadJson(
                    root / "package-manifest.json",
                    packageManifest,
                    error))
            {
                return false;
            }

            if (!Gate5ProjectManifestAccepted(projectManifest) ||
                !Gate5ReportAccepted(buildReport) ||
                !Gate5PackageManifestAccepted(packageManifest))
            {
                error = "Gate 5 build metadata does not describe a validated final-path candidate.";
                return false;
            }
            error.clear();
            return true;
        }

        bool RenameDirectory(
            const fs::path& from,
            const fs::path& to,
            std::string& error)
        {
            constexpr DWORD RetryDelayMilliseconds = 100;
            constexpr unsigned int MaximumAttempts = 50;
            DWORD code = ERROR_SUCCESS;
            for (unsigned int attempt = 1; attempt <= MaximumAttempts; ++attempt)
            {
                if (MoveFileExW(
                        from.c_str(),
                        to.c_str(),
                        MOVEFILE_WRITE_THROUGH))
                {
                    error.clear();
                    return true;
                }

                code = GetLastError();
                const bool transient = code == ERROR_ACCESS_DENIED ||
                    code == ERROR_SHARING_VIOLATION ||
                    code == ERROR_LOCK_VIOLATION;
                if (!transient || attempt == MaximumAttempts)
                {
                    error = "Windows directory rename failed (error " +
                        std::to_string(code) + ", attempts " +
                        std::to_string(attempt) + "): " +
                        from.generic_u8string() + " -> " +
                        to.generic_u8string();
                    return false;
                }
                Sleep(RetryDelayMilliseconds);
            }
            error = "Windows directory rename exhausted its retry policy.";
            return false;
        }

        bool RemoveTree(const fs::path& path, std::string& error)
        {
            std::error_code ec;
            fs::remove_all(path, ec);
            if (ec)
            {
                error = "Gate 5 could not retire rollback backup: " +
                    path.generic_u8string() + " (" + ec.message() + ")";
                return false;
            }

            bool stillExists = false;
            if (!QueryExists(path, stillExists, error))
                return false;
            if (stillExists)
            {
                error = "Gate 5 could not retire rollback backup: " +
                    path.generic_u8string();
                return false;
            }
            error.clear();
            return true;
        }

        bool InvokeHook(
            const detail::WindowsGameBuildPromotionHook& hook,
            const detail::WindowsGameBuildPromotionHookPoint point,
            const fs::path& candidate,
            const fs::path& finalOutput,
            const fs::path& rollback,
            std::string& error)
        {
            if (!hook)
                return true;

            std::string hookError;
            if (hook(
                    point,
                    candidate.generic_u8string(),
                    finalOutput.generic_u8string(),
                    rollback.generic_u8string(),
                    hookError))
            {
                return true;
            }

            error = hookError.empty()
                ? "Gate 5 promotion was interrupted by the private test seam."
                : std::move(hookError);
            return false;
        }

        void SetFailure(
            WindowsGameBuildPromotionResult& result,
            const WindowsGameBuildPromotionCode code,
            const std::string& message,
            std::string& error)
        {
            result.succeeded = false;
            result.code = code;
            result.message = message;
            error = message;
        }
    }

    const char* WindowsGameBuildPromotionCodeName(
        const WindowsGameBuildPromotionCode code) noexcept
    {
        switch (code)
        {
        case WindowsGameBuildPromotionCode::Success: return "SUCCESS";
        case WindowsGameBuildPromotionCode::InvalidRequest: return "INVALID_REQUEST";
        case WindowsGameBuildPromotionCode::CandidateRejected: return "CANDIDATE_REJECTED";
        case WindowsGameBuildPromotionCode::ExistingBuildRejected: return "EXISTING_BUILD_REJECTED";
        case WindowsGameBuildPromotionCode::StaleTransaction: return "STALE_TRANSACTION";
        case WindowsGameBuildPromotionCode::RecoveredPreviousBuild: return "RECOVERED_PREVIOUS_BUILD";
        case WindowsGameBuildPromotionCode::PromotionFailed: return "PROMOTION_FAILED";
        case WindowsGameBuildPromotionCode::RollbackFailed: return "ROLLBACK_FAILED";
        case WindowsGameBuildPromotionCode::FinalValidationFailed: return "FINAL_VALIDATION_FAILED";
        default: return "UNKNOWN";
        }
    }

    namespace detail
    {
        bool PromoteWindowsGameBuildWithHook(
            const WindowsGameBuildPromotionRequest& request,
            WindowsGameBuildPromotionResult& result,
            std::string& error,
            const WindowsGameBuildPromotionHook& hook)
        {
            result = {};
            try
            {
                if (request.candidatePath.empty() ||
                    request.finalOutputPath.empty())
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::InvalidRequest,
                        "Gate 5 promotion request is incomplete.",
                        error);
                    return false;
                }

                const fs::path candidateDeclared =
                    fs::u8path(request.candidatePath);
                const fs::path finalDeclared =
                    fs::u8path(request.finalOutputPath);
                if (!candidateDeclared.is_absolute() ||
                    !finalDeclared.is_absolute() ||
                    finalDeclared.filename().empty() ||
                    finalDeclared.filename() == ".renegade-staging")
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::InvalidRequest,
                        "Gate 5 requires absolute governed candidate/final paths.",
                        error);
                    return false;
                }

                std::string pathError;
                if (!RequireDeclaredDirectory(
                        candidateDeclared,
                        true,
                        pathError))
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::CandidateRejected,
                        pathError,
                        error);
                    return false;
                }

                bool finalDeclaredExists = false;
                if (!QueryExists(
                        finalDeclared,
                        finalDeclaredExists,
                        pathError) ||
                    (finalDeclaredExists &&
                     !RequireDeclaredDirectory(
                         finalDeclared,
                         true,
                         pathError)))
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::InvalidRequest,
                        pathError,
                        error);
                    return false;
                }

                std::error_code ec;
                const fs::path outputParent = fs::weakly_canonical(
                    finalDeclared.parent_path(), ec);
                if (ec || !IsDirectoryNonSymlink(outputParent))
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::InvalidRequest,
                        "Gate 5 final output parent is not an accessible non-symlink directory.",
                        error);
                    return false;
                }

                const fs::path finalOutput =
                    outputParent / finalDeclared.filename();
                const fs::path stagingParent =
                    outputParent / ".renegade-staging";
                if (!IsDirectoryNonSymlink(stagingParent))
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::InvalidRequest,
                        "Gate 5 governed .renegade-staging directory is missing or symlinked.",
                        error);
                    return false;
                }

                const fs::path candidate = fs::weakly_canonical(
                    candidateDeclared, ec);
                if (ec || !IsDirectoryNonSymlink(candidate))
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::CandidateRejected,
                        "Gate 5 candidate is missing, inaccessible or symlinked.",
                        error);
                    return false;
                }

                std::error_code parentError;
                if (!fs::equivalent(
                        candidate.parent_path(),
                        stagingParent,
                        parentError) ||
                    parentError)
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::InvalidRequest,
                        "Gate 5 candidate must be a direct child of the governed staging directory.",
                        error);
                    return false;
                }

                const fs::path rollback = stagingParent /
                    fs::u8path(finalOutput.filename().generic_u8string() +
                        ".rollback");
                if (ComponentEqual(candidate.filename(), rollback.filename()))
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::InvalidRequest,
                        "Gate 5 candidate collides with the rollback path.",
                        error);
                    return false;
                }

                result.candidatePath = candidate.generic_u8string();
                result.finalOutputPath = finalOutput.generic_u8string();
                result.rollbackPath = rollback.generic_u8string();

                bool rollbackExists = false;
                bool finalExists = false;
                if (!QueryExists(rollback, rollbackExists, pathError) ||
                    !QueryExists(finalOutput, finalExists, pathError))
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::InvalidRequest,
                        pathError,
                        error);
                    return false;
                }

                if (rollbackExists)
                {
                    if (!RequireDeclaredDirectory(rollback, true, pathError))
                    {
                        SetFailure(
                            result,
                            WindowsGameBuildPromotionCode::StaleTransaction,
                            pathError,
                            error);
                        return false;
                    }

                    WindowsGamePackageIntegrityResult rollbackIntegrity;
                    std::string validationError;
                    if (!ValidateGate5Build(
                            rollback,
                            rollbackIntegrity,
                            validationError))
                    {
                        SetFailure(
                            result,
                            WindowsGameBuildPromotionCode::StaleTransaction,
                            "Gate 5 stale rollback backup is invalid: " +
                                validationError,
                            error);
                        return false;
                    }

                    if (!finalExists)
                    {
                        std::string renameError;
                        if (!RenameDirectory(
                                rollback,
                                finalOutput,
                                renameError))
                        {
                            SetFailure(
                                result,
                                WindowsGameBuildPromotionCode::RollbackFailed,
                                "Gate 5 could not restore the previous build from stale rollback state: " +
                                    renameError,
                                error);
                            return false;
                        }

                        result.previousBuildExisted = true;
                        result.previousBuildPreserved = true;
                        result.previousPackageManifestSha256 =
                            rollbackIntegrity.packageManifestSha256;
                        SetFailure(
                            result,
                            WindowsGameBuildPromotionCode::RecoveredPreviousBuild,
                            "Gate 5 restored the previous successful build from stale rollback state; retry promotion explicitly.",
                            error);
                        return false;
                    }

                    WindowsGamePackageIntegrityResult currentFinalIntegrity;
                    if (!ValidateGate5Build(
                            finalOutput,
                            currentFinalIntegrity,
                            validationError))
                    {
                        SetFailure(
                            result,
                            WindowsGameBuildPromotionCode::StaleTransaction,
                            "Gate 5 found both final and rollback directories but the final build is invalid; preserving both for recovery.",
                            error);
                        return false;
                    }
                    if (currentFinalIntegrity.projectId !=
                            rollbackIntegrity.projectId ||
                        currentFinalIntegrity.gameName !=
                            rollbackIntegrity.gameName)
                    {
                        SetFailure(
                            result,
                            WindowsGameBuildPromotionCode::StaleTransaction,
                            "Gate 5 final and stale rollback identities disagree; preserving both.",
                            error);
                        return false;
                    }

                    std::string cleanupError;
                    if (!RemoveTree(rollback, cleanupError))
                    {
                        SetFailure(
                            result,
                            WindowsGameBuildPromotionCode::StaleTransaction,
                            cleanupError,
                            error);
                        return false;
                    }
                    rollbackExists = false;
                }

                WindowsGamePackageIntegrityResult candidateIntegrity;
                std::string candidateError;
                if (!PrepareCandidateMetadata(
                        candidate,
                        candidateIntegrity,
                        candidateError))
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::CandidateRejected,
                        candidateError,
                        error);
                    return false;
                }

                const fs::path expectedFinalFolder = fs::u8path(
                    candidateIntegrity.gameName + " Windows Build");
                if (!ComponentEqual(
                        finalOutput.filename(),
                        expectedFinalFolder))
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::InvalidRequest,
                        "Gate 5 final folder must match the governed '<Game Name> Windows Build' identity.",
                        error);
                    return false;
                }

                bool hasPrevious = false;
                if (!QueryExists(finalOutput, hasPrevious, pathError))
                {
                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::InvalidRequest,
                        pathError,
                        error);
                    return false;
                }
                result.previousBuildExisted = hasPrevious;

                WindowsGamePackageIntegrityResult previousIntegrity;
                if (hasPrevious)
                {
                    if (!RequireDeclaredDirectory(
                            finalOutput,
                            true,
                            pathError))
                    {
                        SetFailure(
                            result,
                            WindowsGameBuildPromotionCode::ExistingBuildRejected,
                            pathError,
                            error);
                        return false;
                    }

                    std::string previousError;
                    if (!ValidateGate5Build(
                            finalOutput,
                            previousIntegrity,
                            previousError))
                    {
                        SetFailure(
                            result,
                            WindowsGameBuildPromotionCode::ExistingBuildRejected,
                            "Gate 5 refuses to replace an invalid/unknown existing build: " +
                                previousError,
                            error);
                        return false;
                    }
                    if (previousIntegrity.projectId !=
                            candidateIntegrity.projectId ||
                        previousIntegrity.gameName !=
                            candidateIntegrity.gameName)
                    {
                        SetFailure(
                            result,
                            WindowsGameBuildPromotionCode::ExistingBuildRejected,
                            "Gate 5 refuses to replace a final build with different project/game identity.",
                            error);
                        return false;
                    }

                    result.previousPackageManifestSha256 =
                        previousIntegrity.packageManifestSha256;

                    std::string renameError;
                    if (!RenameDirectory(
                            finalOutput,
                            rollback,
                            renameError))
                    {
                        result.previousBuildPreserved = true;
                        SetFailure(
                            result,
                            WindowsGameBuildPromotionCode::PromotionFailed,
                            "Gate 5 could not move the previous build to rollback: " +
                                renameError,
                            error);
                        return false;
                    }

                    std::string hookError;
                    if (!InvokeHook(
                            hook,
                            WindowsGameBuildPromotionHookPoint::AfterPreviousBuildMoved,
                            candidate,
                            finalOutput,
                            rollback,
                            hookError))
                    {
                        std::string rollbackError;
                        if (!RenameDirectory(
                                rollback,
                                finalOutput,
                                rollbackError))
                        {
                            SetFailure(
                                result,
                                WindowsGameBuildPromotionCode::RollbackFailed,
                                hookError + " Rollback also failed: " +
                                    rollbackError,
                                error);
                            return false;
                        }

                        result.previousBuildPreserved = true;
                        SetFailure(
                            result,
                            WindowsGameBuildPromotionCode::PromotionFailed,
                            hookError,
                            error);
                        return false;
                    }
                }

                std::string promoteError;
                if (!RenameDirectory(
                        candidate,
                        finalOutput,
                        promoteError))
                {
                    if (hasPrevious)
                    {
                        std::string rollbackError;
                        if (!RenameDirectory(
                                rollback,
                                finalOutput,
                                rollbackError))
                        {
                            SetFailure(
                                result,
                                WindowsGameBuildPromotionCode::RollbackFailed,
                                "Gate 5 candidate commit failed and previous-build rollback also failed: " +
                                    promoteError + " / " + rollbackError,
                                error);
                            return false;
                        }
                        result.previousBuildPreserved = true;
                    }

                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::PromotionFailed,
                        "Gate 5 could not commit candidate to the final path: " +
                            promoteError,
                        error);
                    return false;
                }

                std::string hookError;
                const bool hookAllowsValidation = InvokeHook(
                    hook,
                    WindowsGameBuildPromotionHookPoint::BeforeFinalValidation,
                    candidate,
                    finalOutput,
                    rollback,
                    hookError);

                WindowsGamePackageIntegrityResult finalIntegrity;
                std::string finalError;
                const bool finalValid = hookAllowsValidation &&
                    ValidateGate5Build(
                        finalOutput,
                        finalIntegrity,
                        finalError);
                if (!finalValid)
                {
                    if (finalError.empty())
                        finalError = hookError;

                    std::string failedCandidateError;
                    if (!RenameDirectory(
                            finalOutput,
                            candidate,
                            failedCandidateError))
                    {
                        SetFailure(
                            result,
                            WindowsGameBuildPromotionCode::RollbackFailed,
                            "Gate 5 final validation failed and the failed candidate could not be removed from the final path: " +
                                finalError + " / " + failedCandidateError,
                            error);
                        return false;
                    }

                    if (hasPrevious)
                    {
                        std::string rollbackError;
                        if (!RenameDirectory(
                                rollback,
                                finalOutput,
                                rollbackError))
                        {
                            SetFailure(
                                result,
                                WindowsGameBuildPromotionCode::RollbackFailed,
                                "Gate 5 final validation failed and previous-build rollback failed: " +
                                    finalError + " / " + rollbackError,
                                error);
                            return false;
                        }
                        result.previousBuildPreserved = true;
                    }

                    SetFailure(
                        result,
                        WindowsGameBuildPromotionCode::FinalValidationFailed,
                        "Gate 5 rejected the committed candidate during final-path validation: " +
                            finalError,
                        error);
                    return false;
                }

                result.finalPackageManifestSha256 =
                    finalIntegrity.packageManifestSha256;

                if (hasPrevious)
                {
                    std::string cleanupHookError;
                    if (!InvokeHook(
                            hook,
                            WindowsGameBuildPromotionHookPoint::BeforePreviousBackupCleanup,
                            candidate,
                            finalOutput,
                            rollback,
                            cleanupHookError))
                    {
                        result.rollbackBackupRetained = true;
                    }
                    else
                    {
                        std::string cleanupError;
                        if (!RemoveTree(rollback, cleanupError))
                            result.rollbackBackupRetained = true;
                    }
                }

                result.succeeded = true;
                result.code = WindowsGameBuildPromotionCode::Success;
                result.previousBuildPreserved = false;
                result.message = result.rollbackBackupRetained
                    ? "Gate 5 promoted and validated the new final build; the previous rollback backup remains for later safe cleanup."
                    : "Gate 5 promoted and validated the owner-visible final build.";
                error.clear();
                return true;
            }
            catch (const std::exception& exception)
            {
                SetFailure(
                    result,
                    WindowsGameBuildPromotionCode::PromotionFailed,
                    std::string("Gate 5 promotion failed: ") + exception.what(),
                    error);
                return false;
            }
        }
    }

    bool PromoteWindowsGameBuild(
        const WindowsGameBuildPromotionRequest& request,
        WindowsGameBuildPromotionResult& result,
        std::string& error)
    {
        return detail::PromoteWindowsGameBuildWithHook(
            request,
            result,
            error,
            {});
    }
}
