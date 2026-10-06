#include "ModelImportPreview.h"
#include <algorithm>
#include <cmath>

namespace renegade::studio
{
    bool ModelImportPreview::Prepare(wi::scene::Scene& source, std::string& error)
    {
        paired_ = pairedPlaying_ = false;assemblyHands_={};assemblyShieldHeld_=false;
        previewScene_ = wi::allocator::make_shared<wi::scene::Scene>();
        wi::Archive archive;
        source.Serialize(archive);
        archive.SetReadModeAndResetPos(true);
        previewScene_->Serialize(archive);
        scene = previewScene_.get();
        for (size_t i = 0; i < source.materials.GetCount(); ++i) {
            for (size_t slot = 0; slot < wi::scene::MaterialComponent::TEXTURESLOT_COUNT; ++slot)
                scene->materials[i].textures[slot] = source.materials[i].textures[slot];
            scene->materials[i].SetDirty();
        }
        camera = &previewCamera_;
        animationPreview_.Prepare(*scene);
        scene->Update(0);
        wi::primitive::AABB visibleBounds;
        if (!bridge::ComputeVisibleModelBounds(*scene, visibleBounds))
        {
            error = "Model has no finite visible preview bounds.";
            return false;
        }
        center_ = visibleBounds.getCenter();
        const auto extent = visibleBounds.getHalfWidth();
        radius_ = std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);
        if (!std::isfinite(radius_) || radius_ < 0.000001f)
        {
            error = "Model has no finite preview bounds.";
            return false;
        }
        sourceCenter_ = center_; sourceRadius_ = radius_; appearanceScale_ = 1;
        modelSize_ = {extent.x*2,extent.y*2,extent.z*2};
        angle_ = 0.5f; elevation_ = 0.2425f; zoom_ = 1;
        appearanceRoot_ = wi::ecs::INVALID_ENTITY;
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

    void ModelImportPreview::UseFirstPersonCamera()
    {
        camera->CreatePerspective(512, 320, 0.005f, 10.0f, XMConvertToRadians(80));
        camera->TransformCamera(XMMatrixIdentity()); camera->UpdateCamera();
        renderedFrames_ = 0;
    }
    bool ModelImportPreview::PoseAssembly(const std::string& action,float time,std::string& error)
    {
        wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;
        for(size_t i=0;i<scene->metadatas.GetCount();++i) {
            const auto& m=scene->metadatas[i];
            if(m.bool_values.has("renegade.first_person.independent_hands") &&
               m.bool_values.get("renegade.first_person.independent_hands"))root=scene->metadatas.GetEntity(i);
        }
        if(root==wi::ecs::INVALID_ENTITY)return bridge::FirstPersonAssemblyService().Pose(*scene,action,time,error);
        if(!assemblyHands_.enabled && !runtime::InitializeRuntimePlayerHandAnimations(*scene,root,assemblyHands_,error))return false;
        auto& h=assemblyHands_;
        auto right=h.primary[0].front(),left=assemblyShieldHeld_?h.block[1]:h.offMovement[0];
        bool offAction=false;
        unsigned variant=0;
        std::string semantic=action;
        if(action.compare(0,7,"Attack#")==0){semantic="Attack";variant=unsigned(std::stoul(action.substr(7)));}
        const int index=runtime::PlayerHandSemanticIndex(semantic);
        if(index>=0 && !h.primary[index].empty())right=h.primary[index][std::min<size_t>(variant,h.primary[index].size()-1)];
        else {
            bool found=false;
            for(unsigned i=0;i<12;++i)if(action==bridge::FirstPersonDirectionalActions[i]) {
                right=h.directionalClips[i/3][i%3];found=true;
            }
            const char* blocks[]={"BlockStart","BlockLoop","BlockEnd"};
            for(unsigned i=0;i<3;++i)if(action==blocks[i]){left=h.block[i];offAction=found=true;}
            if(!found){error="Preview action is unavailable.";return false;}
        }
        if(left==wi::ecs::INVALID_ENTITY)left=h.block[1];
        pairedEnd_=scene->animations.GetComponent(offAction?left:right)->end-scene->animations.GetComponent(offAction?left:right)->start;
        for(auto e:h.generated)scene->animations.GetComponent(e)->amount=0;
        const auto pose=[&](wi::ecs::Entity e,float seconds) {
            auto& c=*scene->animations.GetComponent(e);c.Pause();c.RootMotionOff();c.amount=1;
            c.timer=std::clamp(c.start+seconds,c.start,c.end);c.last_update_time=-std::numeric_limits<float>::max();
        };
        pose(h.base,0);pose(right,offAction?0:time);pose(left,offAction?time:0);
        h.avoidance.offset={};
        runtime::EvaluateRuntimePlayerHandAvoidance(*scene,h.avoidance,h.generated,1.0f/60);
        scene->Update(1.0f/60);
        error.clear();return true;
    }

    bool ModelImportPreview::SetPairedAction(const std::string& action)
    {
        std::string error;
        if (!PoseAssembly(action,0,error)) return false;
        paired_ = true; pairedPlaying_ = false; pairedTime_ = 0; pairedAction_ = action;
        if(!assemblyHands_.enabled)pairedEnd_=0;
        for (size_t i = 0; !assemblyHands_.enabled && i < scene->animations.GetCount(); ++i)
            if (scene->animations[i].amount > 0)
                pairedEnd_ = std::max(pairedEnd_, scene->animations[i].end - scene->animations[i].start);
        renderedFrames_ = 0; return true;
    }

    void ModelImportPreview::FitCamera()
    {
        const float distance = radius_ / std::sin(XM_PI / 8.0f) * 1.12f * zoom_;
        const XMVECTOR target = XMLoadFloat3(&center_);
        const XMVECTOR eye = target + XMVectorSet(std::sin(angle_) * distance * std::cos(elevation_),
            distance * std::sin(elevation_), -std::cos(angle_) * distance * std::cos(elevation_), 0);
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

    void ModelImportPreview::Orbit(float yaw, float pitch)
    {
        angle_ += yaw; elevation_ = std::clamp(elevation_ + pitch, -1.45f, 1.45f);
        FitCamera(); renderedFrames_ = 0;
    }
    void ModelImportPreview::SetView(float yaw, float pitch)
    {
        angle_ = yaw; elevation_ = std::clamp(pitch,-1.45f,1.45f);
        FitCamera(); renderedFrames_ = 0;
    }
    void ModelImportPreview::Zoom(float factor)
    {
        zoom_ = std::clamp(zoom_ * factor, 0.15f, 8.0f);
        FitCamera(); renderedFrames_ = 0;
    }
    void ModelImportPreview::FitModel()
    {
        radius_ = sourceRadius_ * appearanceScale_; zoom_ = 1;
        previewCamera_.CreatePerspective(512,320,std::max(0.00001f,radius_*0.001f),
            radius_*20.0f,XM_PI/4.0f);
        FitCamera(); renderedFrames_ = 0;
    }
    void ModelImportPreview::SetModelAppearance(float scale, const std::array<float,3>& rotation)
    {
        if (!scene) return;
        if (appearanceRoot_ == wi::ecs::INVALID_ENTITY) {
            scene->rigidbodies.Clear();scene->softbodies.Clear();scene->colliders.Clear();
            scene->scripts.Clear();scene->characters.Clear();scene->animations.Clear();
            std::vector<wi::ecs::Entity> parents;
            for (size_t i=0;i<scene->transforms.GetCount();++i) {
                const auto e=scene->transforms.GetEntity(i);
                if (scene->lights.Contains(e)) continue;
                const auto* h=scene->hierarchy.GetComponent(e);
                if (!h || h->parentID==wi::ecs::INVALID_ENTITY) parents.push_back(e);
            }
            appearanceRoot_=scene->Entity_CreateTransform("Projectile preview appearance");
            for (const auto e:parents) scene->Component_Attach(e,appearanceRoot_,true);
        }
        appearanceScale_=scale;
        const auto matrix=XMMatrixScaling(scale,scale,scale) *
            XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotation[0]),
                XMConvertToRadians(rotation[1]),XMConvertToRadians(rotation[2]));
        auto& transform=*scene->transforms.GetComponent(appearanceRoot_);
        transform.ClearTransform();transform.MatrixTransform(matrix);transform.UpdateTransform();
        XMStoreFloat3(&center_,XMVector3TransformCoord(XMLoadFloat3(&sourceCenter_),matrix));
        FitCamera();renderedFrames_=0;
    }

    bool ModelImportPreview::SelectClip(int index)
    {
        if (!animationPreview_.Select(index)) return false;
        renderedFrames_ = 0;
        return true;
    }
    bool ModelImportPreview::PlayPause()
    {
        if (paired_) {
            if (!pairedPlaying_ && pairedTime_ >= pairedEnd_) {
                std::string error;
                if (!PoseAssembly(pairedAction_,0,error)) return false;
                pairedTime_ = 0;
            }
            pairedPlaying_ = !pairedPlaying_; renderedFrames_ = 0; return true;
        }
        if (!animationPreview_.PlayPause()) return false;
        renderedFrames_ = 0;
        return true;
    }
    bool ModelImportPreview::Scrub(float time)
    {
        if (paired_) {
            std::string error;
            pairedTime_ = std::clamp(time, 0.0f, pairedEnd_); pairedPlaying_ = false;
            if (!PoseAssembly(pairedAction_,pairedTime_,error)) return false;
            renderedFrames_ = 0; return true;
        }
        if (!animationPreview_.Scrub(time)) return false;
        renderedFrames_ = 0;
        return true;
    }
    bool ModelImportPreview::SetSpeed(float speed)
    {
        return animationPreview_.Speed(speed);
    }

    void ModelImportPreview::Update(float dt)
    {
        const bool wasPlaying = IsPlaying();
        if (paired_ && pairedPlaying_) {
            pairedTime_ = std::min(pairedEnd_, pairedTime_ + dt);
            std::string error;
            (void)PoseAssembly(pairedAction_,pairedTime_,error);
            if (pairedTime_ >= pairedEnd_) pairedPlaying_ = false;
        }
        wi::RenderPath3D::Update(dt);
        if (wasPlaying && !IsPlaying()) renderedFrames_ = 0;
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
