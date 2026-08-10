#include "renegade/bridge/BuildPromotionService.h"
#include "renegade/bridge/PackageIntegrityService.h"
#include "BuildPromotionServiceInternal.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "json.hpp"

namespace
{
    namespace fs = std::filesystem;
    using renegade::bridge::WindowsGameBuildPromotionCode;
    using renegade::bridge::WindowsGameBuildPromotionRequest;
    using renegade::bridge::WindowsGameBuildPromotionResult;

    constexpr const char* ProjectId =
        "11111111-1111-4111-8111-111111111111";
    constexpr const char* SaveDataId =
        "22222222-2222-4222-8222-222222222222";
    constexpr const char* GameName = "Proof Game";
    constexpr const char* FinalFolder = "Proof Game Windows Build";

    void Require(const bool condition, const std::string& message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    void WriteText(
        const fs::path& path,
        const std::string& text)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
            throw std::runtime_error("could not create fixture directory: " + ec.message());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("could not write fixture: " + path.generic_u8string());
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        output.close();
        if (!output)
            throw std::runtime_error("could not complete fixture write: " + path.generic_u8string());
    }

    std::string ReadText(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("could not read fixture: " + path.generic_u8string());
        return std::string(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
    }

    void WriteJson(const fs::path& path, const nlohmann::json& value)
    {
        WriteText(path, value.dump());
    }

    std::vector<std::string> EnumeratePackageFiles(const fs::path& root)
    {
        std::vector<std::string> files;
        for (const auto& entry : fs::recursive_directory_iterator(root))
        {
            if (!entry.is_regular_file())
                continue;
            const std::string relative =
                fs::relative(entry.path(), root).generic_u8string();
            if (relative != "package-manifest.json")
                files.push_back(relative);
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    void RefreshPackageManifest(const fs::path& root)
    {
        nlohmann::json manifest;
        manifest["format"] = "renegade-package-manifest";
        manifest["schema_version"] = 2;
        manifest["self_path"] = "package-manifest.json";
        manifest["self_sha256_excluded"] = true;
        manifest["project_id"] = ProjectId;
        manifest["game_name"] = GameName;
        manifest["executable"] = "ProofGame.exe";
        manifest["stage_only"] = true;
        manifest["distribution_ready"] = false;
        manifest["files"] = nlohmann::json::array();

        for (const std::string& relative : EnumeratePackageFiles(root))
        {
            renegade::bridge::WindowsGamePackageFileDigest digest;
            std::string error;
            Require(
                renegade::bridge::DigestWindowsGamePackageFile(
                    (root / fs::u8path(relative)).generic_u8string(),
                    digest,
                    error),
                "fixture digest failed for " + relative + ": " + error);
            nlohmann::json item;
            item["path"] = relative;
            item["bytes"] = digest.byteCount;
            item["sha256"] = digest.sha256;
            item["class"] = "gate5-test";
            item["provenance"] = nlohmann::json::array({"repo:gate5-test"});
            manifest["files"].push_back(std::move(item));
        }
        WriteJson(root / "package-manifest.json", manifest);
    }

    void CreateCandidate(
        const fs::path& root,
        const std::string& marker,
        const bool smokePassed = true)
    {
        std::error_code ec;
        fs::remove_all(root, ec);
        ec.clear();
        fs::create_directories(root, ec);
        Require(!ec, "could not create candidate root");

        WriteText(root / "ProofGame.exe", "runtime-" + marker);
        WriteText(root / "dxcompiler.dll", "dxcompiler-" + marker);
        WriteText(root / "GameData" / "Project.renegade", "project-fixture");
        WriteJson(
            root / "GameData" / "content-manifest.json",
            nlohmann::json{{"format", "content-fixture"}, {"marker", marker}});
        WriteJson(
            root / "Engine" / "runtime-support-manifest.json",
            nlohmann::json{{"format", "runtime-support-fixture"}, {"marker", marker}});
        WriteText(root / "ReadMe.txt", "Proof Game test package\n");
        WriteText(
            root / "Licences" / "Renegade-Licence-or-Notice.txt",
            "Renegade test notice\n");
        WriteText(
            root / "Licences" / "WickedEngine-LICENSE.txt",
            "Wicked test licence\n");

        nlohmann::json projectManifest;
        projectManifest["format"] = "renegade-project-package-manifest";
        projectManifest["schema_version"] = 2;
        projectManifest["project_id"] = ProjectId;
        projectManifest["save_data_id"] = SaveDataId;
        projectManifest["game_name"] = GameName;
        projectManifest["executable"] = "ProofGame.exe";
        projectManifest["public_version"] = "0.1.0";
        projectManifest["internal_build_id"] = "gate5-" + marker;
        projectManifest["build_timestamp_utc"] = "2026-08-10T16:00:00Z";
        projectManifest["developer_publisher"] = "Maverick Media Studio";
        projectManifest["application_manifest_policy"] =
            "asInvoker+PerMonitorV2+longPathAware+utf8";
        projectManifest["icon_resource"] = true;
        projectManifest["bootstrap_mode"] = "package_relative";
        projectManifest["project_document"] = "GameData/Project.renegade";
        projectManifest["stage_only"] = true;
        WriteJson(root / "GameData" / "project.manifest.json", projectManifest);

        nlohmann::json report;
        report["format"] = "renegade-build-report";
        report["schema_version"] = 3;
        report["status"] = smokePassed
            ? "isolated_smoke_passed_not_promoted"
            : "identity_applied_not_launched";
        report["stage_only"] = true;
        report["distribution_ready"] = false;
        report["smoke_test"] = smokePassed
            ? "passed_gate4"
            : "not_run_gate4";
        report["package_isolation"] = smokePassed
            ? "passed_gate4"
            : "not_run_gate4";
        report["test_all_parity"] = smokePassed
            ? "passed_gate4"
            : "not_run_gate4";
        report["promotion"] = "not_attempted_gate4";
        report["marker"] = marker;
        WriteJson(root / "build-report.json", report);

        RefreshPackageManifest(root);

        renegade::bridge::WindowsGamePackageIntegrityResult integrity;
        std::string error;
        Require(
            renegade::bridge::ValidateWindowsGamePackage(
                root.generic_u8string(),
                integrity,
                error),
            "candidate fixture is not a valid package: " + error);
    }

    std::map<std::string, std::string> SnapshotTree(const fs::path& root)
    {
        std::map<std::string, std::string> snapshot;
        for (const auto& entry : fs::recursive_directory_iterator(root))
        {
            if (!entry.is_regular_file())
                continue;
            snapshot.emplace(
                fs::relative(entry.path(), root).generic_u8string(),
                ReadText(entry.path()));
        }
        return snapshot;
    }

    void RequireValidFinal(const fs::path& finalPath)
    {
        renegade::bridge::WindowsGamePackageIntegrityResult integrity;
        std::string error;
        Require(
            renegade::bridge::ValidateWindowsGamePackage(
                finalPath.generic_u8string(),
                integrity,
                error),
            "final package integrity failed: " + error);

        const nlohmann::json report = nlohmann::json::parse(
            ReadText(finalPath / "build-report.json"));
        const nlohmann::json projectManifest = nlohmann::json::parse(
            ReadText(finalPath / "GameData" / "project.manifest.json"));
        const nlohmann::json packageManifest = nlohmann::json::parse(
            ReadText(finalPath / "package-manifest.json"));
        Require(
            report.value("status", std::string{}) ==
                "gate5_validated_for_final_path",
            "final build report did not advance to Gate 5");
        Require(!report.value("stage_only", true),
            "final build report still claims stage-only");
        Require(!report.value("distribution_ready", true),
            "Gate 5 must not claim legal redistribution readiness");
        Require(
            projectManifest.value("promotion_state", std::string{}) ==
                "gate5_final_path_commit",
            "project manifest did not advance to Gate 5");
        Require(!projectManifest.value("stage_only", true),
            "project manifest still claims stage-only");
        Require(
            packageManifest.value("promotion", std::string{}) ==
                "commit_by_directory_rename",
            "package manifest did not record Gate 5 promotion mode");
        Require(!packageManifest.value("stage_only", true),
            "package manifest still claims stage-only");
    }

    struct Fixture
    {
        fs::path root;
        fs::path outputParent;
        fs::path stagingParent;
        fs::path finalPath;
        fs::path candidateA;
        fs::path candidateB;
        fs::path rollbackPath;

        explicit Fixture(const std::string& caseName)
        {
            root = fs::absolute(
                fs::temp_directory_path() /
                fs::u8path(
                    "renegade-gate5-" + caseName + "-" +
                    std::to_string(GetCurrentProcessId())));
            std::error_code ec;
            fs::remove_all(root, ec);
            outputParent = root / "Builds" / "Windows";
            stagingParent = outputParent / ".renegade-staging";
            finalPath = outputParent / FinalFolder;
            candidateA = stagingParent / "candidate-a";
            candidateB = stagingParent / "candidate-b";
            rollbackPath = stagingParent /
                fs::u8path(std::string(FinalFolder) + ".rollback");
            fs::create_directories(stagingParent, ec);
            Require(!ec, "could not create Gate 5 fixture staging root");
        }

        ~Fixture()
        {
            std::error_code ec;
            fs::remove_all(root, ec);
        }
    };

    bool Promote(
        const fs::path& candidate,
        const fs::path& finalPath,
        WindowsGameBuildPromotionResult& result,
        std::string& error)
    {
        WindowsGameBuildPromotionRequest request;
        request.candidatePath = candidate.generic_u8string();
        request.finalOutputPath = finalPath.generic_u8string();
        return renegade::bridge::PromoteWindowsGameBuild(
            request, result, error);
    }

    void EstablishPrevious(Fixture& fixture)
    {
        CreateCandidate(fixture.candidateA, "A");
        WindowsGameBuildPromotionResult result;
        std::string error;
        Require(
            Promote(fixture.candidateA, fixture.finalPath, result, error),
            "could not establish previous build: " + error);
        RequireValidFinal(fixture.finalPath);
    }

    void TestFirstPromotion()
    {
        Fixture fixture("first");
        CreateCandidate(fixture.candidateA, "A");
        WindowsGameBuildPromotionResult result;
        std::string error;
        Require(
            Promote(fixture.candidateA, fixture.finalPath, result, error),
            "first promotion failed: " + error);
        Require(result.succeeded, "first promotion result was not successful");
        Require(!result.previousBuildExisted,
            "first promotion unexpectedly reported a previous build");
        Require(fs::is_directory(fixture.finalPath),
            "first promotion did not create final directory");
        Require(!fs::exists(fixture.candidateA),
            "first promotion left the candidate in staging");
        Require(!fs::exists(fixture.rollbackPath),
            "first promotion left a rollback directory");
        RequireValidFinal(fixture.finalPath);
        Require(ReadText(fixture.finalPath / "ProofGame.exe") == "runtime-A",
            "first promotion changed the executable bytes");
    }

    void TestSuccessfulReplacement()
    {
        Fixture fixture("replace");
        EstablishPrevious(fixture);
        const auto previous = SnapshotTree(fixture.finalPath);
        CreateCandidate(fixture.candidateB, "B");

        WindowsGameBuildPromotionResult result;
        std::string error;
        Require(
            Promote(fixture.candidateB, fixture.finalPath, result, error),
            "replacement failed: " + error);
        Require(result.previousBuildExisted,
            "replacement did not detect previous build");
        Require(!fs::exists(fixture.rollbackPath),
            "successful replacement left rollback backup");
        RequireValidFinal(fixture.finalPath);
        Require(ReadText(fixture.finalPath / "ProofGame.exe") == "runtime-B",
            "replacement did not commit candidate B");
        Require(SnapshotTree(fixture.finalPath) != previous,
            "replacement did not change final package bytes");
    }

    void TestIncompleteCandidate()
    {
        Fixture fixture("incomplete");
        EstablishPrevious(fixture);
        const auto previous = SnapshotTree(fixture.finalPath);
        CreateCandidate(fixture.candidateB, "B");
        Require(fs::remove(
            fixture.candidateB / "Engine" / "runtime-support-manifest.json"),
            "could not inject incomplete candidate");

        WindowsGameBuildPromotionResult result;
        std::string error;
        Require(!Promote(fixture.candidateB, fixture.finalPath, result, error),
            "incomplete candidate was promoted");
        Require(result.code == WindowsGameBuildPromotionCode::CandidateRejected,
            "incomplete candidate returned wrong failure code");
        Require(SnapshotTree(fixture.finalPath) == previous,
            "incomplete candidate changed previous final bytes");
        Require(fs::is_directory(fixture.candidateB),
            "incomplete candidate escaped staging");
        Require(!fs::exists(fixture.rollbackPath),
            "incomplete candidate touched rollback state");
    }

    void TestFailedSmokeCandidate()
    {
        Fixture fixture("failed-smoke");
        EstablishPrevious(fixture);
        const auto previous = SnapshotTree(fixture.finalPath);
        CreateCandidate(fixture.candidateB, "B", false);

        WindowsGameBuildPromotionResult result;
        std::string error;
        Require(!Promote(fixture.candidateB, fixture.finalPath, result, error),
            "failed-smoke candidate was promoted");
        Require(result.code == WindowsGameBuildPromotionCode::CandidateRejected,
            "failed-smoke candidate returned wrong failure code");
        Require(SnapshotTree(fixture.finalPath) == previous,
            "failed-smoke candidate changed previous final bytes");
        Require(!fs::exists(fixture.rollbackPath),
            "failed-smoke candidate touched rollback state");
    }

    void TestLockedFinalOutput()
    {
        Fixture fixture("locked");
        EstablishPrevious(fixture);
        const auto previous = SnapshotTree(fixture.finalPath);
        CreateCandidate(fixture.candidateB, "B");

        HANDLE lock = CreateFileW(
            fixture.finalPath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr);
        Require(lock != INVALID_HANDLE_VALUE,
            "could not acquire directory handle for lock proof");

        WindowsGameBuildPromotionResult result;
        std::string error;
        const bool promoted = Promote(
            fixture.candidateB, fixture.finalPath, result, error);
        CloseHandle(lock);

        Require(!promoted, "locked final output was unexpectedly replaced");
        Require(result.code == WindowsGameBuildPromotionCode::PromotionFailed,
            "locked final output returned wrong failure code");
        Require(SnapshotTree(fixture.finalPath) == previous,
            "locked-output failure changed previous final bytes");
        Require(fs::is_directory(fixture.candidateB),
            "locked-output failure lost candidate staging evidence");
        Require(!fs::exists(fixture.rollbackPath),
            "locked-output failure left rollback state");
    }

    void TestInterruptedAfterBackup()
    {
        Fixture fixture("interrupted");
        EstablishPrevious(fixture);
        const auto previous = SnapshotTree(fixture.finalPath);
        CreateCandidate(fixture.candidateB, "B");

        WindowsGameBuildPromotionRequest request;
        request.candidatePath = fixture.candidateB.generic_u8string();
        request.finalOutputPath = fixture.finalPath.generic_u8string();
        WindowsGameBuildPromotionResult result;
        std::string error;
        const bool promoted =
            renegade::bridge::detail::PromoteWindowsGameBuildWithHook(
                request,
                result,
                error,
                [](const auto point,
                   const std::string&,
                   const std::string&,
                   const std::string&,
                   std::string& hookError)
                {
                    if (point == renegade::bridge::detail::
                            WindowsGameBuildPromotionHookPoint::AfterPreviousBuildMoved)
                    {
                        hookError = "injected interruption after previous-build backup";
                        return false;
                    }
                    return true;
                });

        Require(!promoted, "injected interruption unexpectedly succeeded");
        Require(result.code == WindowsGameBuildPromotionCode::PromotionFailed,
            "interrupted promotion returned wrong failure code");
        Require(SnapshotTree(fixture.finalPath) == previous,
            "interrupted promotion did not restore previous bytes");
        Require(fs::is_directory(fixture.candidateB),
            "interrupted promotion lost candidate staging evidence");
        Require(!fs::exists(fixture.rollbackPath),
            "interrupted promotion left rollback path after recovery");
    }

    void TestPostMoveValidationFailure()
    {
        Fixture fixture("post-move-validation");
        EstablishPrevious(fixture);
        const auto previous = SnapshotTree(fixture.finalPath);
        CreateCandidate(fixture.candidateB, "B");

        WindowsGameBuildPromotionRequest request;
        request.candidatePath = fixture.candidateB.generic_u8string();
        request.finalOutputPath = fixture.finalPath.generic_u8string();
        WindowsGameBuildPromotionResult result;
        std::string error;
        const bool promoted =
            renegade::bridge::detail::PromoteWindowsGameBuildWithHook(
                request,
                result,
                error,
                [](const auto point,
                   const std::string&,
                   const std::string& finalPath,
                   const std::string&,
                   std::string&)
                {
                    if (point == renegade::bridge::detail::
                            WindowsGameBuildPromotionHookPoint::BeforeFinalValidation)
                    {
                        WriteText(
                            fs::u8path(finalPath) / "unexpected-after-move.txt",
                            "injected tamper");
                    }
                    return true;
                });

        Require(!promoted, "post-move invalid candidate unexpectedly succeeded");
        Require(result.code == WindowsGameBuildPromotionCode::FinalValidationFailed,
            "post-move validation returned wrong failure code");
        Require(SnapshotTree(fixture.finalPath) == previous,
            "post-move validation failure did not restore previous bytes");
        Require(fs::is_directory(fixture.candidateB),
            "post-move validation failure did not return candidate to staging");
        Require(fs::exists(
            fixture.candidateB / "unexpected-after-move.txt"),
            "post-move failure evidence was not retained with candidate");
        Require(!fs::exists(fixture.rollbackPath),
            "post-move validation failure left rollback path");
    }

    void TestStaleRollbackRecovery()
    {
        Fixture fixture("stale-recovery");
        EstablishPrevious(fixture);
        const auto previous = SnapshotTree(fixture.finalPath);
        CreateCandidate(fixture.candidateB, "B");

        Require(
            MoveFileExW(
                fixture.finalPath.c_str(),
                fixture.rollbackPath.c_str(),
                MOVEFILE_WRITE_THROUGH) != FALSE,
            "could not create stale rollback fixture");
        Require(!fs::exists(fixture.finalPath) && fs::exists(fixture.rollbackPath),
            "stale rollback fixture state is incorrect");

        WindowsGameBuildPromotionResult result;
        std::string error;
        Require(!Promote(fixture.candidateB, fixture.finalPath, result, error),
            "stale rollback recovery should require explicit retry");
        Require(result.code == WindowsGameBuildPromotionCode::RecoveredPreviousBuild,
            "stale rollback recovery returned wrong code");
        Require(SnapshotTree(fixture.finalPath) == previous,
            "stale rollback recovery changed previous final bytes");
        Require(fs::is_directory(fixture.candidateB),
            "stale rollback recovery touched new candidate");
        Require(!fs::exists(fixture.rollbackPath),
            "stale rollback recovery left rollback directory");
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: RenegadeBuildPromotionTests <case>\n";
        return 2;
    }

    const std::string testCase = argv[1];
    try
    {
        if (testCase == "first")
            TestFirstPromotion();
        else if (testCase == "replace")
            TestSuccessfulReplacement();
        else if (testCase == "incomplete")
            TestIncompleteCandidate();
        else if (testCase == "failed-smoke")
            TestFailedSmokeCandidate();
        else if (testCase == "locked")
            TestLockedFinalOutput();
        else if (testCase == "interrupted")
            TestInterruptedAfterBackup();
        else if (testCase == "post-move-validation")
            TestPostMoveValidationFailure();
        else if (testCase == "stale-recovery")
            TestStaleRollbackRecovery();
        else
            throw std::runtime_error("unknown Gate 5 test case: " + testCase);

        std::cout << "LP06_GATE5_PASS case=" << testCase << '\n';
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "LP06_GATE5_FAIL case=" << testCase
                  << " error=" << exception.what() << '\n';
        return 1;
    }
}
