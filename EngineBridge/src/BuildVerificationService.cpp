#include "renegade/bridge/BuildVerificationService.h"

#include "renegade/bridge/PackageIntegrityService.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <system_error>
#include <utility>

#include "json.hpp"

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        bool ReadText(
            const fs::path& path,
            std::string& text,
            std::string& error)
        {
            std::error_code ec;
            const fs::file_status status = fs::symlink_status(path, ec);
            if (ec || fs::is_symlink(status) || !fs::is_regular_file(status))
            {
                error = "Gate 4 evidence input is missing or symlinked: " +
                    path.generic_u8string();
                return false;
            }
            std::ifstream input(path, std::ios::binary);
            if (!input)
            {
                error = "Gate 4 could not read evidence input: " +
                    path.generic_u8string();
                return false;
            }
            text.assign(
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>());
            if (input.bad())
            {
                error = "Gate 4 could not complete evidence read: " +
                    path.generic_u8string();
                return false;
            }
            error.clear();
            return true;
        }

        bool WriteText(
            const fs::path& path,
            const std::string& text,
            std::string& error)
        {
            std::error_code ec;
            const fs::file_status status = fs::symlink_status(path, ec);
            if (ec || fs::is_symlink(status) || !fs::is_regular_file(status))
            {
                error = "Gate 4 refuses to rewrite missing or symlinked package metadata: " +
                    path.generic_u8string();
                return false;
            }
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                error = "Gate 4 could not rewrite package metadata: " +
                    path.generic_u8string();
                return false;
            }
            output.write(text.data(), static_cast<std::streamsize>(text.size()));
            output.close();
            if (!output)
            {
                error = "Gate 4 could not complete package metadata rewrite: " +
                    path.generic_u8string();
                return false;
            }
            error.clear();
            return true;
        }

        bool ParseEvidence(
            const std::string& text,
            std::map<std::string, std::string>& values,
            std::string& error)
        {
            values.clear();
            std::size_t begin = 0;
            while (begin <= text.size())
            {
                const std::size_t end = text.find('\n', begin);
                std::string line = text.substr(
                    begin,
                    end == std::string::npos ?
                        std::string::npos : end - begin);
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (!line.empty())
                {
                    const std::size_t equals = line.find('=');
                    if (equals == std::string::npos || equals == 0)
                    {
                        error = "Gate 4 Runtime evidence contains a malformed line.";
                        return false;
                    }
                    const std::string key = line.substr(0, equals);
                    if (!values.emplace(key, line.substr(equals + 1)).second)
                    {
                        error = "Gate 4 Runtime evidence contains a duplicate key: " + key;
                        return false;
                    }
                }
                if (end == std::string::npos)
                    break;
                begin = end + 1;
            }
            error.clear();
            return true;
        }

        bool Require(
            const std::map<std::string, std::string>& values,
            const std::string& key,
            const std::string& expected,
            std::string& error)
        {
            const auto found = values.find(key);
            if (found == values.end() || found->second != expected)
            {
                error = "Gate 4 Runtime evidence does not prove " + key +
                    "=" + expected + ".";
                return false;
            }
            return true;
        }

        bool IsWithin(const fs::path& root, const fs::path& candidate)
        {
            std::error_code ec;
            const fs::path relative = fs::relative(candidate, root, ec);
            if (ec || relative.empty() || relative.is_absolute())
                return false;
            return std::none_of(
                relative.begin(),
                relative.end(),
                [](const fs::path& part)
                {
                    return part == "..";
                });
        }

        bool ParseUnsigned(const std::string& text, std::size_t& value)
        {
            if (text.empty() ||
                !std::all_of(
                    text.begin(),
                    text.end(),
                    [](const unsigned char character)
                    {
                        return character >= '0' && character <= '9';
                    }))
            {
                return false;
            }
            try
            {
                value = static_cast<std::size_t>(std::stoull(text));
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    }

    bool RecordWindowsGameBuildVerification(
        const WindowsGameBuildVerificationRequest& request,
        WindowsGameBuildVerificationResult& result,
        std::string& error)
    {
        result = {};
        try
        {
            if (request.packageRootPath.empty() ||
                request.runtimeEvidencePath.empty() ||
                request.expectedGraphicsBackend.empty() ||
                request.windowsPrerequisitePolicy !=
                    WindowsVcRuntimePrerequisitePolicy ||
                request.expectedFlowTrace.empty())
            {
                error = "Gate 4 verification request is incomplete or uses an unapproved prerequisite policy.";
                return false;
            }

            WindowsGamePackageIntegrityResult integrity;
            if (!ValidateWindowsGamePackage(
                    request.packageRootPath,
                    integrity,
                    error))
            {
                return false;
            }
            const fs::path packageRoot = fs::u8path(integrity.packageRootPath);
            result.packageRootPath = integrity.packageRootPath;

            std::error_code ec;
            const fs::path evidencePath = fs::weakly_canonical(
                fs::u8path(request.runtimeEvidencePath), ec);
            if (ec || evidencePath.empty() || IsWithin(packageRoot, evidencePath))
            {
                error = "Gate 4 Runtime evidence must be a regular file outside the immutable package root.";
                return false;
            }

            std::string evidenceText;
            if (!ReadText(evidencePath, evidenceText, error))
                return false;

            WindowsGamePackageFileDigest evidenceDigest;
            if (!DigestWindowsGamePackageFile(
                    evidencePath.generic_u8string(),
                    evidenceDigest,
                    error))
            {
                return false;
            }

            std::map<std::string, std::string> evidence;
            if (!ParseEvidence(evidenceText, evidence, error) ||
                !Require(evidence, "schema", "renegade-runtime-bootstrap-v2", error) ||
                !Require(evidence, "status", "PASS", error) ||
                !Require(evidence, "code", "SUCCESS", error) ||
                !Require(evidence, "package_integrity", "PASS", error) ||
                !Require(evidence, "graphics_backend_requested",
                    request.expectedGraphicsBackend, error) ||
                !Require(evidence, "graphics_backend",
                    request.expectedGraphicsBackend, error) ||
                !Require(evidence, "graphics_capability", "STARTED", error) ||
                !Require(evidence, "windows_prerequisite_policy",
                    request.windowsPrerequisitePolicy, error) ||
                !Require(evidence, "smoke_status", "PASS", error) ||
                !Require(evidence, "smoke_quit_reason", "smoke_complete", error) ||
                !Require(evidence, "flow_terminal", "complete_game", error))
            {
                return false;
            }

            // LP06 originally proved a legacy project-level startup Screen by
            // auto-activating its Play action before Story Flow. Gate 10 modern
            // projects are Flow-native: their packaged descriptor has no
            // startup Screen, Runtime enters Story Flow directly, and the build
            // supplies the exact authored outcomes required to reach Complete
            // Game. Keep both contracts strict instead of forcing modern Flow
            // through fabricated legacy Screen evidence.
            const auto startupScreen = evidence.find("startup_screen");
            if (startupScreen == evidence.end())
            {
                error = "Gate 4 Runtime evidence is missing startup Screen state.";
                return false;
            }
            const bool legacyStartupScreen = !startupScreen->second.empty();
            if (legacyStartupScreen)
            {
                if (!Require(evidence, "screen_was_loaded", "true", error) ||
                    !Require(evidence, "last_action_id", "play", error) ||
                    !Require(evidence, "last_action_code", "success", error))
                {
                    return false;
                }
            }
            else
            {
                const auto startupFlow = evidence.find("startup_flow");
                const auto flowDocumentId = evidence.find("flow_document_id");
                if (startupFlow == evidence.end() || startupFlow->second.empty() ||
                    flowDocumentId == evidence.end() || flowDocumentId->second.empty())
                {
                    error =
                        "Gate 4 Flow-native Runtime evidence does not prove a governed startup Story Flow.";
                    return false;
                }
                if (!Require(evidence, "screen_loaded", "false", error) ||
                    !Require(evidence, "screen_was_loaded", "false", error) ||
                    !Require(evidence, "last_action_id", "", error) ||
                    !Require(evidence, "last_action_code", "", error) ||
                    !Require(evidence, "last_action_sequence", "0", error))
                {
                    return false;
                }
            }

            const auto packageRootEvidence = evidence.find("package_root");
            const auto projectRootEvidence = evidence.find("project_root");
            if (packageRootEvidence == evidence.end() ||
                projectRootEvidence == evidence.end())
            {
                error = "Gate 4 Runtime evidence is missing package/project root proof.";
                return false;
            }

            std::error_code packageError;
            const fs::path loggedPackageRoot = fs::weakly_canonical(
                fs::u8path(packageRootEvidence->second), packageError);
            std::error_code equivalentError;
            if (packageError ||
                !fs::equivalent(packageRoot, loggedPackageRoot, equivalentError) ||
                equivalentError)
            {
                error = "Gate 4 Runtime evidence came from a different package root.";
                return false;
            }

            std::error_code projectError;
            const fs::path loggedProjectRoot = fs::weakly_canonical(
                fs::u8path(projectRootEvidence->second), projectError);
            if (projectError || !IsWithin(packageRoot, loggedProjectRoot))
            {
                error = "Gate 4 Runtime project root was not contained by the package.";
                return false;
            }

            const auto countValue = evidence.find("flow_trace_count");
            std::size_t flowCount = 0;
            if (countValue == evidence.end() ||
                !ParseUnsigned(countValue->second, flowCount) ||
                flowCount != request.expectedFlowTrace.size())
            {
                error = "Gate 4 Runtime flow trace count does not match Test All evidence.";
                return false;
            }
            for (std::size_t index = 0; index < flowCount; ++index)
            {
                const std::string key = "flow_trace_" + std::to_string(index);
                const auto found = evidence.find(key);
                if (found == evidence.end() ||
                    found->second != request.expectedFlowTrace[index])
                {
                    error = "Gate 4 packaged Runtime flow trace diverged from Test All at step " +
                        std::to_string(index) + ".";
                    return false;
                }
            }

            const fs::path buildReportPath = packageRoot / "build-report.json";
            std::string buildReportText;
            if (!ReadText(buildReportPath, buildReportText, error))
                return false;

            nlohmann::json buildReport;
            try
            {
                buildReport = nlohmann::json::parse(buildReportText);
            }
            catch (const std::exception& exception)
            {
                error = std::string("Gate 4 build report is invalid JSON: ") +
                    exception.what();
                return false;
            }
            if (!buildReport.is_object() ||
                buildReport.value("stage_only", false) != true ||
                buildReport.value("distribution_ready", true) != false ||
                buildReport.value("status", std::string{}) !=
                    "identity_applied_not_launched")
            {
                error = "Gate 4 expected the accepted Gate 3 pre-launch build-report state.";
                return false;
            }

            buildReport["schema_version"] = 3;
            buildReport["status"] = "isolated_smoke_passed_not_promoted";
            buildReport["stage_only"] = true;
            buildReport["distribution_ready"] = false;
            buildReport["smoke_test"] = "passed_gate4";
            buildReport["smoke_backend"] = request.expectedGraphicsBackend;
            buildReport["runtime_entry_mode"] = legacyStartupScreen
                ? "legacy_startup_screen"
                : "story_flow_native";
            buildReport["windows_prerequisite_policy"] =
                request.windowsPrerequisitePolicy;
            buildReport["package_isolation"] = "passed_gate4";
            buildReport["test_all_parity"] = "passed_gate4";
            buildReport["runtime_evidence_bytes"] = evidenceDigest.byteCount;
            buildReport["runtime_evidence_sha256"] = evidenceDigest.sha256;
            buildReport["promotion"] = "not_attempted_gate4";

            if (!WriteText(buildReportPath, buildReport.dump(), error))
                return false;

            WindowsGamePackageFileDigest buildReportDigest;
            if (!DigestWindowsGamePackageFile(
                    buildReportPath.generic_u8string(),
                    buildReportDigest,
                    error))
            {
                return false;
            }

            const fs::path packageManifestPath =
                packageRoot / "package-manifest.json";
            std::string packageManifestText;
            if (!ReadText(packageManifestPath, packageManifestText, error))
                return false;

            nlohmann::json packageManifest;
            try
            {
                packageManifest = nlohmann::json::parse(packageManifestText);
            }
            catch (const std::exception& exception)
            {
                error = std::string("Gate 4 package manifest is invalid JSON: ") +
                    exception.what();
                return false;
            }
            if (!packageManifest.is_object() ||
                !packageManifest.contains("files") ||
                !packageManifest["files"].is_array())
            {
                error = "Gate 4 package manifest has no file array to refresh.";
                return false;
            }

            bool refreshed = false;
            for (auto& item : packageManifest["files"])
            {
                if (item.is_object() &&
                    item.value("path", std::string{}) == "build-report.json")
                {
                    if (refreshed)
                    {
                        error = "Gate 4 package manifest contains duplicate build-report records.";
                        return false;
                    }
                    item["bytes"] = buildReportDigest.byteCount;
                    item["sha256"] = buildReportDigest.sha256;
                    refreshed = true;
                }
            }
            if (!refreshed)
            {
                error = "Gate 4 package manifest lost build-report.json.";
                return false;
            }
            if (!WriteText(packageManifestPath, packageManifest.dump(), error))
                return false;

            WindowsGamePackageIntegrityResult verified;
            if (!ValidateWindowsGamePackage(
                    packageRoot.generic_u8string(),
                    verified,
                    error))
            {
                return false;
            }

            result.succeeded = true;
            result.message =
                "Gate 4 isolated Runtime smoke and Test All parity evidence accepted; package remains staged and unpromoted.";
            result.runtimeEvidenceBytes = evidenceDigest.byteCount;
            result.runtimeEvidenceSha256 = std::move(evidenceDigest.sha256);
            result.buildReportSha256 = std::move(buildReportDigest.sha256);
            result.packageManifestSha256 =
                std::move(verified.packageManifestSha256);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = std::string("Gate 4 verification failed: ") + exception.what();
            return false;
        }
    }
}
