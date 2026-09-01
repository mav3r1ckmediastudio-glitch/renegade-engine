#pragma once

#include <WickedEngine.h>

#include "RuntimeActions.h"
#include "RuntimeBootstrap.h"
#include "RuntimeFlow.h"
#include "RuntimeScreen.h"
#include "renegade/bridge/RenderSettingsService.h"
#include "renegade/bridge/RenderLutService.h"
#include "renegade/bridge/SceneService.h"

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
        void Load() override;
        void Update(float dt) override;

    private:
        void SyncRenderSettings(bool resizeBuffersForMSAA);

        bridge::SceneService* scenes_ = nullptr;
        std::string projectRoot_;
        bridge::RenderSettingsState renderSettings_;
        bool renderSettingsInitialized_ = false;
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

        void Initialize() override;
        void Update(float dt) override;

    private:
        [[nodiscard]] bool ConfigureActions(std::string& error);
        [[nodiscard]] bool LoadStartupScreen(std::string& error);
        [[nodiscard]] bool LoadCurrentFlowScreen(std::string& error);
        void QueueAction(RuntimeActionRequest request);
        void ProcessPendingActions();
        void RecordAction(const RuntimeActionResult& result);

        bridge::SceneService scenes_;
        RuntimeFlowController flow_;
        RuntimeRenderPath renderer_;
        RuntimeScreenController screenController_;
        RuntimeScreenPresenter screenPresenter_;
        RuntimeActionDispatcher actions_;
        RuntimeBootstrapResult startupResult_;
        std::vector<RuntimeActionRequest> pendingActions_;
        bool startupFinished_ = false;
        bool flowStarted_ = false;
        bool quitRequested_ = false;
        bool smokeAutoPlay_ = false;
        bool smokeExitOnComplete_ = false;
        int exitCode_ = 0;
        std::uint64_t evidenceRevision_ = 0;
    };
}
