#include "StudioApplication.h"

#include <utility>

namespace renegade::studio
{
    void StudioRenderPath::BindSession(bridge::StudioSession& session) noexcept
    {
        session_ = &session;
        scene = &session.Scenes().GetScene();
    }

    void StudioRenderPath::Load()
    {
        setSSREnabled(false);
        setReflectionsEnabled(true);
        setFXAAEnabled(false);

        wi::scene::TransformComponent cameraTransform;
        cameraTransform.Translate(XMFLOAT3(0.0f, 1.5f, -4.0f));
        cameraTransform.UpdateTransform();
        camera->TransformCamera(cameraTransform);

        statusLabel_.Create("RenegadeStudioStatus");
        statusLabel_.SetSize(XMFLOAT2(720.0f, 24.0f));
        statusLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        GetGUI().AddWidget(&statusLabel_);
        RefreshStatus();

        RenderPath3D::Load();
    }

    void StudioRenderPath::ResizeLayout()
    {
        RenderPath3D::ResizeLayout();
        statusLabel_.SetPos(XMFLOAT2(16.0f, GetLogicalHeight() - 40.0f));
    }

    void StudioRenderPath::RefreshStatus()
    {
        if (session_ == nullptr)
        {
            statusLabel_.SetText("Renegade Studio | EngineBridge session unavailable");
            return;
        }

        const auto& scenes = session_->Scenes();
        if (!scenes.LastError().empty())
        {
            statusLabel_.SetText("Renegade Studio | " + scenes.LastError());
            return;
        }

        statusLabel_.SetText(
            "Renegade Studio | " +
            std::to_string(scenes.EntityCount()) +
            " entities | " +
            (scenes.CurrentPath().empty() ? "Untitled" : scenes.CurrentPath()));
    }

    void StudioApplication::SetStartupScene(std::string filePath)
    {
        if (!filePath.empty())
        {
            startupScene_ = std::move(filePath);
        }
    }

    void StudioApplication::Initialize()
    {
        wi::Application::Initialize();

        infoDisplay.active = true;
        infoDisplay.watermark = false;
        infoDisplay.resolution = true;
        infoDisplay.fpsinfo = true;

        renderer_.BindSession(session_);
        renderer_.init(canvas);
        session_.LoadScene(startupScene_);
        renderer_.Load();
        ActivatePath(&renderer_);
    }
}
