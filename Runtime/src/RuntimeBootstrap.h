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
        std::vector<std::string> flowOutcomes;
        std::string flowDocumentId;
        std::string flowNodeId;
        std::string flowNodeName;
        std::string flowEntry;
        bridge::FlowTerminalAction flowTerminalAction =
            bridge::FlowTerminalAction::None;
        std::vector<std::string> flowTrace;
        std::size_t entityCount = 0;
    };

    // Parses the Renegade-owned launch contract from already tokenized
    // arguments. Platform code must use the operating system's argument parser
    // first so quoted project paths containing spaces arrive as one token.
    [[nodiscard]] RuntimeBootstrapResult ParseRuntimeLaunchArguments(
        const std::vector<std::string>& arguments);

    // Validates the .renegade descriptor without opening it as an editor
    // project, resolves the startup WISCENE and optional Story Flow document,
    // and rejects lexical or symlink escape outside the project root.
    [[nodiscard]] RuntimeBootstrapResult ResolveRuntimeProject(
        RuntimeBootstrapResult result);

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
