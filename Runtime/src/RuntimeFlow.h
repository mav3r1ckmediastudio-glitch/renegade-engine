#pragma once

#include "RuntimeBootstrap.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/SceneService.h"

#include <string>
#include <vector>

namespace renegade::runtime
{
    // Runtime-owned execution boundary over the bridge's serialized,
    // renderer-independent flow model. LP02 exposes only deterministic startup
    // and named-outcome advancement; gameplay triggers and screen actions join
    // this same contract in later proofs.
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

    private:
        void RecordStep(const bridge::FlowStepResult& step);

        bridge::FlowInterpreter interpreter_;
        std::string projectRoot_;
        std::vector<std::string> trace_;
    };

    // Starts at the permanent Game Start node, emits game.start through the
    // same named-outcome contract used later by UI/gameplay, loads the selected
    // Level node, and then consumes any diagnostic --flow-outcome arguments.
    [[nodiscard]] RuntimeBootstrapResult LoadRuntimeProjectFlow(
        bridge::SceneService& scenes,
        RuntimeFlowController& flow,
        RuntimeBootstrapResult result);
}
