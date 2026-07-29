#pragma once

#include <string>

#include <WickedEngine.h>

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
        void SetStartupScene(std::string filePath);
        void Initialize() override;

    private:
        bridge::SceneService scenes_;
        RuntimeRenderPath renderer_;
        std::string startupScene_ = "Content/cube.wiscene";
    };
}
