#pragma once

#include "renegade/bridge/BuildService.h"

#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct WindowsRuntimeSupportSource
    {
        std::string destinationPath;
        std::string sourcePath;
    };

    struct WindowsPackageDocumentInput
    {
        std::string destinationPath;
        std::string sourcePath;
        std::string component;
        std::string provenance;
    };

    struct WindowsGameBuildStagingRequest
    {
        std::string projectRootPath;
        std::string outputParentPath;
        std::string stagingId;
        std::string renegadeRevision;
        std::string wickedRevision;
        std::vector<WindowsRuntimeSupportSource> runtimeSupportSources;
        std::vector<WindowsPackageDocumentInput> packageDocuments;
    };

    struct WindowsGameStagedFile
    {
        std::string destinationPath;
        std::uint64_t byteCount = 0;
        std::string sha256;
        std::string fileClass;
        std::vector<std::string> provenance;
    };

    struct WindowsGameBuildStageResult
    {
        std::string stagingPath;
        std::string finalOutputPath;
        std::string projectManifestJson;
        std::string contentManifestJson;
        std::string runtimeSupportManifestJson;
        std::string packageManifestJson;
        std::string projectManifestSha256;
        std::string contentManifestSha256;
        std::string runtimeSupportManifestSha256;
        std::string packageManifestSha256;
        std::vector<WindowsGameStagedFile> files;
    };

    // LP06 Gate 2: materialize a validated Gate 1 plan into a unique staging
    // directory below outputParentPath/.renegade-staging. The final owner-visible
    // build path is never created, replaced or promoted by this function.
    [[nodiscard]] bool StageWindowsGameBuild(
        const WindowsGameBuildPlan& plan,
        const WindowsGameBuildStagingRequest& request,
        WindowsGameBuildStageResult& result,
        std::string& error);

    // Re-enumerates and re-hashes the staged tree. Every file must match the
    // exact governed set captured by StageWindowsGameBuild; extras, missing
    // files, symlinks, case-collisions and tampering fail closed.
    [[nodiscard]] bool ValidateWindowsGameBuildStage(
        const WindowsGameBuildStageResult& result,
        std::string& error);
}
