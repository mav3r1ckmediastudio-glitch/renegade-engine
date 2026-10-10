#pragma once
#include "renegade/bridge/PlayerViewAsset.h"
#include "renegade/bridge/PlayerViewAnimation.h"
#include "renegade/bridge/EquipmentAssetService.h"

namespace renegade::bridge
{
    // Frozen private world: never installs controllers, runs input or authors
    // the document. Shared view-model code also serves the standalone Runtime.
    class PlayerCameraPreviewService final : public wi::RenderPath3D
    {
    public:
        bool Prepare(wi::scene::Scene& source, const PlayerStart& start,
            const std::string& root, const StableId& project, std::string& error)
        {
            world_ = wi::allocator::make_shared<wi::scene::Scene>();
            wi::Archive archive;
            source.Serialize(archive);
            archive.SetReadModeAndResetPos(true);
            world_->Serialize(archive);
            scene = world_.get();
            for (size_t i=0; i<source.materials.GetCount(); ++i) {
                for(size_t slot=0; slot<wi::scene::MaterialComponent::TEXTURESLOT_COUNT; ++slot)
                    scene->materials[i].textures[slot]=source.materials[i].textures[slot];
                scene->materials[i].SetDirty();
            }
            scene->characters.Clear(); scene->rigidbodies.Clear();
            scene->colliders.Clear(); scene->softbodies.Clear();
            scene->sounds.Clear(); scene->scripts.Clear();
            for(size_t i=0;i<scene->humanoids.GetCount();++i) {
                scene->humanoids[i].SetRagdollPhysicsEnabled(false);
                scene->humanoids[i].SetRagdollDisabled(true);
                scene->humanoids[i].ragdoll={};
            }
            for(size_t i=0;i<scene->animations.GetCount();++i) scene->animations[i].Pause();
            camera=&viewCamera_;
            init(432,243,96);
            setOcclusionCullingEnabled(false);
            setEyeAdaptionEnabled(false); setMotionBlurEnabled(false);
            setDepthOfFieldEnabled(false); setBloomEnabled(false);
            setFXAAEnabled(true);
            // Runtime uses Wicked's default perspective (60 degrees).
            viewCamera_.CreatePerspective(432,243,0.1f,5000.0f,XM_PI/3.0f);
            const auto settings=SanitizePlayerControllerSettings(start.settings);
            const auto yaw=wi::math::QuaternionToRollPitchYaw(start.transform.rotation).y;
            wi::scene::TransformComponent pose;
            pose.Translate(XMFLOAT3(start.transform.translation.x,
                start.transform.translation.y+settings.eyeHeight,start.transform.translation.z));
            pose.RotateRollPitchYaw(XMFLOAT3(0,yaw,0)); pose.UpdateTransform();
            viewCamera_.TransformCamera(pose); viewCamera_.UpdateCamera();

            StableId presentation=settings.firstPersonArmsAssetId;
            const bool authored=!settings.primaryEquipmentAssetId.empty() ||
                !settings.offHandEquipmentAssetId.empty();
            if(authored) {
                presentation.clear();
                if(!ValidateStartingEquipment(root,project,settings.primaryEquipmentAssetId,
                    settings.offHandEquipmentAssetId,error)) return false;
                if(!settings.primaryEquipmentAssetId.empty()) {
                    EquipmentAssetDocument equipment;
                    if(!LoadEquipmentAsset(root,project,settings.primaryEquipmentAssetId,equipment,error)) return false;
                    presentation=equipment.equipment.presentationAssetId;
                }
            }
            if(!presentation.empty()) {
                const auto anchor=scene->Entity_CreateTransform("__renegade_preview_player");
                scene->transforms.GetComponent(anchor)->Translate(start.transform.translation);
                scene->transforms.GetComponent(anchor)->UpdateTransform();
                runtime::RuntimePlayerViewRigState rig;
                runtime::RuntimePlayerViewRigSettings rigSettings;
                rigSettings.createProofGeometry=false;
                if(!runtime::SpawnRuntimePlayerViewRig(*scene,rig,anchor,settings.eyeHeight,error,rigSettings) ||
                    !runtime::LoadRuntimePlayerViewAsset(*scene,rig,root,project,presentation,error)) return false;
                runtime::RuntimePlayerViewAnimationState animations;
                if(!runtime::InitializeRuntimePlayerViewAnimations(*scene,rig,animations,error)) return false;
                if(!runtime::RequestRuntimePlayerViewAnimation(*scene,animations,runtime::PlayerViewAction::Idle)) {
                    error="Equipped presentation has no Idle action."; return false;
                }
                (void)runtime::PoseRuntimePlayerViewRig(*scene,rig,yaw,0);
            }
            scene->camera=viewCamera_;
            scene->Update(0);
            ResizeBuffers();
            error.clear();
            return true;
        }
        void Update(float) override {
            // Wicked skips GPU geometry/instance allocation at dt==0. A minimal
            // render preparation tick is required; actors/scripts/audio are absent
            // and native clips remain paused. GPU effects receive a frozen clock.
            wi::RenderPath3D::Update(0.000001f);
            scene->dt=0;
        }
        bool NeedsRender() const { return renderedFrames_ < 8; }
        void Render() const override {
            wi::RenderPath3D::Render();
            if(wi::renderer::IsPipelineCreationActive()==0) ++renderedFrames_;
            else renderedFrames_=0;
        }
    private:
        mutable unsigned renderedFrames_=0;
        wi::allocator::shared_ptr<wi::scene::Scene> world_;
        wi::scene::CameraComponent viewCamera_;
    };
}
