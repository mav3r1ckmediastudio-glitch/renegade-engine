#include "RuntimeFlow.h"

#include <utility>

namespace
{
    renegade::runtime::RuntimeBootstrapResult Fail(
        renegade::runtime::RuntimeBootstrapResult result,
        const renegade::runtime::RuntimeBootstrapCode code,
        std::string message)
    {
        result.succeeded = false;
        result.code = code;
        result.message = std::move(message);
        result.entityCount = 0;
        return result;
    }

    void CopyStepToResult(
        renegade::runtime::RuntimeBootstrapResult& result,
        const renegade::bridge::FlowStepResult& step)
    {
        result.flowNodeId = step.currentNodeId;
        result.flowNodeName = step.currentNodeName;
        result.flowEntry = step.destinationEntry;
        result.flowTerminalAction = step.terminalAction;
    }
}

namespace renegade::runtime
{
    bool RuntimeFlowController::Initialize(
        const RuntimeBootstrapResult& bootstrap,
        std::string& error)
    {
        if (bootstrap.startupFlowPath.empty())
        {
            error = "The project does not declare a startup Story Flow document.";
            return false;
        }

        bridge::FlowDocument document;
        if (!bridge::ReadFlowDocument(
                bootstrap.startupFlowPath,
                bootstrap.project.projectId,
                document,
                error))
        {
            return false;
        }
        if (document.envelope.documentId != bootstrap.project.startupFlowId)
        {
            error = "Resolved Story Flow document ID does not match the project manifest.";
            return false;
        }
        if (!interpreter_.Initialize(std::move(document), error))
        {
            return false;
        }

        projectRoot_ = bootstrap.project.rootPath;
        trace_.clear();
        error.clear();
        return true;
    }

    bridge::FlowStepResult RuntimeFlowController::Start()
    {
        auto step = interpreter_.Start();
        if (step.succeeded)
        {
            RecordStep(step);
        }
        return step;
    }

    bridge::FlowStepResult RuntimeFlowController::EmitOutcome(
        const std::string& outcome)
    {
        auto step = interpreter_.EmitOutcome(outcome);
        if (step.succeeded)
        {
            RecordStep(step);
        }
        return step;
    }

    bool RuntimeFlowController::ApplyStep(
        bridge::SceneService& scenes,
        RuntimeBootstrapResult& bootstrap,
        const bridge::FlowStepResult& step,
        std::string& error)
    {
        CopyStepToResult(bootstrap, step);
        bootstrap.flowTrace = trace_;

        if (step.currentNodeKind != bridge::FlowNodeKind::Level)
        {
            error.clear();
            return true;
        }

        std::string scenePath;
        if (!bridge::ResolveSceneDocumentPath(
                projectRoot_,
                bootstrap.project.projectId,
                step.sceneAssetId,
                step.scenePathHint,
                scenePath,
                error))
        {
            return false;
        }
        if (!scenes.LoadScene(scenePath))
        {
            error = "Could not load Story Flow Level scene: " +
                scenes.LastError();
            return false;
        }
        if (!RefreshRuntimeReusableAssets(scenes, bootstrap, error))
        {
            error =
                "Could not refresh packaged reusable assets in Story Flow Level scene: " +
                error;
            return false;
        }

        bootstrap.startupScenePath = scenePath;
        bootstrap.entityCount = scenes.EntityCount();
        error.clear();
        return true;
    }

    const std::vector<std::string>& RuntimeFlowController::Trace() const noexcept
    {
        return trace_;
    }

    const bridge::FlowDocument& RuntimeFlowController::Document() const noexcept
    {
        return interpreter_.Document();
    }

    void RuntimeFlowController::RecordStep(
        const bridge::FlowStepResult& step)
    {
        if (step.previousNodeId.empty())
        {
            trace_.push_back("ENTER " + step.currentNodeName);
            return;
        }

        std::string line = step.previousNodeId + " --" + step.outcome +
            "--> " + step.currentNodeId + " [" + step.currentNodeName + ']';
        if (!step.destinationEntry.empty())
        {
            line += " entry=" + step.destinationEntry;
        }
        trace_.push_back(std::move(line));
    }

    RuntimeBootstrapResult LoadRuntimeProjectFlow(
        bridge::SceneService& scenes,
        RuntimeFlowController& flow,
        RuntimeBootstrapResult result)
    {
        if (!result.succeeded)
        {
            return result;
        }

        std::string error;
        if (!flow.Initialize(result, error))
        {
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::FlowRejected,
                "Could not initialize project Story Flow: " + error);
        }

        result.flowDocumentId = flow.Document().envelope.documentId;

        auto step = flow.Start();
        if (!step.succeeded)
        {
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::FlowExecutionFailed,
                step.message);
        }
        if (!flow.ApplyStep(scenes, result, step, error))
        {
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::SceneLoadFailed,
                error);
        }

        step = flow.EmitOutcome(bridge::GameStartOutcome);
        if (!step.succeeded)
        {
            result.flowTrace = flow.Trace();
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::FlowExecutionFailed,
                step.message);
        }
        if (!flow.ApplyStep(scenes, result, step, error))
        {
            return Fail(
                std::move(result),
                RuntimeBootstrapCode::SceneLoadFailed,
                error);
        }

        for (const auto& outcome : result.flowOutcomes)
        {
            step = flow.EmitOutcome(outcome);
            if (!step.succeeded)
            {
                result.flowTrace = flow.Trace();
                return Fail(
                    std::move(result),
                    RuntimeBootstrapCode::FlowExecutionFailed,
                    step.message);
            }
            if (!flow.ApplyStep(scenes, result, step, error))
            {
                return Fail(
                    std::move(result),
                    RuntimeBootstrapCode::SceneLoadFailed,
                    error);
            }
        }

        result.flowTrace = flow.Trace();
        result.succeeded = true;
        result.code = RuntimeBootstrapCode::Success;
        if (result.flowTerminalAction != bridge::FlowTerminalAction::None)
        {
            result.message = "Story Flow reached terminal node: " +
                result.flowNodeName;
        }
        else
        {
            result.message = "Story Flow entered node: " + result.flowNodeName;
        }
        return result;
    }
}
