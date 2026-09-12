#include "renegade/bridge/SpecialistComponentService.h"

#include <algorithm>
#include <cmath>
#include <utility>

void ImportModel_PLY(const std::string& fileName, wi::scene::Scene& scene);

namespace
{
    constexpr float Epsilon = 0.00001f;

    bool NearlyEqual(float a, float b) noexcept
    {
        return std::abs(a - b) <= Epsilon;
    }

    bool SameVec4(const XMFLOAT4& a, const XMFLOAT4& b) noexcept
    {
        return NearlyEqual(a.x, b.x) && NearlyEqual(a.y, b.y) &&
            NearlyEqual(a.z, b.z) && NearlyEqual(a.w, b.w);
    }

    bool SameHair(
        const renegade::bridge::HairParticleState& a,
        const renegade::bridge::HairParticleState& b) noexcept
    {
        if (a.mesh != b.mesh || a.cameraBend != b.cameraBend ||
            a.strandCount != b.strandCount || a.segmentCount != b.segmentCount ||
            a.billboardCount != b.billboardCount || a.randomSeed != b.randomSeed ||
            !NearlyEqual(a.length, b.length) || !NearlyEqual(a.width, b.width) ||
            !NearlyEqual(a.stiffness, b.stiffness) || !NearlyEqual(a.drag, b.drag) ||
            !NearlyEqual(a.gravity, b.gravity) || !NearlyEqual(a.randomness, b.randomness) ||
            !NearlyEqual(a.viewDistance, b.viewDistance) || !NearlyEqual(a.uniformity, b.uniformity) ||
            a.atlasRects.size() != b.atlasRects.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < a.atlasRects.size(); ++i)
        {
            if (!SameVec4(a.atlasRects[i].texMulAdd, b.atlasRects[i].texMulAdd) ||
                !NearlyEqual(a.atlasRects[i].size, b.atlasRects[i].size))
            {
                return false;
            }
        }
        return true;
    }

    bool SameForce(
        const renegade::bridge::ForceFieldState& a,
        const renegade::bridge::ForceFieldState& b) noexcept
    {
        return a.type == b.type && NearlyEqual(a.gravity, b.gravity) &&
            NearlyEqual(a.range, b.range);
    }

    bool SameVideo(
        const renegade::bridge::VideoAuthoredState& a,
        const renegade::bridge::VideoAuthoredState& b) noexcept
    {
        return a.filename == b.filename && a.looped == b.looped;
    }

    bool SameSpline(
        const renegade::bridge::SplineState& a,
        const renegade::bridge::SplineState& b) noexcept
    {
        return a.looped == b.looped && a.filled == b.filled &&
            a.drawAligned == b.drawAligned && NearlyEqual(a.width, b.width) &&
            NearlyEqual(a.rotationRadians, b.rotationRadians) &&
            a.horizontalSubdivisions == b.horizontalSubdivisions &&
            a.verticalSubdivisions == b.verticalSubdivisions &&
            NearlyEqual(a.terrainModifier, b.terrainModifier) &&
            NearlyEqual(a.terrainTextureFalloff, b.terrainTextureFalloff) &&
            NearlyEqual(a.terrainPushDown, b.terrainPushDown) &&
            a.fillNormals == b.fillNormals;
    }

    bool EntityExists(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    void RemoveImportedEntities(
        wi::scene::Scene& scene,
        const wi::unordered_set<wi::ecs::Entity>& before)
    {
        wi::unordered_set<wi::ecs::Entity> after;
        scene.FindAllEntities(after);
        wi::vector<wi::ecs::Entity> roots;
        for (const auto entity : after)
        {
            if (before.count(entity) != 0)
                continue;
            const auto* hierarchy = scene.hierarchy.GetComponent(entity);
            const bool parentIsAlsoNew = hierarchy != nullptr &&
                hierarchy->parentID != wi::ecs::INVALID_ENTITY &&
                before.count(hierarchy->parentID) == 0 &&
                after.count(hierarchy->parentID) != 0;
            if (!parentIsAlsoNew)
                roots.push_back(entity);
        }
        for (const auto root : roots)
            scene.Entity_Remove(root, true);
    }
}

namespace renegade::bridge
{
    CreateSpecialistComponentCommand::CreateSpecialistComponentCommand(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        SpecialistComponentKind kind)
        : scene_(&scene), entity_(entity), kind_(kind)
    {
    }

    bool CreateSpecialistComponentCommand::Execute()
    {
        return Create();
    }

    void CreateSpecialistComponentCommand::Undo()
    {
        (void)Remove();
    }

    bool CreateSpecialistComponentCommand::Create()
    {
        if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY)
            return false;

        switch (kind_)
        {
        case SpecialistComponentKind::HairParticle:
            if (scene_->hairs.Contains(entity_)) return false;
            scene_->hairs.Create(entity_);
            if (!scene_->materials.Contains(entity_))
            {
                scene_->materials.Create(entity_);
                createdMaterial_ = true;
            }
            return true;
        case SpecialistComponentKind::ForceField:
            if (scene_->forces.Contains(entity_)) return false;
            scene_->forces.Create(entity_);
            return true;
        case SpecialistComponentKind::Video:
            if (scene_->videos.Contains(entity_)) return false;
            scene_->videos.Create(entity_);
            return true;
        case SpecialistComponentKind::Spline:
            if (scene_->splines.Contains(entity_)) return false;
            scene_->splines.Create(entity_);
            if (!scene_->transforms.Contains(entity_))
            {
                scene_->transforms.Create(entity_);
                createdTransform_ = true;
            }
            return true;
        }
        return false;
    }

    bool CreateSpecialistComponentCommand::Remove()
    {
        if (scene_ == nullptr)
            return false;
        switch (kind_)
        {
        case SpecialistComponentKind::HairParticle:
            if (!scene_->hairs.Contains(entity_)) return false;
            scene_->hairs.Remove(entity_);
            if (createdMaterial_) scene_->materials.Remove(entity_);
            return true;
        case SpecialistComponentKind::ForceField:
            if (!scene_->forces.Contains(entity_)) return false;
            scene_->forces.Remove(entity_);
            return true;
        case SpecialistComponentKind::Video:
            if (!scene_->videos.Contains(entity_)) return false;
            scene_->videos.Remove(entity_);
            return true;
        case SpecialistComponentKind::Spline:
            if (!scene_->splines.Contains(entity_)) return false;
            scene_->splines.Remove(entity_);
            if (createdTransform_) scene_->transforms.Remove(entity_);
            return true;
        }
        return false;
    }

    HairParticleState CaptureHairParticle(const wi::HairParticleSystem& hair)
    {
        HairParticleState state;
        state.mesh = hair.meshID;
        state.cameraBend = hair.IsCameraBendEnabled();
        state.strandCount = hair.strandCount;
        state.segmentCount = hair.segmentCount;
        state.billboardCount = hair.billboardCount;
        state.randomSeed = hair.randomSeed;
        state.length = hair.length;
        state.width = hair.width;
        state.stiffness = hair.stiffness;
        state.drag = hair.drag;
        state.gravity = hair.gravityPower;
        state.randomness = hair.randomness;
        state.viewDistance = hair.viewDistance;
        state.uniformity = hair.uniformity;
        state.atlasRects.reserve(hair.atlas_rects.size());
        for (const auto& rect : hair.atlas_rects)
            state.atlasRects.push_back({rect.texMulAdd, rect.size});
        return state;
    }

    void ApplyHairParticle(wi::HairParticleSystem& hair, const HairParticleState& state)
    {
        hair.meshID = state.mesh;
        hair.SetCameraBendEnabled(state.cameraBend);
        hair.strandCount = std::min<std::uint32_t>(state.strandCount, 100000u);
        hair.segmentCount = std::clamp<std::uint32_t>(state.segmentCount, 1u, 10u);
        hair.billboardCount = std::clamp<std::uint32_t>(state.billboardCount, 1u, 10u);
        hair.randomSeed = std::max<std::uint32_t>(state.randomSeed, 1u);
        hair.length = std::clamp(state.length, 0.0f, 4.0f);
        hair.width = std::clamp(state.width, 0.0f, 2.0f);
        hair.stiffness = std::clamp(state.stiffness, 0.0f, 10.0f);
        hair.drag = std::clamp(state.drag, 0.0f, 1.0f);
        hair.gravityPower = std::clamp(state.gravity, 0.0f, 1.0f);
        hair.randomness = std::clamp(state.randomness, 0.0f, 1.0f);
        hair.viewDistance = std::clamp(state.viewDistance, 0.0f, 1000.0f);
        hair.uniformity = std::clamp(state.uniformity, 0.01f, 2.0f);
        hair.atlas_rects.clear();
        hair.atlas_rects.reserve(state.atlasRects.size());
        for (const auto& rect : state.atlasRects)
        {
            auto& nativeRect = hair.atlas_rects.emplace_back();
            nativeRect.texMulAdd = rect.texMulAdd;
            nativeRect.size = std::clamp(rect.size, 0.0f, 2.0f);
        }
        hair.SetDirty();
    }

    SetHairParticleStateCommand::SetHairParticleStateCommand(
        wi::scene::Scene& scene, wi::ecs::Entity entity, HairParticleState state)
        : scene_(&scene), entity_(entity), after_(std::move(state))
    {
        if (const auto* hair = scene.hairs.GetComponent(entity))
            before_ = CaptureHairParticle(*hair);
    }

    bool SetHairParticleStateCommand::Execute()
    {
        return !SameHair(before_, after_) && Apply(after_);
    }

    void SetHairParticleStateCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetHairParticleStateCommand::Apply(const HairParticleState& state)
    {
        if (scene_ == nullptr) return false;
        auto* hair = scene_->hairs.GetComponent(entity_);
        if (hair == nullptr) return false;
        ApplyHairParticle(*hair, state);
        return true;
    }

    ForceFieldState CaptureForceField(const wi::scene::ForceFieldComponent& force) noexcept
    {
        return {force.type, force.gravity, force.range};
    }

    void ApplyForceField(wi::scene::ForceFieldComponent& force, const ForceFieldState& state) noexcept
    {
        force.type = state.type;
        force.gravity = std::clamp(state.gravity, -100.0f, 100.0f);
        force.range = std::max(0.0f, state.range);
    }

    SetForceFieldStateCommand::SetForceFieldStateCommand(
        wi::scene::Scene& scene, wi::ecs::Entity entity, ForceFieldState state)
        : scene_(&scene), entity_(entity), after_(state)
    {
        if (const auto* force = scene.forces.GetComponent(entity))
            before_ = CaptureForceField(*force);
    }

    bool SetForceFieldStateCommand::Execute()
    {
        return !SameForce(before_, after_) && Apply(after_);
    }

    void SetForceFieldStateCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetForceFieldStateCommand::Apply(const ForceFieldState& state)
    {
        if (scene_ == nullptr) return false;
        auto* force = scene_->forces.GetComponent(entity_);
        if (force == nullptr) return false;
        ApplyForceField(*force, state);
        return true;
    }

    VideoAuthoredState CaptureVideoAuthoredState(const wi::scene::VideoComponent& video)
    {
        return {video.filename, video.IsLooped()};
    }

    VideoInfo CaptureVideoInfo(const wi::scene::VideoComponent& video)
    {
        VideoInfo info;
        info.loaded = video.videoResource.IsValid();
        info.playing = video.IsPlaying();
        info.currentTime = video.currentTimer;
        if (!info.loaded)
            return info;
        const auto& native = video.videoResource.GetVideo();
        info.duration = native.duration_seconds;
        info.width = native.width;
        info.height = native.height;
        info.framesPerSecond = native.average_frames_per_second;
        if (native.profile == wi::graphics::VideoProfile::H264)
            info.profile = "H264";
        else if (native.profile == wi::graphics::VideoProfile::H265)
            info.profile = "H265";
        else
            info.profile = "Unsupported";
        return info;
    }

    bool ApplyVideoAuthoredState(
        wi::scene::VideoComponent& video, const VideoAuthoredState& state)
    {
        if (video.filename != state.filename)
        {
            wi::Resource nextResource;
            wi::video::VideoInstance nextInstance;
            if (!state.filename.empty())
            {
                nextResource = wi::resourcemanager::Load(state.filename);
                if (!nextResource.IsValid())
                    return false;
                if (!wi::video::CreateVideoInstance(
                        &nextResource.GetVideo(), &nextInstance))
                {
                    return false;
                }
            }

            video.Stop();
            video.filename = state.filename;
            video.videoResource = std::move(nextResource);
            video.videoinstance = std::move(nextInstance);
        }
        video.SetLooped(state.looped);
        return true;
    }

    SetVideoAuthoredStateCommand::SetVideoAuthoredStateCommand(
        wi::scene::Scene& scene, wi::ecs::Entity entity, VideoAuthoredState state)
        : scene_(&scene), entity_(entity), after_(std::move(state))
    {
        if (const auto* video = scene.videos.GetComponent(entity))
            before_ = CaptureVideoAuthoredState(*video);
    }

    bool SetVideoAuthoredStateCommand::Execute()
    {
        return !SameVideo(before_, after_) && Apply(after_);
    }

    void SetVideoAuthoredStateCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetVideoAuthoredStateCommand::Apply(const VideoAuthoredState& state)
    {
        if (scene_ == nullptr) return false;
        auto* video = scene_->videos.GetComponent(entity_);
        return video != nullptr && ApplyVideoAuthoredState(*video, state);
    }

    bool PlayVideo(wi::scene::Scene& scene, wi::ecs::Entity entity) noexcept
    {
        auto* video = scene.videos.GetComponent(entity);
        if (video == nullptr || !video->videoResource.IsValid()) return false;
        video->Play();
        return true;
    }

    bool PauseVideo(wi::scene::Scene& scene, wi::ecs::Entity entity) noexcept
    {
        auto* video = scene.videos.GetComponent(entity);
        if (video == nullptr || !video->videoResource.IsValid()) return false;
        video->Pause();
        return true;
    }

    bool StopVideo(wi::scene::Scene& scene, wi::ecs::Entity entity) noexcept
    {
        auto* video = scene.videos.GetComponent(entity);
        if (video == nullptr || !video->videoResource.IsValid()) return false;
        video->Stop();
        return true;
    }

    bool SeekVideo(wi::scene::Scene& scene, wi::ecs::Entity entity, float seconds) noexcept
    {
        auto* video = scene.videos.GetComponent(entity);
        if (video == nullptr || !video->videoResource.IsValid()) return false;
        const float duration = video->videoResource.GetVideo().duration_seconds;
        video->Seek(std::clamp(seconds, 0.0f, std::max(0.0f, duration)));
        return true;
    }

    SplineState CaptureSpline(const wi::scene::SplineComponent& spline) noexcept
    {
        SplineState state;
        state.looped = spline.IsLooped();
        state.filled = spline.IsFilled();
        state.drawAligned = spline.IsDrawAligned();
        state.width = spline.width;
        state.rotationRadians = spline.rotation;
        state.horizontalSubdivisions = spline.mesh_generation_subdivision;
        state.verticalSubdivisions = spline.mesh_generation_vertical_subdivision;
        state.terrainModifier = spline.terrain_modifier_amount;
        state.terrainTextureFalloff = spline.terrain_texture_falloff;
        state.terrainPushDown = spline.terrain_pushdown;
        state.fillNormals = spline.fill_normals_mode;
        return state;
    }

    void ApplySpline(wi::scene::SplineComponent& spline, const SplineState& state) noexcept
    {
        spline.SetLooped(state.looped);
        spline.SetFilled(state.looped && state.filled);
        spline.SetDrawAligned(state.drawAligned);
        spline.width = std::clamp(state.width, 0.001f, 100.0f);
        spline.rotation = state.rotationRadians;
        spline.mesh_generation_subdivision = std::clamp(state.horizontalSubdivisions, 0, 100);
        spline.mesh_generation_vertical_subdivision = std::clamp(state.verticalSubdivisions, 0, 36);
        spline.terrain_modifier_amount = std::clamp(state.terrainModifier, 0.0f, 1.0f);
        spline.terrain_texture_falloff = std::clamp(state.terrainTextureFalloff, 0.0f, 1.0f);
        spline.terrain_pushdown = std::clamp(state.terrainPushDown, 0.0f, 100.0f);
        spline.fill_normals_mode = state.fillNormals;
        spline.SetDirty();
    }

    SetSplineStateCommand::SetSplineStateCommand(
        wi::scene::Scene& scene, wi::ecs::Entity entity, SplineState state)
        : scene_(&scene), entity_(entity), after_(state)
    {
        if (const auto* spline = scene.splines.GetComponent(entity))
            before_ = CaptureSpline(*spline);
    }

    bool SetSplineStateCommand::Execute()
    {
        return !SameSpline(before_, after_) && Apply(after_);
    }

    void SetSplineStateCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetSplineStateCommand::Apply(const SplineState& state)
    {
        if (scene_ == nullptr) return false;
        auto* spline = scene_->splines.GetComponent(entity_);
        if (spline == nullptr) return false;
        ApplySpline(*spline, state);
        return true;
    }

    AddSplineNodeCommand::AddSplineNodeCommand(
        wi::scene::Scene& scene, wi::ecs::Entity splineEntity)
        : scene_(&scene), splineEntity_(splineEntity)
    {
    }

    bool AddSplineNodeCommand::Execute()
    {
        if (scene_ == nullptr) return false;
        auto* spline = scene_->splines.GetComponent(splineEntity_);
        if (spline == nullptr) return false;

        if (nodeEntity_ != wi::ecs::INVALID_ENTITY)
        {
            if (!hasSnapshot_ || EntityExists(*scene_, nodeEntity_)) return false;
            snapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            const auto restored = scene_->Entity_Serialize(snapshot_, serializer);
            if (restored != nodeEntity_) return false;
            return AddExistingNode();
        }

        nodeEntity_ = wi::ecs::CreateEntity();
        scene_->names.Create(nodeEntity_) =
            "spline_node_" + std::to_string(spline->spline_node_entities.size());
        auto& transform = scene_->transforms.Create(nodeEntity_);
        if (!spline->spline_node_entities.empty())
        {
            if (const auto* last = scene_->transforms.GetComponent(
                    spline->spline_node_entities.back()))
            {
                transform = *last;
                transform.translation_local.x += 1.0f;
                transform.SetDirty();
            }
        }
        spline->spline_node_entities.push_back(nodeEntity_);
        spline->spline_node_transforms.push_back(transform);
        scene_->Component_Attach(nodeEntity_, splineEntity_);
        spline->SetDirty();
        return true;
    }

    bool AddSplineNodeCommand::AddExistingNode()
    {
        auto* spline = scene_->splines.GetComponent(splineEntity_);
        auto* transform = scene_->transforms.GetComponent(nodeEntity_);
        if (spline == nullptr || transform == nullptr) return false;
        if (std::find(spline->spline_node_entities.begin(),
                      spline->spline_node_entities.end(), nodeEntity_) ==
            spline->spline_node_entities.end())
        {
            spline->spline_node_entities.push_back(nodeEntity_);
            spline->spline_node_transforms.push_back(*transform);
        }
        spline->SetDirty();
        return true;
    }

    void AddSplineNodeCommand::RemoveNodeReference()
    {
        auto* spline = scene_->splines.GetComponent(splineEntity_);
        if (spline == nullptr) return;
        const auto it = std::find(
            spline->spline_node_entities.begin(),
            spline->spline_node_entities.end(), nodeEntity_);
        if (it == spline->spline_node_entities.end()) return;
        const auto index = static_cast<std::size_t>(
            std::distance(spline->spline_node_entities.begin(), it));
        spline->spline_node_entities.erase(it);
        if (index < spline->spline_node_transforms.size())
            spline->spline_node_transforms.erase(spline->spline_node_transforms.begin() + index);
        spline->SetDirty();
    }

    void AddSplineNodeCommand::Undo()
    {
        if (scene_ == nullptr || !EntityExists(*scene_, nodeEntity_)) return;
        RemoveNodeReference();
        if (!hasSnapshot_)
        {
            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, nodeEntity_);
            hasSnapshot_ = true;
        }
        scene_->Entity_Remove(nodeEntity_);
    }

    RemoveLastSplineNodeCommand::RemoveLastSplineNodeCommand(
        wi::scene::Scene& scene, wi::ecs::Entity splineEntity)
        : scene_(&scene), splineEntity_(splineEntity)
    {
    }

    bool RemoveLastSplineNodeCommand::Execute()
    {
        if (scene_ == nullptr) return false;
        auto* spline = scene_->splines.GetComponent(splineEntity_);
        if (spline == nullptr || spline->spline_node_entities.empty()) return false;

        if (!captured_)
        {
            nodeEntity_ = spline->spline_node_entities.back();
            if (!EntityExists(*scene_, nodeEntity_)) return false;
            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, nodeEntity_);
            captured_ = true;
        }
        else if (!EntityExists(*scene_, nodeEntity_))
        {
            return false;
        }

        const auto it = std::find(
            spline->spline_node_entities.begin(),
            spline->spline_node_entities.end(), nodeEntity_);
        if (it == spline->spline_node_entities.end()) return false;
        const auto index = static_cast<std::size_t>(
            std::distance(spline->spline_node_entities.begin(), it));
        spline->spline_node_entities.erase(it);
        if (index < spline->spline_node_transforms.size())
            spline->spline_node_transforms.erase(spline->spline_node_transforms.begin() + index);
        scene_->Entity_Remove(nodeEntity_);
        spline->SetDirty();
        return true;
    }

    void RemoveLastSplineNodeCommand::Undo()
    {
        if (scene_ == nullptr || !captured_ || EntityExists(*scene_, nodeEntity_)) return;
        auto* spline = scene_->splines.GetComponent(splineEntity_);
        if (spline == nullptr) return;
        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        const auto restored = scene_->Entity_Serialize(snapshot_, serializer);
        if (restored != nodeEntity_) return;
        auto* transform = scene_->transforms.GetComponent(nodeEntity_);
        if (transform == nullptr) return;
        spline->spline_node_entities.push_back(nodeEntity_);
        spline->spline_node_transforms.push_back(*transform);
        spline->SetDirty();
    }

    GaussianSplatInfo CaptureGaussianSplatInfo(
        const wi::scene::Scene& scene, wi::ecs::Entity entity) noexcept
    {
        GaussianSplatInfo info;
        const auto* splat = scene.gaussian_splats.GetComponent(entity);
        if (splat == nullptr) return info;
        info.valid = true;
        info.splatCount = splat->GetSplatCount();
        if (info.splatCount > 0)
            info.sphericalHarmonicsDegree = splat->GetSphericalHarmonicsDegree();
        info.cpuMemoryBytes = splat->GetMemorySizeCPU();
        info.gpuMemoryBytes = splat->GetMemorySizeGPU();
        return info;
    }

    ImportGaussianSplatCommand::ImportGaussianSplatCommand(
        wi::scene::Scene& scene, std::string filename)
        : scene_(&scene), filename_(std::move(filename))
    {
    }

    bool ImportGaussianSplatCommand::Execute()
    {
        error_.clear();
        if (scene_ == nullptr || filename_.empty())
        {
            error_ = "invalid PLY import request";
            return false;
        }
        if (hasSnapshot_)
            return Restore();

        wi::unordered_set<wi::ecs::Entity> before;
        scene_->FindAllEntities(before);
        const std::size_t splatCountBefore = scene_->gaussian_splats.GetCount();
        ImportModel_PLY(filename_, *scene_);
        if (scene_->gaussian_splats.GetCount() <= splatCountBefore)
        {
            RemoveImportedEntities(*scene_, before);
            error_ = "PLY did not contain Gaussian-splat SH data";
            return false;
        }

        entity_ = scene_->gaussian_splats.GetEntity(
            scene_->gaussian_splats.GetCount() - 1);
        if (entity_ == wi::ecs::INVALID_ENTITY)
        {
            RemoveImportedEntities(*scene_, before);
            error_ = "native PLY importer returned no Gaussian entity";
            return false;
        }
        return true;
    }

    void ImportGaussianSplatCommand::Undo()
    {
        if (scene_ == nullptr || !EntityExists(*scene_, entity_)) return;
        if (!hasSnapshot_)
        {
            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, entity_);
            hasSnapshot_ = true;
        }
        scene_->Entity_Remove(entity_);
    }

    bool ImportGaussianSplatCommand::Restore()
    {
        if (scene_ == nullptr || EntityExists(*scene_, entity_)) return false;
        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        const auto restored = scene_->Entity_Serialize(snapshot_, serializer);
        if (restored != entity_ || !scene_->gaussian_splats.Contains(restored))
        {
            error_ = "failed to restore imported Gaussian splat";
            return false;
        }
        if (auto* splat = scene_->gaussian_splats.GetComponent(restored))
            splat->CreateRenderData();
        return true;
    }
}
