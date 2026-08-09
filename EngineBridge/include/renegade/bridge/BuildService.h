#pragma once

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ProjectService.h"

#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    enum class WindowsGameBuildFileKind
    {
        ProjectContent,
        RuntimeSupport,
    };

    struct WindowsGameBuildRequest
    {
        std::string gameName;
        std::string executableBaseName;
        std::string publicVersion = "0.1.0";
        StableId saveDataId;
        std::string platform = "windows-x64";
        std::string configuration = "Release";
    };

    // Runtime support is governed separately from creator content. Gate 1
    // consumes already inspected support-file records; later LP06 gates own
    // authoritative source resolution, copying, prerequisite deployment and
    // staged package verification.
    struct WindowsRuntimeSupportInput
    {
        std::string logicalName;
        std::string destinationPath;
        std::uint64_t byteCount = 0;
        std::string sha256;
        std::string provenance;
    };

    struct WindowsGameBuildFile
    {
        WindowsGameBuildFileKind kind =
            WindowsGameBuildFileKind::ProjectContent;
        std::string destinationPath;

        // Project-content fields. Runtime-support entries leave these empty.
        std::string projectRelativeSourcePath;
        StableId assetId;
        DependencyClass dependencyClass = DependencyClass::Data;
        DependencyRequirement requirement = DependencyRequirement::Required;
        std::string sourceContentHash;

        // Runtime-support fields. Project-content entries leave these empty/0.
        std::string runtimeSupportName;
        std::uint64_t byteCount = 0;
        std::string sha256;

        // Stable, sorted provenance labels explaining why the entry belongs in
        // the plan. For project content these come from graph roots/edges; for
        // Runtime support the caller supplies a versioned support provenance.
        std::vector<std::string> provenance;
    };

    struct WindowsGameBuildPlan
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;

        std::string formatIdentifier = "renegade-windows-game-build-plan";
        std::uint32_t schemaVersion = CurrentSchemaVersion;
        StableId projectId;
        std::string gameName;
        std::string executableFileName;
        std::string buildFolderName;
        std::string publicVersion;
        StableId saveDataId;
        std::string platform = "windows-x64";
        std::string configuration = "Release";
        std::vector<WindowsGameBuildFile> files;
        std::vector<std::string> warnings;
        std::uint32_t excludedEditorOnly = 0;
        std::uint32_t excludedOptionalMissing = 0;
        std::uint32_t excludedUnreachable = 0;
    };

    // LP06 Gate 1: create a deterministic, UI-free loose-build plan from the
    // accepted dependency graph and LC01 registry. This does not copy files,
    // mutate the project, rename an executable, stage a package or promote an
    // output directory. Runtime support remains a separate governed input.
    [[nodiscard]] bool CreateWindowsGameBuildPlan(
        const ProjectMetadata& project,
        const DependencyGraph& graph,
        const AssetRegistry& registry,
        const WindowsGameBuildRequest& request,
        const std::vector<WindowsRuntimeSupportInput>& runtimeSupport,
        WindowsGameBuildPlan& plan,
        std::string& error);

    // Canonical, sorted UTF-8 JSON evidence for Gate 1 and later LP06 gates.
    [[nodiscard]] bool SerializeWindowsGameBuildPlan(
        const WindowsGameBuildPlan& plan,
        std::string& json,
        std::string& error);
}
