#include "renegade/bridge/VegetationService.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    constexpr const char* ManualTerrainKey =
        "renegade.vegetation.manual_terrain";
    constexpr const char* ManualChunkKey =
        "renegade.vegetation.manual_chunk";
    constexpr float MaskEpsilon = 0.0001f;

    std::string BundledWickedGrassScenePath()
    {
        const std::string executablePath = wi::helper::GetExecutablePath();
        if (!executablePath.empty())
        {
            return (fs::u8path(executablePath).parent_path() /
                    "Content" / "terrain" / "grass.wiscene")
                .lexically_normal()
                .generic_u8string();
        }
        return (fs::u8path(wi::helper::GetCurrentPath()) /
                "Content" / "terrain" / "grass.wiscene")
            .lexically_normal()
            .generic_u8string();
    }

    bool HasBoolMarker(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const char* key) noexcept
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr && metadata->bool_values.has(key) &&
            metadata->bool_values.get(key);
    }

    void SetBoolMarker(
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

    std::uint32_t CountPaintedStrands(
        const wi::terrain::Terrain& terrain,
        const wi::HairParticleSystem& grass) noexcept
    {
        std::size_t paintedVertices = 0;
        for (const float value : grass.vertex_lengths)
        {
            if (value > MaskEpsilon)
                ++paintedVertices;
        }
        if (paintedVertices == 0)
            return 0;

        const float nativeCount =
            static_cast<float>(paintedVertices) * 3.0f *
            terrain.chunk_scale * terrain.chunk_scale;
        return static_cast<std::uint32_t>(nativeCount);
    }

    bool EnsureLiveChunkGrass(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        wi::terrain::ChunkData& chunk)
    {
        auto* mesh = scene.meshes.GetComponent(chunk.entity);
        if (mesh == nullptr)
            return false;

        chunk.grass.meshID = chunk.entity;
        chunk.grass.CreateFromMesh(*mesh);
        chunk.grass_density_current = terrain.grass_density;

        if (chunk.grass_entity == wi::ecs::INVALID_ENTITY &&
            chunk.grass.strandCount == 0)
        {
            return true;
        }

        bool created = false;
        if (chunk.grass_entity == wi::ecs::INVALID_ENTITY)
        {
            chunk.grass_entity = wi::ecs::CreateEntity();
            created = true;
        }

        auto* live = scene.hairs.GetComponent(chunk.grass_entity);
        if (live == nullptr)
            live = &scene.hairs.Create(chunk.grass_entity);
        *live = chunk.grass;
        live->strandCount = static_cast<std::uint32_t>(
            static_cast<float>(chunk.grass.strandCount) *
            std::max(0.0f, terrain.grass_density));
        live->CreateFromMesh(*mesh);

        auto* material = scene.materials.GetComponent(chunk.grass_entity);
        if (material == nullptr)
            material = &scene.materials.Create(chunk.grass_entity);
        *material = terrain.grass_material;
        material->SetDirty();
        material->CreateRenderData();

        if (scene.transforms.GetComponent(chunk.grass_entity) == nullptr)
            scene.transforms.Create(chunk.grass_entity);
        if (scene.names.GetComponent(chunk.grass_entity) == nullptr)
            scene.names.Create(chunk.grass_entity).name = "grass";

        if (created)
            scene.Component_Attach(chunk.grass_entity, chunk.entity, true);

        return true;
    }

    bool InitializeManualChunk(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        wi::terrain::ChunkData& chunk)
    {
        if (HasBoolMarker(scene, chunk.entity, ManualChunkKey))
            return false;

        auto* mesh = scene.meshes.GetComponent(chunk.entity);
        if (mesh == nullptr || mesh->vertex_positions.empty())
            return false;

        chunk.grass = terrain.grass_properties;
        chunk.grass.DeleteRenderData();
        chunk.grass.meshID = chunk.entity;
        chunk.grass.vertex_lengths.assign(mesh->vertex_positions.size(), 0.0f);
        chunk.grass.strandCount = 0;
        chunk.grass.CreateFromMesh(*mesh);
        EnsureLiveChunkGrass(scene, terrain, chunk);
        SetBoolMarker(scene, chunk.entity, ManualChunkKey);
        return true;
    }

    bool CaptureChunkBeforeIfNeeded(
        renegade::bridge::VegetationStrokeState* state,
        const wi::terrain::ChunkData& chunk)
    {
        if (state == nullptr)
            return true;
        for (const auto& existing : state->chunks)
        {
            if (existing.chunkEntity == chunk.entity)
                return true;
        }
        renegade::bridge::VegetationChunkMaskState snapshot;
        snapshot.chunkEntity = chunk.entity;
        snapshot.vertexLengths = chunk.grass.vertex_lengths;
        snapshot.strandCount = chunk.grass.strandCount;
        state->chunks.push_back(std::move(snapshot));
        return true;
    }

    bool SameMask(
        const renegade::bridge::VegetationChunkMaskState& left,
        const renegade::bridge::VegetationChunkMaskState& right) noexcept
    {
        if (left.chunkEntity != right.chunkEntity ||
            left.strandCount != right.strandCount ||
            left.vertexLengths.size() != right.vertexLengths.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < left.vertexLengths.size(); ++index)
        {
            if (std::abs(left.vertexLengths[index] -
                    right.vertexLengths[index]) > MaskEpsilon)
            {
                return false;
            }
        }
        return true;
    }
}

namespace renegade::bridge
{
    bool IsManualVegetationEnabled(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain) noexcept
    {
        return terrain.terrainEntity != wi::ecs::INVALID_ENTITY &&
            HasBoolMarker(scene, terrain.terrainEntity, ManualTerrainKey);
    }

    bool EnsureDefaultGrassVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        std::string* error)
    {
        if (IsManualVegetationEnabled(scene, terrain))
        {
            SynchronizeManualVegetation(scene, terrain);
            if (error != nullptr)
                error->clear();
            return true;
        }

        const std::string grassScenePath = BundledWickedGrassScenePath();
        if (!wi::helper::FileExists(grassScenePath))
        {
            if (error != nullptr)
                *error = "Bundled Wicked grass preset is missing: " + grassScenePath;
            return false;
        }

        wi::scene::Scene grassScene;
        wi::scene::LoadModel(grassScene, grassScenePath);
        if (grassScene.hairs.GetCount() == 0)
        {
            if (error != nullptr)
                *error = "Bundled Wicked grass preset contains no HairParticleSystem.";
            return false;
        }

        const wi::ecs::Entity sourceEntity = grassScene.hairs.GetEntity(0);
        const auto* sourceMaterial =
            grassScene.materials.GetComponent(sourceEntity);
        if (sourceMaterial == nullptr)
        {
            if (error != nullptr)
                *error = "Bundled Wicked grass preset contains no matching material.";
            return false;
        }

        terrain.SetGrassEnabled(true);
        terrain.grass_properties = grassScene.hairs[0];
        terrain.grass_properties.DeleteRenderData();
        terrain.grass_properties.meshID = wi::ecs::INVALID_ENTITY;
        terrain.grass_properties.vertex_lengths.clear();
        terrain.grass_properties.indices.clear();
        terrain.grass_properties.strandCount = 0;
        terrain.grass_material = *sourceMaterial;
        terrain.grass_material.SetDirty(false);

        if (terrain.grassEntity == wi::ecs::INVALID_ENTITY)
            terrain.grassEntity = wi::ecs::CreateEntity();

        auto* masterHair = scene.hairs.GetComponent(terrain.grassEntity);
        if (masterHair == nullptr)
            masterHair = &scene.hairs.Create(terrain.grassEntity);
        *masterHair = terrain.grass_properties;

        auto* masterMaterial = scene.materials.GetComponent(terrain.grassEntity);
        if (masterMaterial == nullptr)
            masterMaterial = &scene.materials.Create(terrain.grassEntity);
        *masterMaterial = terrain.grass_material;
        masterMaterial->SetDirty();
        masterMaterial->CreateRenderData();

        if (scene.names.GetComponent(terrain.grassEntity) == nullptr)
            scene.names.Create(terrain.grassEntity).name = "grass";

        SetBoolMarker(scene, terrain.terrainEntity, ManualTerrainKey);
        SynchronizeManualVegetation(scene, terrain);

        if (error != nullptr)
            error->clear();
        return true;
    }

    std::size_t SynchronizeManualVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain)
    {
        if (!IsManualVegetationEnabled(scene, terrain))
            return 0;

        std::size_t initialized = 0;
        for (auto& entry : terrain.chunks)
        {
            if (InitializeManualChunk(scene, terrain, entry.second))
                ++initialized;
        }
        return initialized;
    }

    VegetationPaintResult PaintVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const XMFLOAT3& center,
        const float radius,
        const VegetationBrushMode mode,
        VegetationStrokeState* beforeState)
    {
        VegetationPaintResult result;
        if (!IsManualVegetationEnabled(scene, terrain))
        {
            result.error = "Vegetation painting has not been initialized.";
            return result;
        }

        SynchronizeManualVegetation(scene, terrain);
        const float brushRadius = std::clamp(radius, 0.5f, 250.0f);
        const float radiusSquared = brushRadius * brushRadius;
        const float target =
            mode == VegetationBrushMode::Paint ? 1.0f : 0.0f;

        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;
            const float chunkDx = chunk.sphere.center.x - center.x;
            const float chunkDz = chunk.sphere.center.z - center.z;
            const float reach = brushRadius + chunk.sphere.radius +
                terrain.chunk_scale * 2.0f;
            if (chunkDx * chunkDx + chunkDz * chunkDz > reach * reach)
                continue;

            auto* mesh = scene.meshes.GetComponent(chunk.entity);
            const auto* transform = scene.transforms.GetComponent(chunk.entity);
            if (mesh == nullptr || transform == nullptr ||
                mesh->vertex_positions.empty())
            {
                continue;
            }
            if (chunk.grass.vertex_lengths.size() != mesh->vertex_positions.size())
            {
                chunk.grass.vertex_lengths.assign(
                    mesh->vertex_positions.size(), 0.0f);
            }

            const XMMATRIX world = transform->GetWorldMatrix();
            std::vector<std::size_t> changedIndices;
            changedIndices.reserve(128);
            for (std::size_t index = 0;
                index < mesh->vertex_positions.size(); ++index)
            {
                XMFLOAT3 worldPosition;
                XMStoreFloat3(
                    &worldPosition,
                    XMVector3TransformCoord(
                        XMLoadFloat3(&mesh->vertex_positions[index]),
                        world));
                const float dx = worldPosition.x - center.x;
                const float dz = worldPosition.z - center.z;
                if (dx * dx + dz * dz > radiusSquared)
                    continue;
                if (std::abs(chunk.grass.vertex_lengths[index] - target) <=
                    MaskEpsilon)
                {
                    continue;
                }
                changedIndices.push_back(index);
            }

            if (changedIndices.empty())
                continue;

            CaptureChunkBeforeIfNeeded(beforeState, chunk);
            for (const std::size_t index : changedIndices)
                chunk.grass.vertex_lengths[index] = target;

            chunk.grass.strandCount = CountPaintedStrands(terrain, chunk.grass);
            EnsureLiveChunkGrass(scene, terrain, chunk);
            result.changed = true;
            ++result.affectedChunks;
            result.affectedVertices += changedIndices.size();
        }

        return result;
    }

    VegetationStrokeState CaptureVegetationAfter(
        const wi::scene::Scene&,
        const wi::terrain::Terrain& terrain,
        const VegetationStrokeState& beforeState)
    {
        VegetationStrokeState after;
        after.chunks.reserve(beforeState.chunks.size());
        for (const auto& beforeChunk : beforeState.chunks)
        {
            const auto* chunk = FindChunk(terrain, beforeChunk.chunkEntity);
            if (chunk == nullptr)
                continue;
            VegetationChunkMaskState snapshot;
            snapshot.chunkEntity = chunk->entity;
            snapshot.vertexLengths = chunk->grass.vertex_lengths;
            snapshot.strandCount = chunk->grass.strandCount;
            if (!SameMask(beforeChunk, snapshot))
                after.chunks.push_back(std::move(snapshot));
        }
        return after;
    }

    bool ApplyVegetationState(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const VegetationStrokeState& state)
    {
        bool applied = false;
        for (const auto& snapshot : state.chunks)
        {
            auto* chunk = FindChunk(terrain, snapshot.chunkEntity);
            if (chunk == nullptr)
                continue;
            auto* mesh = scene.meshes.GetComponent(chunk->entity);
            if (mesh == nullptr ||
                snapshot.vertexLengths.size() != mesh->vertex_positions.size())
            {
                continue;
            }
            chunk->grass = terrain.grass_properties;
            chunk->grass.DeleteRenderData();
            chunk->grass.meshID = chunk->entity;
            chunk->grass.vertex_lengths = snapshot.vertexLengths;
            chunk->grass.strandCount = snapshot.strandCount;
            chunk->grass.CreateFromMesh(*mesh);
            EnsureLiveChunkGrass(scene, terrain, *chunk);
            SetBoolMarker(scene, chunk->entity, ManualChunkKey);
            applied = true;
        }
        return applied || state.chunks.empty();
    }

    float CaptureVegetationDensity(
        const wi::terrain::Terrain& terrain) noexcept
    {
        return terrain.grass_density;
    }

    bool SetVegetationDensity(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const float density)
    {
        const float clamped = std::clamp(density, 0.0f, 4.0f);
        if (std::abs(terrain.grass_density - clamped) <= MaskEpsilon)
            return false;

        terrain.grass_density = clamped;
        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;
            chunk.grass_density_current = clamped;
            if (chunk.grass_entity == wi::ecs::INVALID_ENTITY)
                continue;
            auto* live = scene.hairs.GetComponent(chunk.grass_entity);
            if (live == nullptr)
                continue;
            live->strandCount = static_cast<std::uint32_t>(
                static_cast<float>(chunk.grass.strandCount) * clamped);
        }
        return true;
    }

    VegetationStrokeCommand::VegetationStrokeCommand(
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

    bool VegetationStrokeCommand::Apply(const VegetationStrokeState& state)
    {
        if (scene_ == nullptr)
            return false;
        auto* terrain = scene_->terrains.GetComponent(terrainEntity_);
        return terrain != nullptr && ApplyVegetationState(*scene_, *terrain, state);
    }

    bool VegetationStrokeCommand::Execute()
    {
        return Apply(after_);
    }

    void VegetationStrokeCommand::Undo()
    {
        Apply(before_);
    }

    SetVegetationDensityCommand::SetVegetationDensityCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity terrainEntity,
        const float before,
        const float after)
        : scene_(&scene)
        , terrainEntity_(terrainEntity)
        , before_(before)
        , after_(after)
    {
    }

    bool SetVegetationDensityCommand::Apply(const float density)
    {
        if (scene_ == nullptr)
            return false;
        auto* terrain = scene_->terrains.GetComponent(terrainEntity_);
        if (terrain == nullptr)
            return false;
        SetVegetationDensity(*scene_, *terrain, density);
        return true;
    }

    bool SetVegetationDensityCommand::Execute()
    {
        return Apply(after_);
    }

    void SetVegetationDensityCommand::Undo()
    {
        Apply(before_);
    }
}
