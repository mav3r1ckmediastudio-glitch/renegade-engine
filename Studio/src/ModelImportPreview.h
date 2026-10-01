#pragma once
#include <WickedEngine.h>
#include <string>
#include <vector>

namespace renegade::studio
{
    // Preview scene, lights and camera are private; the import candidate is
    // never merged into the editor and preview helpers never enter its payload.
    class ModelImportPreview final : public wi::RenderPath3D
    {
    public:
        bool Prepare(wi::scene::Scene& source, std::string& error);
        void Rotate(float radians);
        void Render() const override;
        bool CapturePng(std::vector<std::uint8_t>& png, std::string& error) const;
        bool IsReady() const { return renderedFrames_ >= 8; }
    private:
        void FitCamera();
        wi::allocator::shared_ptr<wi::scene::Scene> previewScene_;
        wi::scene::CameraComponent previewCamera_;
        XMFLOAT3 center_ = {};
        float radius_ = 1;
        float angle_ = 0.5f;
        mutable unsigned renderedFrames_ = 0;
    };
}
