#include "RuntimeApplication.h"
#include "RuntimeProjectileAim.h"
#include "renegade/bridge/LaunchSocketService.h"

#include "renegade/bridge/PhysicsLuaService.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/PrecipitationService.h"

#include <algorithm>
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
        renderSettingsSceneRevision_ = 0;
    }

    void RuntimeRenderPath::SetPaused(const bool paused) noexcept
    {
        paused_ = paused;
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
        RenderPath3D::Update(dt);
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
        diagnosticService_.StopLocalEndpoint();
        StopCreatorScripts();
        playerEquipment_ = {};
        pendingProjectileShots_.clear();
        impactAudio_.StopVoices();
        projectileVisuals_.Reset(scenes_.GetScene());
        projectiles_.Reset();
        ResetRuntimePlayerViewAnimations(
            scenes_.GetScene(), playerViewAnimation_);
        DespawnRuntimePlayerViewRig(scenes_.GetScene(), playerViewRig_);
        creatorScripts_.Shutdown();
        scriptSceneRevision_ = 0;
        reportedScriptDiagnostics_ = 0;
    }

    void RuntimeApplication::Initialize()
    {
        wi::Application::Initialize();
        diagnosticService_.Record(bridge::DiagnosticSeverity::Info,
            "runtime", "runtime.initialized", "Runtime initialized");
        diagnosticService_.Identify("runtime");
        diagnosticService_.StartLocalEndpoint(38742);

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
        UpdateLiveDiagnostics();
        SyncAudioForScene();
        bridge::RefreshPrecipitationVisual(scenes_.GetScene());

        const bool gameplayFrame = !screenPresenter_.IsLoaded();
        if (gameplayFrame)
        {
            // Feed current input into the one authoritative Player before
            // Wicked advances physics. The View Rig is parented to that Player,
            // so the normal physics -> hierarchy -> object update carries the
            // first-person presentation to the same post-physics position.
            gameplayInput_ = bridge::CaptureGameplayInput(inputMap_, dt);
            const auto& gameplayInput = gameplayInput_;
            creatorScripts_.SetGameplayState(&player_, &gameplayInput_);
            if (gameplayInput.pausePressed)
                SetPaused(!paused_);

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

            if (!screenPresenter_.IsLoaded())
            {
                SyncPlayerForScene();
                if (player_.IsSpawned())
                {
                    if (!paused_)
                    {
                        auto playerInput=gameplayInput.player;
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
                            melee.directional, chainWindow, melee.direction, melee.fullChargeSeconds, melee.queuedReleaseSeconds);
                    if(playerEquipment_.chainedChargeStarted) {
                        auto& hand=playerViewAnimation_.handLayers;
                        hand.direction=playerEquipment_.chainedDirection;
                        hand.chargePhase=1;hand.chargeSeconds=std::max(0.0f,playerEquipment_.chainedSeconds-(playerEquipment_.chargePresentation?dt:0.0f));
                        hand.rightTime=0;hand.gestureX=hand.gestureY=0;
                    }
                    if(!paused_ && playerEquipment_.chainedReleasedCharge)
                        playerViewAnimation_.handLayers.chargeSeconds=std::max(0.0f,playerEquipment_.chainedSeconds-(playerEquipment_.chargePresentation?dt:0.0f));
                    UpdateRuntimePlayerViewAnimations(
                        scenes_.GetScene(),
                        playerViewAnimation_,
                        playerViewRig_.action,
                        paused_ ? 0.0f : dt,
                        !paused_ && equipmentInput.firePressed,
                        !paused_ && equipmentInput.reloadPressed,
                        equipmentInput.aimDown,
                        !paused_ && equipmentInput.toggleEquipmentPressed,
                        grounded, playerEquipment_.chargePresentation, playerEquipment_.releasePresentation,
                        playerEquipment_.offHandBlockPresentation, gameplayInput.player.lookYaw, gameplayInput.player.lookPitch, gameplayInput.cancelEquipmentPressed);
                    for (auto& request : playerEquipment_.TakeProjectileRequests()) {
                        // Native firearm acceptance includes ammunition, cooldown,
                        // equipped state and successfully resolved paired clips.
                        const bool accepted = request.action == bridge::EquipmentAction::PrimaryUse
                            ? playerViewAnimation_.shotAccepted
                            : request.action == bridge::EquipmentAction::Release &&
                              playerViewAnimation_.pairedAssembly && playerViewAnimation_.oneShotPlaying &&
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
        if (!screenPresenter_.IsLoaded())
        {
            if (player_.IsSpawned() && renderer_.camera != nullptr)
                bridge::ApplyRuntimePlayerCamera(scenes_.GetScene(), player_,
                    *renderer_.camera, playerSettings_);
            UpdatePlayerProjectiles(paused_ ? 0.0f : dt);
        }

        // Wicked owns the actual Jolt step, hierarchy propagation and GPU
        // instance update. A paused session still refreshes the device while
        // advancing the active 3D path with zero simulation time.
        wi::Application::Update(paused_ ? 0.0f : dt);
        // Current-pose marks must reach decal visibility/render upload after
        // native skinning, while transient entity mutation remains before it.
        if (!screenPresenter_.IsLoaded())
            projectileVisuals_.RefreshImpactMarkPose(scenes_.GetScene());


        if (!screenPresenter_.IsLoaded())
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
                projectileVisuals_.Reset(scenes_.GetScene());
                projectiles_.Reset();
            }
            pendingProjectileShots_.clear();
            playerEquipment_.scheduledProjectiles.clear();
            renderer_.SetProjectileAim(false);
            renderer_.SetProjectileContacts({});
            renderer_.ClearHitConfirmation();
            renderer_.SetInteractionPrompt({});
            StopCreatorScripts();
            if (paused_)
                SetPaused(false);
            wi::input::HidePointer(false);
            screenPresenter_.UpdateInput(renderer_, screenController_);
        }

        ProcessPendingActions();
        if (screenPresenter_.IsLoaded())
            StopCreatorScripts();
    }

    void RuntimeApplication::UpdatePlayerProjectiles(const float dt)
    {
        if (!player_.IsSpawned() || playerSceneRevision_ != scenes_.Revision() ||
            renderer_.camera == nullptr) {
            pendingProjectileShots_.clear();
            renderer_.SetProjectileContacts({});
            return;
        }
        auto& scene = scenes_.GetScene();
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
            const float time=playerViewAnimation_.pairedAssembly ? playerViewAnimation_.pairedTime :
                clip ? clip->timer-clip->start : 0;
            auto due=playerEquipment_.ReleaseProjectiles(playerViewAnimation_.activeClip,time,!playerViewAnimation_.equipped);
            pendingProjectileShots_.insert(pendingProjectileShots_.end(),
                std::make_move_iterator(due.begin()),std::make_move_iterator(due.end()));
        }
        const ProjectileOwnerBinding owner{RuntimePlayerKnowledgeId, player_.entity, &projectileVisuals_.roots};
        const auto emit = [this](bridge::GameplayEvent event, std::string& error) {
            diagnosticService_.Record(bridge::DiagnosticSeverity::Info,
                "runtime.projectile", event.name, event.payload);
            // Governed queue remains the shared script/event boundary.
            return creatorScripts_.EnqueueGameplayEvent(std::move(event), error);
        };
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
                    if(!projectileVisuals_.Spawn(scene,request.projectile.assetId,id,!request.projectile.meshAssetId.empty()))
                        diagnosticService_.Record(bridge::DiagnosticSeverity::Error,
                            "runtime.projectile","projectile.visual_failed",projectileVisuals_.error);
                    std::string ignored;
                    (void)emit({0, "projectile.launched",
                        "projectile=" + std::to_string(id) +
                        ";asset=" + request.projectile.assetId +
                        ";equipment=" + request.equipmentId,
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
            if (damage.characterContact)
                renderer_.ConfirmTargetHit();
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
        projectileVisuals_.Reset(scenes_.GetScene());
        projectiles_.Reset();
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
        projectileVisuals_.Reset(scenes_.GetScene());
        projectiles_.Reset();
        playerSceneRevision_ = 0;
        audioSceneRevision_ = 0;
        audioPauseState_ = {};
        pendingActions_.clear();
        screenPresenter_.Reset(renderer_);
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

        if (!startupResult_.startupScreenPath.empty())
        {
            if (!ConfigureActions(error) || !LoadStartupScreen(error))
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

        SyncPlayerForScene();
        SyncAudioForScene();
        if (!screenPresenter_.IsLoaded())
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
