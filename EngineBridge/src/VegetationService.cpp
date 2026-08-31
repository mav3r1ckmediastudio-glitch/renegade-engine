#include "renegade/bridge/VegetationService.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    constexpr const char* ManualTerrainKey =
        "renegade.wicked_vegetation.manual_terrain_v1";
    constexpr const char* ManualChunkKey =
        "renegade.wicked_vegetation.manual_chunk_v1";
    constexpr float MaskEpsilon = 0.0001f;

    fs::path BundledGrassRoot()
    {
        const auto executable = wi::helper::GetExecutablePath();
        if (!executable.empty())
        {
            return (fs::u8path(executable).parent_path() /
                    "Content" / "terrain")
                .lexically_normal();
        }
        return (fs::u8path(wi::helper::GetCurrentPath()) /
                "Content" / "terrain")
            .lexically_normal();
    }

    bool HasMarker(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const char* key) noexcept
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr && metadata->bool_values.has(key) &&
            metadata->bool_values.get(key);
    }

    void SetMarker(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const char* key)
    {
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
            metadata = &scene.metadatas.Create(entity);
        metadata->bool_values.set(key, true);
    }

    wi::terrain::ChunkData* FindChunk(
        wi::terrain::Terrain& terrain,
        const wi::ecs::Entity entity) noexcept
    {
        for (auto& entry : terrain.chunks)
        {
            if (entry.second.entity == entity)
                return &entry.second;
        }
        return nullptr;
    }

    const wi::terrain::ChunkData* FindChunk(
        const wi::terrain::Terrain& terrain,
        const wi::ecs::Entity entity) noexcept
    {
        for (const auto& entry : terrain.chunks)
        {
            if (entry.second.entity == entity)
                return &entry.second;
        }
        return nullptr;
    }

    std::size_t ActiveVertexCount(
        const wi::HairParticleSystem& grass) noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            grass.vertex_lengths.begin(),
            grass.vertex_lengths.end(),
            [](const float value) { return value > MaskEpsilon; }));
    }

    std::uint32_t NativeStrandCount(
        const wi::terrain::Terrain& terrain,
        const std::size_t activeVertices) noexcept
    {
        const float strands =
            static_cast<float>(activeVertices) * 3.0f *
            terrain.chunk_scale * terrain.chunk_scale;
        return static_cast<std::uint32_t>(
            std::max(0.0f, strands));
    }

    void ApplySettings(
        wi::HairParticleSystem& grass,
        const renegade::bridge::WickedVegetationSettings& settings,
        const bool rebuildTopology)
    {
        grass.length = settings.length;
        grass.width = settings.width;
        grass.stiffness = settings.stiffness;
        grass.drag = settings.drag;
        grass.gravityPower = settings.gravityPower;
        grass.randomness = settings.randomness;
        grass.randomSeed = settings.randomSeed;
        grass.segmentCount = settings.segmentCount;
        grass.billboardCount = settings.billboardCount;
        grass.viewDistance = settings.viewDistance;
        grass.uniformity = settings.uniformity;
        grass.SetCameraBendEnabled(settings.cameraBend);
        if (rebuildTopology)
            grass._flags |= wi::HairParticleSystem::REBUILD_BUFFERS;
        grass.SetDirty();
    }

    bool SameSettings(
        const renegade::bridge::WickedVegetationSettings& left,
        const renegade::bridge::WickedVegetationSettings& right) noexcept
    {
        const auto same = [](const float a, const float b)
        {
            return std::abs(a - b) <= 0.0001f;
        };
        return same(left.density, right.density) &&
            same(left.length, right.length) &&
            same(left.width, right.width) &&
            same(left.stiffness, right.stiffness) &&
            same(left.drag, right.drag) &&
            same(left.gravityPower, right.gravityPower) &&
            same(left.randomness, right.randomness) &&
            left.randomSeed == right.randomSeed &&
            left.segmentCount == right.segmentCount &&
            left.billboardCount == right.billboardCount &&
            same(left.viewDistance, right.viewDistance) &&
            same(left.uniformity, right.uniformity) &&
            left.cameraBend == right.cameraBend;
    }

    void RemoveLiveGrass(
        wi::scene::Scene& scene,
        wi::terrain::ChunkData& chunk)
    {
        if (chunk.grass_entity != wi::ecs::INVALID_ENTITY)
        {
            scene.Entity_Remove(chunk.grass_entity);
            chunk.grass_entity = wi::ecs::INVALID_ENTITY;
        }
    }

    bool PrepareManualChunk(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        wi::terrain::ChunkData& chunk)
    {
        if (HasMarker(scene, chunk.entity, ManualChunkKey))
            return false;

        const auto* mesh = scene.meshes.GetComponent(chunk.entity);
        if (mesh == nullptr || mesh->vertex_positions.empty())
            return false;

        RemoveLiveGrass(scene, chunk);
        chunk.grass = terrain.grass_properties;
        chunk.grass.DeleteRenderData();
        chunk.grass.meshID = wi::ecs::INVALID_ENTITY;
        chunk.grass.vertex_lengths.assign(
            mesh->vertex_positions.size(), 0.0f);
        chunk.grass.indices.clear();
        chunk.grass.strandCount = 0;
        chunk.grass_density_current = terrain.grass_density;
        SetMarker(scene, chunk.entity, ManualChunkKey);
        return true;
    }

    bool SynchronizeLiveGrass(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        wi::terrain::ChunkData& chunk,
        const bool rebuildAuthoringDistribution)
    {
        const auto* mesh = scene.meshes.GetComponent(chunk.entity);
        if (mesh == nullptr ||
            chunk.grass.vertex_lengths.size() !=
                mesh->vertex_positions.size())
        {
            return false;
        }

        const auto activeVertices = ActiveVertexCount(chunk.grass);
        if (activeVertices == 0)
        {
            RemoveLiveGrass(scene, chunk);
            chunk.grass.meshID = wi::ecs::INVALID_ENTITY;
            chunk.grass.strandCount = 0;
            chunk.grass.indices.clear();
            return true;
        }

        chunk.grass.meshID = chunk.entity;
        chunk.grass.strandCount =
            NativeStrandCount(terrain, activeVertices);
        if (rebuildAuthoringDistribution || chunk.grass.indices.empty())
            chunk.grass.CreateFromMesh(*mesh);

        const bool createEntity =
            chunk.grass_entity == wi::ecs::INVALID_ENTITY;
        if (createEntity)
            chunk.grass_entity = wi::ecs::CreateEntity();

        auto* live = scene.hairs.GetComponent(chunk.grass_entity);
        const bool createLive = live == nullptr;
        if (createLive)
            live = &scene.hairs.Create(chunk.grass_entity);

        if (createLive)
        {
            *live = chunk.grass;
            live->strandCount = static_cast<std::uint32_t>(
                static_cast<float>(chunk.grass.strandCount) *
                terrain.grass_density);
            live->CreateRenderData();
        }
        else
        {
            live->meshID = chunk.entity;
            live->vertex_lengths = chunk.grass.vertex_lengths;
            live->strandCount = static_cast<std::uint32_t>(
                static_cast<float>(chunk.grass.strandCount) *
                terrain.grass_density);
            live->_flags |= wi::HairParticleSystem::REBUILD_BUFFERS;
        }

        auto* material = scene.materials.GetComponent(chunk.grass_entity);
        if (material == nullptr)
            material = &scene.materials.Create(chunk.grass_entity);
        *material = terrain.grass_material;
        material->SetDirty();
        if (createLive)
            material->CreateRenderData();

        if (!scene.transforms.Contains(chunk.grass_entity))
            scene.transforms.Create(chunk.grass_entity);
        if (!scene.names.Contains(chunk.grass_entity))
            scene.names.Create(chunk.grass_entity) = "grass";
        if (createEntity)
            scene.Component_Attach(
                chunk.grass_entity, chunk.entity, true);

        chunk.grass_density_current = terrain.grass_density;
        return true;
    }

    void CaptureBefore(
        renegade::bridge::VegetationStrokeState* state,
        const wi::terrain::ChunkData& chunk)
    {
        if (state == nullptr)
            return;
        for (const auto& existing : state->chunks)
        {
            if (existing.chunkEntity == chunk.entity)
                return;
        }
        renegade::bridge::VegetationChunkState snapshot;
        snapshot.chunkEntity = chunk.entity;
        snapshot.vertexLengths = chunk.grass.vertex_lengths;
        state->chunks.push_back(std::move(snapshot));
    }

    std::vector<XMFLOAT3> StrokeSamples(
        renegade::bridge::VegetationStrokeState* state,
        const XMFLOAT3& center,
        const float radius)
    {
        std::vector<XMFLOAT3> samples;
        if (state == nullptr || !state->hasLastCenter)
        {
            samples.push_back(center);
        }
        else
        {
            const float dx = center.x - state->lastCenter.x;
            const float dy = center.y - state->lastCenter.y;
            const float dz = center.z - state->lastCenter.z;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float spacing = std::max(0.5f, radius * 0.35f);
            const int steps = std::max(
                1, static_cast<int>(std::ceil(distance / spacing)));
            samples.reserve(static_cast<std::size_t>(steps));
            for (int step = 1; step <= steps; ++step)
            {
                const float t =
                    static_cast<float>(step) /
                    static_cast<float>(steps);
                samples.push_back({
                    state->lastCenter.x + dx * t,
                    state->lastCenter.y + dy * t,
                    state->lastCenter.z + dz * t,
                });
            }
        }
        if (state != nullptr)
        {
            state->hasLastCenter = true;
            state->lastCenter = center;
        }
        return samples;
    }
}

namespace renegade::bridge
{
    bool IsWickedVegetationInitialized(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain) noexcept
    {
        return terrain.terrainEntity != wi::ecs::INVALID_ENTITY &&
            HasMarker(scene, terrain.terrainEntity, ManualTerrainKey);
    }

    bool InitializeWickedVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        std::string* error)
    {
        if (IsWickedVegetationInitialized(scene, terrain))
        {
            SynchronizeWickedVegetation(scene, terrain);
            if (error != nullptr)
                error->clear();
            return true;
        }

        const fs::path scenePath =
            BundledGrassRoot() / "grass.wiscene";
        const fs::path texturePath =
            BundledGrassRoot() / "grassparticle.png";
        if (!wi::helper::FileExists(scenePath.generic_u8string()))
        {
            if (error != nullptr)
                *error = "Missing Wicked grass.wiscene.";
            return false;
        }
        if (!wi::helper::FileExists(texturePath.generic_u8string()))
        {
            if (error != nullptr)
                *error = "Missing Wicked grassparticle.png.";
            return false;
        }

        wi::scene::Scene preset;
        wi::scene::LoadModel(preset, scenePath.generic_u8string());
        if (preset.hairs.GetCount() == 0)
        {
            if (error != nullptr)
                *error = "Wicked grass preset contains no HairParticleSystem.";
            return false;
        }

        const wi::ecs::Entity sourceEntity =
            preset.hairs.GetEntity(0);
        const auto* sourceMaterial =
            preset.materials.GetComponent(sourceEntity);
        if (sourceMaterial == nullptr)
        {
            if (error != nullptr)
                *error = "Wicked grass preset contains no material.";
            return false;
        }

        terrain.SetGrassEnabled(true);
        terrain.grass_properties = preset.hairs[0];
        terrain.grass_properties.DeleteRenderData();
        terrain.grass_properties.meshID = wi::ecs::INVALID_ENTITY;
        terrain.grass_properties.vertex_lengths.clear();
        terrain.grass_properties.indices.clear();
        terrain.grass_properties.strandCount = 0;
        terrain.grass_material = *sourceMaterial;

        if (terrain.grassEntity == wi::ecs::INVALID_ENTITY)
            terrain.grassEntity = wi::ecs::CreateEntity();

        auto* master = scene.hairs.GetComponent(terrain.grassEntity);
        if (master == nullptr)
            master = &scene.hairs.Create(terrain.grassEntity);
        *master = terrain.grass_properties;

        auto* material =
            scene.materials.GetComponent(terrain.grassEntity);
        if (material == nullptr)
            material = &scene.materials.Create(terrain.grassEntity);
        *material = terrain.grass_material;
        material->SetDirty();
        material->CreateRenderData();

        if (!scene.names.Contains(terrain.grassEntity))
            scene.names.Create(terrain.grassEntity) = "grass";

        SetMarker(scene, terrain.terrainEntity, ManualTerrainKey);
        SynchronizeWickedVegetation(scene, terrain);
        if (error != nullptr)
            error->clear();
        return true;
    }

    std::size_t SynchronizeWickedVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain)
    {
        if (!IsWickedVegetationInitialized(scene, terrain))
            return 0;

        std::size_t initialized = 0;
        for (auto& entry : terrain.chunks)
        {
            if (PrepareManualChunk(scene, terrain, entry.second))
                ++initialized;
        }
        return initialized;
    }

    VegetationPaintResult PaintWickedVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const XMFLOAT3& center,
        const float radius,
        const VegetationBrushMode mode,
        VegetationStrokeState* beforeState)
    {
        VegetationPaintResult result;
        if (!IsWickedVegetationInitialized(scene, terrain))
        {
            result.error = "Initialize Wicked grass before painting.";
            return result;
        }

        SynchronizeWickedVegetation(scene, terrain);
        const float brushRadius = std::clamp(radius, 0.5f, 250.0f);
        const float radiusSquared = brushRadius * brushRadius;
        const auto samples =
            StrokeSamples(beforeState, center, brushRadius);

        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;
            auto* mesh = scene.meshes.GetComponent(chunk.entity);
            const auto* transform =
                scene.transforms.GetComponent(chunk.entity);
            if (mesh == nullptr || transform == nullptr ||
                mesh->vertex_positions.empty())
            {
                continue;
            }

            const XMMATRIX world = transform->GetWorldMatrix();
            const auto worldBounds = mesh->aabb.transform(world);
            bool intersects = false;
            for (const auto& sample : samples)
            {
                if (wi::primitive::Sphere(sample, brushRadius)
                        .intersects(worldBounds))
                {
                    intersects = true;
                    break;
                }
            }
            if (!intersects)
                continue;

            std::vector<std::size_t> changedVertices;
            for (std::size_t index = 0;
                index < mesh->vertex_positions.size(); ++index)
            {
                XMVECTOR position =
                    XMLoadFloat3(&mesh->vertex_positions[index]);
                position = XMVector3TransformCoord(position, world);

                bool underBrush = false;
                for (const auto& sample : samples)
                {
                    const XMVECTOR delta =
                        position - XMLoadFloat3(&sample);
                    if (XMVectorGetX(
                            XMVector3LengthSq(delta)) <= radiusSquared)
                    {
                        underBrush = true;
                        break;
                    }
                }
                if (!underBrush)
                    continue;

                const float current =
                    chunk.grass.vertex_lengths[index];
                const float target =
                    mode == VegetationBrushMode::Paint ? 1.0f : 0.0f;
                if (std::abs(current - target) <= MaskEpsilon)
                    continue;
                changedVertices.push_back(index);
            }

            if (changedVertices.empty())
                continue;

            CaptureBefore(beforeState, chunk);
            const float target =
                mode == VegetationBrushMode::Paint ? 1.0f : 0.0f;
            for (const auto index : changedVertices)
                chunk.grass.vertex_lengths[index] = target;

            SynchronizeLiveGrass(scene, terrain, chunk, false);
            result.changed = true;
            ++result.affectedChunks;
            result.affectedVertices += changedVertices.size();
        }
        return result;
    }

    void FinalizeWickedVegetationStroke(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const VegetationStrokeState& touchedState)
    {
        for (const auto& snapshot : touchedState.chunks)
        {
            auto* chunk = FindChunk(terrain, snapshot.chunkEntity);
            if (chunk != nullptr)
                SynchronizeLiveGrass(scene, terrain, *chunk, true);
        }
    }

    VegetationStrokeState CaptureWickedVegetationAfter(
        const wi::terrain::Terrain& terrain,
        const VegetationStrokeState& beforeState)
    {
        VegetationStrokeState after;
        for (const auto& before : beforeState.chunks)
        {
            const auto* chunk =
                FindChunk(terrain, before.chunkEntity);
            if (chunk == nullptr ||
                chunk->grass.vertex_lengths ==
                    before.vertexLengths)
            {
                continue;
            }
            VegetationChunkState snapshot;
            snapshot.chunkEntity = before.chunkEntity;
            snapshot.vertexLengths =
                chunk->grass.vertex_lengths;
            after.chunks.push_back(std::move(snapshot));
        }
        return after;
    }

    bool ApplyWickedVegetationState(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const VegetationStrokeState& state)
    {
        bool applied = state.chunks.empty();
        for (const auto& snapshot : state.chunks)
        {
            auto* chunk =
                FindChunk(terrain, snapshot.chunkEntity);
            const auto* mesh = chunk == nullptr
                ? nullptr
                : scene.meshes.GetComponent(chunk->entity);
            if (chunk == nullptr || mesh == nullptr ||
                snapshot.vertexLengths.size() !=
                    mesh->vertex_positions.size())
            {
                continue;
            }
            chunk->grass.vertex_lengths =
                snapshot.vertexLengths;
            SynchronizeLiveGrass(
                scene, terrain, *chunk, true);
            applied = true;
        }
        return applied;
    }

    WickedVegetationSettings CaptureWickedVegetationSettings(
        const wi::terrain::Terrain& terrain) noexcept
    {
        WickedVegetationSettings settings;
        const auto& grass = terrain.grass_properties;
        settings.density = terrain.grass_density;
        settings.length = grass.length;
        settings.width = grass.width;
        settings.stiffness = grass.stiffness;
        settings.drag = grass.drag;
        settings.gravityPower = grass.gravityPower;
        settings.randomness = grass.randomness;
        settings.randomSeed = grass.randomSeed;
        settings.segmentCount = grass.segmentCount;
        settings.billboardCount = grass.billboardCount;
        settings.viewDistance = grass.viewDistance;
        settings.uniformity = grass.uniformity;
        settings.cameraBend = grass.IsCameraBendEnabled();
        return settings;
    }

    bool ApplyWickedVegetationSettings(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const WickedVegetationSettings& requested)
    {
        WickedVegetationSettings settings = requested;
        settings.density =
            std::clamp(settings.density, 0.0f, 4.0f);
        settings.length =
            std::clamp(settings.length, 0.0f, 4.0f);
        settings.width =
            std::clamp(settings.width, 0.0f, 2.0f);
        settings.stiffness =
            std::clamp(settings.stiffness, 0.0f, 10.0f);
        settings.drag =
            std::clamp(settings.drag, 0.0f, 1.0f);
        settings.gravityPower =
            std::clamp(settings.gravityPower, 0.0f, 1.0f);
        settings.randomness =
            std::clamp(settings.randomness, 0.0f, 1.0f);
        settings.randomSeed =
            std::clamp(settings.randomSeed, 1u, 12345u);
        settings.segmentCount =
            std::clamp(settings.segmentCount, 1u, 10u);
        settings.billboardCount =
            std::clamp(settings.billboardCount, 1u, 10u);
        settings.viewDistance =
            std::clamp(settings.viewDistance, 0.0f, 1000.0f);
        settings.uniformity =
            std::clamp(settings.uniformity, 0.01f, 2.0f);

        const auto before =
            CaptureWickedVegetationSettings(terrain);
        if (SameSettings(before, settings))
            return false;

        const bool rebuildTopology =
            before.segmentCount != settings.segmentCount ||
            before.billboardCount != settings.billboardCount;
        terrain.grass_density = settings.density;
        ApplySettings(
            terrain.grass_properties,
            settings,
            rebuildTopology);

        if (terrain.grassEntity != wi::ecs::INVALID_ENTITY)
        {
            if (auto* master =
                    scene.hairs.GetComponent(terrain.grassEntity))
            {
                ApplySettings(*master, settings, rebuildTopology);
            }
        }

        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;
            ApplySettings(
                chunk.grass, settings, rebuildTopology);
            chunk.grass_density_current = settings.density;
            if (chunk.grass_entity ==
                wi::ecs::INVALID_ENTITY)
            {
                continue;
            }
            if (auto* live =
                    scene.hairs.GetComponent(chunk.grass_entity))
            {
                ApplySettings(*live, settings, rebuildTopology);
                live->strandCount =
                    static_cast<std::uint32_t>(
                        static_cast<float>(
                            chunk.grass.strandCount) *
                        settings.density);
            }
        }
        return true;
    }

    WickedVegetationStrokeCommand::
        WickedVegetationStrokeCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity terrainEntity,
            VegetationStrokeState before,
            VegetationStrokeState after)
        : scene_(&scene)
        , terrainEntity_(terrainEntity)
        , before_(std::move(before))
        , after_(std::move(after))
    {
    }

    bool WickedVegetationStrokeCommand::Apply(
        const VegetationStrokeState& state)
    {
        if (scene_ == nullptr)
            return false;
        auto* terrain =
            scene_->terrains.GetComponent(terrainEntity_);
        return terrain != nullptr &&
            ApplyWickedVegetationState(
                *scene_, *terrain, state);
    }

    bool WickedVegetationStrokeCommand::Execute()
    {
        return Apply(after_);
    }

    void WickedVegetationStrokeCommand::Undo()
    {
        Apply(before_);
    }

    SetWickedVegetationSettingsCommand::
        SetWickedVegetationSettingsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity terrainEntity,
            WickedVegetationSettings before,
            WickedVegetationSettings after)
        : scene_(&scene)
        , terrainEntity_(terrainEntity)
        , before_(before)
        , after_(after)
    {
    }

    bool SetWickedVegetationSettingsCommand::Apply(
        const WickedVegetationSettings& settings)
    {
        if (scene_ == nullptr)
            return false;
        auto* terrain =
            scene_->terrains.GetComponent(terrainEntity_);
        return terrain != nullptr &&
            (ApplyWickedVegetationSettings(
                 *scene_, *terrain, settings) ||
             SameSettings(
                 CaptureWickedVegetationSettings(*terrain),
                 settings));
    }

    bool SetWickedVegetationSettingsCommand::Execute()
    {
        return Apply(after_);
    }

    void SetWickedVegetationSettingsCommand::Undo()
    {
        Apply(before_);
    }
}
