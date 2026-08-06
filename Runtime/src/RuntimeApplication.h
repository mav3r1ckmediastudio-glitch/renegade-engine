#pragma once

#include <WickedEngine.h>

#include "RuntimeActions.h"
#include "RuntimeBootstrap.h"
#include "RuntimeFlow.h"
#include "RuntimeScreen.h"
#include "renegade/bridge/SceneService.h"

#include <cstdint>
#include <vector>

namespace renegade::runtime
{
    class RuntimeRenderPath final : public wi::RenderPath3D
    {
    public:
        void BindScene(bridge::SceneService& scenes) noexcept;
        void Load() override;

    private:
        bridge::SceneService* scenes_ = nullptr;
    };

    class RuntimeApplication final : public wi::Application
    {
    public:
        void SetBootstrapResult(RuntimeBootstrapResult result);
        [[nodiscard]] bool StartupFinished() const noexcept;
        [[nodiscard]] const RuntimeBootstrapResult& StartupResult() const noexcept;
        [[nodiscard]] bool QuitRequested() const noexcept;
        [[nodiscard]] std::uint64_t EvidenceRevision() const noexcept;

        void Initialize() override;
        void Update(float dt) override;

    private:
        [[nodiscard]] bool ConfigureActions(std::string& error);
        [[nodiscard]] bool LoadStartupScreen(std::string& error);
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
        std::uint64_t evidenceRevision_ = 0;
    };
}
