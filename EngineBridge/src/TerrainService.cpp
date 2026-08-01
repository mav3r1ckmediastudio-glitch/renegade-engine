#include "renegade/bridge/TerrainService.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace
{
    constexpr float Epsilon = 0.00001f;

    bool NearlyEqual(float left, float right) noexcept
    {
        return std::abs(left - right) <= Epsilon;
    }

    bool IsMeaningful(
        const renegade::bridge::TerrainState& before,
        const renegade::bridge::TerrainState& after) noexcept
    {
        return before.centerToCamera != after.centerToCamera ||
            before.removeDistantChunks != after.removeDistantChunks ||
            before.physics != after.physics ||
            before.tessellation != after.tessellation ||
            before.visibleChunkRadius != after.visibleChunkRadius ||
            before.propChunkRadius != after.propChunkRadius ||
            before.physicsChunkRadius != after.physicsChunkRadius ||
            !NearlyEqual(before.chunkScale, after.chunkScale) ||
            before.seed != after.seed ||
            !NearlyEqual(before.minimumHeight, after.minimumHeight) ||
            !NearlyEqual(before.maximumHeight, after.maximumHeight) ||
            !NearlyEqual(before.lowAltitudeBlend, after.lowAltitudeBlend) ||
            !NearlyEqual(before.baseBlend, after.baseBlend) ||
            !NearlyEqual(before.slopeBlend, after.slopeBlend) ||
            !NearlyEqual(before.lodBias, after.lodBias);
    }

    bool EntityExists(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

}

namespace renegade::bridge
{
    TerrainState CaptureTerrain(const wi::terrain::Terrain& terrain) noexcept
    {
        TerrainState state;
        state.centerToCamera = terrain.IsCenterToCamEnabled();
        state.removeDistantChunks = terrain.IsRemovalEnabled();
        state.physics = terrain.IsPhysicsEnabled();
        state.tessellation = terrain.IsTessellationEnabled();
        state.visibleChunkRadius = terrain.generation;
        state.propChunkRadius = terrain.prop_generation;
        state.physicsChunkRadius = terrain.physics_generation;
        state.chunkScale = terrain.chunk_scale;
        state.seed = terrain.seed;
        state.minimumHeight = terrain.bottomLevel;
        state.maximumHeight = terrain.topLevel;
        state.lowAltitudeBlend = terrain.region1;
        state.baseBlend = terrain.region2;
        state.slopeBlend = terrain.region3;
        state.lodBias = terrain.lod_bias;
        return state;
    }

    void ApplyTerrain(
        wi::terrain::Terrain& terrain,
        const TerrainState& state,
        const bool restartGeneration) noexcept
    {
        const TerrainState before = CaptureTerrain(terrain);
        terrain.SetCenterToCamEnabled(state.centerToCamera);
        terrain.SetRemovalEnabled(state.removeDistantChunks);
        terrain.SetPhysicsEnabled(state.physics);
        terrain.SetTessellationEnabled(state.tessellation);
        terrain.generation = std::clamp(state.visibleChunkRadius, 1, 16);
        terrain.prop_generation = std::clamp(state.propChunkRadius, 0, 16);
        terrain.physics_generation = std::clamp(state.physicsChunkRadius, 0, 8);
        terrain.chunk_scale = std::clamp(state.chunkScale, 0.25f, 16.0f);
        terrain.seed = state.seed;
        terrain.bottomLevel = std::clamp(state.minimumHeight, -2000.0f, 1999.0f);
        terrain.topLevel = std::clamp(
            state.maximumHeight,
            terrain.bottomLevel + 1.0f,
            2000.0f);
        terrain.region1 = std::clamp(state.lowAltitudeBlend, 0.0f, 1.0f);
        terrain.region2 = std::clamp(state.baseBlend, 0.0f, 1.0f);
        terrain.region3 = std::clamp(state.slopeBlend, 0.0f, 1.0f);
        terrain.lod_bias = std::clamp(state.lodBias, -4.0f, 4.0f);

        if (restartGeneration && terrain.scene != nullptr &&
            IsMeaningful(before, CaptureTerrain(terrain)))
        {
            terrain.Generation_Restart();
        }
    }

    TerrainState MakeTerrainPreset(
        const TerrainState& current,
        const TerrainPreset preset) noexcept
    {
        TerrainState result = current;
        result.centerToCamera = false;
        result.removeDistantChunks = false;
        result.physics = true;
        result.visibleChunkRadius = 6;
        result.propChunkRadius = 4;
        result.physicsChunkRadius = 3;
        result.chunkScale = 2.0f;
        result.lowAltitudeBlend = 0.12f;
        result.baseBlend = 0.42f;
        result.slopeBlend = 0.72f;
        switch (preset)
        {
        case TerrainPreset::FlatWorld:
            result.minimumHeight = -1.0f;
            result.maximumHeight = 2.0f;
            result.slopeBlend = 8.0f;
            break;
        case TerrainPreset::Island:
            result.minimumHeight = -35.0f;
            result.maximumHeight = 95.0f;
            result.lowAltitudeBlend = 0.20f;
            result.baseBlend = 0.48f;
            result.slopeBlend = 0.80f;
            break;
        case TerrainPreset::Coastline:
            result.minimumHeight = -18.0f;
            result.maximumHeight = 70.0f;
            result.lowAltitudeBlend = 0.16f;
            result.baseBlend = 0.50f;
            result.slopeBlend = 0.76f;
            break;
        case TerrainPreset::Highlands:
            result.minimumHeight = -15.0f;
            result.maximumHeight = 240.0f;
            result.chunkScale = 3.0f;
            result.lowAltitudeBlend = 0.08f;
            result.baseBlend = 0.34f;
            result.slopeBlend = 0.58f;
            break;
        }
        return result;
    }

    wi::ecs::Entity CreateTerrain(
        wi::scene::Scene& scene,
        const TerrainState& state,
        const char* name)
    {
        const wi::ecs::Entity entity = wi::ecs::CreateEntity();
        auto& terrain = scene.terrains.Create(entity);
        terrain.terrainEntity = entity;
        terrain.scene = &scene;
        scene.names.Create(entity) = name == nullptr ? "Terrain" : name;
        scene.transforms.Create(entity);

        constexpr const char* materialNames[] = {
            "Terrain Base", "Terrain Rock", "Terrain Low", "Terrain High"};
        const XMFLOAT4 materialColors[] = {
            XMFLOAT4(0.10f, 0.14f, 0.10f, 1.0f),
            XMFLOAT4(0.18f, 0.19f, 0.20f, 1.0f),
            XMFLOAT4(0.16f, 0.13f, 0.09f, 1.0f),
            XMFLOAT4(0.32f, 0.34f, 0.33f, 1.0f),
        };
        terrain.materialEntities.resize(wi::terrain::MATERIAL_COUNT);
        for (std::size_t index = 0; index < wi::terrain::MATERIAL_COUNT; ++index)
        {
            const auto materialEntity = wi::ecs::CreateEntity();
            terrain.materialEntities[index] = materialEntity;
            scene.names.Create(materialEntity) = materialNames[index];
            auto& material = scene.materials.Create(materialEntity);
            material.baseColor = materialColors[index];
            material.SetRoughness(index == wi::terrain::MATERIAL_SLOPE ? 0.82f : 0.95f);
            material.SetReflectance(0.02f);
            scene.Component_Attach(materialEntity, entity);
        }

        ApplyTerrain(terrain, state, false);
        terrain.Generation_Restart();
        return entity;
    }

    CreateTerrainCommand::CreateTerrainCommand(
        wi::scene::Scene& scene,
        const TerrainState& terrain,
        const char* name)
        : scene_(&scene)
        , terrain_(terrain)
        , name_(name == nullptr ? "Terrain" : name)
    {
    }

    bool CreateTerrainCommand::Execute()
    {
        if (scene_ == nullptr)
        {
            return false;
        }
        if (entity_ == wi::ecs::INVALID_ENTITY)
        {
            entity_ = CreateTerrain(*scene_, terrain_, name_.c_str());
            return entity_ != wi::ecs::INVALID_ENTITY;
        }
        if (!hasSnapshot_ || EntityExists(*scene_, entity_))
        {
            return false;
        }
        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        const auto restored = scene_->Entity_Serialize(snapshot_, serializer);
        auto* restoredTerrain = scene_->terrains.GetComponent(restored);
        if (restoredTerrain != nullptr)
        {
            restoredTerrain->scene = scene_;
            restoredTerrain->Generation_Restart();
        }
        return restored == entity_;
    }

    void CreateTerrainCommand::Undo()
    {
        if (scene_ == nullptr || !EntityExists(*scene_, entity_))
        {
            return;
        }
        if (!hasSnapshot_)
        {
            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, entity_);
            hasSnapshot_ = true;
        }
        scene_->Entity_Remove(entity_);
    }

    wi::ecs::Entity CreateTerrainCommand::CreatedEntity() const noexcept
    {
        return entity_;
    }

    SetTerrainCommand::SetTerrainCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const TerrainState& terrain)
        : scene_(&scene), entity_(entity), after_(terrain)
    {
        const auto* existing = scene.terrains.GetComponent(entity);
        before_ = existing == nullptr ? TerrainState{} : CaptureTerrain(*existing);
    }

    SetTerrainCommand::SetTerrainCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const TerrainState& before,
        const TerrainState& after)
        : scene_(&scene), entity_(entity), before_(before), after_(after)
    {
    }

    bool SetTerrainCommand::Execute()
    {
        return IsMeaningful(before_, after_) && Apply(after_);
    }

    void SetTerrainCommand::Undo()
    {
        Apply(before_);
    }

    bool SetTerrainCommand::Apply(const TerrainState& state)
    {
        if (scene_ == nullptr)
        {
            return false;
        }
        auto* terrain = scene_->terrains.GetComponent(entity_);
        if (terrain == nullptr)
        {
            return false;
        }
        ApplyTerrain(*terrain, state);
        return true;
    }

    TerrainSculptState CaptureTerrainSculpt(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain)
    {
        TerrainSculptState state;
        state.chunks.reserve(terrain.chunks.size());
        for (const auto& entry : terrain.chunks)
        {
            const auto& chunk = entry.second;
            const auto* mesh = scene.meshes.GetComponent(chunk.entity);
            if (mesh == nullptr || mesh->vertex_positions.empty()) continue;
            auto& saved = state.chunks.emplace_back();
            saved.entity = chunk.entity;
            saved.heights.reserve(mesh->vertex_positions.size());
            for (const auto& position : mesh->vertex_positions)
                saved.heights.push_back(position.y);
        }
        return state;
    }

    bool ApplyTerrainSculpt(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const TerrainSculptState& state)
    {
        bool changed = false;
        for (const auto& saved : state.chunks)
        {
            auto* mesh = scene.meshes.GetComponent(saved.entity);
            if (mesh == nullptr || mesh->vertex_positions.size() != saved.heights.size())
                continue;
            wi::terrain::ChunkData* chunk = nullptr;
            for (auto& entry : terrain.chunks)
                if (entry.second.entity == saved.entity) { chunk = &entry.second; break; }
            if (chunk == nullptr) continue;
            for (std::size_t i = 0; i < saved.heights.size(); ++i)
                mesh->vertex_positions[i].y = saved.heights[i];
            mesh->CreateRenderData();
            if (mesh->bvh.IsValid()) mesh->BuildBVH();
            chunk->heightmap_data.resize(saved.heights.size());
            for (std::size_t i = 0; i < saved.heights.size(); ++i)
            {
                const float normalized = std::clamp(
                    (saved.heights[i] - terrain.bottomLevel) /
                    std::max(0.001f, terrain.topLevel - terrain.bottomLevel), 0.0f, 1.0f);
                chunk->heightmap_data[i] = static_cast<std::uint16_t>(normalized * 65535.0f);
            }
            chunk->heightmap = {};
            terrain.CreateChunkRegionTexture(*chunk);
            changed = true;
        }
        return changed;
    }

    bool SculptTerrain(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const XMFLOAT3& center,
        const float radius,
        const float strength,
        const float falloff,
        const TerrainSculptMode mode,
        const float flattenHeight)
    {
        if (radius <= 0.0f || strength <= 0.0f) return false;
        bool changed = false;
        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;
            bool chunkChanged = false;
            auto* mesh = scene.meshes.GetComponent(chunk.entity);
            auto* transform = scene.transforms.GetComponent(chunk.entity);
            if (mesh == nullptr || transform == nullptr) continue;
            const XMMATRIX world = transform->GetMatrix();
            float localMean = 0.0f;
            std::size_t localCount = 0;
            for (const auto& p : mesh->vertex_positions)
            {
                XMFLOAT3 wp; XMStoreFloat3(&wp, XMVector3TransformCoord(XMLoadFloat3(&p), world));
                const float dx = wp.x - center.x, dz = wp.z - center.z;
                if (dx * dx + dz * dz <= radius * radius) { localMean += p.y; ++localCount; }
            }
            if (localCount == 0) continue;
            localMean /= static_cast<float>(localCount);
            for (auto& p : mesh->vertex_positions)
            {
                XMFLOAT3 wp; XMStoreFloat3(&wp, XMVector3TransformCoord(XMLoadFloat3(&p), world));
                const float dx = wp.x - center.x, dz = wp.z - center.z;
                const float distance = std::sqrt(dx * dx + dz * dz);
                if (distance > radius) continue;
                const float edge = std::clamp(1.0f - distance / radius, 0.0f, 1.0f);
                const float weight = strength * std::pow(edge, 1.0f + falloff * 3.0f);
                switch (mode)
                {
                case TerrainSculptMode::Raise: p.y += weight; break;
                case TerrainSculptMode::Lower: p.y -= weight; break;
                case TerrainSculptMode::Smooth: p.y += (localMean - p.y) * std::min(weight, 1.0f); break;
                case TerrainSculptMode::Flatten: p.y += (flattenHeight - p.y) * std::min(weight, 1.0f); break;
                }
                changed = true;
                chunkChanged = true;
            }
            if (chunkChanged)
            {
                mesh->CreateRenderData();
                if (mesh->bvh.IsValid()) mesh->BuildBVH();
                chunk.heightmap_data.resize(mesh->vertex_positions.size());
                for (std::size_t i = 0; i < mesh->vertex_positions.size(); ++i)
                {
                    const float normalized = std::clamp((mesh->vertex_positions[i].y - terrain.bottomLevel) /
                        std::max(0.001f, terrain.topLevel - terrain.bottomLevel), 0.0f, 1.0f);
                    chunk.heightmap_data[i] = static_cast<std::uint16_t>(normalized * 65535.0f);
                }
                chunk.heightmap = {};
                terrain.CreateChunkRegionTexture(chunk);
            }
        }
        return changed;
    }

    SculptTerrainCommand::SculptTerrainCommand(wi::scene::Scene& scene,
        const wi::ecs::Entity terrainEntity, TerrainSculptState before,
        TerrainSculptState after)
        : scene_(&scene), terrainEntity_(terrainEntity), before_(std::move(before)), after_(std::move(after)) {}

    bool SculptTerrainCommand::Execute() { return Apply(after_); }
    void SculptTerrainCommand::Undo() { Apply(before_); }
    bool SculptTerrainCommand::Apply(const TerrainSculptState& state)
    {
        if (scene_ == nullptr) return false;
        auto* terrain = scene_->terrains.GetComponent(terrainEntity_);
        return terrain != nullptr && ApplyTerrainSculpt(*scene_, *terrain, state);
    }
}
