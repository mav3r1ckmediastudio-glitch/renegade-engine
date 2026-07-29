#pragma once

#include <string>

#include <WickedEngine.h>

#include "renegade/bridge/StudioSession.h"

namespace renegade::studio
{
    class StudioRenderPath final : public wi::RenderPath3D
    {
    public:
        void BindSession(bridge::StudioSession& session) noexcept;
        void Load() override;
        void ResizeLayout() override;
        void RefreshStatus();

    private:
        bridge::StudioSession* session_ = nullptr;
        wi::gui::Label statusLabel_;
    };

    class StudioApplication final : public wi::Application
    {
    public:
        void SetStartupScene(std::string filePath);
        void Initialize() override;

    private:
        bridge::StudioSession session_;
        StudioRenderPath renderer_;
        std::string startupScene_ = "Content/cube.wiscene";
    };
}
