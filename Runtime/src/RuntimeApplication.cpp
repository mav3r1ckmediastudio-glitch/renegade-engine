#include "RuntimeApplication.h"

#include "renegade/bridge/ScreenService.h"

#include <utility>

namespace renegade::runtime
{
    void RuntimeRenderPath::BindScene(bridge::SceneService& scenes) noexcept
    {
        scenes_ = &scenes;
        scene = &scenes.GetScene();
    }

    void RuntimeRenderPath::Load()
    {
        setSSREnabled(false);
        setReflectionsEnabled(true);
        setFXAAEnabled(false);

        wi::scene::TransformComponent cameraTransform;
        cameraTransform.Translate(XMFLOAT3(0.0f, 1.5f, -4.0f));
        cameraTransform.UpdateTransform();
        camera->TransformCamera(cameraTransform);

        RenderPath3D::Load();
    }

    void RuntimeApplication::SetBootstrapResult(RuntimeBootstrapResult result)
    {
        startupResult_ = std::move(result);
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

    std::uint64_t RuntimeApplication::EvidenceRevision() const noexcept
    {
        return evidenceRevision_;
    }

    void RuntimeApplication::Initialize()
    {
        wi::Application::Initialize();

        infoDisplay.active = true;
        infoDisplay.watermark = false;
        infoDisplay.device_name = true;
        infoDisplay.resolution = true;
        infoDisplay.logical_size = true;
        infoDisplay.colorspace = true;
        infoDisplay.fpsinfo = true;

        renderer_.BindScene(scenes_);
        renderer_.init(canvas);

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
            startupResult_.message =
                "Loaded project Runtime screen: " +
                startupResult_.startupScreenPath;
            startupFinished_ = true;
            ++evidenceRevision_;
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

        renderer_.Load();
        ActivatePath(&renderer_);
    }

    void RuntimeApplication::Update(const float dt)
    {
        // Wicked refreshes device state and runs the active path GUI from the
        // base application update. Renegade reads that current-frame state
        // afterwards so keyboard and gamepad activation are not one frame stale.
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
                        "Runtime action play entered the existing LP02 Story Flow.",
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
            const RuntimeActionResult result = actions_.Dispatch(request);
            RecordAction(result);
            if (result.succeeded &&
                result.request.actionId == bridge::RuntimeScreenPlayAction)
            {
                screenPresenter_.Reset(renderer_);
                startupResult_.screenLoaded = false;
            }
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
        ++evidenceRevision_;
    }
}
