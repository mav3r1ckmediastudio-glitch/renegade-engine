#pragma once
#include <WickedEngine.h>
#include "renegade/bridge/ModelAnimationPreviewService.h"
#include <string>
#include <array>
#include "renegade/bridge/LaunchSocketService.h"
#include "renegade/bridge/ProjectileAssetService.h"
#include "renegade/bridge/FirstPersonAssemblyService.h"
#include <vector>
#include "renegade/bridge/PlayerViewHandAnimation.h"

namespace renegade::studio
{
    // Preview scene, lights and camera are private; the import candidate is
    // never merged into the editor and preview helpers never enter its payload.
    class ModelImportPreview final : public wi::RenderPath3D
    {
    public:
        bool Prepare(wi::scene::Scene& source, std::string& error);
        void Rotate(float radians);
        void Orbit(float yaw, float pitch);
        void SetView(float yaw, float pitch);
        void Zoom(float factor);
        void Pan(float horizontal, float vertical);
        void FitModel();
        void SetModelAppearance(float scale, const std::array<float,3>& rotation);
        void SetProjectileEffects(const std::vector<bridge::ProjectileEffectLayer>&);
        XMFLOAT3 ModelSize() const { return modelSize_; }
        std::vector<bridge::PlayerViewBoneChoice> SocketParents() const;
        bool PickSocket(float u,float v,const bridge::LaunchSocketDefinition&,XMFLOAT3&,std::string&) const;
        bool ShowSocket(const bridge::LaunchSocketDefinition&,std::string&);
        std::vector<bridge::AnimationClipInfo> Clips() const { return animationPreview_.Clips(); }
        bool SelectClip(int index);
        bool PlayPause();
        bool Scrub(float time);
        bool SetSpeed(float speed);
        bool SetPairedAction(const std::string& action);
        void UseFirstPersonCamera();
        void SetAssemblyShieldHeld(bool held) { assemblyShieldHeld_=held; }
        bool IsPlaying() const { return paired_ ? pairedPlaying_ : animationPreview_.IsPlaying(); }
        bool HasClip() const { return paired_ || animationPreview_.HasSelection(); }
        float ClipDuration() const { return paired_ ? pairedEnd_ : 0; }
        float ClipTime() const { return paired_ ? pairedTime_ : animationPreview_.Time(); }
        bool NeedsRender() const { return !IsReady() || IsPlaying() || !projectileEffects_.empty(); }
        void Update(float dt) override;
        void Render() const override;
        bool CapturePng(std::vector<std::uint8_t>& png, std::string& error) const;
        bool IsReady() const { return renderedFrames_ >= 8; }
    private:
        bool PoseAssembly(const std::string& action,float time,std::string& error);
        runtime::RuntimePlayerHandAnimationState assemblyHands_;
        bool assemblyShieldHeld_=false;
        bool paired_ = false, pairedPlaying_ = false;
        float pairedTime_ = 0, pairedEnd_ = 0;
        std::string pairedAction_;
        void FitCamera();
        bridge::ModelAnimationPreviewService animationPreview_;
        wi::allocator::shared_ptr<wi::scene::Scene> previewScene_;
        wi::scene::CameraComponent previewCamera_;
        XMFLOAT3 center_ = {}, pan_ = {};
        float radius_ = 1;
        float angle_ = 0.5f;
        float elevation_ = 0.2425f, zoom_ = 1;
        float sourceRadius_ = 1, appearanceScale_ = 1;
        XMFLOAT3 sourceCenter_ = {}, modelSize_ = {};
        wi::ecs::Entity appearanceRoot_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity socketMarker_ = wi::ecs::INVALID_ENTITY;
        std::vector<bridge::ProjectileEffectLayer> projectileEffects_;
        std::vector<wi::ecs::Entity> projectileEmitters_;
        mutable unsigned renderedFrames_ = 0;
    };
}
