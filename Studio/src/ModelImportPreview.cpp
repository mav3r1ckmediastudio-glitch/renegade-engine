#include "ModelImportPreview.h"
#include <algorithm>
#include <cmath>

namespace renegade::studio
{
    bool ModelImportPreview::Prepare(wi::scene::Scene& source, std::string& error)
    {
        previewScene_ = wi::allocator::make_shared<wi::scene::Scene>();
        wi::Archive archive;
        source.Serialize(archive);
        archive.SetReadModeAndResetPos(true);
        previewScene_->Serialize(archive);
        scene = previewScene_.get();
        camera = &previewCamera_;
        scene->Update(0);
        center_ = scene->bounds.getCenter();
        const auto extent = scene->bounds.getHalfWidth();
        radius_ = std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);
        if (!std::isfinite(radius_) || radius_ < 0.000001f)
        {
            error = "Model has no finite preview bounds.";
            return false;
        }
        // Discard source lights/weather on the copy only, then use neutral
        // fixed illumination so the preview doesn't depend on the open level.
        scene->lights.Clear();
        scene->weathers.Clear();
        const auto weatherEntity = wi::ecs::CreateEntity();
        auto& weather = scene->weathers.Create(weatherEntity);
        weather.horizon = weather.zenith = XMFLOAT3(0.055f, 0.065f, 0.08f);
        weather.ambient = XMFLOAT3(0.25f, 0.25f, 0.25f);
        weather.fogDensity = 0;
        scene->weather = weather;
        const auto lightEntity = scene->Entity_CreateLight("Preview Key");
        auto* light = scene->lights.GetComponent(lightEntity);
        light->SetType(wi::scene::LightComponent::DIRECTIONAL);
        light->intensity = 3.0f;
        auto* transform = scene->transforms.GetComponent(lightEntity);
        transform->RotateRollPitchYaw(XMFLOAT3(-0.7f, 0.6f, 0));
        transform->UpdateTransform();
        init(512, 320, 96);
        setShadowsEnabled(false);
        setOcclusionCullingEnabled(false);
        setBloomEnabled(false);
        setEyeAdaptionEnabled(false);
        setMotionBlurEnabled(false);
        setDepthOfFieldEnabled(false);
        setFXAAEnabled(true);
        previewCamera_.CreatePerspective(512, 320,
            std::max(0.00001f, radius_ * 0.001f), radius_ * 20.0f, XM_PI / 4.0f);
        FitCamera();
        ResizeBuffers();
        renderedFrames_ = 0;
        error.clear();
        return true;
    }

    void ModelImportPreview::FitCamera()
    {
        const float distance = radius_ / std::sin(XM_PI / 8.0f) * 1.12f;
        const XMVECTOR target = XMLoadFloat3(&center_);
        const XMVECTOR eye = target + XMVectorSet(std::sin(angle_) * distance * 0.97f,
            distance * 0.24f, -std::cos(angle_) * distance * 0.97f, 0);
        previewCamera_.TransformCamera(XMMatrixInverse(nullptr,
            XMMatrixLookAtLH(eye, target, XMVectorSet(0, 1, 0, 0))));
        previewCamera_.UpdateCamera();
    }

    void ModelImportPreview::Rotate(float radians)
    {
        angle_ += radians;
        FitCamera();
        renderedFrames_ = 0;
    }

    void ModelImportPreview::Render() const
    {
        wi::RenderPath3D::Render();
        // Wicked starts object pipeline compilation in background. Frames
        // rendered before those pipelines exist do not count as a usable preview.
        if (wi::renderer::IsPipelineCreationActive() == 0)
            ++renderedFrames_;
        else
            renderedFrames_ = 0;
    }

    bool ModelImportPreview::CapturePng(std::vector<std::uint8_t>& png, std::string& error) const
    {
        if (!IsReady() || !GetRenderResult3D().IsValid())
        {
            error = "Wait for the model preview to finish rendering.";
            return false;
        }
        wi::vector<std::uint8_t> bytes;
        if (!wi::helper::saveTextureToMemoryFile(GetRenderResult3D(), "PNG", bytes))
        {
            error = "Could not capture the rendered model thumbnail.";
            return false;
        }
        png.assign(bytes.begin(), bytes.end());
        error.clear();
        return !png.empty();
    }
}
