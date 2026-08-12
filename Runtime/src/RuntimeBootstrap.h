#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneService.h"

namespace renegade::runtime
{
    enum class RuntimeBootstrapCode : int
    {
        Success = 0,
        MissingProjectArgument = 20,
        InvalidArguments = 21,
        ProjectRejected = 22,
        StartupSceneOutsideProject = 23,
        SceneLoadFailed = 24,
        StartupFlowRejected = 25,
        FlowRejected = 26,
        FlowExecutionFailed = 27,
        StartupScreenRejected = 28,
        ScreenLoadFailed = 29,
        PackageIntegrityFailed = 30,
        GraphicsPrerequisiteMissing = 31,
    };

    struct RuntimeBootstrapResult
    {
        bool succeeded = false;
        RuntimeBootstrapCode code =
            RuntimeBootstrapCode::MissingProjectArgument;
        std::string message;
        std::string projectDescriptorPath;
        bridge::ProjectMetadata project;
        std::string startupScenePath;
        std::string startupFlowPath;
        std::string startupScreenPath;
        std::vector<std::string> flowOutcomes;
        std::string flowDocumentId;
        std::string flowNodeId;
        std::string flowNodeName;
        std::string flowEntry;
        bridge::FlowTerminalAction flowTerminalAction =
            bridge::FlowTerminalAction::None;
        std::vector<std::string> flowTrace;
        std::string screenDocumentId;
        std::string screenFocusedWidgetId;
        bool screenLoaded = false;
        bool screenWasLoaded = false;
        std::string lastActionId;
        std::string lastActionWidgetId;
        std::string lastActionInput;
        std::string lastActionCode;
        std::string lastActionMessage;
        std::uint64_t lastActionSequence = 0;
        std::size_t entityCount = 0;

        // LP06 Gate 4 packaged-runtime evidence. Explicit --project/Test Level
        // launches leave these empty so the accepted LP04 contract is not
        // silently converted into a package launch.
        bool packageRelativeLaunch = false;
        std::string packageRootPath;
        std::string saveDataId;
        std::string packageIntegrityStatus;
        std::string packageIntegrityCode;
        std::string packageManifestSha256;
        std::string graphicsBackendRequested;
        std::string graphicsBackend;
        std::string graphicsCapability;
        std::string windowsPrerequisitePolicy;
        std::string smokeStatus;
        std::string smokeQuitReason;

        // LP07 Gate 6 packaged-runtime evidence. The scene stores durable
        // reusable asset IDs on authored wrapper entities; a package-relative
        // launch resolves those IDs through GameData/content-manifest.json and
        // replaces only their child payloads from the packaged current .rasset.
        std::size_t reusableAssetInstancesDiscovered = 0;
        std::size_t reusableAssetInstancesRefreshed = 0;
        std::vector<std::string> reusableAssetRefreshTrace;
    };

    // Parses the Renegade-owned launch contract from already tokenized
    // arguments. Platform code must use the operating system's argument parser
    // first so quoted project paths containing spaces arrive as one token.
    [[nodiscard]] RuntimeBootstrapResult ParseRuntimeLaunchArguments(
        const std::vector<std::string>& arguments);

    // Validates the .renegade descriptor without opening it as an editor
    // project, resolves the startup WISCENE and optional Story Flow / Runtime
    // screen documents, and rejects lexical or symlink escape outside the
    // project root.
    [[nodiscard]] RuntimeBootstrapResult ResolveRuntimeProject(
        RuntimeBootstrapResult result);

    // Gate 6 post-load package refresh. Explicit --project/Test Level launches
    // are unchanged; package-relative launches resolve and refresh every saved
    // reusable asset instance from the already integrity-validated package.
    [[nodiscard]] bool RefreshRuntimeReusableAssets(
        bridge::SceneService& scenes,
        RuntimeBootstrapResult& result,
        std::string& error);

    // Loads the already resolved WISCENE through the shared SceneService and
    // converts its diagnostic into the same structured startup result.
    [[nodiscard]] RuntimeBootstrapResult LoadRuntimeProjectScene(
        bridge::SceneService& scenes,
        RuntimeBootstrapResult result);

    [[nodiscard]] const char* RuntimeBootstrapCodeName(
        RuntimeBootstrapCode code) noexcept;

    // Writes a deterministic text evidence record. Failure to write the log
    // does not change the Runtime result, but the caller receives the error.
    [[nodiscard]] bool WriteRuntimeBootstrapLog(
        const RuntimeBootstrapResult& result,
        const std::string& logPath,
        std::string& error);
}
