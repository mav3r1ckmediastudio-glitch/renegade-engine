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

    void RuntimeApplication::SetStartupScene(std::string filePath)
    {
        if (!filePath.empty())
        {
            startupScene_ = std::move(filePath);
        }
    }

    void RuntimeApplication::Initialize()
    {
        wi::Application::Initialize();

        infoDisplay.active = true;
        infoDisplay.watermark = false;
        infoDisplay.resolution = true;
        infoDisplay.fpsinfo = true;

        renderer_.BindScene(scenes_);
        renderer_.init(canvas);
        scenes_.LoadScene(startupScene_);
        renderer_.Load();
        ActivatePath(&renderer_);
    }
}
