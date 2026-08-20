#pragma once

#include "RuntimeBootstrap.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/SceneService.h"

#include <string>
#include <vector>

namespace renegade::runtime
{
    // Runtime-owned execution boundary over the bridge's serialized,
    // renderer-independent flow model. Screen actions and gameplay outcomes
    // both advance the same deterministic FlowInterpreter contract.
    class RuntimeFlowController
    {
    public:
        [[nodiscard]] bool Initialize(
            const RuntimeBootstrapResult& bootstrap,
            std::string& error);

        [[nodiscard]] bridge::FlowStepResult Start();
        [[nodiscard]] bridge::FlowStepResult EmitOutcome(
            const std::string& outcome);

        [[nodiscard]] bool ApplyStep(
            bridge::SceneService& scenes,
            RuntimeBootstrapResult& bootstrap,
            const bridge::FlowStepResult& step,
            std::string& error);

        [[nodiscard]] const std::vector<std::string>& Trace() const noexcept;
        [[nodiscard]] const bridge::FlowDocument& Document() const noexcept;
        [[nodiscard]] const bridge::FlowNode* CurrentNode() const noexcept;

    private:
        void RecordStep(const bridge::FlowStepResult& step);

        bridge::FlowInterpreter interpreter_;
        std::string projectRoot_;
        std::vector<std::string> trace_;
    };

    // Starts at the permanent Game Start node, emits game.start through the
    // shared named-outcome contract, enters the selected Level or Screen, and
    // then consumes any diagnostic --flow-outcome arguments.
    [[nodiscard]] RuntimeBootstrapResult LoadRuntimeProjectFlow(
        bridge::SceneService& scenes,
        RuntimeFlowController& flow,
        RuntimeBootstrapResult result);
}
