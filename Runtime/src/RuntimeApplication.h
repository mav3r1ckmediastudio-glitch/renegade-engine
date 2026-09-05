#pragma once

#include <WickedEngine.h>

#include "RuntimeActions.h"
#include "RuntimeBootstrap.h"
#include "RuntimeFlow.h"
#include "RuntimeScreen.h"
#include "RuntimeScriptRuntime.h"
#include "renegade/bridge/AudioService.h"
#include "renegade/bridge/GameplayInputService.h"
#include "renegade/bridge/RenderSettingsService.h"
#include "renegade/bridge/RenderLutService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/PlayerService.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::runtime
{
    class RuntimeRenderPath final : public wi::RenderPath3D
    {
    public:
        void BindScene(
            bridge::SceneService& scenes,
            std::string projectRoot) noexcept;
        void SetPaused(bool paused) noexcept;
        void Load() override;
        void Update(float dt) override;
        void Compose(wi::graphics::CommandList cmd) const override;

    private:
        void SyncRenderSettings(bool resizeBuffersForMSAA);

        bridge::SceneService* scenes_ = nullptr;
        std::string projectRoot_;
        bridge::RenderSettingsState renderSettings_;
        bool renderSettingsInitialized_ = false;
        bool paused_ = false;
        std::uint64_t renderSettingsSceneRevision_ = 0;
    };

    class RuntimeApplication final : public wi::Application
    {
    public:
        void SetBootstrapResult(RuntimeBootstrapResult result);
        void SetSmokeOptions(bool autoPlay, bool exitOnComplete) noexcept;
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
        [[nodiscard]] bool ResetPlaySession(std::string& error);
        void SetPaused(bool paused) noexcept;
        void QueueAction(RuntimeActionRequest request);
        void ProcessPendingActions();
        void RecordAction(const RuntimeActionResult& result);
        void SyncPlayerForScene();
        void SyncAudioForScene();
        void SyncCreatorScriptsForScene();
        void StopCreatorScripts() noexcept;
        void ReportCreatorScriptDiagnostics();

        bridge::SceneService scenes_;
        RuntimeFlowController flow_;
        RuntimeRenderPath renderer_;
        RuntimeScreenController screenController_;
        RuntimeScreenPresenter screenPresenter_;
        RuntimeActionDispatcher actions_;
        RuntimeScriptRuntime creatorScripts_;
        bridge::RuntimePlayerState player_;
        bridge::PlayerControllerSettings playerSettings_;
        bridge::GameplayInputMap inputMap_ = bridge::MakeDefaultGameplayInputMap();
        bridge::GameplayInputFrame gameplayInput_;
        bridge::SceneAudioPauseState audioPauseState_;
        RuntimeBootstrapResult initialBootstrapResult_;
        RuntimeBootstrapResult startupResult_;
        std::vector<RuntimeActionRequest> pendingActions_;
        bool startupFinished_ = false;
        bool flowStarted_ = false;
        bool quitRequested_ = false;
        bool smokeAutoPlay_ = false;
        bool smokeExitOnComplete_ = false;
        bool paused_ = false;
        bool physicsSimulationBeforePause_ = true;
        int exitCode_ = 0;
        std::uint64_t evidenceRevision_ = 0;
        std::uint64_t playerSceneRevision_ = 0;
        std::uint64_t audioSceneRevision_ = 0;
        std::uint64_t scriptSceneRevision_ = 0;
        std::size_t reportedScriptDiagnostics_ = 0;
    };
}
