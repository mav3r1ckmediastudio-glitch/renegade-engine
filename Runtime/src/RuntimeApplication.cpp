#include "../../EngineBridge/src/PlaceableWaterfallAuthoring.h"
#include <cstdlib>
#include <fstream>
#include <cmath>
#include <filesystem>
#include "RuntimeApplication.h"
#include "renegade/bridge/ScreenLiveBinding.h"
#include "renegade/bridge/ScreenThemeResolver.h"
#include "RuntimeProjectileAim.h"
#include "RuntimeGrenadeExplosion.h"
#include "RuntimeThrownHeldVisual.h"
#include "renegade/bridge/LaunchSocketService.h"

#include "renegade/bridge/PhysicsLuaService.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/PrecipitationService.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <utility>

namespace renegade::runtime
{
    // Held particles share the native first-person depth range. World particles
    // cannot overlap foreground arms in the ordinary world-depth particle pass.
    // Airborne effects remain on their ordinary native world path.
    void RuntimeRenderPath::RenderTransparents(wi::graphics::CommandList cmd) const
    {
        RenderWaterfallTransparents(cmd);
        if(!scene->emitters.Contains(heldFireballParticles_[0]))return;
        auto* device=wi::graphics::GetDevice();
        using namespace wi::graphics;
        RenderPassImage rp[3];uint32_t count=0;
        rp[count++]=RenderPassImage::RenderTarget(&rtMain_render,RenderPassImage::LoadOp::LOAD);
        if(getMSAASampleCount()>1)rp[count++]=RenderPassImage::Resolve(&rtMain);
        rp[count++]=RenderPassImage::DepthStencil(&depthBuffer_Main,RenderPassImage::LoadOp::LOAD,
            RenderPassImage::StoreOp::STORE,ResourceState::DEPTHSTENCIL,ResourceState::DEPTHSTENCIL,ResourceState::DEPTHSTENCIL);
        device->RenderPassBegin(rp,count,cmd);
        Viewport vp;vp.width=float(depthBuffer_Main.desc.width);vp.height=float(depthBuffer_Main.desc.height);
        vp.min_depth=.99f;vp.max_depth=1;
        device->BindViewports(1,&vp,cmd);
        wi::renderer::BindCommonResources(cmd);
        auto heldCamera=*camera;heldCamera.texture_depth_index=-1;
        wi::renderer::BindCameraCB(heldCamera,camera_previous,camera_reflection,cmd);
        for(const auto entity:heldFireballParticles_) {
            const auto* emitter=scene->emitters.GetComponent(entity);
            const auto* material=scene->materials.GetComponent(entity);
            if(emitter && material)emitter->Draw(*material,cmd);
        }
        wi::renderer::BindCameraCB(*camera,camera_previous,camera_reflection,cmd);
        device->RenderPassEnd(cmd);
        vp.min_depth=0;device->BindViewports(1,&vp,cmd);
        wi::renderer::Postprocess_Downsample4x(rtMain,rtSceneCopy,cmd);
    }

    void RuntimeRenderPath::BindScene(
        bridge::SceneService& scenes,
        std::string projectRoot) noexcept
    {
        scenes_ = &scenes;
        projectRoot_ = std::move(projectRoot);
        scene = &scenes.GetScene();
        renderSettingsInitialized_ = false;
        renderSettingsSceneRevision_ = 0;
    }

    void RuntimeRenderPath::SetPaused(const bool paused) noexcept
    {
        paused_ = paused;
    }

    void RuntimeRenderPath::SetInterfaceDeltaTime(const float dt) noexcept
    {
        interfaceDeltaTime_=std::isfinite(dt) && dt>0?dt:0;
    }

    void RuntimeRenderPath::SetInteractionPrompt(std::string prompt) noexcept
    {
        interactionPrompt_ = std::move(prompt);
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
        renderSettingsSceneRevision_ = scenes_->Revision();
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
        // render path. SceneService exposes that lifecycle explicitly, so a
        // normal frame performs only an O(1) revision check and never repeats
        // LUT filesystem validation or full render-state capture.
        if (scenes_ != nullptr &&
            (!renderSettingsInitialized_ ||
                renderSettingsSceneRevision_ != scenes_->Revision()))
        {
            SyncRenderSettings(true);
        }
        if (!paused_ && std::isfinite(dt) && dt > 0)
            hitConfirmSeconds_ = std::max(0.0f, hitConfirmSeconds_ - dt);
        const auto flowStart=std::chrono::steady_clock::now();
        if(scenes_)waterfallRenderer_.Prepare(*scene,scenes_->Revision(),paused_?0.f:dt,false);
        const auto afterWaterfall=std::chrono::steady_clock::now();
        if(scenes_)riverFlow_.Prepare(*scene,scenes_->Revision(),paused_?0.f:dt);
        const auto afterRiver=std::chrono::steady_clock::now();
        if(const char* screenshot=std::getenv("RENEGADE_G13E14_PLAYER_CAPTURE")){
            static int frame=0;
            if(++frame==60){
                const bool saved=wi::helper::saveTextureToFile(
                    GetRenderResult3D(),screenshot);
                std::ofstream result(std::string(screenshot)+".status.txt");
                result<<"G13E14_ACTUAL_PLAYER_CAMERA_CAPTURE success="<<saved
                      <<" waterfall_active="<<waterfallRenderer_.ActiveCount()
                      <<" river_active="<<riverFlow_.ActiveCount()
                      <<" emitters="<<scene->emitters.GetCount()<<"\n";
            }
        }
        riverFlowPrepareMilliseconds_=std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-flowStart).count();
        // Explicit diagnostic mode only; ordinary player/camera ownership is unchanged.
        if(const char* proof=std::getenv("RENEGADE_WATERFALL_RUNTIME_PROOF")){
            static uint64_t frame=0;++frame;
            std::vector<bridge::placeable_waterfall::AuthoredWaterfall> records;std::string error;
            if(bridge::placeable_waterfall::EnumerateWaterfalls(*scene,records,error)&&!records.empty()){
                auto a=records.front().settings.crest,b=records.front().settings.landing;
                auto target=XMVectorSet((a.x+b.x)*.5f,(a.y+b.y)*.5f,(a.z+b.z)*.5f,1);
                // Explicit opt-in only for the isolated G13E13 moving-camera
                // benchmark. The previously approved G13C camera and actual
                // gameplay camera path are unchanged.
                float cameraX=20.f,cameraY=2.f,cameraZ=-13.f;
                if(std::getenv("RENEGADE_G13E13_CAMERA_MOTION")){
                    const float phase=float(frame)*.014f;
                    cameraX+=3.5f*std::sin(phase);
                    cameraY+=1.0f*std::sin(phase*.6f);
                    cameraZ+=2.5f*(std::cos(phase)-1.f);
                }
                wi::scene::TransformComponent pose;pose.MatrixTransform(XMMatrixInverse(nullptr,XMMatrixLookAtLH(target+XMVectorSet(cameraX,cameraY,cameraZ,0),target,XMVectorSet(0,1,0,0))));pose.UpdateTransform();camera->TransformCamera(pose);camera->UpdateCamera();
                if(frame==240 && !std::getenv("RENEGADE_G13E13_NO_SCREENSHOT")){
                    auto capture=std::filesystem::path(proof);capture.replace_extension(".png");bool saved=wi::helper::saveTextureToFile(GetRenderResult3D(),capture.generic_string());
                    std::ofstream log(proof);log<<"RUNTIME_WATERFALL_PROOF authored="<<records.size()<<" active="<<waterfallRenderer_.ActiveCount()<<" emitters="<<scene->emitters.GetCount()<<" capture="<<saved<<" status="<<waterfallRenderer_.Status()<<std::endl;
                    for(const auto& record:records)log<<"id="<<record.id<<" width="<<record.settings.width<<" impact="<<record.settings.impactSpray<<std::endl;
                    for(size_t i=0;i<scene->materials.GetCount();++i){auto& m=scene->materials[i];auto* md=scene->metadatas.GetComponent(scene->materials.GetEntity(i));if(md&&md->string_values.has("renegade.waterfall.generated.v1"))continue;for(int slot=0;slot<wi::scene::MaterialComponent::TEXTURESLOT_COUNT;++slot)if(!m.textures[slot].name.empty())log<<"texture="<<m.textures[slot].name<<" valid="<<m.textures[slot].resource.IsValid()<<std::endl;}
                }
            }
        }
        const auto nativeBegin=std::chrono::steady_clock::now();
        {auto riverBindings=riverFlow_.Bind();RenderPath3D::Update(dt);}
        if(const char* trace=std::getenv("RENEGADE_G13E14_STAGE_TRACE")){
            static std::uint64_t frame=0;
            if(++frame<=20 || frame%10==0){
                const auto end=std::chrono::steady_clock::now();
                auto ms=[](auto a,auto b){
                    return std::chrono::duration<double,std::milli>(b-a).count();};
                std::ofstream out(trace,std::ios::app);
                out<<"RENDER_UPDATE frame="<<frame
                   <<" waterfall_ms="<<ms(flowStart,afterWaterfall)
                   <<" river_ms="<<ms(afterWaterfall,afterRiver)
                   <<" native_update_ms="<<ms(nativeBegin,end)
                   <<" terrain_chunks="<<(scene->terrains.GetCount()?
                        scene->terrains[0].chunks.size():0);
                if(scene->splines.GetCount()){
                    bridge::RiverFlowDiagnostics diag;
                    std::string why;
                    const auto river=scene->splines.GetEntity(0);
                    if(riverFlow_.Diagnostics(river,diag,why))
                        out<<" river_rebuilds="<<diag.rebuilds
                           <<" last_rebuild_ms="<<diag.lastBuildMilliseconds;
                    else
                        out<<" river_error="<<why;
                }
                out<<"\n";
            }
        }
        // Wicked disables native button input at dt=0. World pause must not
        // disable the menu that owns that pause. The base zero-time GUI pass
        // performs no input; advance the native GUI once with real UI time.
        if(paused_ && interfaceDeltaTime_>0) GetGUI().Update(*this,interfaceDeltaTime_);
    }

    void RuntimeRenderPath::Render() const
    {
        auto riverBindings=riverFlow_.Bind();
        riverFlow_.Dispatch();
        wi::RenderPath3D::Render();
    }

    void RuntimeRenderPath::WriteRiverFlowDiagnostics(bridge::DiagnosticService& diagnostics) const
    {
        std::uint64_t rebuilds=0;double lastBuild=0;
        if(scene)for(size_t n=0;n<scene->splines.GetCount();++n)
        {
            bridge::RiverFlowDiagnostics state;std::string error;
            if(riverFlow_.Diagnostics(scene->splines.GetEntity(n),state,error))
            {
                rebuilds+=state.rebuilds;
                lastBuild=std::max(lastBuild,state.lastBuildMilliseconds);
            }
        }
        diagnostics.SetState("waterfall",{{"active",static_cast<std::uint64_t>(waterfallRenderer_.ActiveCount())},{"status",waterfallRenderer_.Status()}});
        diagnostics.SetState("river_flow",{
            {"active",static_cast<std::uint64_t>(riverFlow_.ActiveCount())},
            {"submitted_frames",static_cast<std::uint64_t>(wi::graphics::GetDevice()->GetFrameCount())},
            {"rebuilds",rebuilds},
            {"prepare_cpu_ms",std::to_string(riverFlowPrepareMilliseconds_)},
            {"last_build_ms",std::to_string(lastBuild)}});
    }

    void RuntimeRenderPath::PreRender()
    {
        riverUnderwater_.Update(*this);
        auto riverBindings=riverFlow_.Bind();
        wi::RenderPath3D::PreRender();
    }

    void RuntimeRenderPath::Compose(
        const wi::graphics::CommandList cmd) const
    {
        RenderPath3D::Compose(cmd);
        const float width = std::max(1.0f, GetLogicalWidth());
        const float height = std::max(1.0f, GetLogicalHeight());
        if (!paused_ && projectileAim_) {
            wi::font::Params aim(width*0.5f,height*0.5f,18,
                wi::font::WIFALIGN_CENTER,wi::font::WIFALIGN_CENTER,
                wi::Color(245,245,245,220),wi::Color(0,0,0,160));
            wi::font::Draw("+",aim,cmd);
        }
        if (!paused_ && hitConfirmSeconds_ > 0) {
            wi::font::Params confirm(width*0.5f,height*0.5f,24,
                wi::font::WIFALIGN_CENTER,wi::font::WIFALIGN_CENTER,
                wi::Color(255,245,245,255),wi::Color(0,0,0,200));
            wi::font::Draw("x",confirm,cmd);
        }
        for (const auto& contact : projectileContacts_) {
            wi::font::Params marker(contact.x,contact.y,24,
                wi::font::WIFALIGN_CENTER,wi::font::WIFALIGN_CENTER,
                wi::Color(255,105,25,255),wi::Color(0,0,0,200));
            wi::font::Draw("x",marker,cmd);
        }
        if(!paused_ && !meleePrompt_.empty()) {
            wi::font::Params hint(width*0.5f,height*0.55f,18,
                wi::font::WIFALIGN_CENTER,wi::font::WIFALIGN_CENTER,
                wi::Color(245,245,245,255),wi::Color(0,0,0,180));
            wi::font::Draw(meleePrompt_,hint,cmd);
        }
        if (!paused_)
        {
            if (!interactionPrompt_.empty())
            {
                const float panelWidth = std::max(
                    1.0f,
                    std::min(560.0f, width - 32.0f));
                wi::image::Params panel(
                    width * 0.5f - panelWidth * 0.5f,
                    height - 104.0f,
                    panelWidth,
                    44.0f,
                    wi::Color(4, 8, 10, 210));
                panel.blendFlag = wi::enums::BLENDMODE_ALPHA;
                wi::image::Draw(nullptr, panel, cmd);

                wi::font::Params prompt(
                    width * 0.5f,
                    height - 82.0f,
                    16,
                    wi::font::WIFALIGN_CENTER,
                    wi::font::WIFALIGN_CENTER,
                    wi::Color(245, 245, 245, 255),
                    wi::Color::Transparent());
                prompt.bolden = 0.08f;
                wi::font::Draw(interactionPrompt_, prompt, cmd);
            }
            return;
        }

        if(!pauseOverlayVisible_) return;
        wi::image::Params shade(0.0f, 0.0f, width, height,
            wi::Color(0, 0, 0, 145));
        shade.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, shade, cmd);

        wi::font::Params title(
            width * 0.5f,
            height * 0.5f - 22.0f,
            28,
            wi::font::WIFALIGN_CENTER,
            wi::font::WIFALIGN_CENTER,
            wi::Color(245, 245, 245, 255),
            wi::Color::Transparent());
        title.bolden = 0.12f;
        wi::font::Draw("PAUSED", title, cmd);

        wi::font::Params hint(
            width * 0.5f,
            height * 0.5f + 24.0f,
            14,
            wi::font::WIFALIGN_CENTER,
            wi::font::WIFALIGN_CENTER,
            wi::Color(220, 220, 220, 255),
            wi::Color::Transparent());
        wi::font::Draw("ESC RESUME   //   R RESET", hint, cmd);
    }

    void RuntimeApplication::SetBootstrapResult(RuntimeBootstrapResult result)
    {
        initialBootstrapResult_ = result;
        startupResult_ = std::move(result);
    }

    void RuntimeApplication::SetSmokeOptions(
        const bool autoPlay,
        const bool exitOnComplete) noexcept
    {
        smokeAutoPlay_ = autoPlay;
        smokeExitOnComplete_ = exitOnComplete;
    }

    bool RuntimeApplication::ConfigureUiAccessibilityPreferences(
        std::string path,std::string& error)
    {
        RuntimeUiAccessibilityPreferences loaded;
        if(!RuntimeUiAccessibilityPreferences::Load(
            std::filesystem::u8path(path),loaded,error))return false;
        uiAccessibilityPath_=std::move(path);
        uiAccessibility_=loaded;
        screenPresenter_->SetReducedMotion(uiAccessibility_.reducedMotion);
        screenPresenter_->SetAccessibilitySettings(uiAccessibility_.presentation);
        if(hudSession_.IsLoaded())
            hudSession_.Presenter()->SetReducedMotion(uiAccessibility_.reducedMotion);
        error.clear();return true;
    }

    bool RuntimeApplication::ChangeUiAccessibility(const RuntimeUiAccessibilityPreferences& settings,std::string& error)
    {
        if(uiAccessibilityPath_.empty())
        {error="Player UI accessibility preferences are unavailable on this computer.";return false;}
        if(!settings.Save(std::filesystem::u8path(uiAccessibilityPath_),error))return false;
        uiAccessibility_=settings;
        screenPresenter_->SetReducedMotion(settings.reducedMotion);
        screenPresenter_->SetAccessibilitySettings(settings.presentation);
        if(hudSession_.IsLoaded())
        {
            hudSession_.Presenter()->SetReducedMotion(settings.reducedMotion);
            hudSession_.Presenter()->SetAccessibilitySettings(settings.presentation);
        }
        error.clear();return true;
    }

    bool RuntimeApplication::ChangeReducedMotion(bool enabled,std::string& error)
    {
        if(uiAccessibilityPath_.empty())
        {error="Player UI accessibility preferences are unavailable on this computer.";return false;}
        if(uiAccessibility_.reducedMotion==enabled){error.clear();return true;}
        auto updated=uiAccessibility_;
        updated.reducedMotion=enabled;
        return ChangeUiAccessibility(updated,error);
    }

    void RuntimeApplication::SetGraphicsRuntimeEvidence(
        std::string actualBackend,
        std::string capability)
    {
        startupResult_.graphicsBackend = actualBackend;
        startupResult_.graphicsCapability = capability;
        initialBootstrapResult_.graphicsBackend = std::move(actualBackend);
        initialBootstrapResult_.graphicsCapability = std::move(capability);
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

    void RuntimeApplication::ShutdownForProcessExit() noexcept
    {
        hudSession_.Reset(renderer_);
        diagnosticService_.StopLocalEndpoint();
        StopCreatorScripts();
        playerEquipment_ = {};
        pendingProjectileShots_.clear();
        impactAudio_.StopVoices();
        heldFireball_.Reset(scenes_.GetScene());
        channelVisual_.Reset(scenes_.GetScene());channel_.Reset();channelScorchTrail_.Reset();lightningStorm_.Reset(scenes_.GetScene());lightningColumnReview_.Reset(scenes_.GetScene());lightningStormCastLatch_=false;
        projectileVisuals_.Reset(scenes_.GetScene());
        projectiles_.Reset();
        burning_.Reset(scenes_.GetScene());
        ResetRuntimePlayerViewAnimations(
            scenes_.GetScene(), playerViewAnimation_);
        DespawnRuntimePlayerViewRig(scenes_.GetScene(), playerViewRig_);
        creatorScripts_.Shutdown();
        scriptSceneRevision_ = 0;
        reportedScriptDiagnostics_ = 0;
    }

    void RuntimeApplication::Initialize()
    {
        uiBindingCpuTiming_.Reset();
        wi::Application::Initialize();
        diagnosticService_.Record(bridge::DiagnosticSeverity::Info,
            "runtime", "runtime.initialized", "Runtime initialized");
        diagnosticService_.Identify("runtime");
        diagnosticService_.StartLocalEndpoint(38742);
        creatorScripts_.SetGameplayEventObserver([this](const bridge::GameplayEvent& event)
        {OnGovernedHudGameplayEvent(event);});

        // This is retained for existing engine-internal/unsafe Wicked Lua
        // integrations. Creator S3 scripts never borrow that global VM: their
        // RuntimeScriptRuntime owns a separate governed lua_State.
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

        std::string inputError;
        if (!LoadGameplayInput(inputError))
        {
            startupResult_.succeeded = false;
            startupResult_.code = RuntimeBootstrapCode::ProjectRejected;
            startupResult_.message =
                "Could not load project gameplay input map: " + inputError;
            startupFinished_ = true;
            ++evidenceRevision_;
            return;
        }

        if(!bridge::ReadScreenPerformanceBudget(startupResult_.project.rootPath,
            startupResult_.project.projectId,uiBudget_,uiBudgetDiagnostic_))
        {
            uiBudget_={};
            wi::backlog::post("UI performance settings unavailable; using advisory defaults: "+uiBudgetDiagnostic_,
                wi::backlog::LogLevel::Warning);
        }
        (void)uiBindingCpuTiming_.SetBudgetUs(uiBudget_.bindingCpuUs);
        if(!LoadGameplayAttributes(inputError))
        {
            startupResult_.succeeded=false;startupResult_.code=RuntimeBootstrapCode::ProjectRejected;
            startupResult_.message="Could not load gameplay attribute module: "+inputError;
            startupFinished_=true;++evidenceRevision_;return;
        }

        renderer_.BindScene(scenes_, startupResult_.project.rootPath);
        renderer_.init(canvas);

        // Governed Runtime actions must exist for BOTH boot shapes. A direct
        // StoryFlow/Level start may still host a clickable HUD or Screen; the
        // former startup-Screen-only registration silently made such actions
        // unknown in native Runtime.
        {
            std::string error;
            if (!ConfigureActions(error))
            {
                startupResult_.succeeded=false;
                startupResult_.code=RuntimeBootstrapCode::ScreenLoadFailed;
                startupResult_.message="Could not initialize Runtime actions: "+error;
                startupFinished_=true;
                ++evidenceRevision_;
                return;
            }
        }

        // LP03 compatibility path: an explicitly declared project startup
        // screen still appears before Story Flow. Gate 2 additionally allows
        // Story Flow itself to enter Screen destinations after play.
        if (!startupResult_.startupScreenPath.empty())
        {
            std::string error;
            if (!LoadStartupScreen(error))
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
                const auto* focused = screenController_->FocusedWidget();
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
        SyncLevelHud();
        RefreshUiComposition();
        UpdateLiveDiagnostics();
        SyncAudioForScene();
        bridge::RefreshPrecipitationVisual(scenes_.GetScene());

        const bool gameplayInputAllowed = !uiComposition_.Resolve().gameplayInputBlocked &&
            !renderer_.GetGUI().IsTyping();
        const bool gameplayFrame = !scenes_.CurrentPath().empty();
        // G13E14 one-level verification uses the exact regular controller and
        // Jolt/scene movement path; synthetic input is gated to the developer
        // benchmark environment, never active for real players.
        const char* g13e14Trace=std::getenv("RENEGADE_G13E14_PLAYER_TRACE");
        static std::uint64_t g13e14Frame=0;
        if(g13e14Trace && gameplayFrame) ++g13e14Frame;
        if (gameplayFrame)
        {
            // Feed current input into the one authoritative Player before
            // Wicked advances physics. The View Rig is parented to that Player,
            // so the normal physics -> hierarchy -> object update carries the
            // first-person presentation to the same post-physics position.
            gameplayInput_ = gameplayInputAllowed
                ? bridge::CaptureGameplayInput(inputMap_, dt) : bridge::GameplayInputFrame{};
            const auto& gameplayInput = gameplayInput_;
            creatorScripts_.SetGameplayState(&player_, &gameplayInput_);
            if (gameplayInput.pausePressed)
            {
                const auto* current=flowStarted_?flow_.CurrentNode():nullptr;
                bool authoredPause=false;
                if(current&&current->kind==bridge::FlowNodeKind::Level)
                    for(const auto& route:flow_.Document().routes)
                        authoredPause=authoredPause||(route.sourceNodeId==current->id&&route.outcome=="ui.pause");
                if(authoredPause)pendingActions_.push_back({"ui.pause",{},RuntimeInputSource::Gameplay,
                    startupResult_.lastActionSequence+1});
                else SetPaused(!paused_); // Retain legacy projects' manual pause.
            }

            if (gameplayInput.resetPressed)
            {
                std::string error;
                if (!ResetPlaySession(error))
                {
                    wi::backlog::post(
                        "Renegade Runtime: play-session reset failed: " + error,
                        wi::backlog::LogLevel::Error);
                }
            }

            if (gameplayFrame)
            {
                SyncPlayerForScene();
                if (player_.IsSpawned())
                {
                    if (!paused_)
                    {
                        auto playerInput=gameplayInput.player;
                        if(g13e14Trace){
                            const auto phase=g13e14Frame%960;
                            playerInput.moveForward=phase<320?0.75f:(phase<640?-0.65f:0.25f);
                            playerInput.moveRight=phase<320?0.20f:(phase<640?0.35f:-0.40f);
                            playerInput.lookYaw=0.0f;
                            playerInput.lookPitch=0.0f;
                            playerInput.sprintDown=false;
                        }
                        if(playerViewAnimation_.handLayers.directional && gameplayInput.fireDown)
                            playerInput.lookYaw=playerInput.lookPitch=0;
                        (void)bridge::UpdateRuntimePlayer(
                            scenes_.GetScene(),
                            player_,
                            playerInput,
                            playerSettings_);
                    }
                    (void)PoseRuntimePlayerViewRig(
                        scenes_.GetScene(),
                        playerViewRig_,
                        player_.yaw,
                        player_.pitch);
                    UpdateRuntimePlayerViewRigPresentation(
                        scenes_.GetScene(),
                        playerViewRig_,
                        gameplayInput.player,
                        paused_ ? 0.0f : dt);
                    auto* body = scenes_.GetScene().rigidbodies.GetComponent(player_.entity);
                    const bool grounded = body == nullptr || wi::physics::IsCharacterGroundSupported(*body);
                    const bool presentationBusy = playerViewAnimation_.oneShotPlaying ||
                        (playerViewAnimation_.equipped && (playerViewAnimation_.jumpCycleActive ||
                            playerViewAnimation_.takeoffPending || playerViewAnimation_.landingPending ||
                            (playerViewAnimation_.groundKnown && playerViewAnimation_.wasGrounded != grounded)));
                    const auto& melee=playerViewAnimation_.handLayers;
                    const auto* strike=scenes_.GetScene().animations.GetComponent(melee.right);
                    const bool chainWindow=melee.directional && melee.attacking && strike &&
                        strike->end-strike->start-melee.rightTime<=melee.chainWindowSeconds;
                    const auto equipmentInput =
                        playerEquipment_.RouteStaged(gameplayInput, playerViewAnimation_.equipped,
                            presentationBusy, paused_ ? 0.0f : dt, playerViewAnimation_.aiming,
                            !playerViewAnimation_.clips[PlayerViewActionIndex(PlayerViewAction::Charge)].empty() &&
                            !playerViewAnimation_.clips[PlayerViewActionIndex(PlayerViewAction::Release)].empty(),
                            playerViewAnimation_.handLayers.enabled &&
                            !playerEquipment_.offHand.equipment.assetId.empty() &&
                            playerEquipment_.offHand.equipment.presentationAssetId == playerViewRig_.viewModelAssetId,
                            melee.directional, chainWindow, melee.direction, melee.fullChargeSeconds, melee.queuedReleaseSeconds,
                            playerViewAnimation_.loadedShells < playerViewAnimation_.firearm.capacity);
                    if(playerEquipment_.chainedChargeStarted) {
                        auto& hand=playerViewAnimation_.handLayers;
                        hand.direction=playerEquipment_.chainedDirection;
                        hand.chargePhase=1;hand.chargeSeconds=std::max(0.0f,playerEquipment_.chainedSeconds-(playerEquipment_.chargePresentation?dt:0.0f));
                        hand.rightTime=0;hand.gestureX=hand.gestureY=0;
                    }
                    if(!paused_ && playerEquipment_.chainedReleasedCharge)
                        playerViewAnimation_.handLayers.chargeSeconds=std::max(0.0f,playerEquipment_.chainedSeconds-(playerEquipment_.chargePresentation?dt:0.0f));
                    // Natural Lightning is an instant placed AoE, not a sustained
                    // channel: after the storm starts, complete casting/recovery
                    // without waiting for physical LMB release.
                    const bool naturalStormEquipped=playerEquipment_.StormBinding() &&
                        playerEquipment_.primary.equipment.name=="Lightning Storm - Natural Playground";
                    const auto stormAnimation=ResolveStormOneShotAnimation(
                        naturalStormEquipped,lightningStormCastLatch_,playerViewAnimation_.spellPhase,
                        playerEquipment_.chargePresentation,playerEquipment_.releasePresentation);
                    UpdateRuntimePlayerViewAnimations(
                        scenes_.GetScene(),
                        playerViewAnimation_,
                        playerViewRig_.action,
                        paused_ ? 0.0f : dt,
                        !paused_ && equipmentInput.firePressed,
                        !paused_ && equipmentInput.reloadPressed,
                        equipmentInput.aimDown,
                        !paused_ && equipmentInput.toggleEquipmentPressed,
                        grounded, stormAnimation.chargeHeld, stormAnimation.releasePressed,
                        playerEquipment_.offHandBlockPresentation, gameplayInput.player.lookYaw, gameplayInput.player.lookPitch, equipmentInput.cancelEquipmentPressed,
                        &playerEquipment_.reloadStageRequest);
                    // Re-arm only on a real mouse-up edge, after the native
                    // animation has consumed this frame's input.
                    if(!paused_ && !gameplayInput.fireDown && naturalStormEquipped)
                        lightningStormCastLatch_=false;
                    if(!paused_)playerEquipment_.AfterNativePresentation(playerViewAnimation_);
                    for (auto& request : playerEquipment_.TakeProjectileRequests()) {
                        // Native firearm acceptance includes ammunition, cooldown,
                        // equipped state and successfully resolved paired clips.
                        const bool accepted = request.action == bridge::EquipmentAction::PrimaryUse
                            ? playerViewAnimation_.shotAccepted
                            : request.action == bridge::EquipmentAction::Release &&
                              (playerViewAnimation_.spellReleaseAccepted || (playerViewAnimation_.pairedAssembly && playerViewAnimation_.oneShotPlaying)) &&
                              playerViewAnimation_.activeAction == PlayerViewAction::Release;
                        if (!paused_ && accepted)
                            playerEquipment_.ScheduleProjectile(std::move(request),playerViewAnimation_.activeClip);
                    }
                    if(playerEquipment_.releasePresentation)playerEquipment_.chainedReleasedCharge=false;
                    const auto& hand=playerViewAnimation_.handLayers;
                    const char* directions[]={"LEFT","RIGHT","DOWN","STAB"};
                    renderer_.SetMeleePrompt(hand.directional?
                        std::string(directions[hand.direction])+"  "+(playerEquipment_.queuedStrike.pending?
                            std::string("NEXT ")+directions[playerEquipment_.queuedStrike.direction]+" "+
                                (playerEquipment_.queuedStrike.released?"QUEUED":"CHARGING"):
                            hand.chargePhase?
                            "CHARGE "+std::to_string(static_cast<int>(hand.chargeSeconds/hand.fullChargeSeconds*100))+"%":
                            hand.attacking?(playerEquipment_.chainInputWindow?"STRIKE; PREPARE NEXT":"STRIKE"):"HOLD LMB + MOVE; RELEASE TO STRIKE"):"");
                }
            }
        }

        // Transient visual creation/removal must precede Wicked's GPU instance
        // upload. Erasing a sheet afterwards can move a droplet into its CPU slot
        // while the GPU still holds the sheet's metre-scale transform.
        // Sample the current completed scene for aiming, then refresh the camera
        // again after physics below for the rendered view.
        if (gameplayFrame)
        {
            if (player_.IsSpawned() && renderer_.camera != nullptr)
                bridge::ApplyRuntimePlayerCamera(scenes_.GetScene(), player_,
                    *renderer_.camera, playerSettings_);
            const CombatEventEmitter burnEvents=[this](bridge::GameplayEvent event,std::string& error) {
                diagnosticService_.Record(bridge::DiagnosticSeverity::Info,"runtime.combat",event.name,event.payload);
                return creatorScripts_.EnqueueGameplayEvent(std::move(event),error);
            };
            burning_.Update(scenes_.GetScene(),characterAiState_,characterPerceptionState_,combatState_,paused_?0.f:dt,burnEvents);
            UpdatePlayerProjectiles(paused_ ? 0.0f : dt);
        }

        // Wicked owns the actual Jolt step, hierarchy propagation and GPU
        // instance update. A paused session still refreshes the device while
        // advancing the active 3D path with zero simulation time.
        // Publish one coherent gameplay-boundary UI snapshot before native GUI
        // input. A button disabled/hidden by this snapshot cannot activate using
        // the previous frame's presentation state.
        UpdateGameplayAttributes(dt);
        const auto bindingStart=std::chrono::steady_clock::now();
        UpdateUiBindings();
        if(screenPresenter_->IsLoaded()||hudSession_.IsLoaded())
        {
            const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now()-bindingStart).count();
            uiBindingCpuTiming_.Observe(static_cast<std::uint64_t>(std::max<std::int64_t>(0,elapsed)));
        }
        RefreshUiComposition();
        renderer_.SetInterfaceDeltaTime(dt);
        wi::Application::Update(paused_ ? 0.0f : dt);
        // Current-pose marks must reach decal visibility/render upload after
        // native skinning, while transient entity mutation remains before it.
        if (gameplayFrame && g13e14Trace && player_.IsSpawned()) {
            // Post-native-physics, not a fabricated camera path: record actual
            // player transform/trajectory from the ordinary runtime controller.
            auto* transform=scenes_.GetScene().transforms.GetComponent(player_.entity);
            if(transform){
                const XMFLOAT3 pos=transform->GetPosition();
                static bool tracking=false;
                static XMFLOAT3 original{},previous{};
                static double distance=0;
                if(!tracking){
                    original=previous=pos;tracking=true;
                    std::ofstream out(g13e14Trace,std::ios::trunc);
                    out<<"frame,x,y,z,horizontal_distance_from_start_m,travelled_m\n";
                }
                const double dx=pos.x-previous.x,dz=pos.z-previous.z;
                distance+=std::sqrt(dx*dx+dz*dz);
                previous=pos;
                if(g13e14Frame%120==0){
                    const double fx=pos.x-original.x,fz=pos.z-original.z;
                    std::ofstream out(g13e14Trace,std::ios::app);
                    out<<g13e14Frame<<","<<pos.x<<","<<pos.y<<","<<pos.z<<","
                       <<std::sqrt(fx*fx+fz*fz)<<","<<distance<<"\n";
                }
            }
        }
        if (gameplayFrame) {
            heldFireball_.RefreshEmissionFrame(scenes_.GetScene(),renderer_.camera?&renderer_.camera->Eye:nullptr);
            renderer_.SetHeldFireballParticles(heldFireball_.particles);
            projectileVisuals_.RefreshImpactMarkPose(scenes_.GetScene());
            burning_.RefreshAppearance(scenes_.GetScene());
        }


        if (gameplayFrame)
        {
            SyncCreatorScriptsForScene();
            if (player_.IsSpawned())
            {
                if (renderer_.camera != nullptr)
                {
                    // Camera samples the same post-physics Player position that
                    // the parented View Rig inherited during Scene::Update.
                    bridge::ApplyRuntimePlayerCamera(
                        scenes_.GetScene(),
                        player_,
                        *renderer_.camera,
                        playerSettings_);
                }
                wi::input::HidePointer(!paused_);
            }
            else
            {
                wi::input::HidePointer(false);
            }

            if (!paused_)
                creatorScripts_.Update(dt);
            renderer_.SetInteractionPrompt(creatorScripts_.CurrentPrompt());
            ReportCreatorScriptDiagnostics();
        }
        else
        {
            impactAudio_.StopVoices();
            if (!projectiles_.simulation.Records().empty() || !projectiles_.markers.empty() ||
                !projectileVisuals_.roots.empty() || projectileVisuals_.EffectCount()>0) {
                impactAudio_.StopVoices();
                heldFireball_.Reset(scenes_.GetScene());
        channelVisual_.Reset(scenes_.GetScene());channel_.Reset();channelScorchTrail_.Reset();lightningStorm_.Reset(scenes_.GetScene());lightningColumnReview_.Reset(scenes_.GetScene());lightningStormCastLatch_=false;
        projectileVisuals_.Reset(scenes_.GetScene());
                projectiles_.Reset();
        burning_.Reset(scenes_.GetScene());
            }
            channelVisual_.Reset(scenes_.GetScene());channel_.Reset();channelScorchTrail_.Reset();lightningStorm_.Reset(scenes_.GetScene());lightningColumnReview_.Reset(scenes_.GetScene());lightningStormCastLatch_=false;
            pendingProjectileShots_.clear();
            playerEquipment_.scheduledProjectiles.clear();
            renderer_.SetProjectileAim(false);
            renderer_.SetProjectileContacts({});
            renderer_.ClearHitConfirmation();
            renderer_.SetInteractionPrompt({});
            StopCreatorScripts();
            wi::input::HidePointer(false);

        }

        const auto inputComposition=uiComposition_.Resolve();
        if (screenPresenter_->IsLoaded())
            screenPresenter_->UpdateInput(renderer_, *screenController_,
                inputComposition.keyboardFocusOwner==uiComposition_.PrimaryIdentity(),dt);
        screenPresenter_->SetAccessibilitySettings(uiAccessibility_.presentation);
        if(hudSession_.IsLoaded())hudSession_.Presenter()->SetAccessibilitySettings(uiAccessibility_.presentation);
        if(hudSession_.IsLoaded() && uiComposition_.IsHudVisible())
        {
            hudSession_.Presenter()->SetReducedMotion(uiAccessibility_.reducedMotion);
            hudSession_.Presenter()->UpdateInput(renderer_,*hudSession_.Controller(),
                inputComposition.keyboardFocusOwner==uiComposition_.HudIdentity(),dt);
        }
        ProcessHudActions();
        ProcessPendingActions();
        SyncLevelHud();
        RefreshUiComposition();
        if (scenes_.CurrentPath().empty())
            StopCreatorScripts();
    }

    void RuntimeApplication::SyncLevelHud()
    {
        if(!startupResult_.succeeded)return;
        std::string error;
        const auto cancelExit=[&]()
        {
            if(hudSession_.IsLoaded()&&
                (hudExitRequested_||hudSession_.Presenter()->TransitionPhase()==bridge::ScreenTransitionPhase::Exiting||
                 hudSession_.Presenter()->TransitionPhase()==bridge::ScreenTransitionPhase::Exited))
            {
                // Empty/reduced-motion exits complete immediately, so recovery
                // cannot depend only on the asynchronous waiting flag.
                hudSession_.Presenter()->CancelExit();
                std::string ignored;(void)uiComposition_.CancelHudExit(ignored);
            }
            hudExitRequested_=false;
        };
        const auto* level=flowStarted_?flow_.CurrentNode():nullptr;
        if(flowStarted_&&(!level||level->kind!=bridge::FlowNodeKind::Level))
        {
            // A destination Screen may retain its Level HUD as a dormant
            // underlay. It must not inherit a half-finished context switch.
            cancelExit();pendingHudSwitch_.reset();return;
        }
        if(!flowStarted_&&startupResult_.startupScenePath.empty())return;
        const auto key=(level?level->id:std::string("legacy-scene"))+":"+std::to_string(scenes_.Revision());
        const bool changedLevel=key!=hudLevelKey_;
        if(changedLevel)
        {
            cancelExit();pendingHudSwitch_.reset();hudFailedRevision_.reset();hudTransitionError_.clear();
            std::optional<bridge::HudReference> project;
            if(!startupResult_.project.defaultHudId.empty())
                project=bridge::HudReference{startupResult_.project.defaultHudId,startupResult_.project.defaultHud};
            if(!hudSelection_.EnterLevel(++hudLevelSession_,project,level?level->levelHud:bridge::LevelHudSelection{},error))
            {wi::backlog::post("Renegade Runtime HUD selection: "+error,wi::backlog::LogLevel::Error);return;}
            if(!hudContextEvents_.Configure(level?level->hudContextRules:std::vector<bridge::HudContextEventRule>{},error))
            {wi::backlog::post("Renegade Runtime HUD event rules: "+error,wi::backlog::LogLevel::Error);return;}
            hudContextEvents_.BeginLevel(hudLevelSession_);hudLevelKey_=key;
        }
        const auto revision=hudSelection_.Revision();
        if(!changedLevel&&!pendingHudSwitch_&&hudAppliedSelectionRevision_==revision)return;
        if(!changedLevel&&hudFailedRevision_&&*hudFailedRevision_==revision)return;
        if(!pendingHudSwitch_||pendingHudSwitch_->revision!=revision)
        {
            // Resolve/parse at most once per selector revision, never once per
            // animation frame while waiting for the outgoing HUD to finish.
            std::map<bridge::StableId,bridge::ScreenDocument> documents;
            const auto selected=hudSelection_.Resolve([&](const bridge::HudReference& reference)
            {
                std::string path;bridge::ScreenDocument document;
                if(!bridge::ResolveRuntimeScreenDocumentPath(startupResult_.project.rootPath,startupResult_.project.projectId,
                    reference.documentId,reference.pathHint,path,error)||
                    !bridge::ReadScreenDocument(path,startupResult_.project.projectId,document,error)||
                    !document.enabledInGame||document.usage!=bridge::ScreenUsage::Hud)
                {
                    wi::backlog::post("Renegade Runtime HUD unavailable: "+reference.documentId+" // "+error,
                        wi::backlog::LogLevel::Warning);return false;
                }
                documents.emplace(reference.documentId,std::move(document));return true;
            });
            PendingHudSwitch pending;pending.revision=revision;
            if(selected)pending.destination=std::move(documents.at(selected->documentId));
            pendingHudSwitch_=std::move(pending);
        }
        auto& pending=*pendingHudSwitch_;
        if(!changedLevel&&pending.destination&&hudSession_.IsLoaded()&&
            hudSession_.Controller()->AuthoredDocument().envelope.documentId==pending.destination->envelope.documentId&&
            hudSession_.Controller()->AuthoredDocument().envelope.pathHint==pending.destination->envelope.pathHint)
        {
            // A context popped back before exit completed: retain instance,
            // focus and local values instead of replacing it with a duplicate.
            cancelExit();hudAppliedSelectionRevision_=revision;pendingHudSwitch_.reset();
            hudFailedRevision_.reset();hudTransitionError_.clear();RefreshUiComposition();return;
        }
        if(hudExitRequested_&&!uiComposition_.IsHudVisible())cancelExit();
        if(!changedLevel&&hudSession_.IsLoaded()&&uiComposition_.IsHudVisible()&&!hudExitRequested_)
        {
            if(!hudSession_.Presenter()->BeginExit(nullptr,error))
            {hudTransitionError_=error;hudFailedRevision_=revision;pendingHudSwitch_.reset();return;}
            if(!hudSession_.Presenter()->ExitComplete())
            {
                hudExitRequested_=true;(void)uiComposition_.BeginHudExit(error);
                RefreshUiComposition();return;
            }
        }
        if(hudExitRequested_&&!hudSession_.Presenter()->ExitComplete())return;
        if(!pending.destination)
        {
            hudSession_.Reset(renderer_);uiComposition_.ClearHud();
            hudExitRequested_=false;hudAppliedSelectionRevision_=revision;pendingHudSwitch_.reset();
            hudFailedRevision_.reset();hudTransitionError_.clear();
            pendingHudActions_.clear();RefreshUiComposition();RefreshUiBindingSources();return;
        }
        const auto& document=*pending.destination;
        auto composition=uiComposition_;
        const bool focusable=std::any_of(document.widgets.begin(),document.widgets.end(),[](const auto& w)
            {return bridge::IsScreenInteractive(w.kind)&&w.visible&&w.enabled;});
        if(!composition.ReplaceHud(document,focusable,error)||
            !hudSession_.Load(document,startupResult_.project.rootPath,renderer_,
                [this](RuntimeActionRequest request)
                {pendingHudActions_.emplace_back(uiComposition_.HudIdentity(),std::move(request));},
                error,uiAccessibility_.reducedMotion,uiAccessibility_.presentation))
        {
            hudTransitionError_=error;cancelExit();hudFailedRevision_=revision;
            pendingHudSwitch_.reset();RefreshUiComposition();
            wi::backlog::post("Renegade Runtime HUD prepare failed: "+error,wi::backlog::LogLevel::Error);return;
        }
        const auto identity=document.envelope.documentId;
        uiComposition_=std::move(composition);pendingHudActions_.clear();
        hudExitRequested_=false;hudAppliedSelectionRevision_=revision;pendingHudSwitch_.reset();
        hudFailedRevision_.reset();hudTransitionError_.clear();
        RefreshUiComposition();RefreshUiBindingSources();
        wi::backlog::post("Renegade Runtime: HUD attached "+identity+" without replacing the Level.");
    }

    void RuntimeApplication::OnGovernedHudGameplayEvent(const bridge::GameplayEvent& event)
    {
        if(hudContextEvents_.RuleCount()==0||!hudSelection_.LevelSession())return;
        bool matched=false;std::string error;
        if(!hudContextEvents_.Handle(event,hudLevelSession_,hudSelection_,matched,error))
            diagnosticService_.Record(bridge::DiagnosticSeverity::Warning,
                "runtime.ui","hud.event.rejected",error);
        // Actual selection happens in SyncLevelHud() at the end of the same
        // gameplay frame, preserving scene, Flow and creator script lifetimes.
    }

    void RuntimeApplication::ProcessHudActions()
    {
        std::vector<std::pair<std::uint64_t,RuntimeActionRequest>> pending;pending.swap(pendingHudActions_);
        for(const auto& item:pending)
        {
            if(item.first!=uiComposition_.HudIdentity() || !uiComposition_.HudAcceptsInput())continue;
            // HUD commands use governed Runtime handlers; they are not outcomes
            // of whichever unrelated destination Screen happens to be open.
            RecordAction(actions_.Dispatch(item.second));
        }
    }

    void RuntimeApplication::RefreshUiBindingSources()
    {
        uiBindingSources_.clear();
        std::set<std::tuple<int,bridge::StableId,std::string>> seen;
        const auto add = [&](const bridge::ScreenBindingSource& source)
        {
            if (seen.emplace(static_cast<int>(source.scope),source.targetId,source.attributeId).second)
                uiBindingSources_.push_back(source);
        };
        const auto collect=[&](const RuntimeScreenController& controller)
        {
            for(const auto& widget:controller.AuthoredDocument().widgets)
                for(const auto& binding:widget.bindings)
                {add(binding.source);if(binding.operation==bridge::ScreenBindingOperation::Ratio)add(binding.denominator);}
        };
        if(screenPresenter_->IsLoaded())collect(*screenController_);
        if(hudSession_.IsLoaded() && uiComposition_.IsHudVisible())collect(*hudSession_.Controller());
        uiBindingDiagnostic_.clear();
    }

    bool RuntimeApplication::LoadGameplayAttributes(std::string& error)
    {
        const auto& project=startupResult_.project;
        if(project.attributeDocumentId.empty())
        {gameplayAttributes_.Clear();attributeDocument_.reset();attributeDiagnostic_.clear();error.clear();return true;}
        std::string path;bridge::GameplayAttributeDocument document;RuntimeGameplayAttributeWorld candidate;
        if(!bridge::ResolveGameplayAttributeDocumentPath(project.rootPath,project.projectId,project.attributeDocumentId,
            project.attributeDocument,path,error)||!bridge::ReadGameplayAttributeDocument(path,project.projectId,document,error)||
            !candidate.Configure(document,true,error))return false;
        gameplayAttributes_=std::move(candidate);attributeDocument_=std::move(document);attributeDiagnostic_.clear();
        error.clear();return true;
    }
    void RuntimeApplication::UpdateGameplayAttributes(float dt)
    {
        if(!attributeDocument_)return;
        const auto& scene=scenes_.GetScene();const auto generation=scenes_.CurrentPath().empty()?0:scenes_.Revision();
        std::vector<GameplayAttributeActorLifetime> actors;
        if(characterSceneRevision_==generation&&!characterSceneSyncFailed_)
            for(const auto& actor:characterAiState_.characters)
                if(scene.characters.Contains(actor.entity)&&scene.transforms.Contains(actor.entity))
                    actors.push_back({actor.stableEntityId,static_cast<std::uint64_t>(actor.entity)});
        std::uint64_t playerEntity=0;
        if(player_.IsSpawned()&&playerSceneRevision_==generation)
        {
            const auto* body=scene.rigidbodies.GetComponent(player_.entity);
            if(body&&body->IsCharacterPhysics()&&scene.transforms.Contains(player_.entity))
                playerEntity=static_cast<std::uint64_t>(player_.entity);
        }
        std::string error;
        if(!gameplayAttributes_.SyncActors(generation,playerEntity,actors,error)||
            !gameplayAttributes_.Tick(dt,paused_,error))
        {
            if(error!=attributeDiagnostic_)wi::backlog::post("Gameplay attribute module: "+error,wi::backlog::LogLevel::Warning);
            attributeDiagnostic_=error;return;
        }
        attributeDiagnostic_.clear();
    }

    void RuntimeApplication::UpdateUiBindings()
    {
        if (uiBindingSources_.empty())
        { diagnosticService_.SetState("ui_live_binding",{{"available",false}}); return; }
        std::string error;
        const auto report = [&](const std::string& message)
        {
            if (message == uiBindingDiagnostic_) return;
            uiBindingDiagnostic_ = message;
            wi::backlog::post("Renegade Runtime UI bindings: " + message,
                wi::backlog::LogLevel::Warning);
        };
        if (!uiPublisherStarted_)
        {
            if (!uiSnapshotPublisher_.BeginSession(++uiSessionIdentity_,error))
            { report(error); return; }
            uiPublisherStarted_ = true;
        }
        RuntimeUiGameplaySources sources;
        sources.projectAttributes=gameplayAttributes_.Project();
        sources.playerAttributes=gameplayAttributes_.Player();
        sources.characterAttributes=&gameplayAttributes_.Characters();
        sources.player = &player_;
        sources.scene = &scenes_.GetScene();
        sources.combat = &combatState_;
        sources.characters = &characterAiState_;
        sources.playerView = &playerViewAnimation_;
        sources.sceneGeneration = scenes_.Revision();
        sources.combatReady = characterSceneRevision_ != 0 &&
            characterSceneRevision_ == scenes_.Revision() && !characterSceneSyncFailed_;
        bridge::ScreenBindingSnapshot snapshot;
        bridge::ScreenBindingContext context;
        if (!uiSnapshotPublisher_.Publish(uiBindingSources_,++uiGameplayRevision_,sources,snapshot,context,error))
        {diagnosticService_.SetState("ui_live_binding",{{"available",false}});report(error);return;}
        const auto liveNow=diagnosticService_.ElapsedMs();
        if(!uiLivePublishedMs_||liveNow-uiLivePublishedMs_>=100)
        {
            uiLivePublishedMs_=liveNow;
            std::string wire,wireError;
            bridge::DiagnosticState live;
            const bool available=bridge::EncodeScreenLiveBinding({snapshot,context},wire,wireError);
            live["available"]=available;
            live["published_elapsed_ms"]=liveNow;
            if(available)
            {
                const auto count=(wire.size()+959)/960;
                live["chunk_count"]=std::uint64_t(count);
                for(std::size_t i=0;i<count;++i)
                    live["chunk_"+std::to_string(i)]=wire.substr(i*960,960);
            }
            diagnosticService_.SetState("ui_live_binding",std::move(live));
        }
        std::string diagnostic;
        const auto apply=[&](RuntimeScreenController& controller,RuntimeScreenPresenter& presenter)
        {
            if(!controller.ApplyBindings(snapshot,context,error)){report(error);return false;}
            presenter.RefreshVisualFocus(controller);
            for(const auto& issue:controller.BindingFrame().issues)
                diagnostic+=controller.AuthoredDocument().envelope.documentId+":"+issue.widgetId+":"+
                    std::to_string(static_cast<int>(issue.property))+":"+issue.message+"; ";
            return true;
        };
        if(screenPresenter_->IsLoaded() && !apply(*screenController_,*screenPresenter_))return;
        if(hudSession_.IsLoaded() && uiComposition_.IsHudVisible() &&
            !apply(*hudSession_.Controller(),*hudSession_.Presenter()))return;
        if (!diagnostic.empty()) report(diagnostic);
        else if (!uiBindingDiagnostic_.empty())
        {
            uiBindingDiagnostic_.clear();
            wi::backlog::post("Renegade Runtime UI bindings: all requested sources resolved.");
        }
    }

    void RuntimeApplication::UpdatePlayerProjectiles(const float dt)
    {
        if (!player_.IsSpawned() || playerSceneRevision_ != scenes_.Revision() ||
            renderer_.camera == nullptr) {
            pendingProjectileShots_.clear();
            channelVisual_.Reset(scenes_.GetScene());channel_.Reset();channelScorchTrail_.Reset();lightningStorm_.Reset(scenes_.GetScene());lightningColumnReview_.Reset(scenes_.GetScene());lightningStormCastLatch_=false;
            renderer_.SetProjectileContacts({});
            return;
        }
        auto& scene = scenes_.GetScene();
        // Private visual approval fixture only: a persistent, easily inspected
        // single column. Do not enter the rejected Storm renderer or issue damage.
        if(playerEquipment_.primary.equipment.name=="Lightning Column - Strip Review") {
            const auto folder=(std::filesystem::u8path(startupResult_.project.rootPath)/
                "Content/Effects/LightningColumn").generic_u8string();
            const bool previouslyFailed=lightningColumnReview_.failed;
            if(lightningColumnReview_.Create(scene,folder))
                lightningColumnReview_.Update(scene,{0,0,6},dt);
            else if(!previouslyFailed)
                wi::backlog::post("Lightning column: could not load all eleven supplied frames.",
                    wi::backlog::LogLevel::Error);
            pendingProjectileShots_.clear();
            renderer_.SetProjectileContacts({});
            return;
        }
        // Same named hand socket and native pose hierarchy as the authoritative release.
        // Parenting follows completed native skin/hand transforms; particle velocities are world-space.
        const auto chargeBinding=std::find_if(playerEquipment_.resolvedProjectiles.begin(),playerEquipment_.resolvedProjectiles.end(),
            [&](const auto& request){return request.equipmentId==playerEquipment_.primary.equipment.assetId &&
                request.action==bridge::EquipmentAction::Release && request.projectile.fireballPresentation;});
        const bool holding=playerViewAnimation_.spellPresentation && playerViewAnimation_.equipped &&
            (playerViewAnimation_.spellPhase==1 || playerViewAnimation_.spellPhase==2);
        if(chargeBinding!=playerEquipment_.resolvedProjectiles.end() && holding) {
            if(!heldFireball_.Ready() && dt>0) {
                std::string error;const auto parent=bridge::FindLaunchSocketEntity(scene,playerViewRig_.viewModelRoot,chargeBinding->launchSocketName,error);
                if(parent!=wi::ecs::INVALID_ENTITY)heldFireball_.Create(scene,parent);
            }
            if(heldFireball_.Ready()) {
                const float limit=chargeBinding->projectile.fullChargeSeconds;
                if(dt>0 && limit>0)heldFireball_.charge=std::clamp(playerEquipment_.projectileChargeSeconds/limit,0.f,1.f);
                heldFireball_.Hand(scene,dt,limit<=0);
            }
        } else if(heldFireball_.Ready()) {
            if(playerViewAnimation_.spellPresentation && playerViewAnimation_.equipped && playerViewAnimation_.spellPhase==3)
                heldFireball_.Hand(scene,dt,false); // keep energy in the palm until the release marker
            else heldFireball_.Reset(scene);
        }
        wi::audio::SoundInstance3D impactListener;
        impactListener.listenerPos = renderer_.camera->Eye;
        impactListener.listenerFront = renderer_.camera->At;
        impactListener.listenerUp = renderer_.camera->Up;
        impactAudio_.Update(dt, impactListener);
        const auto presentImpactAudio = [&](const bridge::ProjectileImpact& impact)
        {
            bridge::StableId played;
            if (impactAudio_.Play(impact.contact.surfaceType,
                ProjectileNativeVector(impact.contact.position), impactListener, played))
                diagnosticService_.Record(bridge::DiagnosticSeverity::Info,
                    "runtime.audio", "impact.audio.played",
                    "surface_type=" + std::string(bridge::ImpactSurfaceTypeToken(impact.contact.surfaceType)) +
                    ";asset=" + played + ";voices=" + std::to_string(impactAudio_.VoiceCount()));
        };
        if(dt>0) {
            const auto* clip=scene.animations.GetComponent(playerViewAnimation_.activeClip);
            const float time=(playerViewAnimation_.pairedAssembly || playerViewAnimation_.spellPresentation) ? playerViewAnimation_.pairedTime :
                clip ? clip->timer-clip->start : 0;
            auto due=playerEquipment_.ReleaseProjectiles(playerViewAnimation_.activeClip,time,!playerViewAnimation_.equipped);
            pendingProjectileShots_.insert(pendingProjectileShots_.end(),
                std::make_move_iterator(due.begin()),std::make_move_iterator(due.end()));
        }
        // This one thrown reference holds a grenade in the palm until its
        // authored release point, then hides the held mesh while the separate
        // physical projectile travels. Restore it on recovery/next ready.
        if(playerEquipment_.primary.equipment.name=="Thrown Grenade P3 Playground" &&
           playerViewRig_.viewModelRoot!=wi::ecs::INVALID_ENTITY) {
            const bool released=playerViewAnimation_.spellPhase==3 &&
                playerViewAnimation_.pairedTime>=.64f;
            SetHeldGrenadeBodyVisible(scene,playerViewRig_.viewModelRoot,!released);
        }
        auto excludedVisuals=projectileVisuals_.roots;
        if(channelVisual_.root!=wi::ecs::INVALID_ENTITY)excludedVisuals.push_back(channelVisual_.root);
        if(lightningStorm_.root!=wi::ecs::INVALID_ENTITY)excludedVisuals.push_back(lightningStorm_.root);
        const ProjectileOwnerBinding owner{RuntimePlayerKnowledgeId, player_.entity, &excludedVisuals};
        const auto emit = [this](bridge::GameplayEvent event, std::string& error) {
            diagnosticService_.Record(bridge::DiagnosticSeverity::Info,
                "runtime.projectile", event.name, event.payload);
            // Governed queue remains the shared script/event boundary.
            return creatorScripts_.EnqueueGameplayEvent(std::move(event), error);
        };
        if(std::isfinite(dt) && dt>0) {
            const auto applyChannel=[&](const bridge::ProjectileImpact& impact) {
                const auto damage=ApplyProjectileCharacterImpact(scene,characterAiState_,
                    characterPerceptionState_,combatState_,impact,emit);
                if(damage.damageApplied)renderer_.ConfirmTargetHit();
                std::string ignored;
                (void)emit({0,"ability.channel.tick","damage="+std::to_string(impact.damage),
                    impact.source.ownerSubjectId,impact.contact.targetSubjectId},ignored);
            };
            const auto* binding=playerEquipment_.ChannelBinding();
            const bool casting=binding && playerEquipment_.channelActive && playerViewAnimation_.equipped &&
                playerViewAnimation_.spellPresentation &&
                (playerViewAnimation_.spellPhase==1 || playerViewAnimation_.spellPhase==2);
            if(casting) {
                bridge::ProjectileSource source;
                source.ownerSubjectId=owner.subjectId;source.sourceAssetId=binding->equipmentId;source.factionId="Player";
                if(const auto* transform=scene.transforms.GetComponent(player_.entity))
                    source.knownPosition=ProjectileBridgeVector(transform->GetPosition());
                XMFLOAT3 velocity{};(void)bridge::GetLinearVelocity(scene,player_.entity,velocity);
                source.knownVelocity=ProjectileBridgeVector(velocity);
                auto origin=renderer_.camera->Eye,direction=renderer_.camera->At;
                XMFLOAT3 palm{},palmForward{};std::string error;
                wi::ecs::Entity channelHitEntity=wi::ecs::INVALID_ENTITY;
                const auto channelQuery=[&](const auto& record,const auto& from,const auto& to){
                    wi::ecs::Entity hit=wi::ecs::INVALID_ENTITY;
                    auto result=QueryProjectileSceneSegment(scene,characterAiState_,owner,record,from,to,~0u,&hit);
                    channelHitEntity=result.status==bridge::ProjectileQueryStatus::Hit?hit:wi::ecs::INVALID_ENTITY;
                    return result;
                };
                bridge::ProjectileQueryResult contact;
                const bool aimed=bridge::ReadLaunchSocketPose(scene,playerViewRig_.viewModelRoot,
                    binding->launchSocketName,palm,palmForward,error) &&
                    ResolveMuzzleAim(source,binding->hitscanRangeMetres,origin,direction,palm,palmForward,
                        channelQuery,origin,direction,error);
                // The earlier cover/aim probes must never masquerade as the
                // actual beam receiver when it comes time to paint a scorch.
                channelHitEntity=wi::ecs::INVALID_ENTITY;
                const bool valid=aimed &&
                    QueryHitscan(source,origin,direction,binding->hitscanRangeMetres,channelQuery,contact,error);
                if(valid) {
                    if(!channel_.active){std::string ignored;(void)emit({0,"ability.channel.started",
                        "equipment="+binding->equipmentId,owner.subjectId,{}},ignored);}
                    channel_.Update(dt,source,contact,binding->channelDamagePerSecond,applyChannel);
                    XMFLOAT3 end;
                    XMStoreFloat3(&end,XMLoadFloat3(&origin)+XMVector3Normalize(XMLoadFloat3(&direction))*binding->hitscanRangeMetres);
                    if(contact.status==bridge::ProjectileQueryStatus::Hit)end=ProjectileNativeVector(contact.contact.position);
                    const bool contacted=contact.status==bridge::ProjectileQueryStatus::Hit;
                    channelVisual_.Update(scene,origin,end,contacted,dt,
                        contacted?ProjectileNativeVector(contact.contact.normal):XMFLOAT3{});
                    channelScorchTrail_.Update(contact,channelHitEntity,
                        [&](const XMFLOAT3& point,const XMFLOAT3& normal,wi::ecs::Entity receiver){
                            bridge::ProjectileImpact impact;
                            impact.source=source;impact.contact=contact.contact;
                            impact.contact.position=ProjectileBridgeVector(point);
                            impact.contact.normal=ProjectileBridgeVector(normal);
                            impact.damage=20.f;
                            projectileVisuals_.PresentChannelScorch(scene,impact,receiver);
                        });
                } else {
                    channel_.Stop(applyChannel);channelVisual_.Reset(scene);channelScorchTrail_.Reset();
                    projectiles_.lastError=error;
                }
            } else {
                if(channel_.active){std::string ignored;(void)emit({0,"ability.channel.stopped","",owner.subjectId,{}},ignored);}
                channel_.Stop(applyChannel);channelVisual_.Reset(scene);channelScorchTrail_.Reset();
            }
            // P8 area ability: the targeting circle follows the crosshair;
            // a single accepted Cast starts an independent timed storm.
            // Releasing LMB ends the arms cast but not the placed storm.
            const auto* stormBinding=playerEquipment_.StormBinding();
            const bool stormEquipped=stormBinding && playerViewAnimation_.equipped &&
                playerViewAnimation_.spellPresentation && renderer_.camera;
            // Opt-in solely for the isolated Stage 3 approval playground.
            // All previously-authored Storm assets retain the V14 rendering.
            if(stormEquipped){
                const auto& stormName=playerEquipment_.primary.equipment.name;
                lightningStorm_.UseStage3(scene,stormName=="Lightning Storm - Stage 3 Playground");
                lightningStorm_.UseNatural(scene,stormName=="Lightning Storm - Natural Playground");
                if(lightningStorm_.naturalEnabled && !lightningStorm_.active) {
                    lightningStorm_.natural.refinement3.Warmup(scene,6);
                    projectileVisuals_.PrewarmLightningScorch();
                }
            }
            XMFLOAT3 ground{};bool groundValid=false;
            bridge::ProjectileSource stormSource;
            if(stormEquipped) {
                stormSource.ownerSubjectId=owner.subjectId;
                stormSource.sourceAssetId=stormBinding->equipmentId;
                stormSource.factionId="Player";
                if(const auto* playerT=scene.transforms.GetComponent(player_.entity))
                    stormSource.knownPosition=ProjectileBridgeVector(playerT->GetPosition());
                std::string aimError;bridge::ProjectileQueryResult hit;
                const auto probe=[&](const auto& record,const auto& a,const auto& b) {
                    return QueryProjectileSceneSegment(scene,characterAiState_,owner,record,a,b);
                };
                groundValid=QueryHitscan(stormSource,renderer_.camera->Eye,
                    renderer_.camera->At,stormBinding->hitscanRangeMetres,probe,hit,aimError) &&
                    hit.status==bridge::ProjectileQueryStatus::Hit;
                if(groundValid) {
                    if(hit.contact.surfaceType==bridge::ImpactSurfaceType::Character){
                        const auto* victim=FindCharacterCombat(combatState_,hit.contact.targetSubjectId);
                        const auto* position=victim?scene.transforms.GetComponent(victim->entity):nullptr;
                        if(position)ground=position->GetPosition();else groundValid=false;
                    } else if(hit.contact.surfaceType!=bridge::ImpactSurfaceType::Water &&
                              hit.contact.normal.y>.62f)
                        ground=ProjectileNativeVector(hit.contact.position);
                    else groundValid=false;
                }
            }
            const bool pressed=stormEquipped && playerEquipment_.channelActive &&
                (playerViewAnimation_.spellPhase==1 || playerViewAnimation_.spellPhase==2);
            if(!pressed && (!stormEquipped || !lightningStorm_.naturalEnabled))
                lightningStormCastLatch_=false;
            if(stormEquipped && groundValid && !lightningStorm_.active && !pressed)
                lightningStorm_.SetMarker(scene,ground,stormBinding->stormRadiusMetres,true);
            else if(!lightningStorm_.active && lightningStorm_.root!=wi::ecs::INVALID_ENTITY && !pressed)
                lightningStorm_.SetMarker(scene,lightningStorm_.center,lightningStorm_.radius,false);
            if(pressed && !lightningStormCastLatch_ && !lightningStorm_.active) {
                lightningStormCastLatch_=true;
                if(groundValid) {
                    lightningStorm_.Begin(scene,stormSource,ground,
                        stormBinding->stormRadiusMetres,stormBinding->stormDurationSeconds,
                        stormBinding->stormStrikeCount,stormBinding->stormStrikeDamage);
                    std::string ignored;
                    (void)emit({0,"ability.storm.started","radius="+std::to_string(stormBinding->stormRadiusMetres),
                        owner.subjectId,{}},ignored);
                }
            }
            if(lightningStorm_.active) {
                lightningStorm_.Tick(scene,dt,[&](XMFLOAT3& strike,unsigned index){
                    // V11: never snap strikes onto targets. Each governed
                    // impact stays at the moving leader's arrival point, so
                    // the centre-first/outward-shell choreography remains
                    // truthful while NPCs are hit by the actual contact zone.
                    (void)index;
                    for(const auto& victim:combatState_.characters) {
                        if(victim.dead)continue;
                        const auto* t=scene.transforms.GetComponent(victim.entity);
                        if(!t)continue;
                        const auto p=t->GetPosition();
                        if(!RuntimeLightningStorm::Within(p.x,p.z,lightningStorm_.center,lightningStorm_.radius) ||
                           !RuntimeLightningStorm::Within(p.x,p.z,strike,1.55f))continue;
                        bridge::ProjectileImpact hit;
                        hit.source=lightningStorm_.source;
                        hit.contact.targetSubjectId=victim.characterId;
                        hit.contact.surfaceType=bridge::ImpactSurfaceType::Character;
                        hit.contact.position=ProjectileBridgeVector(p);
                        hit.contact.normal={0,1,0};
                        hit.incomingVelocity={0,-1,0};
                        hit.damage=lightningStorm_.strikeDamage;
                        const auto result=ApplyProjectileCharacterImpact(scene,characterAiState_,
                            characterPerceptionState_,combatState_,hit,emit);
                        if(result.damageApplied)renderer_.ConfirmTargetHit();
                    }
                    // Contact soot follows the hit normal. Natural Lightning uses
                    // round, irregular marks, not the continuous-beam tile canvas.
                    wi::vector<wi::scene::Scene::RayIntersectionResult> floorHits;
                    scene.IntersectsAll(floorHits,
                        wi::primitive::Ray(XMFLOAT3{strike.x,strike.y+1.4f,strike.z},XMFLOAT3{0,-1,0},0,2.5f),
                        wi::enums::FILTER_OPAQUE|wi::enums::FILTER_TERRAIN);
                    for(const auto& landed:floorHits) {
                        if(landed.entity==wi::ecs::INVALID_ENTITY||landed.normal.y<.75f ||
                           std::abs(landed.position.y-strike.y)>.9f)continue;
                        bridge::ProjectileImpact mark;
                        mark.source=lightningStorm_.source;
                        mark.contact.position=ProjectileBridgeVector(landed.position);
                        mark.contact.normal=ProjectileBridgeVector(landed.normal);
                        mark.contact.surfaceType=bridge::ImpactSurfaceType::Concrete;
                        mark.damage=lightningStorm_.strikeDamage;
                        if(lightningStorm_.naturalEnabled)
                            projectileVisuals_.PresentLightningScorch(scene,mark,landed.entity);
                        else
                            projectileVisuals_.PresentChannelScorch(scene,mark,landed.entity);
                        break;
                    }
                    std::string ignored;
                    (void)emit({0,"ability.storm.strike","index="+std::to_string(index),
                        owner.subjectId,{}},ignored);
                });
            } else if(lightningStorm_.root!=wi::ecs::INVALID_ENTITY) {
                // Inactive targeting still needs to age/retire the last strike
                // particles; visual lifetimes are not gameplay lifetimes.
                lightningStorm_.Tick(scene,dt,[](XMFLOAT3&,unsigned){});
            }
        }
        if (std::isfinite(dt) && dt > 0) {
            XMFLOAT3 position{}, velocity{};
            const auto* transform = scene.transforms.GetComponent(player_.entity);
            if (transform) position = transform->GetPosition();
            (void)bridge::GetLinearVelocity(scene, player_.entity, velocity);
            for (const auto& request : pendingProjectileShots_) {
                bridge::ProjectileSource source;
                source.ownerSubjectId = owner.subjectId;
                source.sourceAssetId = request.equipmentId;
                source.factionId = "Player";
                source.knownPosition = ProjectileBridgeVector(position);
                source.knownVelocity = ProjectileBridgeVector(velocity);
                XMFLOAT3 origin=renderer_.camera->Eye,direction=renderer_.camera->At;
                wi::ecs::Entity queryHitEntity = wi::ecs::INVALID_ENTITY;
                const auto query=[&](const auto& record,const auto& from,const auto& to){
                    wi::ecs::Entity hitEntity = wi::ecs::INVALID_ENTITY;
                    auto result = QueryProjectileSceneSegment(
                        scene,characterAiState_,owner,record,from,to,~0u,&hitEntity);
                    if (result.status == bridge::ProjectileQueryStatus::Hit)
                        queryHitEntity = hitEntity;
                    return result;
                };
                if(!request.launchSocketName.empty()) {
                    XMFLOAT3 muzzle,forward;std::string error;
                    const bool aimed = bridge::ReadLaunchSocketPose(scene,playerViewRig_.viewModelRoot,
                            request.launchSocketName,muzzle,forward,error) &&
                        (request.fireMode==bridge::EquipmentFireMode::Hitscan
                            ? ResolveMuzzleAim(source,request.hitscanRangeMetres,origin,direction,
                                muzzle,forward,query,origin,direction,error)
                            : ResolveProjectileMuzzleAim(request.projectile,source,origin,direction,
                                muzzle,forward,query,origin,direction,error));
                    if(!aimed) {
                        projectiles_.lastError=error;
                        diagnosticService_.Record(bridge::DiagnosticSeverity::Error,
                            "runtime.projectile",
                            request.fireMode==bridge::EquipmentFireMode::Hitscan?
                                "hitscan.socket_failed":"projectile.socket_failed",error);
                        continue;
                    }
                }
                if(request.fireMode==bridge::EquipmentFireMode::Hitscan) {
                    bridge::ProjectileQueryResult contact;std::string error;
                    queryHitEntity = wi::ecs::INVALID_ENTITY;
                    if(!QueryHitscan(source,origin,direction,request.hitscanRangeMetres,query,contact,error)) {
                        diagnosticService_.Record(bridge::DiagnosticSeverity::Error,
                            "runtime.projectile","hitscan.query_failed",error);
                        continue;
                    }
                    std::string ignored;
                    (void)emit({0,"hitscan.fired",
                        "equipment="+request.equipmentId+
                        ";range="+std::to_string(request.hitscanRangeMetres),
                        owner.subjectId,{}},ignored);
                    if(contact.status==bridge::ProjectileQueryStatus::Hit) {
                        bridge::ProjectileImpact impact;
                        impact.source=source;impact.contact=contact.contact;
                        impact.damage=request.hitscanDamage;
                        impact.incomingVelocity=ProjectileBridgeVector(direction);
                        const auto& p=contact.contact.position;
                        (void)emit({0,"hitscan.impact",
                            "equipment="+request.equipmentId+
                            ";surface="+contact.contact.surfaceId+
                            ";surface_type="+std::string(
                                bridge::ImpactSurfaceTypeToken(contact.contact.surfaceType))+
                            ";x="+std::to_string(p.x)+";y="+std::to_string(p.y)+
                            ";z="+std::to_string(p.z),
                            owner.subjectId,contact.contact.targetSubjectId},ignored);
                        projectileVisuals_.PresentSurfaceImpact(
                            scene, impact, queryHitEntity);
                        presentImpactAudio(impact);
                        const auto damage=ApplyProjectileCharacterImpact(scene,characterAiState_,
                            characterPerceptionState_,combatState_,impact,emit);
                        if(damage.characterContact)renderer_.ConfirmTargetHit();
                    }
                    continue;
                }
                std::uint64_t id = 0;
                if (projectiles_.Launch(request.projectile, source,origin,direction,id)) {
                    projectiles_.lastLaunchSocket=request.launchSocketName;
                    if(!projectileVisuals_.Spawn(scene,request.projectile.assetId,id,!request.projectile.meshAssetId.empty(),
                        request.projectile.fireballPresentation?&heldFireball_:nullptr))
                        diagnosticService_.Record(bridge::DiagnosticSeverity::Error,
                            "runtime.projectile","projectile.visual_failed",projectileVisuals_.error);
                    std::string ignored;
                    (void)emit({0, "projectile.launched",
                        "projectile=" + std::to_string(id) +
                        ";asset=" + request.projectile.assetId +
                        ";equipment=" + request.equipmentId + ";damage=" + std::to_string(request.projectile.damage),
                        owner.subjectId, {}}, ignored);
                } else {
                    diagnosticService_.Record(bridge::DiagnosticSeverity::Error,
                        "runtime.projectile", "projectile.launch_failed", projectiles_.lastError);
                }
            }
        }
        pendingProjectileShots_.clear();
        std::vector<bridge::ProjectileImpact> impacts;
        std::map<std::uint64_t,wi::ecs::Entity> impactParents;
        const auto query = [&](const bridge::ProjectileRecord& record,
                               const bridge::ProjectileVector& from,
                               const bridge::ProjectileVector& to) {
            wi::ecs::Entity hit=wi::ecs::INVALID_ENTITY;
            auto result=QueryProjectileSceneSegment(scene, characterAiState_, owner, record, from, to,~0u,&hit);
            if(result.status==bridge::ProjectileQueryStatus::Hit)impactParents[record.id]=hit;
            return result;
        };
        if (!projectiles_.Update(dt, query, impacts)) {
            diagnosticService_.Record(bridge::DiagnosticSeverity::Error,
                "runtime.projectile", "projectile.update_failed", projectiles_.lastError);
        }
        for (const auto& impact : impacts) {
            const auto parent = impactParents[impact.projectileId];
            projectileVisuals_.PresentSurfaceImpact(scene,impact,parent);
            presentImpactAudio(impact);
            projectileVisuals_.Impact(scene,impact,parent);
            std::string ignored;
            const auto& p = impact.contact.position;
            (void)emit({0, "projectile.impact",
                "projectile=" + std::to_string(impact.projectileId) +
                ";surface=" + impact.contact.surfaceId +
                ";surface_type=" + std::string(
                    bridge::ImpactSurfaceTypeToken(impact.contact.surfaceType)) +
                ";x=" + std::to_string(p.x) + ";y=" + std::to_string(p.y) +
                ";z=" + std::to_string(p.z),
                impact.source.ownerSubjectId, impact.contact.targetSubjectId}, ignored);
            // Existing damage integration seam; contacts/effects work independently
            // of usable Player/NPC health authoring.
            const auto damage = ApplyProjectileCharacterImpact(
                scene, characterAiState_, characterPerceptionState_,
                combatState_, impact, emit);
            if(damage.damageApplied)burning_.Ignite(scene,characterAiState_,characterPerceptionState_,combatState_,impact,true,emit);
            if (damage.characterContact)
                renderer_.ConfirmTargetHit();
        }
        // Grenades use an authoritative fuse, not damage on bounce.
        // No explosion events are produced by ordinary hitscan/physical spells.
        for(const auto& bounce:projectiles_.grenadeBounces) {
            std::string ignored;
            (void)emit({0,"projectile.grenade.bounce",
                "projectile="+std::to_string(bounce.projectileId)+
                ";surface="+bounce.contact.surfaceId,
                bounce.source.ownerSubjectId,bounce.contact.targetSubjectId},ignored);
        }
        for(const auto& explosion:projectiles_.grenadeExplosions) {
            projectileVisuals_.PresentGrenadeExplosion(scene,explosion);
            const auto count=ApplyGrenadeBlastToCharacters(scene,characterAiState_,
                characterPerceptionState_,combatState_,owner,explosion,emit);
            if(count)renderer_.ConfirmTargetHit();
            std::string ignored;
            const auto& p=explosion.position;
            (void)emit({0,"projectile.grenade.explode",
                "projectile="+std::to_string(explosion.projectileId)+
                ";x="+std::to_string(p.x)+";y="+std::to_string(p.y)+
                ";z="+std::to_string(p.z)+";radius="+std::to_string(explosion.radius)+
                ";characters_hit="+std::to_string(count),
                explosion.source.ownerSubjectId,{}},ignored);
        }
        projectileVisuals_.Sync(scene,projectiles_.simulation,dt);
        // Basic flight/contact feedback uses Wicked's bounded native primitives.
        // Runtime defaults debug drawing off; these confirmed contacts opt in.
        if (!playerEquipment_.resolvedProjectiles.empty())
            wi::renderer::SetDebugDrawEnabled(true);
        projectiles_.Draw();
        // Surface VFX/decals own world-impact presentation. Keep target-hit HUD feedback.
        renderer_.SetProjectileContacts({});
    }

    void RuntimeApplication::SyncPlayerForScene()
    {
        if (playerSceneRevision_ == scenes_.Revision())
            return;

        player_ = {};
        playerViewRig_ = {};
        playerViewAnimation_ = {};
        playerEquipment_ = {};
        pendingProjectileShots_.clear();
        impactAudio_.StopVoices();
        heldFireball_.Reset(scenes_.GetScene());
        channelVisual_.Reset(scenes_.GetScene());channel_.Reset();channelScorchTrail_.Reset();lightningStorm_.Reset(scenes_.GetScene());lightningColumnReview_.Reset(scenes_.GetScene());lightningStormCastLatch_=false;
        projectileVisuals_.Reset(scenes_.GetScene());
        projectiles_.Reset();
        burning_.Reset(scenes_.GetScene());
        renderer_.SetProjectileAim(false);
        renderer_.SetProjectileContacts({});
        renderer_.ClearHitConfirmation();
        playerSceneRevision_ = scenes_.Revision();
        const auto resolved = bridge::ResolvePlayerStart(scenes_.GetScene());
        if (resolved.resolution == bridge::PlayerStartResolution::Missing)
        {
            diagnosticService_.Record(bridge::DiagnosticSeverity::Info, "RuntimeApplication.cpp:SyncPlayerForScene",
                "player.start.missing", "No Player Start; spectator camera retained");
            wi::backlog::post(
                "Renegade Runtime: Level has no Player Start; retaining the legacy spectator camera.",
                wi::backlog::LogLevel::Default);
            return;
        }
        if (resolved.resolution != bridge::PlayerStartResolution::Success)
        {
            diagnosticService_.Record(bridge::DiagnosticSeverity::Error, "RuntimeApplication.cpp:SyncPlayerForScene",
                "player.start.invalid", resolved.message);
            wi::backlog::post(
                "Renegade Runtime: " + resolved.message,
                wi::backlog::LogLevel::Error);
            return;
        }

        playerSettings_ = resolved.start.settings;
        if (!playerEquipment_.Load(startupResult_.project.rootPath,
                startupResult_.project.projectId, playerSettings_))
        {
            diagnosticService_.Record(bridge::DiagnosticSeverity::Error,
                "runtime.player.equipment", "player.equipment.load_failed", playerEquipment_.error);
        }
        for(const auto& request:playerEquipment_.resolvedProjectiles)
            if(request.fireMode==bridge::EquipmentFireMode::Projectile &&
               !projectileVisuals_.Prepare(startupResult_.project.rootPath,
                    startupResult_.packageRelativeLaunch?startupResult_.packageRootPath:"",
                    startupResult_.project.projectId,request.projectile))
                diagnosticService_.Record(bridge::DiagnosticSeverity::Error,
                    "runtime.projectile","projectile.visual_prepare_failed",projectileVisuals_.error);
        renderer_.SetProjectileAim(playerEquipment_.ready && !playerEquipment_.resolvedProjectiles.empty());
        // Authored equipment owns its presentation. Keep the original WISCENE
        // settings intact; this resolved copy belongs only to Runtime.
        playerSettings_.firstPersonArmsAssetId =
            playerEquipment_.Presentation(playerSettings_.firstPersonArmsAssetId);


        std::string error;
        if (!bridge::SpawnRuntimePlayer(
                scenes_.GetScene(),
                resolved.start,
                player_,
                error,
                playerSettings_))
        {
            diagnosticService_.Record(bridge::DiagnosticSeverity::Error, "RuntimeApplication.cpp:SyncPlayerForScene",
                "player.spawn.failed", error);
            wi::backlog::post(
                "Renegade Runtime: could not possess Player Start: " + error,
                wi::backlog::LogLevel::Error);
            return;
        }
        if (!SpawnRuntimePlayerViewRig(
                scenes_.GetScene(),
                playerViewRig_,
                player_.entity,
                playerSettings_.eyeHeight,
                error))
        {
            diagnosticService_.Record(
                bridge::DiagnosticSeverity::Error,
                "runtime.player.view_rig",
                "player.view_rig.spawn.failed",
                error);
            wi::backlog::post(
                "Renegade Runtime: Player spawned but the first-person View Rig could not be created: " +
                    error,
                wi::backlog::LogLevel::Error);
        }
        else
        {
            diagnosticService_.Record(
                bridge::DiagnosticSeverity::Info,
                "runtime.player.view_rig",
                "player.view_rig.spawned",
                "First-person primary/off-hand View Rig spawned on Wicked foreground rendering.");

            if (!playerSettings_.firstPersonArmsAssetId.empty())
            {
                std::string armsError;
                const bool loaded = startupResult_.packageRelativeLaunch
                    ? LoadPackagedRuntimePlayerViewAsset(
                        scenes_.GetScene(),
                        playerViewRig_,
                        startupResult_.packageRootPath,
                        startupResult_.project.projectId,
                        playerSettings_.firstPersonArmsAssetId,
                        armsError)
                    : LoadRuntimePlayerViewAsset(
                        scenes_.GetScene(),
                        playerViewRig_,
                        startupResult_.project.rootPath,
                        startupResult_.project.projectId,
                        playerSettings_.firstPersonArmsAssetId,
                        armsError);

                if (!loaded)
                {
                    diagnosticService_.Record(
                        bridge::DiagnosticSeverity::Error,
                        "runtime.player.view_rig",
                        "player.view_rig.asset_failed",
                        armsError);
                    wi::backlog::post(
                        "Renegade Runtime: first-person arms asset could not be loaded; retaining P1 proxy geometry: " +
                            armsError,
                        wi::backlog::LogLevel::Error);
                }
                else
                {
                    diagnosticService_.Record(
                        bridge::DiagnosticSeverity::Info,
                        "runtime.player.view_rig",
                        "player.view_rig.asset_loaded",
                        "Governed first-person arms asset loaded: " +
                            playerSettings_.firstPersonArmsAssetId);

                    std::string animationError;
                    if (!InitializeRuntimePlayerViewAnimations(
                            scenes_.GetScene(),
                            playerViewRig_,
                            playerViewAnimation_,
                            animationError))
                    {
                        diagnosticService_.Record(
                            bridge::DiagnosticSeverity::Error,
                            "runtime.player.view_rig",
                            "player.view_rig.animation_failed",
                            animationError);
                    }
                    else
                    {
                        (void)RequestRuntimePlayerViewAnimation(
                            scenes_.GetScene(),
                            playerViewAnimation_,
                            PlayerViewAction::Idle);
                        diagnosticService_.Record(
                            bridge::DiagnosticSeverity::Info,
                            "runtime.player.view_rig",
                            "player.view_rig.animation_ready",
                            "First-person arms native animation binding is ready.");
                    }

                    wi::backlog::post(
                        "Renegade Runtime: loaded governed first-person arms asset.",
                        wi::backlog::LogLevel::Default);
                }
            }
        }

        wi::backlog::post(
            "Renegade Runtime: possessed Player Start with the Wicked character controller.",
            wi::backlog::LogLevel::Default);
    }

    void RuntimeApplication::SyncAudioForScene()
    {
        if (audioSceneRevision_ == scenes_.Revision())
            return;

        audioPauseState_ = {};
        std::string impactAudioError;
        if (!impactAudio_.Prepare(startupResult_.project.rootPath,
                startupResult_.packageRelativeLaunch ? startupResult_.packageRootPath : "",
                startupResult_.project.projectId, impactAudioError, true))
            diagnosticService_.Record(bridge::DiagnosticSeverity::Error,
                "runtime.audio", "impact.audio.prepare_failed", impactAudioError);
        else
            diagnosticService_.Record(bridge::DiagnosticSeverity::Info,
                "runtime.audio", "impact.audio.ready", "clips=" + std::to_string(impactAudio_.ClipCount()));
        impactAudio_.SetPaused(paused_);
        bridge::ActivateSceneAudio(scenes_.GetScene());
        if (paused_)
        {
            bridge::SetSceneAudioPaused(
                scenes_.GetScene(), true, audioPauseState_);
        }
        audioSceneRevision_ = scenes_.Revision();
        diagnosticService_.Record(bridge::DiagnosticSeverity::Info,
            "runtime.audio", "audio.scene.synced",
            "Authored scene audio synchronized");
        wi::backlog::post(
            "Renegade Runtime: applied authored Scene audio mix and Play On Start sources.",
            wi::backlog::LogLevel::Default);
    }

    void RuntimeApplication::SyncCreatorScriptsForScene()
    {
        if (scriptSceneRevision_ == scenes_.Revision())
            return;

        if (creatorScripts_.IsRunning())
            creatorScripts_.StopScene();
        ReportCreatorScriptDiagnostics();

        scriptSceneRevision_ = scenes_.Revision();
        if (startupResult_.startupScenePath.empty())
            return;

        std::string error;
        if (!creatorScripts_.StartSceneFromCompanion(
                startupResult_.startupScenePath,
                startupResult_.project.projectId,
                scenes_.GetScene(),
                startupResult_.project.rootPath,
                error))
        {
            diagnosticService_.Record(bridge::DiagnosticSeverity::Error, "RuntimeApplication.cpp:SyncCreatorScriptsForScene",
                "script.start.failed", error);
            wi::backlog::post(
                "Renegade Runtime: governed creator scripts could not start: " +
                    error,
                wi::backlog::LogLevel::Error);
            return;
        }

        // StartScene/BeginScene intentionally clears diagnostics for the new
        // Level generation. Reset the publication cursor only after that
        // successful transition so every new instance failure is surfaced.
        reportedScriptDiagnostics_ = 0;
        if (paused_)
            creatorScripts_.Pause();

        ReportCreatorScriptDiagnostics();
        diagnosticService_.Record(bridge::DiagnosticSeverity::Info,
            "runtime.script", "script.scene.started",
            "Governed creator Lua started " +
                std::to_string(creatorScripts_.ActiveInstanceCount()) +
                " script instance(s)");
        wi::backlog::post(
            "Renegade Runtime: governed creator Lua started " +
                std::to_string(creatorScripts_.ActiveInstanceCount()) +
                " script instance(s) for the active Level.",
            wi::backlog::LogLevel::Default);
    }

    void RuntimeApplication::StopCreatorScripts() noexcept
    {
        if (creatorScripts_.IsRunning())
            creatorScripts_.StopScene();
        renderer_.SetInteractionPrompt({});
        ReportCreatorScriptDiagnostics();
        scriptSceneRevision_ = 0;
    }

    void RuntimeApplication::ReportCreatorScriptDiagnostics()
    {
        const auto& diagnostics = creatorScripts_.Diagnostics();
        while (reportedScriptDiagnostics_ < diagnostics.size())
        {
            const auto& diagnostic =
                diagnostics[reportedScriptDiagnostics_++];
            std::string message =
                "Renegade creator Lua [" + diagnostic.scriptInstanceId + "] ";
            if (!diagnostic.sourcePath.empty())
                message += diagnostic.sourcePath + " ";
            if (!diagnostic.callback.empty())
                message += "(" + diagnostic.callback + ") ";
            message += diagnostic.message;
            diagnosticService_.Record(
                diagnostic.disabledInstance
                    ? bridge::DiagnosticSeverity::Error
                    : bridge::DiagnosticSeverity::Info,
                "runtime.script", diagnostic.callback.empty()
                    ? "script.diagnostic" : "script." + diagnostic.callback,
                message);
            wi::backlog::post(
                message,
                diagnostic.disabledInstance
                    ? wi::backlog::LogLevel::Error
                    : wi::backlog::LogLevel::Default);
        }
    }

    bool RuntimeApplication::LoadGameplayInput(std::string& error)
    {
        if (bridge::ReadGameplayInputMap(
                startupResult_.project.rootPath, inputMap_, error))
        {
            error.clear();
            return true;
        }

        if (startupResult_.packageRelativeLaunch)
        {
            const std::string declaration =
                "data:" +
                std::string(bridge::GameplayInputDocumentRelativePath);
            const bool governedInputMap =
                std::find(
                    startupResult_.project.alwaysInclude.begin(),
                    startupResult_.project.alwaysInclude.end(),
                    declaration) != startupResult_.project.alwaysInclude.end();

            if (governedInputMap)
            {
                error = "Packaged Runtime requires '" +
                    std::string(bridge::GameplayInputDocumentRelativePath) +
                    "' in the dependency closure. " + error;
                return false;
            }

            inputMap_ = bridge::MakeDefaultGameplayInputMap();
            wi::backlog::post(
                "Renegade Runtime: legacy packaged project has no gameplay input-map declaration; using the accepted Gate 1 defaults.",
                wi::backlog::LogLevel::Default);
            error.clear();
            return true;
        }

        bool created = false;
        if (!bridge::EnsureGameplayInputMap(
                startupResult_.project.rootPath,
                inputMap_,
                created,
                error))
        {
            return false;
        }
        if (created)
        {
            wi::backlog::post(
                "Renegade Runtime: created the project gameplay input-map with Gate 1 defaults.",
                wi::backlog::LogLevel::Default);
        }
        error.clear();
        return true;
    }

    void RuntimeApplication::SetPaused(const bool paused) noexcept
    {
        manualPauseRequested_ = paused;
        RefreshEffectivePause();
    }

    void RuntimeApplication::RefreshUiComposition()
    {
        if (!screenPresenter_->IsLoaded()) uiComposition_.ClearPrimary();
        if(!hudSession_.IsLoaded())uiComposition_.ClearHud();
        const bool wasHudVisible=uiComposition_.IsHudVisible();
        std::string error;
        (void)uiComposition_.RefreshFocusAvailability(screenPresenter_->IsLoaded() && screenController_->HasFocus(),
            hudSession_.IsLoaded() && hudSession_.Controller()->HasFocus(),error);
        if(!uiComposition_.SetHudUnderlayAllowed(uiComposition_.Resolve().hudUnderlayAllowed,error))
            wi::backlog::post("Renegade Runtime HUD composition: "+error,wi::backlog::LogLevel::Error);
        const auto state = uiComposition_.Resolve();
        uiPauseRequested_ = state.gameplayPauseRequested;
        screenPresenter_->SetInputEnabled(uiComposition_.PrimaryAcceptsInput());
        if(hudSession_.IsLoaded())
        {
            hudSession_.Presenter()->SetInputEnabled(uiComposition_.HudAcceptsInput());
            hudSession_.Presenter()->SetPresentationVisible(uiComposition_.IsHudVisible());
        }
        if(wasHudVisible!=uiComposition_.IsHudVisible())RefreshUiBindingSources();
        if(uiDrawOrder_!=state.drawOrder)
        {
            screenPresenter_->DetachPresentation(renderer_);
            if(hudSession_.IsLoaded())hudSession_.Presenter()->DetachPresentation(renderer_);
            for(auto item=state.drawOrder.rbegin();item!=state.drawOrder.rend();++item)
            {
                if(*item==uiComposition_.PrimaryIdentity())screenPresenter_->AttachPresentation(renderer_);
                else if(*item==uiComposition_.HudIdentity() && hudSession_.IsLoaded())
                    hudSession_.Presenter()->AttachPresentation(renderer_);
            }
            uiDrawOrder_=state.drawOrder;
            RefreshUiBindingSources();
        }
        RefreshEffectivePause();
        if (state.cursor == bridge::ScreenCursorPolicy::Show) wi::input::HidePointer(false);
        else if (state.cursor == bridge::ScreenCursorPolicy::Hide) wi::input::HidePointer(true);
    }

    void RuntimeApplication::RefreshEffectivePause() noexcept
    {
        renderer_.SetPauseOverlayVisible(manualPauseRequested_ && !uiPauseRequested_);
        const bool paused = manualPauseRequested_ || uiPauseRequested_;
        if (paused_ == paused)
            return;

        if (paused)
        {
            physicsSimulationBeforePause_ = wi::physics::IsSimulationEnabled();
            wi::physics::SetSimulationEnabled(false);
        }
        else
        {
            wi::physics::SetSimulationEnabled(physicsSimulationBeforePause_);
        }
        paused_ = paused;
        impactAudio_.SetPaused(paused_);
        renderer_.SetPaused(paused_);
        bridge::SetSceneAudioPaused(
            scenes_.GetScene(), paused_, audioPauseState_);
        if (paused_)
            creatorScripts_.Pause();
        else
            creatorScripts_.Resume();
        ReportCreatorScriptDiagnostics();
        wi::backlog::post(
            paused_
                ? "Renegade Runtime: play session paused."
                : "Renegade Runtime: play session resumed.",
            wi::backlog::LogLevel::Default);
        ++evidenceRevision_;
    }

    bool RuntimeApplication::ResetPlaySession(std::string& error)
    {
        if(!LoadGameplayAttributes(error))return false;
        uiComposition_.ClearPrimary();
        uiComposition_.ClearHud();hudSession_.Reset(renderer_);hudSelection_.LeaveLevel();
        hudContextEvents_.LeaveLevel();
        hudLevelKey_.clear();hudAppliedSelectionRevision_=0;
        uiDrawOrder_.clear();pendingHudActions_.clear();pendingActions_.clear();
        pendingScreenExit_.reset();pendingEnterOverride_.reset();
        pendingHudSwitch_.reset();hudFailedRevision_.reset();hudExitRequested_=false;hudTransitionError_.clear();
        uiPauseRequested_ = false;
        uiSnapshotPublisher_.Stop();
        uiLivePublishedMs_=0;
        diagnosticService_.SetState("ui_live_binding",{{"available",false}});
        uiPublisherStarted_ = false;
        uiGameplayRevision_ = 0;
        uiBindingCpuTiming_.Reset();
        uiBindingSources_.clear();
        uiBindingDiagnostic_.clear();
        if (creatorScripts_.IsRunning())
            creatorScripts_.ResetScene();
        ReportCreatorScriptDiagnostics();
        scriptSceneRevision_ = 0;
        SetPaused(false);
        DespawnRuntimePlayerViewRig(scenes_.GetScene(), playerViewRig_);
        bridge::DespawnRuntimePlayer(scenes_.GetScene(), player_);
        player_ = {};
        playerSettings_ = {};
        playerEquipment_ = {};
        pendingProjectileShots_.clear();
        impactAudio_.StopVoices();
        heldFireball_.Reset(scenes_.GetScene());
        channelVisual_.Reset(scenes_.GetScene());channel_.Reset();channelScorchTrail_.Reset();lightningStorm_.Reset(scenes_.GetScene());lightningColumnReview_.Reset(scenes_.GetScene());lightningStormCastLatch_=false;
        projectileVisuals_.Reset(scenes_.GetScene());
        projectiles_.Reset();
        burning_.Reset(scenes_.GetScene());
        playerSceneRevision_ = 0;
        audioSceneRevision_ = 0;
        audioPauseState_ = {};
        pendingActions_.clear();
        screenPresenter_->Reset(renderer_);
        flow_ = RuntimeFlowController{};
        flowStarted_ = false;

        startupResult_ = initialBootstrapResult_;
        startupResult_.lastActionId.clear();
        startupResult_.lastActionWidgetId.clear();
        startupResult_.lastActionInput.clear();
        startupResult_.lastActionCode.clear();
        startupResult_.lastActionMessage.clear();
        startupResult_.lastActionSequence = 0;
        startupResult_.screenLoaded = false;
        startupResult_.screenWasLoaded = false;
        startupResult_.flowTrace.clear();
        startupResult_.flowNodeId.clear();
        startupResult_.flowNodeName.clear();
        startupResult_.flowEntry.clear();
        startupResult_.flowTerminalAction = bridge::FlowTerminalAction::None;

        // On every reset restore governed action registration irrespective of
        // whether the project enters through a legacy startup Screen or Flow.
        if (!ConfigureActions(error))
        {
            startupResult_.succeeded=false;
            startupResult_.code=RuntimeBootstrapCode::ScreenLoadFailed;
            startupResult_.message="Could not reset Runtime actions: "+error;
            ++evidenceRevision_;
            return false;
        }
        if (!startupResult_.startupScreenPath.empty())
        {
            if (!LoadStartupScreen(error))
            {
                startupResult_.succeeded = false;
                startupResult_.code = RuntimeBootstrapCode::ScreenLoadFailed;
                startupResult_.message =
                    "Could not reset project Runtime screen: " + error;
                ++evidenceRevision_;
                return false;
            }
            startupResult_.succeeded = true;
            startupResult_.code = RuntimeBootstrapCode::Success;
            startupResult_.screenLoaded = true;
            startupResult_.screenWasLoaded = true;
            startupResult_.message = "Reset Runtime to the project startup screen.";
        }
        else if (!startupResult_.startupFlowPath.empty())
        {
            startupResult_ = LoadRuntimeProjectFlow(
                scenes_, flow_, std::move(startupResult_));
            flowStarted_ = startupResult_.succeeded;
            if (!startupResult_.succeeded)
            {
                error = startupResult_.message;
                ++evidenceRevision_;
                return false;
            }
            const auto* current = flow_.CurrentNode();
            if (current != nullptr && current->kind == bridge::FlowNodeKind::Screen)
            {
                if (!LoadCurrentFlowScreen(error))
                {
                    startupResult_.succeeded = false;
                    startupResult_.code = RuntimeBootstrapCode::ScreenLoadFailed;
                    startupResult_.message =
                        "Could not reset Story Flow Runtime screen: " + error;
                    ++evidenceRevision_;
                    return false;
                }
            }
        }
        else
        {
            startupResult_ =
                LoadRuntimeProjectScene(scenes_, std::move(startupResult_));
            if (!startupResult_.succeeded)
            {
                error = startupResult_.message;
                ++evidenceRevision_;
                return false;
            }
        }

        // Reset can load a Level after this frame's Character discovery.
        // Seed inactive native controllers before Wicked writes their pose back;
        // full governed Character/AI initialization still owns the next frame.
        for(const auto entity:bridge::CollectCharacters(scenes_.GetScene()))
            bridge::SyncNativeCharacterPoseFromTransform(scenes_.GetScene(),entity);
        SyncPlayerForScene();
        SyncAudioForScene();
        if (!screenPresenter_->IsLoaded())
            SyncCreatorScriptsForScene();
        wi::backlog::post(
            "Renegade Runtime: play session reset to its authored startup state.",
            wi::backlog::LogLevel::Default);
        error.clear();
        ++evidenceRevision_;
        return true;
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

        // Built-in player accessibility actions are handled by Runtime, not
        // routed as StoryFlow outcomes. They work for modal Screens and HUDs,
        // with per-user persistence shared across project launches.
        for(const auto& entry:std::initializer_list<std::pair<const char*,int>>{
            {bridge::RuntimeUiReducedMotionToggleAction,0},
            {bridge::RuntimeUiReducedMotionOnAction,1},
            {bridge::RuntimeUiReducedMotionOffAction,2}})
        {
            const int mode=entry.second;
            if(!actions_.Register(entry.first,[this,mode](const RuntimeActionRequest& request)
            {
                const bool next=mode==0?!uiAccessibility_.reducedMotion:mode==1;
                std::string failure;
                if(!ChangeReducedMotion(next,failure))
                    return RuntimeActionResult{false,RuntimeActionCode::ActionUnavailable,
                        request,"Player reduced-motion setting not saved: "+failure};
                return RuntimeActionResult{true,RuntimeActionCode::Success,request,
                    next?"Player UI reduced motion enabled.":"Player UI reduced motion disabled."};
            },error))return false;
        }
        for(const auto& entry:std::initializer_list<std::pair<const char*,int>>{
            {bridge::RuntimeUiTextScaleIncreaseAction,0},
            {bridge::RuntimeUiTextScaleDecreaseAction,1},
            {bridge::RuntimeUiTextScaleResetAction,2},
            {bridge::RuntimeUiContrastCycleAction,3},
            {bridge::RuntimeUiFocusColorCycleAction,4},
            {bridge::RuntimeUiFocusWidthCycleAction,5},
            {bridge::RuntimeUiScaleIncreaseAction,6},{bridge::RuntimeUiScaleDecreaseAction,7},
            {bridge::RuntimeUiScaleResetAction,8},{bridge::RuntimeUiTargetSizeCycleAction,9}})
        {
            const int mode=entry.second;
            if(!actions_.Register(entry.first,[this,mode](const RuntimeActionRequest& request)
            {
                auto settings=uiAccessibility_;
                auto& p=settings.presentation;
                switch(mode)
                {
                case 0:p.textScale=std::min(2.0f,p.textScale+0.25f);break;
                case 1:p.textScale=std::max(1.0f,p.textScale-0.25f);break;
                case 2:p.textScale=1;break;
                case 3:p.contrast=(p.contrast+1)%4;break;
                case 4:p.focusStyle=(p.focusStyle+1)%3;break;
                case 5:p.focusWidth=p.focusWidth>=6?1:std::min(6.0f,std::floor(p.focusWidth)+1);break;
                case 6:p.uiScale=std::min(2.0f,p.uiScale+0.25f);break;
                case 7:p.uiScale=std::max(1.0f,p.uiScale-0.25f);break;
                case 8:p.uiScale=1;break;
                case 9:p.minimumTargetSize=p.minimumTargetSize<32?32:
                    p.minimumTargetSize<44?44:p.minimumTargetSize<56?56:0;break;
                }
                std::string failure;
                if(!ChangeUiAccessibility(settings,failure))
                    return RuntimeActionResult{false,RuntimeActionCode::ActionUnavailable,request,
                        "Player UI accessibility setting not saved: "+failure};
                return RuntimeActionResult{true,RuntimeActionCode::Success,request,
                    "Player UI accessibility setting saved."};
            },error))return false;
        }
        error.clear();
        return true;
    }

    bool RuntimeApplication::LoadPrimaryScreenDocument(const bridge::ScreenDocument& document,std::string& error)
    {
        if(!document.enabledInGame){error="Story Flow targets a disabled Screen.";return false;}
        bridge::ScreenDocument effective;
        auto controller=std::make_unique<RuntimeScreenController>();
        auto presenter=std::make_unique<RuntimeScreenPresenter>();
        if(!bridge::ResolveScreenTheme(startupResult_.project.rootPath,document,effective,error)||
            !controller->Initialize(effective,error))return false;
        presenter->SetReducedMotion(uiAccessibility_.reducedMotion);
        presenter->SetAccessibilitySettings(uiAccessibility_.presentation);
        if(!presenter->Load(document,startupResult_.project.rootPath,renderer_,*controller,
            [this](RuntimeActionRequest request){QueueAction(std::move(request));},error))
        {presenter->Reset(renderer_);return false;}
        if(pendingEnterOverride_&&!presenter->OverrideEnterTransition(*pendingEnterOverride_,renderer_,error))
        {presenter->Reset(renderer_);return false;}
        if(!uiComposition_.ReplacePrimary(document,controller->HasFocus(),error))
        {presenter->Reset(renderer_);return false;}
        // Both candidate addresses stay stable after ownership transfer.
        screenPresenter_->Reset(renderer_);
        screenController_=std::move(controller);screenPresenter_=std::move(presenter);
        pendingEnterOverride_.reset();
        RefreshUiComposition();error.clear();return true;
    }

    void RuntimeApplication::CancelScreenExit()
    {
        pendingScreenExit_.reset();pendingEnterOverride_.reset();
        screenPresenter_->CancelExit();
        std::string ignored;(void)uiComposition_.CancelPrimaryExit(ignored);
        RefreshUiComposition();
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
        if(!LoadPrimaryScreenDocument(document,error))return false;
        startupResult_.screenDocumentId = document.envelope.documentId;
        const auto* focused = screenController_->FocusedWidget();
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
        if(!LoadPrimaryScreenDocument(document,error))return false;
        startupResult_.screenDocumentId = document.envelope.documentId;
        const auto* focused = screenController_->FocusedWidget();
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
        bool completedExit=false;
        if(pendingScreenExit_)
        {
            pendingActions_.clear();
            if(!screenPresenter_->ExitComplete())return;
            pendingActions_.push_back(*pendingScreenExit_);
            pendingScreenExit_.reset();completedExit=true;
        }
        std::vector<RuntimeActionRequest> pending;pending.swap(pendingActions_);
        for(const auto& request:pending)
        {
            if(bridge::IsRuntimeUiAccessibilityAction(request.actionId))
            {RecordAction(actions_.Dispatch(request));continue;}
            const auto* current=flowStarted_?flow_.CurrentNode():nullptr;
            const bool flowScreen=current&&current->kind==bridge::FlowNodeKind::Screen;
            const bool navigation=flowScreen||request.actionId==bridge::RuntimeScreenPlayAction||
                request.actionId==bridge::RuntimeScreenQuitAction;
            if(navigation&&screenPresenter_->IsLoaded()&&!completedExit)
            {
                const auto* widget=screenController_->FindWidget(request.widgetId);
                const auto* overridePolicy=widget&&widget->transitionOverride?&*widget->transitionOverride:nullptr;
                pendingEnterOverride_=overridePolicy?
                    std::optional<std::vector<bridge::ScreenAnimationTrack>>(overridePolicy->enter):std::nullopt;
                std::string failure;
                if(!screenPresenter_->BeginExit(overridePolicy?&overridePolicy->exit:nullptr,failure))
                {RecordAction({false,RuntimeActionCode::ActionUnavailable,request,failure});CancelScreenExit();return;}
                if(!screenPresenter_->ExitComplete())
                {
                    pendingScreenExit_=request;
                    (void)uiComposition_.BeginPrimaryExit(failure);
                    RefreshUiComposition();return;
                }
            }
            const bool flowPause=current&&current->kind==bridge::FlowNodeKind::Level&&request.actionId=="ui.pause";
            if(flowScreen||flowPause)
            {
                const auto oldFlow=flow_;
                const auto oldBootstrap=startupResult_;
                RuntimeActionResult result;result.request=request;
                auto step=flow_.EmitOutcome(request.actionId);
                std::string error;
                if(!step.succeeded||!flow_.ApplyStep(scenes_,startupResult_,step,error))
                {
                    flow_=oldFlow;startupResult_=oldBootstrap;CancelScreenExit();
                    result.code=RuntimeActionCode::FlowStartFailed;
                    result.message=step.succeeded?error:step.message;
                    RecordAction(result);return;
                }
                const auto* destination=flow_.CurrentNode();
                if(destination&&destination->kind==bridge::FlowNodeKind::Screen)
                {
                    if(!LoadCurrentFlowScreen(error))
                    {
                        flow_=oldFlow;startupResult_=oldBootstrap;CancelScreenExit();
                        result.code=RuntimeActionCode::FlowStartFailed;result.message=error;
                        RecordAction(result);return;
                    }
                }
                else
                {
                    screenPresenter_->Reset(renderer_);uiComposition_.ClearPrimary();
                    startupResult_.screenLoaded=false;pendingEnterOverride_.reset();
                }
                if(startupResult_.flowTerminalAction==bridge::FlowTerminalAction::Quit)quitRequested_=true;
                result.succeeded=true;result.code=RuntimeActionCode::Success;
                result.message="Runtime Screen action advanced Story Flow to '"+startupResult_.flowNodeName+"'.";
                RecordAction(result);return; // drop additional source-instance requests
            }
            const auto oldFlow=flow_;const auto oldBootstrap=startupResult_;
            RuntimeActionResult result=actions_.Dispatch(request);
            if(result.succeeded&&request.actionId==bridge::RuntimeScreenPlayAction)
            {
                const auto* destination=flow_.CurrentNode();
                if(destination&&destination->kind==bridge::FlowNodeKind::Screen)
                {
                    std::string error;
                    if(!LoadCurrentFlowScreen(error))
                    {
                        flow_=oldFlow;startupResult_=oldBootstrap;flowStarted_=false;CancelScreenExit();
                        result.succeeded=false;result.code=RuntimeActionCode::FlowStartFailed;result.message=error;
                    }
                }
                else
                {
                    screenPresenter_->Reset(renderer_);uiComposition_.ClearPrimary();
                    startupResult_.screenLoaded=false;pendingEnterOverride_.reset();
                }
            }
            if(!result.succeeded&&navigation)CancelScreenExit();
            RecordAction(result);
            if(navigation)return;
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
        const auto* focused = screenController_->FocusedWidget();
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
