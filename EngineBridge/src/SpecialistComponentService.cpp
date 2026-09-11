#include "renegade/bridge/SpecialistComponentService.h"

#include "ModelImporter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        constexpr float Epsilon = 0.00001f;

        bool NearlyEqual(const float left, const float right) noexcept
        {
            return std::abs(left - right) <= Epsilon;
        }

        float FiniteOr(const float value, const float fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        bool EntityExists(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity)
        {
            if (entity == wi::ecs::INVALID_ENTITY)
                return false;
            wi::unordered_set<wi::ecs::Entity> entities;
            scene.FindAllEntities(entities);
            return entities.count(entity) != 0;
        }

        bool HasSpecialistComponent(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const SpecialistComponentKind kind) noexcept
        {
            switch (kind)
            {
            case SpecialistComponentKind::Hair:
                return scene.hairs.Contains(entity);
            case SpecialistComponentKind::ForceField:
                return scene.forces.Contains(entity);
            case SpecialistComponentKind::Video:
                return scene.videos.Contains(entity);
            case SpecialistComponentKind::Spline:
                return scene.splines.Contains(entity);
            }
            return false;
        }

        void CreateSpecialistComponent(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const SpecialistComponentKind kind)
        {
            switch (kind)
            {
            case SpecialistComponentKind::Hair:
                scene.hairs.Create(entity);
                if (!scene.materials.Contains(entity))
                    scene.materials.Create(entity);
                break;
            case SpecialistComponentKind::ForceField:
                scene.forces.Create(entity);
                break;
            case SpecialistComponentKind::Video:
                scene.videos.Create(entity);
                break;
            case SpecialistComponentKind::Spline:
                scene.splines.Create(entity);
                if (!scene.transforms.Contains(entity))
                    scene.transforms.Create(entity);
                break;
            }
        }

        void RemoveSpecialistComponent(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const SpecialistComponentKind kind)
        {
            switch (kind)
            {
            case SpecialistComponentKind::Hair:
                scene.hairs.Remove(entity);
                break;
            case SpecialistComponentKind::ForceField:
                scene.forces.Remove(entity);
                break;
            case SpecialistComponentKind::Video:
                scene.videos.Remove(entity);
                break;
            case SpecialistComponentKind::Spline:
                scene.splines.Remove(entity);
                break;
            }
        }

        bool HairChanged(
            const HairParticleState& left,
            const HairParticleState& right) noexcept
        {
            return left.mesh != right.mesh ||
                left.cameraBend != right.cameraBend ||
                left.strandCount != right.strandCount ||
                !NearlyEqual(left.length, right.length) ||
                !NearlyEqual(left.width, right.width) ||
                !NearlyEqual(left.stiffness, right.stiffness) ||
                !NearlyEqual(left.drag, right.drag) ||
                !NearlyEqual(left.gravityPower, right.gravityPower) ||
                !NearlyEqual(left.randomness, right.randomness) ||
                left.segments != right.segments ||
                left.billboards != right.billboards ||
                left.randomSeed != right.randomSeed ||
                !NearlyEqual(left.viewDistance, right.viewDistance) ||
                !NearlyEqual(left.uniformity, right.uniformity);
        }

        bool ForceChanged(
            const ForceFieldState& left,
            const ForceFieldState& right) noexcept
        {
            return left.type != right.type ||
                !NearlyEqual(left.gravity, right.gravity) ||
                !NearlyEqual(left.range, right.range);
        }

        ForceFieldState SanitizeForce(const ForceFieldState& state) noexcept
        {
            ForceFieldState result = state;
            result.gravity = std::clamp(FiniteOr(result.gravity, 0.0f), -10.0f, 10.0f);
            result.range = std::clamp(FiniteOr(result.range, 10.0f), 0.0f, 100.0f);
            return result;
        }

        bool SplineChanged(
            const SplineAuthoringState& left,
            const SplineAuthoringState& right) noexcept
        {
            return left.looped != right.looped ||
                left.filled != right.filled ||
                left.drawAligned != right.drawAligned ||
                !NearlyEqual(left.width, right.width) ||
                !NearlyEqual(left.rotationDegrees, right.rotationDegrees) ||
                left.meshSubdivision != right.meshSubdivision ||
                left.verticalSubdivision != right.verticalSubdivision ||
                !NearlyEqual(left.terrainModifier, right.terrainModifier) ||
                !NearlyEqual(left.terrainTextureFalloff, right.terrainTextureFalloff) ||
                !NearlyEqual(left.terrainPushdown, right.terrainPushdown) ||
                left.fillNormals != right.fillNormals;
        }

        SplineAuthoringState SanitizeSpline(
            const SplineAuthoringState& state) noexcept
        {
            SplineAuthoringState result = state;
            if (!result.looped)
                result.filled = false;
            result.width = std::clamp(FiniteOr(result.width, 1.0f), 0.001f, 4.0f);
            result.rotationDegrees = std::fmod(FiniteOr(result.rotationDegrees, 0.0f), 360.0f);
            if (result.rotationDegrees < 0.0f)
                result.rotationDegrees += 360.0f;
            result.meshSubdivision = std::clamp(result.meshSubdivision, 0, 100);
            result.verticalSubdivision = std::clamp(result.verticalSubdivision, 0, 36);
            result.terrainModifier = std::clamp(FiniteOr(result.terrainModifier, 0.0f), 0.0f, 1.0f);
            result.terrainTextureFalloff = std::clamp(FiniteOr(result.terrainTextureFalloff, 0.0f), 0.0f, 1.0f);
            result.terrainPushdown = std::clamp(FiniteOr(result.terrainPushdown, 0.0f), 0.0f, 10.0f);
            return result;
        }

        wi::terrain::ChunkData* FindChunk(
            wi::terrain::Terrain& terrain,
            const wi::terrain::Chunk& coordinate) noexcept
        {
            const auto found = terrain.chunks.find(coordinate);
            return found == terrain.chunks.end() ? nullptr : &found->second;
        }

        const wi::terrain::ChunkData* FindChunk(
            const wi::terrain::Terrain& terrain,
            const wi::terrain::Chunk& coordinate) noexcept
        {
            const auto found = terrain.chunks.find(coordinate);
            return found == terrain.chunks.end() ? nullptr : &found->second;
        }

        void RefreshPaintChunk(
            wi::terrain::Terrain& terrain,
            wi::terrain::ChunkData& chunk)
        {
            chunk.invalidated = true;
            if (chunk.vt)
                chunk.vt->invalidate();
            terrain.CreateChunkRegionTexture(chunk);
        }
    }

    EnsureSpecialistComponentCommand::EnsureSpecialistComponentCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const SpecialistComponentKind kind)
        : scene_(&scene)
        , entity_(entity)
        , kind_(kind)
    {
    }

    bool EnsureSpecialistComponentCommand::Execute()
    {
        if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY ||
            !EntityExists(*scene_, entity_) || HasSpecialistComponent(*scene_, entity_, kind_))
        {
            return false;
        }
        CreateSpecialistComponent(*scene_, entity_, kind_);
        created_ = HasSpecialistComponent(*scene_, entity_, kind_);
        return created_;
    }

    void EnsureSpecialistComponentCommand::Undo()
    {
        if (scene_ != nullptr && created_)
            RemoveSpecialistComponent(*scene_, entity_, kind_);
    }

    HairParticleState CaptureHairParticle(const wi::HairParticleSystem& hair) noexcept
    {
        HairParticleState state;
        state.mesh = hair.meshID;
        state.cameraBend = hair.IsCameraBendEnabled();
        state.strandCount = hair.strandCount;
        state.length = hair.length;
        state.width = hair.width;
        state.stiffness = hair.stiffness;
        state.drag = hair.drag;
        state.gravityPower = hair.gravityPower;
        state.randomness = hair.randomness;
        state.segments = hair.segmentCount;
        state.billboards = hair.billboardCount;
        state.randomSeed = hair.randomSeed;
        state.viewDistance = hair.viewDistance;
        state.uniformity = hair.uniformity;
        return state;
    }

    HairParticleState SanitizeHairParticle(
        const wi::scene::Scene& scene,
        const HairParticleState& state) noexcept
    {
        HairParticleState result = state;
        if (result.mesh != wi::ecs::INVALID_ENTITY && !scene.meshes.Contains(result.mesh))
            result.mesh = wi::ecs::INVALID_ENTITY;
        result.strandCount = std::min<std::uint32_t>(result.strandCount, 100000u);
        result.length = std::clamp(FiniteOr(result.length, 1.0f), 0.0f, 4.0f);
        result.width = std::clamp(FiniteOr(result.width, 1.0f), 0.0f, 2.0f);
        result.stiffness = std::clamp(FiniteOr(result.stiffness, 0.5f), 0.0f, 10.0f);
        result.drag = std::clamp(FiniteOr(result.drag, 0.5f), 0.0f, 1.0f);
        result.gravityPower = std::clamp(FiniteOr(result.gravityPower, 0.5f), 0.0f, 1.0f);
        result.randomness = std::clamp(FiniteOr(result.randomness, 0.2f), 0.0f, 1.0f);
        result.segments = std::clamp<std::uint32_t>(result.segments, 1u, 10u);
        result.billboards = std::clamp<std::uint32_t>(result.billboards, 1u, 10u);
        result.randomSeed = std::clamp<std::uint32_t>(result.randomSeed, 1u, 12345u);
        result.viewDistance = std::clamp(FiniteOr(result.viewDistance, 100.0f), 0.0f, 1000.0f);
        result.uniformity = std::clamp(FiniteOr(result.uniformity, 0.1f), 0.01f, 2.0f);
        return result;
    }

    void ApplyHairParticle(
        wi::HairParticleSystem& hair,
        const HairParticleState& state) noexcept
    {
        hair.meshID = state.mesh;
        hair.SetCameraBendEnabled(state.cameraBend);
        hair.strandCount = state.strandCount;
        hair.length = state.length;
        hair.width = state.width;
        hair.stiffness = state.stiffness;
        hair.drag = state.drag;
        hair.gravityPower = state.gravityPower;
        hair.randomness = state.randomness;
        hair.segmentCount = state.segments;
        hair.billboardCount = state.billboards;
        hair.randomSeed = state.randomSeed;
        hair.viewDistance = state.viewDistance;
        hair.uniformity = state.uniformity;
        hair.SetDirty();
    }

    SetHairParticleCommand::SetHairParticleCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        HairParticleState state)
        : scene_(&scene)
        , entity_(entity)
        , after_(SanitizeHairParticle(scene, state))
    {
        if (const auto* hair = scene.hairs.GetComponent(entity_))
            before_ = CaptureHairParticle(*hair);
    }

    bool SetHairParticleCommand::Execute()
    {
        return HairChanged(before_, after_) && Apply(after_);
    }

    void SetHairParticleCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetHairParticleCommand::Apply(const HairParticleState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* hair = scene_->hairs.GetComponent(entity_);
        if (hair == nullptr)
            return false;
        ApplyHairParticle(*hair, SanitizeHairParticle(*scene_, state));
        return true;
    }

    ForceFieldState CaptureForceField(const wi::scene::ForceFieldComponent& force) noexcept
    {
        return {force.type, force.gravity, force.range};
    }

    SetForceFieldCommand::SetForceFieldCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        ForceFieldState state)
        : scene_(&scene)
        , entity_(entity)
        , after_(SanitizeForce(state))
    {
        if (const auto* force = scene.forces.GetComponent(entity_))
            before_ = CaptureForceField(*force);
    }

    bool SetForceFieldCommand::Execute()
    {
        return ForceChanged(before_, after_) && Apply(after_);
    }

    void SetForceFieldCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetForceFieldCommand::Apply(const ForceFieldState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* force = scene_->forces.GetComponent(entity_);
        if (force == nullptr)
            return false;
        const auto safe = SanitizeForce(state);
        force->type = safe.type;
        force->gravity = safe.gravity;
        force->range = safe.range;
        return true;
    }

    VideoAuthoringState CaptureVideoAuthoring(const wi::scene::VideoComponent& video)
    {
        VideoAuthoringState state;
        state.filename = video.filename;
        state.looped = video.IsLooped();
        return state;
    }

    bool ApplyVideoAuthoring(
        wi::scene::VideoComponent& video,
        const VideoAuthoringState& state,
        std::string& error)
    {
        error.clear();
        const bool fileChanged = video.filename != state.filename;
        video.SetLooped(state.looped);
        if (!fileChanged)
            return true;

        video.Stop();
        video.filename = state.filename;
        video.videoResource = {};
        video.videoinstance = {};
        if (video.filename.empty())
            return true;

        video.videoResource = wi::resourcemanager::Load(video.filename);
        if (!video.videoResource.IsValid())
        {
            error = "Wicked could not load the selected video resource: " + video.filename;
            return false;
        }
        wi::video::CreateVideoInstance(
            &video.videoResource.GetVideo(),
            &video.videoinstance);
        return true;
    }

    bool PreviewVideo(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const bool play) noexcept
    {
        auto* video = scene.videos.GetComponent(entity);
        if (video == nullptr || !video->videoResource.IsValid())
            return false;
        if (play)
            video->Play();
        else
            video->Pause();
        return true;
    }

    bool StopVideoPreview(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        auto* video = scene.videos.GetComponent(entity);
        if (video == nullptr)
            return false;
        video->Stop();
        return true;
    }

    bool SeekVideoPreview(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const float seconds) noexcept
    {
        auto* video = scene.videos.GetComponent(entity);
        if (video == nullptr || !video->videoResource.IsValid() || !std::isfinite(seconds))
            return false;
        const float duration = video->videoResource.GetVideo().duration_seconds;
        video->Seek(std::clamp(seconds, 0.0f, std::max(0.0f, duration)));
        return true;
    }

    SetVideoAuthoringCommand::SetVideoAuthoringCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        VideoAuthoringState state)
        : scene_(&scene)
        , entity_(entity)
        , after_(std::move(state))
    {
        if (const auto* video = scene.videos.GetComponent(entity_))
            before_ = CaptureVideoAuthoring(*video);
    }

    bool SetVideoAuthoringCommand::Execute()
    {
        if (before_.filename == after_.filename && before_.looped == after_.looped)
            return false;
        return Apply(after_);
    }

    void SetVideoAuthoringCommand::Undo()
    {
        (void)Apply(before_);
    }

    const std::string& SetVideoAuthoringCommand::LastError() const noexcept
    {
        return lastError_;
    }

    bool SetVideoAuthoringCommand::Apply(const VideoAuthoringState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* video = scene_->videos.GetComponent(entity_);
        if (video == nullptr)
            return false;
        lastError_.clear();
        return ApplyVideoAuthoring(*video, state, lastError_);
    }

    SplineAuthoringState CaptureSplineAuthoring(
        const wi::scene::SplineComponent& spline) noexcept
    {
        SplineAuthoringState state;
        state.looped = spline.IsLooped();
        state.filled = spline.IsFilled();
        state.drawAligned = spline.IsDrawAligned();
        state.width = spline.width;
        state.rotationDegrees = wi::math::RadiansToDegrees(spline.rotation);
        state.meshSubdivision = spline.mesh_generation_subdivision;
        state.verticalSubdivision = spline.mesh_generation_vertical_subdivision;
        state.terrainModifier = spline.terrain_modifier_amount;
        state.terrainTextureFalloff = spline.terrain_texture_falloff;
        state.terrainPushdown = spline.terrain_pushdown;
        state.fillNormals = spline.fill_normals_mode;
        return state;
    }

    void ApplySplineAuthoring(
        wi::scene::SplineComponent& spline,
        const SplineAuthoringState& state) noexcept
    {
        const auto safe = SanitizeSpline(state);
        spline.SetLooped(safe.looped);
        spline.SetFilled(safe.filled);
        spline.SetDrawAligned(safe.drawAligned);
        spline.width = safe.width;
        spline.rotation = wi::math::DegreesToRadians(safe.rotationDegrees);
        spline.mesh_generation_subdivision = safe.meshSubdivision;
        spline.mesh_generation_vertical_subdivision = safe.verticalSubdivision;
        spline.terrain_modifier_amount = safe.terrainModifier;
        spline.terrain_texture_falloff = safe.terrainTextureFalloff;
        spline.terrain_pushdown = safe.terrainPushdown;
        spline.fill_normals_mode = safe.fillNormals;
        spline.SetDirty();
    }

    SetSplineAuthoringCommand::SetSplineAuthoringCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        SplineAuthoringState state)
        : scene_(&scene)
        , entity_(entity)
        , after_(SanitizeSpline(state))
    {
        if (const auto* spline = scene.splines.GetComponent(entity_))
            before_ = CaptureSplineAuthoring(*spline);
    }

    bool SetSplineAuthoringCommand::Execute()
    {
        return SplineChanged(before_, after_) && Apply(after_);
    }

    void SetSplineAuthoringCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool SetSplineAuthoringCommand::Apply(const SplineAuthoringState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* spline = scene_->splines.GetComponent(entity_);
        if (spline == nullptr)
            return false;
        ApplySplineAuthoring(*spline, state);
        return true;
    }

    AddSplineNodeCommand::AddSplineNodeCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity splineEntity,
        const XMFLOAT3 localPosition)
        : scene_(&scene)
        , spline_(splineEntity)
        , localPosition_(localPosition)
    {
    }

    bool AddSplineNodeCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;
        auto* spline = scene_->splines.GetComponent(spline_);
        if (spline == nullptr)
            return false;
        if (snapshotReady_)
            return RestoreNode();

        node_ = wi::ecs::CreateEntity();
        scene_->names.Create(node_).name =
            "spline_node_" + std::to_string(spline->spline_node_entities.size());
        auto& transform = scene_->transforms.Create(node_);
        transform.translation_local = localPosition_;
        transform.SetDirty();
        transform.UpdateTransform();
        insertionIndex_ = spline->spline_node_entities.size();
        spline->spline_node_entities.push_back(node_);
        spline->spline_node_transforms.push_back(transform);
        scene_->Component_Attach(node_, spline_);
        spline->SetDirty();
        return true;
    }

    void AddSplineNodeCommand::Undo()
    {
        if (scene_ == nullptr || node_ == wi::ecs::INVALID_ENTITY)
            return;
        auto* spline = scene_->splines.GetComponent(spline_);
        if (spline == nullptr || !EntityExists(*scene_, node_))
            return;

        if (!snapshotReady_)
        {
            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, node_);
            snapshotReady_ = true;
        }

        const auto found = std::find(
            spline->spline_node_entities.begin(),
            spline->spline_node_entities.end(),
            node_);
        if (found != spline->spline_node_entities.end())
        {
            const auto index = static_cast<std::size_t>(found - spline->spline_node_entities.begin());
            spline->spline_node_entities.erase(found);
            if (index < spline->spline_node_transforms.size())
                spline->spline_node_transforms.erase(spline->spline_node_transforms.begin() + index);
        }
        scene_->Entity_Remove(node_);
        spline->SetDirty();
    }

    wi::ecs::Entity AddSplineNodeCommand::CreatedNode() const noexcept
    {
        return node_;
    }

    bool AddSplineNodeCommand::RestoreNode()
    {
        auto* spline = scene_->splines.GetComponent(spline_);
        if (spline == nullptr || EntityExists(*scene_, node_))
            return false;
        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        if (scene_->Entity_Serialize(snapshot_, serializer) != node_)
            return false;
        auto* transform = scene_->transforms.GetComponent(node_);
        if (transform == nullptr)
            return false;
        const auto position = std::min(insertionIndex_, spline->spline_node_entities.size());
        spline->spline_node_entities.insert(
            spline->spline_node_entities.begin() + static_cast<std::ptrdiff_t>(position), node_);
        spline->spline_node_transforms.insert(
            spline->spline_node_transforms.begin() + static_cast<std::ptrdiff_t>(position), *transform);
        spline->SetDirty();
        return true;
    }

    GaussianSplatInfo InspectGaussianSplat(const wi::GaussianSplatModel& splat) noexcept
    {
        GaussianSplatInfo info;
        info.splatCount = splat.GetSplatCount();
        info.sphericalHarmonicsDegree = splat.GetSphericalHarmonicsDegree();
        info.cpuMemoryBytes = splat.GetMemorySizeCPU();
        info.gpuMemoryBytes = splat.GetMemorySizeGPU();
        return info;
    }

    ImportGaussianSplatCommand::ImportGaussianSplatCommand(
        wi::scene::Scene& scene,
        std::string sourcePath)
        : scene_(&scene)
        , sourcePath_(std::move(sourcePath))
    {
    }

    bool ImportGaussianSplatCommand::Execute()
    {
        lastError_.clear();
        if (scene_ == nullptr || sourcePath_.empty())
            return false;
        return snapshotsReady_ ? Restore() : ImportFirstTime();
    }

    void ImportGaussianSplatCommand::Undo()
    {
        if (scene_ == nullptr)
            return;
        for (const auto entity : entities_)
            scene_->Entity_Remove(entity_);
    }

    const std::vector<wi::ecs::Entity>&
    ImportGaussianSplatCommand::CreatedEntities() const noexcept
    {
        return entities_;
    }

    const std::string& ImportGaussianSplatCommand::LastError() const noexcept
    {
        return lastError_;
    }

    bool ImportGaussianSplatCommand::ImportFirstTime()
    {
        const std::string extension =
            wi::helper::toUpper(wi::helper::GetExtensionFromFileName(sourcePath_));
        if (extension != "PLY")
        {
            lastError_ = "Gaussian Splat import requires a PLY file supported by Wicked's native importer.";
            return false;
        }

        wi::scene::Scene imported;
        ImportModel_PLY(sourcePath_, imported);
        if (imported.gaussian_splats.GetCount() == 0)
        {
            lastError_ = "The PLY file imported but did not contain a native Wicked Gaussian Splat model.";
            return false;
        }

        entities_.clear();
        snapshots_.clear();
        entities_.reserve(imported.gaussian_splats.GetCount());
        snapshots_.reserve(imported.gaussian_splats.GetCount());
        for (std::size_t index = 0; index < imported.gaussian_splats.GetCount(); ++index)
        {
            const auto entity = imported.gaussian_splats.GetEntity(index);
            wi::Archive snapshot;
            snapshot.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer writer;
            imported.Entity_Serialize(snapshot, writer, entity);
            snapshot.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer reader;
            reader.allow_remap = false;
            if (scene_->Entity_Serialize(snapshot, reader) != entity)
            {
                lastError_ = "Failed to adopt the native Gaussian Splat entity into the Renegade scene.";
                for (const auto created : entities_)
                    scene_->Entity_Remove(created);
                entities_.clear();
                snapshots_.clear();
                return false;
            }
            entities_.push_back(entity);
            snapshot.SetReadModeAndResetPos(false);
            snapshots_.push_back(std::move(snapshot));
        }

        // Re-capture from the destination so redo uses exactly the adopted
        // WISCENE entity representation rather than the temporary import Scene.
        snapshots_.clear();
        for (const auto entity : entities_)
        {
            wi::Archive snapshot;
            snapshot.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer writer;
            scene_->Entity_Serialize(snapshot, writer, entity);
            snapshots_.push_back(std::move(snapshot));
        }
        snapshotsReady_ = snapshots_.size() == entities_.size();
        return snapshotsReady_;
    }

    bool ImportGaussianSplatCommand::Restore()
    {
        if (scene_ == nullptr || snapshots_.size() != entities_.size())
            return false;
        for (std::size_t index = 0; index < snapshots_.size(); ++index)
        {
            auto& snapshot = snapshots_[index];
            snapshot.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer reader;
            reader.allow_remap = false;
            if (scene_->Entity_Serialize(snapshot, reader) != entities_[index])
            {
                lastError_ = "Failed to restore Gaussian Splat entity during Redo.";
                return false;
            }
        }
        return true;
    }

    TerrainVirtualTextureStatus InspectTerrainVirtualTextures(
        const wi::terrain::Terrain& terrain) noexcept
    {
        TerrainVirtualTextureStatus status;
        status.atlasValid = terrain.atlas.IsValid();
        status.chunkCount = terrain.chunks.size();
        status.activeVirtualTextureCount = terrain.virtual_textures_in_use.size();
        status.physicalTilesX = terrain.atlas.physical_tile_count_x;
        status.physicalTilesY = terrain.atlas.physical_tile_count_y;
        for (const auto& entry : terrain.chunks)
        {
            if (entry.second.vt)
                ++status.virtualTextureChunkCount;
        }
        return status;
    }

    TerrainPaintState CaptureTerrainPaint(
        const wi::terrain::Terrain& terrain,
        const std::size_t materialIndex)
    {
        TerrainPaintState state;
        if (materialIndex >= wi::terrain::MATERIAL_COUNT)
            return state;
        state.chunks.reserve(terrain.chunks.size());
        for (const auto& entry : terrain.chunks)
        {
            TerrainPaintChunkState chunk;
            chunk.chunk = entry.first;
            chunk.materialIndex = materialIndex;
            if (entry.second.blendmap_layers.size() > materialIndex)
                chunk.pixels = entry.second.blendmap_layers[materialIndex].pixels;
            else
                chunk.pixels.assign(wi::terrain::vertexCount, 0u);
            state.chunks.push_back(std::move(chunk));
        }
        return state;
    }

    bool PaintTerrainMaterial(
        wi::terrain::Terrain& terrain,
        const XMFLOAT3& worldCenter,
        const float radius,
        const float strength,
        const std::size_t materialIndex,
        const bool erase,
        TerrainPaintState& before,
        TerrainPaintState& after,
        std::string& error)
    {
        error.clear();
        before.chunks.clear();
        after.chunks.clear();
        if (terrain.scene == nullptr)
        {
            error = "Terrain material painting requires the native Terrain to be attached to its Scene.";
            return false;
        }
        if (materialIndex >= wi::terrain::MATERIAL_COUNT)
        {
            error = "Terrain material slot is outside Wicked's native material range.";
            return false;
        }
        if (!std::isfinite(radius) || !std::isfinite(strength) || radius <= 0.0f)
        {
            error = "Terrain paint radius and strength must be finite and radius must be above zero.";
            return false;
        }

        const float safeStrength = std::clamp(std::abs(strength), 0.0f, 1.0f);
        if (safeStrength <= 0.0f)
            return false;

        bool changedAny = false;
        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;
            auto* mesh = terrain.scene->meshes.GetComponent(chunk.entity);
            const auto* transform = terrain.scene->transforms.GetComponent(chunk.entity);
            if (mesh == nullptr || transform == nullptr ||
                mesh->vertex_positions.size() != wi::terrain::vertexCount)
            {
                continue;
            }

            std::vector<std::uint8_t> original;
            if (chunk.blendmap_layers.size() > materialIndex)
                original = chunk.blendmap_layers[materialIndex].pixels;
            else
                original.assign(wi::terrain::vertexCount, 0u);
            if (original.size() != wi::terrain::vertexCount)
                original.resize(wi::terrain::vertexCount, 0u);

            std::vector<std::uint8_t> painted = original;
            bool changedChunk = false;
            const XMMATRIX world = transform->GetWorldMatrix();
            for (std::size_t index = 0; index < mesh->vertex_positions.size(); ++index)
            {
                XMFLOAT3 position;
                XMStoreFloat3(
                    &position,
                    XMVector3TransformCoord(
                        XMLoadFloat3(&mesh->vertex_positions[index]),
                        world));
                const float dx = position.x - worldCenter.x;
                const float dz = position.z - worldCenter.z;
                const float distance = std::sqrt(dx * dx + dz * dz);
                if (distance > radius)
                    continue;
                const float falloff = std::clamp(1.0f - distance / radius, 0.0f, 1.0f);
                const int delta = static_cast<int>(
                    std::round(safeStrength * falloff * 255.0f));
                const int value = static_cast<int>(painted[index]) + (erase ? -delta : delta);
                const auto next = static_cast<std::uint8_t>(std::clamp(value, 0, 255));
                if (next != painted[index])
                {
                    painted[index] = next;
                    changedChunk = true;
                }
            }

            if (!changedChunk)
                continue;

            TerrainPaintChunkState beforeChunk;
            beforeChunk.chunk = entry.first;
            beforeChunk.materialIndex = materialIndex;
            beforeChunk.pixels = std::move(original);
            before.chunks.push_back(std::move(beforeChunk));

            chunk.enable_blendmap_layer(materialIndex);
            chunk.blendmap_layers[materialIndex].pixels = painted;
            RefreshPaintChunk(terrain, chunk);

            TerrainPaintChunkState afterChunk;
            afterChunk.chunk = entry.first;
            afterChunk.materialIndex = materialIndex;
            afterChunk.pixels = std::move(painted);
            after.chunks.push_back(std::move(afterChunk));
            changedAny = true;
        }
        return changedAny;
    }

    bool ApplyTerrainPaint(
        wi::terrain::Terrain& terrain,
        const TerrainPaintState& state) noexcept
    {
        bool applied = false;
        for (const auto& saved : state.chunks)
        {
            if (saved.materialIndex >= wi::terrain::MATERIAL_COUNT ||
                saved.pixels.size() != wi::terrain::vertexCount)
            {
                continue;
            }
            auto* chunk = FindChunk(terrain, saved.chunk);
            if (chunk == nullptr)
                continue;
            chunk->enable_blendmap_layer(saved.materialIndex);
            chunk->blendmap_layers[saved.materialIndex].pixels = saved.pixels;
            RefreshPaintChunk(terrain, *chunk);
            applied = true;
        }
        return applied;
    }

    PaintTerrainMaterialCommand::PaintTerrainMaterialCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity terrainEntity,
        TerrainPaintState before,
        TerrainPaintState after)
        : scene_(&scene)
        , terrain_(terrainEntity)
        , before_(std::move(before))
        , after_(std::move(after))
    {
    }

    bool PaintTerrainMaterialCommand::Execute()
    {
        return Apply(after_);
    }

    void PaintTerrainMaterialCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool PaintTerrainMaterialCommand::Apply(const TerrainPaintState& state) noexcept
    {
        if (scene_ == nullptr)
            return false;
        auto* terrain = scene_->terrains.GetComponent(terrain_);
        if (terrain == nullptr)
            return false;
        return ApplyTerrainPaint(*terrain, state);
    }
}
