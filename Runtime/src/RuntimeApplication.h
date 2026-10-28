#include "RuntimeGameplayAttributeWorld.h"
#pragma once
#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/PlaceableWaterfallRenderer.h"

#include <WickedEngine.h>

#include "RuntimeActions.h"
#include "RuntimeBootstrap.h"
#include "RuntimeCharacterAnimation.h"
#include "RuntimeCharacterDecision.h"
#include "RuntimeCharacterPerception.h"
#include "RuntimeCharacterSystem.h"
#include "RuntimeCombatDecision.h"
#include "RuntimeCombatService.h"
#include "RuntimePlayerViewAsset.h"
#include "RuntimePlayerViewAnimation.h"
#include "RuntimePlayerViewRig.h"
#include "RuntimeEquipmentLoadout.h"
#include "RuntimeProjectileSession.h"
#include "RuntimeProjectileVisuals.h"
#include "RuntimeBurningStatus.h"
#include "RuntimeChannelSession.h"
#include "RuntimeChannelVisual.h"
#include "RuntimeChannelScorchTrail.h"
#include "RuntimeLightningStorm.h"
#include "RuntimeLightningColumn.h"
#include "RuntimeFlow.h"
#include "RuntimeScreen.h"
#include "RuntimeUiGameplaySnapshot.h"
#include "RuntimeUiComposition.h"
#include "RuntimeUiTimingWindow.h"
#include "renegade/bridge/ScreenPerformanceBudget.h"
#include "RuntimeUiAccessibilityPreferences.h"
#include "renegade/bridge/HudContextEventRules.h"
#include "RuntimeHudSession.h"
#include "renegade/bridge/HudSelection.h"
#include "RuntimeScriptRuntime.h"
#include "RuntimeInventorySession.h"
#include "RuntimeGameSaveSlot.h"
#include "renegade/bridge/WorldPickupController.h"
#include "renegade/bridge/InventoryItemContextActions.h"
#include "renegade/bridge/InventoryItemCatalogueService.h"
#include "renegade/bridge/InventoryOverlayView.h"
#include "renegade/bridge/InventorySelectionController.h"
#include "renegade/bridge/InventoryScreenAdapter.h"
#include "renegade/bridge/InventoryDragTransfer.h"
#include "renegade/bridge/InventoryPagedScreenAdapter.h"
#include "InventoryPointerGesture.h"
#include "renegade/bridge/AudioService.h"
#include "renegade/bridge/ImpactAudioService.h"
#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/DiagnosticService.h"
#include "renegade/bridge/GameplayInputService.h"
#include "renegade/bridge/NavigationService.h"
#include "renegade/bridge/RenderSettingsService.h"
#include "renegade/bridge/RenderLutService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/RiverUnderwaterService.h"
#include "renegade/bridge/PlayerService.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace renegade::runtime
{
    class RuntimeRenderPath final : public wi::RenderPath3D
    {
    public:
        void BindScene(
            bridge::SceneService& scenes,
            std::string projectRoot) noexcept;
        void SetPaused(bool paused) noexcept;
        void SetPauseOverlayVisible(bool visible) noexcept { pauseOverlayVisible_=visible; }
        void SetInterfaceDeltaTime(float dt) noexcept;
        void SetInteractionPrompt(std::string prompt) noexcept;
        void SetInventoryOverlay(bridge::InventoryOverlayView view, bool visible,
                                int selectedPanel=-1, int selectedRow=-1,
                                std::string status={}) noexcept
        {
            inventoryView_=std::move(view);
            inventoryVisible_=visible;
            inventorySelectedPanel_=selectedPanel;
            inventorySelectedRow_=selectedRow;
            inventoryStatus_=std::move(status);
        }
        void SetMeleePrompt(std::string prompt) { meleePrompt_=std::move(prompt); }
        void SetProjectileAim(bool enabled) { projectileAim_=enabled; }
        void SetProjectileContacts(std::vector<XMFLOAT2> contacts) { projectileContacts_=std::move(contacts); }
        void ConfirmTargetHit() { hitConfirmSeconds_=0.12f; }
        void ClearHitConfirmation() { hitConfirmSeconds_=0; }
        void SetHeldFireballParticles(const std::array<wi::ecs::Entity,4>& entities) { heldFireballParticles_=entities; }
        void PreRender() override;
        void Render() const override;
        void WriteRiverFlowDiagnostics(bridge::DiagnosticService& diagnostics) const;
        void RenderTransparents(wi::graphics::CommandList cmd) const override;
        void Load() override;
        void Update(float dt) override;
        void Compose(wi::graphics::CommandList cmd) const override;

    private:
        bridge::RiverUnderwaterRenderer riverUnderwater_;
        bridge::RiverFlowRenderer riverFlow_;
        bridge::PlaceableWaterfallRenderer waterfallRenderer_;
        void RenderWaterfallTransparents(wi::graphics::CommandList cmd) const;
        double riverFlowPrepareMilliseconds_ = 0;
        void SyncRenderSettings(bool resizeBuffersForMSAA);

        std::array<wi::ecs::Entity,4> heldFireballParticles_{};
        bridge::SceneService* scenes_ = nullptr;
        std::string projectRoot_;
        bridge::RenderSettingsState renderSettings_;
        bool renderSettingsInitialized_ = false;
        bool paused_ = false;
        bool pauseOverlayVisible_ = false;
        float interfaceDeltaTime_ = 0;
        std::string interactionPrompt_;
        bridge::InventoryOverlayView inventoryView_;
        bool inventoryVisible_=false;
        int inventorySelectedPanel_=-1;
        int inventorySelectedRow_=-1;
        std::string inventoryStatus_;
        std::string meleePrompt_;
        bool projectileAim_ = false;
        std::vector<XMFLOAT2> projectileContacts_;
        float hitConfirmSeconds_ = 0;
        std::uint64_t renderSettingsSceneRevision_ = 0;
    };

    class RuntimeApplication final : public wi::Application
    {
    public:
        void SetBootstrapResult(RuntimeBootstrapResult result);
        void SetSmokeOptions(bool autoPlay, bool exitOnComplete) noexcept;
        // Explicit QA-only launch switch; never enabled for ordinary games.
        void SetInventoryHealthTestMode(bool enabled) noexcept {inventoryHealthTestMode_=enabled;}
        void ConfigureGameSaveDirectory(std::string localAppData) {
            gameSaveLocalAppData_=std::move(localAppData);
        }
        [[nodiscard]] bool ConfigureUiAccessibilityPreferences(std::string path,std::string& error);
        [[nodiscard]] bool ChangeReducedMotion(bool enabled,std::string& error);
        [[nodiscard]] bool ChangeUiAccessibility(const RuntimeUiAccessibilityPreferences& settings,std::string& error);
        [[nodiscard]] bool ReducedMotion() const noexcept {return uiAccessibility_.reducedMotion;}
        void SetGraphicsRuntimeEvidence(
            std::string actualBackend,
            std::string capability);
        [[nodiscard]] bool StartupFinished() const noexcept;
        [[nodiscard]] const RuntimeBootstrapResult& StartupResult() const noexcept;
        [[nodiscard]] bool QuitRequested() const noexcept;
        [[nodiscard]] int ExitCode() const noexcept;
        [[nodiscard]] std::uint64_t EvidenceRevision() const noexcept;
        void ShutdownForProcessExit() noexcept;

        void Initialize() override;
        void Update(float dt) override;

    private:
        [[nodiscard]] bool ConfigureActions(std::string& error);
        [[nodiscard]] bool LoadStartupScreen(std::string& error);
        [[nodiscard]] bool LoadCurrentFlowScreen(std::string& error);
        [[nodiscard]] bool LoadGameplayInput(std::string& error);
        [[nodiscard]] bool LoadGameplayAttributes(std::string& error);
        void UpdateGameplayAttributes(float dt);
        [[nodiscard]] bool ResetPlaySession(std::string& error);
        [[nodiscard]] bool SaveQuickGame(std::string& error);
        [[nodiscard]] bool LoadQuickGame(std::string& error);
        [[nodiscard]] bool ResolveQuickSaveSceneId(std::string& sceneId,std::string& error) const;
        void SetPaused(bool paused) noexcept;
        void RefreshEffectivePause() noexcept;
        void RefreshUiComposition();
        void SyncLevelHud();
        void OnGovernedHudGameplayEvent(const bridge::GameplayEvent& event);
        void ProcessHudActions();
        void QueueAction(RuntimeActionRequest request);
        void ProcessPendingActions();
        void CancelScreenExit();
        bool LoadPrimaryScreenDocument(const bridge::ScreenDocument& document,std::string& error);
        void RecordAction(const RuntimeActionResult& result);
        void SyncPlayerForScene();
        void UpdatePlayerProjectiles(float dt);
        void SyncAudioForScene();
        void SyncCreatorScriptsForScene();
        void StopCreatorScripts() noexcept;
        void RefreshInventoryOverlay(bool gameplayFrame, bool acceptsKeyboard);
        void LoadInventoryAuthoredScreen();
        void ResetInventoryAuthoredScreen() noexcept;
        void OnInventoryScreenAction(RuntimeActionRequest request);
        bool TryUseInventoryStack(const std::string& instanceId);
        void OpenInventoryContextMenu(const std::string& action);
        void CloseInventoryContextMenu();
        void RunInventoryContextAction(const std::string& action);
        void SetInventoryContextStatus(const std::string& message);
        void OnInventoryScreenPointerDrag(const bridge::StableId& widgetId,
            bool begin,float x,float y,bool fullStack=false);
        void PollInventoryKeyboardInput();
        void PollInventoryPointerInput();
        void RefreshPagedInventoryScreen();
        void ReportCreatorScriptDiagnostics();

        bridge::SceneService scenes_;
        void RefreshUiBindingSources();
        void UpdateUiBindings();
        void UpdateLiveDiagnostics();
        std::uint64_t lastDiagnosticSampleMs_ = 0;
        std::uint64_t lastDiagnosticProjectileLaunched_ = 0;
        size_t lastDiagnosticProjectilePending_ = 0;
        bridge::DiagnosticService diagnosticService_;
        RuntimeUiTimingWindow uiBindingCpuTiming_;
        bridge::ScreenPerformanceBudget uiBudget_;
        std::string uiBudgetDiagnostic_;
        RuntimeFlowController flow_;
        RuntimeRenderPath renderer_;
        // Heap-stable instances allow transactional prepare/commit without
        // moving native callback targets or releasing the working source early.
        std::unique_ptr<RuntimeScreenController> screenController_=std::make_unique<RuntimeScreenController>();
        std::unique_ptr<RuntimeScreenPresenter> screenPresenter_=std::make_unique<RuntimeScreenPresenter>();
        std::optional<RuntimeActionRequest> pendingScreenExit_;
        std::optional<std::vector<bridge::ScreenAnimationTrack>> pendingEnterOverride_;
        RuntimeUiComposition uiComposition_;
        RuntimeUiAccessibilityPreferences uiAccessibility_;
        std::string uiAccessibilityPath_;
        RuntimeHudSession hudSession_;
        struct PendingHudSwitch
        {
            std::uint64_t revision=0;
            std::optional<bridge::ScreenDocument> destination;
        };
        std::optional<PendingHudSwitch> pendingHudSwitch_;
        std::optional<std::uint64_t> hudFailedRevision_;
        bool hudExitRequested_=false;
        std::string hudTransitionError_;
        bridge::HudSelectionController hudSelection_;
        bridge::HudContextEventRouter hudContextEvents_;
        std::string hudLevelKey_;
        std::uint64_t hudLevelSession_ = 0;
        std::uint64_t hudAppliedSelectionRevision_ = 0;
        std::vector<std::uint64_t> uiDrawOrder_;
        std::vector<std::pair<std::uint64_t,RuntimeActionRequest>> pendingHudActions_;
        bool manualPauseRequested_ = false;
        bool uiPauseRequested_ = false;
        RuntimeGameplayAttributeWorld gameplayAttributes_;
        std::optional<bridge::GameplayAttributeDocument> attributeDocument_;
        std::string attributeDiagnostic_;
        RuntimeUiSnapshotPublisher uiSnapshotPublisher_;
        std::vector<bridge::ScreenBindingSource> uiBindingSources_;
        std::uint64_t uiSessionIdentity_ = 0;
        std::uint64_t uiGameplayRevision_ = 0;
        bool uiPublisherStarted_ = false;
        std::uint64_t uiLivePublishedMs_ = 0;
        std::string uiBindingDiagnostic_;
        RuntimeActionDispatcher actions_;
        // Inventory outlives creator script callbacks during shutdown.
        std::string gameSaveLocalAppData_;
        RuntimeInventorySession inventorySession_;
        bridge::WorldPickupController worldPickups_;
        bridge::InventoryItemCatalogue runtimeItemCatalogue_;
        bool runtimeItemCatalogueLoaded_=false;
        std::string worldPickupFeedback_;
        float worldPickupFeedbackSeconds_=0.0f;
        bool inventoryOverlayOpen_=false;
        std::uint64_t inventoryOverlayRevision_=0;
        bridge::InventoryOverlayView inventoryOverlayCache_;
        bridge::InventorySelectionController inventorySelection_;
        std::unique_ptr<bridge::InventoryGameplayGateway> inventoryUiGateway_;
        std::string inventoryUiStatus_;
        std::string inventoryHoverDetails_;
        bridge::InventoryItemContextMenu inventoryContext_;
        std::vector<std::string> inventoryContextWidgetIds_;
        bridge::InventoryDragTransfer inventoryDrag_;
        bool inventoryPagedPreview_=false;
        bool inventoryPagedDirty_=false;
        bridge::InventoryGridLayout inventoryPagedLayout_;
        bridge::InventoryGridPage inventoryPagedPages_[2];
        std::uint32_t inventoryPagedTopRows_[2]={0,0};
        bool inventoryScrollbarCapture_=false;
        bool inventoryScrollbarWasDown_=false;
        bool inventoryScrollbarThumbDrag_=false;
        std::size_t inventoryScrollbarPanel_=0;
        float inventoryScrollbarGrabY_=0.0f;
        bridge::InventoryPagedTransfer inventoryPagedDrag_;
        InventoryPointerGesture inventoryPointerGesture_;
        float inventoryDragStartX_=0;
        float inventoryDragStartY_=0;
        bridge::ScreenDocument inventoryAuthoredScreen_;
        std::unique_ptr<RuntimeScreenController> inventoryScreenController_=
            std::make_unique<RuntimeScreenController>();
        std::unique_ptr<RuntimeScreenPresenter> inventoryScreenPresenter_=
            std::make_unique<RuntimeScreenPresenter>();
        RuntimeScriptRuntime creatorScripts_;
        bridge::RuntimePlayerState player_;
        RuntimePlayerViewRigState playerViewRig_;
        RuntimePlayerViewAnimationState playerViewAnimation_;
        RuntimeEquipmentLoadout playerEquipment_;
        RuntimeProjectileSession projectiles_;
        RuntimeProjectileVisuals projectileVisuals_;
        RuntimeFireballEffect heldFireball_;
        RuntimeBurningStatus burning_;
        RuntimeChannelSession channel_;
        RuntimeChannelVisual channelVisual_;
        RuntimeChannelScorchTrail channelScorchTrail_;
        RuntimeLightningStorm lightningStorm_;
        RuntimeLightningColumn lightningColumnReview_;
        bool lightningStormCastLatch_=false;
        bridge::ImpactAudioPlayer impactAudio_;
        std::vector<RuntimeEquipmentLoadout::ProjectileRequest> pendingProjectileShots_;
        bridge::PlayerControllerSettings playerSettings_;
        bridge::NavigationRuntimeState navigationState_;
        bridge::CharacterRuntimeState characterState_;
        RuntimeCharacterSystemState characterAiState_;
        RuntimeCharacterPerceptionState characterPerceptionState_;
        RuntimeCharacterDecisionState characterDecisionState_;
        RuntimeCombatState combatState_;
        RuntimeCharacterAnimationState characterAnimationState_;
        bridge::GameplayInputMap inputMap_ = bridge::MakeDefaultGameplayInputMap();
        bridge::GameplayInputFrame gameplayInput_;
        bridge::SceneAudioPauseState audioPauseState_;
        RuntimeBootstrapResult initialBootstrapResult_;
        RuntimeBootstrapResult startupResult_;
        std::vector<RuntimeActionRequest> pendingActions_;
        bool startupFinished_ = false;
        bool flowStarted_ = false;
        bool quitRequested_ = false;
        bool inventoryHealthTestMode_ = false;
        bool smokeAutoPlay_ = false;
        bool smokeExitOnComplete_ = false;
        bool paused_ = false;
        bool physicsSimulationBeforePause_ = true;
        int exitCode_ = 0;
        std::uint64_t evidenceRevision_ = 0;
        std::uint64_t playerSceneRevision_ = 0;
        std::uint64_t audioSceneRevision_ = 0;
        std::uint64_t navigationSceneRevision_ = 0;
        std::uint64_t characterSceneRevision_ = 0;
        std::uint64_t characterSceneAttemptRevision_ = 0;
        bool characterSceneAttempted_ = false;
        bool characterSceneSyncFailed_ = false;
        std::uint64_t scriptSceneRevision_ = 0;
        std::size_t reportedScriptDiagnostics_ = 0;
    };
}
