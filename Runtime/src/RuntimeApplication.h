#pragma once

#include <WickedEngine.h>

#include "RuntimeBootstrap.h"
#include "RuntimeFlow.h"
#include "renegade/bridge/SceneService.h"

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
        void Initialize() override;

    private:
        bridge::SceneService scenes_;
        RuntimeFlowController flow_;
        RuntimeRenderPath renderer_;
        RuntimeBootstrapResult startupResult_;
        bool startupFinished_ = false;
    };
}
