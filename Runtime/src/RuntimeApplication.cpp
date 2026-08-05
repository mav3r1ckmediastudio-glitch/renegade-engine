#include "RuntimeApplication.h"

#include <utility>

namespace renegade::runtime
{
    void RuntimeRenderPath::BindScene(bridge::SceneService& scenes) noexcept
    {
        scenes_ = &scenes;
        scene = &scenes.GetScene();
    }

    void RuntimeRenderPath::Load()
    {
        setSSREnabled(false);
        setReflectionsEnabled(true);
        setFXAAEnabled(false);

        wi::scene::TransformComponent cameraTransform;
        cameraTransform.Translate(XMFLOAT3(0.0f, 1.5f, -4.0f));
        cameraTransform.UpdateTransform();
        camera->TransformCamera(cameraTransform);

        RenderPath3D::Load();
    }

    void RuntimeApplication::SetBootstrapResult(RuntimeBootstrapResult result)
    {
        startupResult_ = std::move(result);
    }

    bool RuntimeApplication::StartupFinished() const noexcept
    {
        return startupFinished_;
    }

    const RuntimeBootstrapResult& RuntimeApplication::StartupResult() const noexcept
    {
        return startupResult_;
    }

    void RuntimeApplication::Initialize()
    {
        wi::Application::Initialize();

        infoDisplay.active = true;
        infoDisplay.watermark = false;
        infoDisplay.device_name = true;
        infoDisplay.resolution = true;
        infoDisplay.logical_size = true;
        infoDisplay.colorspace = true;
        infoDisplay.fpsinfo = true;

        renderer_.BindScene(scenes_);
        renderer_.init(canvas);

        startupResult_ =
            LoadRuntimeProjectScene(scenes_, std::move(startupResult_));
        startupFinished_ = true;
        if (!startupResult_.succeeded)
        {
            return;
        }

        renderer_.Load();
        ActivatePath(&renderer_);
    }
}
