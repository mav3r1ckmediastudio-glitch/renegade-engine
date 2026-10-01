#pragma once
#include <WickedEngine.h>
#include "renegade/bridge/ModelAnimationPreviewService.h"
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
        std::vector<bridge::AnimationClipInfo> Clips() const { return animationPreview_.Clips(); }
        bool SelectClip(int index);
        bool PlayPause();
        bool Scrub(float time);
        bool SetSpeed(float speed);
        bool IsPlaying() const { return animationPreview_.IsPlaying(); }
        bool HasClip() const { return animationPreview_.HasSelection(); }
        float ClipTime() const { return animationPreview_.Time(); }
        bool NeedsRender() const { return !IsReady() || IsPlaying(); }
        void Update(float dt) override;
        void Render() const override;
        bool CapturePng(std::vector<std::uint8_t>& png, std::string& error) const;
        bool IsReady() const { return renderedFrames_ >= 8; }
    private:
        void FitCamera();
        bridge::ModelAnimationPreviewService animationPreview_;
        wi::allocator::shared_ptr<wi::scene::Scene> previewScene_;
        wi::scene::CameraComponent previewCamera_;
        XMFLOAT3 center_ = {};
        float radius_ = 1;
        float angle_ = 0.5f;
        mutable unsigned renderedFrames_ = 0;
    };
}
