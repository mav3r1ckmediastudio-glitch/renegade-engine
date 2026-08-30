#include "RuntimeApplication.h"

#include "renegade/bridge/PhysicsLuaService.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/PrecipitationService.h"

#include <utility>

namespace renegade::runtime
{
    void RuntimeRenderPath::BindScene(
        bridge::SceneService& scenes,
        std::string projectRoot) noexcept
    {
        scenes_ = &scenes;
        projectRoot_ = std::move(projectRoot);
        scene = &scenes.GetScene();
        renderSettingsInitialized_ = false;
    }

    void RuntimeRenderPath::SyncRenderSettings(
        const bool resizeBuffersForMSAA)
    {
        if (scenes_ == nullptr)
            return;

        auto& activeScene = scenes_->GetScene();
        if (!projectRoot_.empty())
        {
            std::string ignored;
            (void)bridge::RefreshColorGradingLutResource(
                activeScene, projectRoot_, ignored);
        }
        const auto authored =
            bridge::CaptureRenderSettings(activeScene);
        if (bridge::RenderSettingsMatchPath(*this, authored))
        {
            renderSettings_ = authored;
            renderSettingsInitialized_ = true;
            return;
        }

        bridge::ApplyRenderSettingsToPath(
            *this,
            authored,
            resizeBuffersForMSAA);
        renderSettings_ = authored;
        renderSettingsInitialized_ = true;
    }

    void RuntimeRenderPath::Load()
    {
        // Gate 6 owns reflections/GI. Gate 5 replaces the former hardcoded
        // FXAA reset with the level's shared persisted image-quality state.
        setSSREnabled(false);
        setReflectionsEnabled(true);
        SyncRenderSettings(false);

        wi::scene::TransformComponent cameraTransform;
        cameraTransform.Translate(XMFLOAT3(0.0f, 1.5f, -4.0f));
        cameraTransform.UpdateTransform();
        camera->TransformCamera(cameraTransform);

        RenderPath3D::Load();
    }

    void RuntimeRenderPath::Update(const float dt)
    {
        // Story Flow can replace the active WISCENE without recreating this
        // render path. Re-capture on Update so each Level receives its own
        // authored Gate 5 state and a carrier-free Level restores defaults.
        SyncRenderSettings(true);
        RenderPath3D::Update(dt);
    }

    void RuntimeApplication::SetBootstrapResult(RuntimeBootstrapResult result)
    {
        startupResult_ = std::move(result);
    }

    void RuntimeApplication::SetSmokeOptions(
        const bool autoPlay,
        const bool exitOnComplete) noexcept
    {
        smokeAutoPlay_ = autoPlay;
        smokeExitOnComplete_ = exitOnComplete;
    }

    void RuntimeApplication::SetGraphicsRuntimeEvidence(
        std::string actualBackend,
        std::string capability)
    {
        startupResult_.graphicsBackend = std::move(actualBackend);
        startupResult_.graphicsCapability = std::move(capability);
        ++evidenceRevision_;
    }

    bool RuntimeApplication::StartupFinished() const noexcept
    {
        return startupFinished_;
    }

    const RuntimeBootstrapResult& RuntimeApplication::StartupResult() const noexcept
    {
        return startupResult_;
    }

    bool RuntimeApplication::QuitRequested() const noexcept
    {
        return quitRequested_;
    }

    int RuntimeApplication::ExitCode() const noexcept
    {
        return exitCode_;
    }

    std::uint64_t RuntimeApplication::EvidenceRevision() const noexcept
    {
        return evidenceRevision_;
    }

    void RuntimeApplication::Initialize()
    {
        wi::Application::Initialize();

        // Wicked's Application::Initialize() owns global subsystem startup and
        // initializes Lua synchronously on the main thread. Only now is it safe
        // for Renegade to install its entity-oriented physics namespace.
        if (!bridge::BindPhysicsLua(scenes_.GetScene()))
        {
            wi::backlog::post(
                "Renegade Runtime: renegade.physics could not bind to Wicked Lua after application initialization.",
                wi::backlog::LogLevel::Error);
        }

        infoDisplay.active = true;
        infoDisplay.watermark = false;
        infoDisplay.device_name = true;
        infoDisplay.resolution = true;
        infoDisplay.logical_size = true;
        infoDisplay.colorspace = true;
        infoDisplay.fpsinfo = true;

        renderer_.BindScene(scenes_, startupResult_.project.rootPath);
        renderer_.init(canvas);

        // LP03 compatibility path: an explicitly declared project startup
        // screen still appears before Story Flow. Gate 2 additionally allows
        // Story Flow itself to enter Screen destinations after play.
        if (!startupResult_.startupScreenPath.empty())
        {
            std::string error;
            if (!ConfigureActions(error) || !LoadStartupScreen(error))
            {
                startupResult_.succeeded = false;
                startupResult_.code = RuntimeBootstrapCode::ScreenLoadFailed;
                startupResult_.message =
                    "Could not load project Runtime screen: " + error;
                startupFinished_ = true;
                ++evidenceRevision_;
                return;
            }

            renderer_.Load();
            ActivatePath(&renderer_);
            startupResult_.succeeded = true;
            startupResult_.code = RuntimeBootstrapCode::Success;
            startupResult_.screenLoaded = true;
            startupResult_.screenWasLoaded = true;
            startupResult_.message =
                "Loaded project Runtime screen: " +
                startupResult_.startupScreenPath;
            startupFinished_ = true;
            ++evidenceRevision_;

            if (smokeAutoPlay_)
            {
                const auto* focused = screenController_.FocusedWidget();
                QueueAction(RuntimeActionRequest{
                    bridge::RuntimeScreenPlayAction,
                    focused == nullptr ? std::string{} : focused->id,
                    RuntimeInputSource::Test,
                    1,
                });
            }
            return;
        }

        if (!startupResult_.startupFlowPath.empty())
        {
            startupResult_ = LoadRuntimeProjectFlow(
                scenes_,
                flow_,
                std::move(startupResult_));
            flowStarted_ = startupResult_.succeeded;
        }
        else
        {
            startupResult_ =
                LoadRuntimeProjectScene(scenes_, std::move(startupResult_));
        }
        startupFinished_ = true;
        ++evidenceRevision_;
        if (!startupResult_.succeeded)
        {
            return;
        }

        if (flowStarted_)
        {
            const auto* current = flow_.CurrentNode();
            if (current != nullptr &&
                current->kind == bridge::FlowNodeKind::Screen)
            {
                std::string error;
                if (!LoadCurrentFlowScreen(error))
                {
                    startupResult_.succeeded = false;
                    startupResult_.code = RuntimeBootstrapCode::ScreenLoadFailed;
                    startupResult_.message =
                        "Could not load Story Flow Runtime screen: " + error;
                    ++evidenceRevision_;
                    return;
                }
            }
        }

        // Gate 10 Flow-native standalone smoke: unlike the retained LP03
        // compatibility path above, a modern project has no project-level
        // startup Screen whose Play action can drive smoke completion. The
        // build supplies the exact authored Story Flow outcomes on the command
        // line, so once bootstrap has consumed them the terminal Flow state is
        // already authoritative evidence. Record PASS/FAIL here and exit
        // instead of waiting for a legacy Play action that will never arrive.
        if (smokeAutoPlay_ && flowStarted_)
        {
            const bool complete =
                startupResult_.flowTerminalAction ==
                    bridge::FlowTerminalAction::CompleteGame;
            startupResult_.smokeStatus = complete ? "PASS" : "FAIL";
            startupResult_.smokeQuitReason = complete
                ? "smoke_complete"
                : "flow_not_complete";
            exitCode_ = complete
                ? 0
                : static_cast<int>(RuntimeBootstrapCode::FlowExecutionFailed);
            if (smokeExitOnComplete_)
                quitRequested_ = true;
            ++evidenceRevision_;
        }

        renderer_.Load();
        ActivatePath(&renderer_);
    }

    void RuntimeApplication::Update(const float dt)
    {
        // Wicked refreshes device state and runs the active path GUI from the
        // base application update. Renegade reads that current-frame state
        // afterwards so keyboard and gamepad activation are not one frame stale.
        bridge::RefreshPrecipitationVisual(scenes_.GetScene());
        wi::Application::Update(dt);

        if (screenPresenter_.IsLoaded())
        {
            screenPresenter_.UpdateInput(renderer_, screenController_);
        }
        ProcessPendingActions();
    }

    bool RuntimeApplication::ConfigureActions(std::string& error)
    {
        actions_.Clear();
        if (!actions_.Register(
                bridge::RuntimeScreenPlayAction,
                [this](const RuntimeActionRequest& request)
                {
                    if (flowStarted_)
                    {
                        return RuntimeActionResult{
                            false,
                            RuntimeActionCode::AlreadyStarted,
                            request,
                            "The project Story Flow has already started.",
                        };
                    }
                    if (startupResult_.startupFlowPath.empty())
                    {
                        return RuntimeActionResult{
                            false,
                            RuntimeActionCode::ActionUnavailable,
                            request,
                            "The project does not declare a startup Story Flow.",
                        };
                    }

                    RuntimeBootstrapResult executed = LoadRuntimeProjectFlow(
                        scenes_,
                        flow_,
                        startupResult_);
                    if (!executed.succeeded)
                    {
                        return RuntimeActionResult{
                            false,
                            RuntimeActionCode::FlowStartFailed,
                            request,
                            executed.message,
                        };
                    }

                    startupResult_ = std::move(executed);
                    flowStarted_ = true;
                    return RuntimeActionResult{
                        true,
                        RuntimeActionCode::Success,
                        request,
                        "Runtime action play entered the project Story Flow.",
                    };
                },
                error))
        {
            return false;
        }

        if (!actions_.Register(
                bridge::RuntimeScreenQuitAction,
                [this](const RuntimeActionRequest& request)
                {
                    quitRequested_ = true;
                    return RuntimeActionResult{
                        true,
                        RuntimeActionCode::QuitRequested,
                        request,
                        "Runtime action quit requested normal window shutdown.",
                    };
                },
                error))
        {
            return false;
        }

        error.clear();
        return true;
    }

    bool RuntimeApplication::LoadStartupScreen(std::string& error)
    {
        if (startupResult_.startupFlowPath.empty())
        {
            error = "LP03 requires play to enter a project startup Story Flow.";
            return false;
        }

        bridge::ScreenDocument document;
        if (!bridge::ReadScreenDocument(
                startupResult_.startupScreenPath,
                startupResult_.project.projectId,
                document,
                error))
        {
            return false;
        }
        if (document.envelope.documentId !=
            startupResult_.project.startupScreenId)
        {
            error = "Resolved Runtime screen document ID does not match the project manifest.";
            return false;
        }
        if (!screenController_.Initialize(document, error))
        {
            return false;
        }
        if (!screenPresenter_.Load(
                document,
                startupResult_.project.rootPath,
                renderer_,
                screenController_,
                [this](RuntimeActionRequest request)
                {
                    QueueAction(std::move(request));
                },
                error))
        {
            return false;
        }

        startupResult_.screenDocumentId = document.envelope.documentId;
        const auto* focused = screenController_.FocusedWidget();
        startupResult_.screenFocusedWidgetId =
            focused == nullptr ? std::string{} : focused->id;
        error.clear();
        return true;
    }

    bool RuntimeApplication::LoadCurrentFlowScreen(std::string& error)
    {
        const auto* current = flow_.CurrentNode();
        if (!flowStarted_ || current == nullptr ||
            current->kind != bridge::FlowNodeKind::Screen)
        {
            error = "Story Flow is not currently positioned on a Screen destination.";
            return false;
        }
        if (startupResult_.startupScreenPath.empty() ||
            startupResult_.screenDocumentId != current->screenDocumentId)
        {
            error = "Story Flow Screen destination has not been resolved into Runtime state.";
            return false;
        }

        bridge::ScreenDocument document;
        if (!bridge::ReadScreenDocument(
                startupResult_.startupScreenPath,
                startupResult_.project.projectId,
                document,
                error))
        {
            return false;
        }
        if (document.envelope.documentId != current->screenDocumentId)
        {
            error = "Resolved Runtime screen document ID does not match the current Story Flow node.";
            return false;
        }
        if (!screenController_.Initialize(document, error))
        {
            return false;
        }
        if (!screenPresenter_.Load(
                document,
                startupResult_.project.rootPath,
                renderer_,
                screenController_,
                [this](RuntimeActionRequest request)
                {
                    QueueAction(std::move(request));
                },
                error))
        {
            return false;
        }

        startupResult_.screenDocumentId = document.envelope.documentId;
        const auto* focused = screenController_.FocusedWidget();
        startupResult_.screenFocusedWidgetId =
            focused == nullptr ? std::string{} : focused->id;
        startupResult_.screenLoaded = true;
        startupResult_.screenWasLoaded = true;
        error.clear();
        return true;
    }

    void RuntimeApplication::QueueAction(RuntimeActionRequest request)
    {
        pendingActions_.push_back(std::move(request));
    }

    void RuntimeApplication::ProcessPendingActions()
    {
        if (pendingActions_.empty())
        {
            return;
        }

        std::vector<RuntimeActionRequest> pending;
        pending.swap(pendingActions_);
        for (const auto& request : pending)
        {
            const auto* current = flowStarted_ ? flow_.CurrentNode() : nullptr;
            if (current != nullptr &&
                current->kind == bridge::FlowNodeKind::Screen)
            {
                RuntimeActionResult result;
                result.request = request;

                auto step = flow_.EmitOutcome(request.actionId);
                if (!step.succeeded)
                {
                    result.succeeded = false;
                    result.code = RuntimeActionCode::FlowStartFailed;
                    result.message = step.message;
                    RecordAction(result);
                    continue;
                }

                std::string error;
                if (!flow_.ApplyStep(scenes_, startupResult_, step, error))
                {
                    result.succeeded = false;
                    result.code = RuntimeActionCode::FlowStartFailed;
                    result.message = error;
                    RecordAction(result);
                    continue;
                }

                screenPresenter_.Reset(renderer_);
                startupResult_.screenLoaded = false;

                const auto* destination = flow_.CurrentNode();
                if (destination != nullptr &&
                    destination->kind == bridge::FlowNodeKind::Screen)
                {
                    if (!LoadCurrentFlowScreen(error))
                    {
                        result.succeeded = false;
                        result.code = RuntimeActionCode::FlowStartFailed;
                        result.message = error;
                        RecordAction(result);
                        continue;
                    }
                }

                if (startupResult_.flowTerminalAction ==
                    bridge::FlowTerminalAction::Quit)
                {
                    quitRequested_ = true;
                }

                result.succeeded = true;
                result.code = RuntimeActionCode::Success;
                result.message = "Runtime Screen action advanced Story Flow to '" +
                    startupResult_.flowNodeName + "'.";
                RecordAction(result);
                continue;
            }

            RuntimeActionResult result = actions_.Dispatch(request);
            if (result.succeeded &&
                result.request.actionId == bridge::RuntimeScreenPlayAction)
            {
                screenPresenter_.Reset(renderer_);
                startupResult_.screenLoaded = false;

                const auto* destination = flow_.CurrentNode();
                if (destination != nullptr &&
                    destination->kind == bridge::FlowNodeKind::Screen)
                {
                    std::string error;
                    if (!LoadCurrentFlowScreen(error))
                    {
                        result.succeeded = false;
                        result.code = RuntimeActionCode::FlowStartFailed;
                        result.message = error;
                    }
                }
            }
            RecordAction(result);
        }
    }

    void RuntimeApplication::RecordAction(const RuntimeActionResult& result)
    {
        startupResult_.lastActionId = result.request.actionId;
        startupResult_.lastActionWidgetId = result.request.widgetId;
        startupResult_.lastActionInput =
            RuntimeInputSourceName(result.request.inputSource);
        startupResult_.lastActionCode = RuntimeActionCodeName(result.code);
        startupResult_.lastActionMessage = result.message;
        startupResult_.lastActionSequence = result.request.sequence;
        const auto* focused = screenController_.FocusedWidget();
        startupResult_.screenFocusedWidgetId =
            focused == nullptr ? std::string{} : focused->id;

        if (smokeAutoPlay_ &&
            result.request.actionId == bridge::RuntimeScreenPlayAction)
        {
            const bool complete = result.succeeded &&
                startupResult_.flowTerminalAction ==
                    bridge::FlowTerminalAction::CompleteGame;
            if (complete)
            {
                startupResult_.smokeStatus = "PASS";
                startupResult_.smokeQuitReason = "smoke_complete";
                exitCode_ = 0;
                if (smokeExitOnComplete_)
                    quitRequested_ = true;
            }
            else
            {
                startupResult_.smokeStatus = "FAIL";
                startupResult_.smokeQuitReason = result.succeeded
                    ? "flow_not_complete"
                    : "play_action_failed";
                exitCode_ = static_cast<int>(
                    RuntimeBootstrapCode::FlowExecutionFailed);
                if (smokeExitOnComplete_)
                    quitRequested_ = true;
            }
        }

        ++evidenceRevision_;
    }
}
