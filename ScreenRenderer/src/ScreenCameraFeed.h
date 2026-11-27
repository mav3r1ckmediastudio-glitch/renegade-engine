#pragma once
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/ScreenService.h"
#include <cmath>

namespace renegade::screen
{
    class ScreenCameraFeed
    {
    public:
        bool Open(const std::string& root,const bridge::StableId& project,
            const bridge::ScreenCameraSource& source,std::string& error)
        {
            std::string path;
            if(!bridge::ResolveSceneDocumentPath(root,project,source.levelId,source.levelPathHint,path,error))return false;
            if(!world_.LoadScene(path)){error=world_.LastError();return false;}
            auto& scene=world_.GetScene();
            for(size_t i=0;i<scene.cameras.GetCount();++i)
            {
                const auto e=scene.cameras.GetEntity(i);
                if(bridge::PersistentEntityId(scene,e)==source.cameraId) cameraEntity_=e;
                scene.cameras[i].render_to_texture.resolution={0,0};
            }
            if(!scene.transforms.Contains(cameraEntity_) || !scene.cameras.Contains(cameraEntity_))
            {error="Camera missing from level: "+source.cameraName;return false;}
            scene.characters.Clear();scene.scripts.Clear();scene.sounds.Clear();scene.videos.Clear();
            scene.rigidbodies.Clear();scene.softbodies.Clear();scene.colliders.Clear();
            for(size_t i=0;i<scene.humanoids.GetCount();++i)
            {
                scene.humanoids[i].SetRagdollPhysicsEnabled(false);
                scene.humanoids[i].SetRagdollDisabled(true);
                scene.humanoids[i].ragdoll={};
            }
            camera_=*scene.cameras.GetComponent(cameraEntity_);
            camera_.width=640;camera_.height=360;
            camera_.UpdateCamera();
            pan_=source.slowPan;
            path_.scene=&scene;path_.camera=&camera_;
            path_.init(640,360,96);
            path_.setMotionBlurEnabled(false);
            path_.setOcclusionCullingEnabled(false);
            path_.setEyeAdaptionEnabled(false);
            path_.ResizeBuffers();
            return true;
        }

        const wi::graphics::Texture* Tick(float dt)
        {
            elapsed_+=std::clamp(dt,0.f,.1f);
            if(!ready_ || elapsed_>=1.f/30)
            {
                const float step=std::max(.00001f,elapsed_);
                clock_+=step;
                elapsed_=0;
                const float yaw=pan_ ? .28f*std::sin(clock_*.2f) : 0;
                const auto* transform=world_.GetScene().transforms.GetComponent(cameraEntity_);
                if(!transform)return nullptr;
                path_.PreUpdate();
                camera_.TransformCamera(XMMatrixRotationY(yaw)*transform->GetWorldMatrix());
                camera_.UpdateCamera();
                path_.Update(step);path_.PostUpdate();
                path_.PreRender();path_.Render();path_.PostRender();
                ready_=true;
            }
            return &path_.GetRenderResult3D();
        }
    private:
        bridge::SceneService world_;
        wi::scene::CameraComponent camera_;
        wi::RenderPath3D path_;
        wi::ecs::Entity cameraEntity_=wi::ecs::INVALID_ENTITY;
        bool pan_=false,ready_=false;
        float elapsed_=0,clock_=0;
    };
}
