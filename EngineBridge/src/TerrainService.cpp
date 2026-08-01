#include "renegade/bridge/TerrainService.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_set>
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

    constexpr std::int64_t TerrainChunkStride =
        wi::terrain::chunk_width - 1;
    constexpr std::int64_t TerrainChunkHalfWidth =
        TerrainChunkStride / 2;

    struct TerrainGridKey
    {
        std::int64_t x = 0;
        std::int64_t z = 0;

        bool operator==(const TerrainGridKey& other) const noexcept
        {
            return x == other.x && z == other.z;
        }
    };

    struct TerrainGridKeyHash
    {
        std::size_t operator()(const TerrainGridKey& key) const noexcept
        {
            const auto x = static_cast<std::uint64_t>(key.x);
            const auto z = static_cast<std::uint64_t>(key.z);
            return static_cast<std::size_t>(
                (x * 0x9E3779B185EBCA87ull) ^
                (z + 0x9E3779B97F4A7C15ull + (x << 6u) + (x >> 2u)));
        }
    };

    struct TerrainVertexReference
    {
        wi::terrain::ChunkData* chunk = nullptr;
        wi::scene::MeshComponent* mesh = nullptr;
        std::size_t index = 0;
    };

    struct TerrainGridVertex
    {
        float height = 0.0f;
        XMFLOAT3 worldPosition = {};
        std::array<TerrainVertexReference, 4> references = {};
        std::size_t referenceCount = 0;
    };

    using TerrainGrid = std::unordered_map<
        TerrainGridKey,
        TerrainGridVertex,
        TerrainGridKeyHash>;

    TerrainGridKey GridKey(
        const wi::terrain::Chunk& chunk,
        const std::size_t vertexIndex) noexcept
    {
        const auto localX = static_cast<std::int64_t>(
            vertexIndex % wi::terrain::chunk_width);
        const auto localZ = static_cast<std::int64_t>(
            vertexIndex / wi::terrain::chunk_width);
        return {
            static_cast<std::int64_t>(chunk.x) * TerrainChunkStride +
                localX - TerrainChunkHalfWidth,
            static_cast<std::int64_t>(chunk.z) * TerrainChunkStride +
                localZ - TerrainChunkHalfWidth,
        };
    }

    TerrainGrid BuildTerrainGrid(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const XMFLOAT3* brushCenter = nullptr,
        const float brushRadius = 0.0f)
    {
        TerrainGrid grid;
        const std::size_t reservedChunkCount = brushCenter == nullptr
            ? terrain.chunks.size()
            : std::min<std::size_t>(terrain.chunks.size(), 16);
        grid.reserve(reservedChunkCount * wi::terrain::vertexCount);

        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;
            if (brushCenter != nullptr)
            {
                const float dx = chunk.sphere.center.x - brushCenter->x;
                const float dz = chunk.sphere.center.z - brushCenter->z;
                const float reach = brushRadius + chunk.sphere.radius +
                    terrain.chunk_scale * 2.0f;
                if (dx * dx + dz * dz > reach * reach)
                {
                    continue;
                }
            }
            auto* mesh = scene.meshes.GetComponent(chunk.entity);
            const auto* transform = scene.transforms.GetComponent(chunk.entity);
            if (mesh == nullptr || transform == nullptr ||
                mesh->vertex_positions.size() != wi::terrain::vertexCount)
            {
                continue;
            }

            const XMMATRIX world = transform->GetWorldMatrix();
            for (std::size_t index = 0;
                index < mesh->vertex_positions.size(); ++index)
            {
                auto& vertex = grid[GridKey(entry.first, index)];
                const auto& position = mesh->vertex_positions[index];
                XMFLOAT3 worldPosition;
                XMStoreFloat3(
                    &worldPosition,
                    XMVector3TransformCoord(XMLoadFloat3(&position), world));

                if (vertex.referenceCount == 0)
                {
                    vertex.height = position.y;
                    vertex.worldPosition = worldPosition;
                }
                else
                {
                    // A shared edge or corner can already be split by a
                    // previous broken stroke. Average the duplicates before
                    // applying the next edit so that touching the seam also
                    // repairs it without a visible jump to either side.
                    const float count = static_cast<float>(
                        vertex.referenceCount);
                    vertex.height =
                        (vertex.height * count + position.y) / (count + 1.0f);
                    vertex.worldPosition.y =
                        (vertex.worldPosition.y * count + worldPosition.y) /
                        (count + 1.0f);
                }
                if (vertex.referenceCount < vertex.references.size())
                {
                    vertex.references[vertex.referenceCount++] =
                        {&chunk, mesh, index};
                }
            }
        }
        return grid;
    }

    float GridHeight(
        const TerrainGrid& grid,
        const TerrainGridKey& key,
        const float fallback) noexcept
    {
        const auto found = grid.find(key);
        return found == grid.end() ? fallback : found->second.height;
    }

    void UpdateTerrainChunkHeightData(
        wi::terrain::Terrain& terrain,
        wi::terrain::ChunkData& chunk,
        wi::scene::MeshComponent& mesh)
    {
        chunk.mesh_vertex_positions = mesh.vertex_positions.data();
        chunk.heightmap_data.resize(mesh.vertex_positions.size());
        const float heightRange = std::max(
            0.001f,
            terrain.topLevel - terrain.bottomLevel);
        for (std::size_t index = 0;
            index < mesh.vertex_positions.size(); ++index)
        {
            const float normalized = std::clamp(
                (mesh.vertex_positions[index].y - terrain.bottomLevel) /
                    heightRange,
                0.0f,
                1.0f);
            chunk.heightmap_data[index] = static_cast<std::uint16_t>(
                normalized * 65535.0f);
        }
        chunk.heightmap = {};
        terrain.CreateChunkRegionTexture(chunk);
    }

    void RebuildTerrainChunks(
        wi::terrain::Terrain& terrain,
        const TerrainGrid& grid,
        const std::unordered_set<wi::terrain::ChunkData*>& chunks,
        const bool refreshPhysics)
    {
        const float spacing = std::max(0.001f, terrain.chunk_scale);
        for (auto* chunk : chunks)
        {
            if (chunk == nullptr || terrain.scene == nullptr)
            {
                continue;
            }
            auto* mesh = terrain.scene->meshes.GetComponent(chunk->entity);
            if (mesh == nullptr ||
                mesh->vertex_positions.size() != wi::terrain::vertexCount)
            {
                continue;
            }

            wi::terrain::Chunk coordinate = {};
            bool foundChunk = false;
            for (const auto& entry : terrain.chunks)
            {
                if (&entry.second == chunk)
                {
                    coordinate = entry.first;
                    foundChunk = true;
                    break;
                }
            }
            if (!foundChunk)
            {
                continue;
            }

            mesh->vertex_normals.resize(mesh->vertex_positions.size());
            mesh->vertex_tangents.resize(mesh->vertex_positions.size());
            for (std::size_t index = 0;
                index < mesh->vertex_positions.size(); ++index)
            {
                const TerrainGridKey key = GridKey(coordinate, index);
                const float center = GridHeight(
                    grid,
                    key,
                    mesh->vertex_positions[index].y);
                const float left = GridHeight(
                    grid,
                    {key.x - 1, key.z},
                    center);
                const float right = GridHeight(
                    grid,
                    {key.x + 1, key.z},
                    center);
                const float down = GridHeight(
                    grid,
                    {key.x, key.z - 1},
                    center);
                const float up = GridHeight(
                    grid,
                    {key.x, key.z + 1},
                    center);

                const XMVECTOR normal = XMVector3Normalize(XMVectorSet(
                    left - right,
                    2.0f * spacing,
                    down - up,
                    0.0f));
                XMStoreFloat3(&mesh->vertex_normals[index], normal);

                const XMVECTOR tangent = XMVector3Normalize(XMVectorSet(
                    2.0f * spacing,
                    right - left,
                    0.0f,
                    0.0f));
                XMStoreFloat4(&mesh->vertex_tangents[index], tangent);
                mesh->vertex_tangents[index].w = 1.0f;
            }

            mesh->CreateRenderData();
            if (mesh->bvh.IsValid())
            {
                mesh->BuildBVH();
            }
            chunk->sphere.center = mesh->aabb.getCenter();
            chunk->sphere.center.x += chunk->position.x;
            chunk->sphere.center.y += chunk->position.y;
            chunk->sphere.center.z += chunk->position.z;
            chunk->sphere.radius = mesh->aabb.getRadius();
            UpdateTerrainChunkHeightData(terrain, *chunk, *mesh);

            if (refreshPhysics && terrain.IsPhysicsEnabled())
            {
                const auto* transform =
                    terrain.scene->transforms.GetComponent(chunk->entity);
                if (transform != nullptr)
                {
                    auto& shape = mesh->precomputed_rigidbody_physics_shape;
                    shape.shape = wi::scene::RigidBodyPhysicsComponent::HEIGHTFIELD;
                    shape.mass = 0.0f;
                    shape.friction = 0.8f;
                    wi::physics::CreateRigidBodyShape(
                        shape,
                        transform->scale_local,
                        mesh);
                    auto* rigidBody =
                        terrain.scene->rigidbodies.GetComponent(chunk->entity);
                    if (rigidBody != nullptr)
                    {
                        rigidBody->SetRefreshParametersNeeded();
                    }
                }
            }
        }
    }

    void ConfigureDefaultGrassMaterial(
        wi::scene::MaterialComponent& material)
    {
        const std::string root = wi::helper::GetCurrentPath() +
            "/Content/terrain/default_grass/";
        material.SetBaseColor(XMFLOAT4(1, 1, 1, 1));
        material.SetRoughness(1.0f);
        material.SetMetalness(0.0f);
        material.SetReflectance(0.02f);
        material.SetNormalMapStrength(1.0f);
        material.SetOcclusionEnabled_Primary(true);
        material.textures[wi::scene::MaterialComponent::BASECOLORMAP].name =
            std::string(root) + "default_grass_basecolor.tga";
        material.textures[wi::scene::MaterialComponent::NORMALMAP].name =
            std::string(root) + "default_grass_normal.tga";
        material.textures[wi::scene::MaterialComponent::SURFACEMAP].name =
            std::string(root) + "default_grass_surface.tga";
        material.CreateRenderData();
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
            ConfigureDefaultGrassMaterial(material);
            scene.Component_Attach(materialEntity, entity);
        }

        ApplyTerrain(terrain, state, false);
        terrain.Generation_Restart();
        return entity;
    }

    void RebindDefaultTerrainMaterials(wi::scene::Scene& scene)
    {
        for (std::size_t terrainIndex = 0;
            terrainIndex < scene.terrains.GetCount(); ++terrainIndex)
        {
            auto& terrain = scene.terrains[terrainIndex];
            for (const auto materialEntity : terrain.materialEntities)
            {
                auto* material = scene.materials.GetComponent(materialEntity);
                if (material == nullptr)
                {
                    continue;
                }
                const auto& baseColor = material->textures[
                    wi::scene::MaterialComponent::BASECOLORMAP].name;
                if (wi::helper::GetFileNameFromPath(baseColor) ==
                    "default_grass_basecolor.tga")
                {
                    ConfigureDefaultGrassMaterial(*material);
                }
            }
        }
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
        TerrainGrid grid = BuildTerrainGrid(scene, terrain);
        std::unordered_set<wi::terrain::ChunkData*> changedChunks;
        for (const auto& saved : state.chunks)
        {
            auto* mesh = scene.meshes.GetComponent(saved.entity);
            if (mesh == nullptr || mesh->vertex_positions.size() != saved.heights.size())
                continue;
            wi::terrain::ChunkData* chunk = nullptr;
            for (auto& entry : terrain.chunks)
                if (entry.second.entity == saved.entity) { chunk = &entry.second; break; }
            if (chunk == nullptr) continue;
            wi::terrain::Chunk coordinate = {};
            bool foundCoordinate = false;
            for (const auto& entry : terrain.chunks)
            {
                if (&entry.second == chunk)
                {
                    coordinate = entry.first;
                    foundCoordinate = true;
                    break;
                }
            }
            if (!foundCoordinate) continue;
            for (std::size_t index = 0; index < saved.heights.size(); ++index)
            {
                mesh->vertex_positions[index].y = saved.heights[index];
                auto found = grid.find(GridKey(coordinate, index));
                if (found != grid.end())
                {
                    found->second.height = saved.heights[index];
                }
            }
            changedChunks.insert(chunk);
        }
        RebuildTerrainChunks(terrain, grid, changedChunks, true);
        return !changedChunks.empty();
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
        TerrainGrid grid = BuildTerrainGrid(
            scene,
            terrain,
            &center,
            radius);
        if (grid.empty()) return false;

        std::unordered_map<TerrainGridKey, float, TerrainGridKeyHash>
            nextHeights;
        for (const auto& entry : grid)
        {
            const auto& key = entry.first;
            const auto& vertex = entry.second;
            const float dx = vertex.worldPosition.x - center.x;
            const float dz = vertex.worldPosition.z - center.z;
            const float distance = std::sqrt(dx * dx + dz * dz);
            if (distance > radius) continue;

            const float edge = std::clamp(
                1.0f - distance / radius,
                0.0f,
                1.0f);
            const float weight = strength * std::pow(
                edge,
                1.0f + std::clamp(falloff, 0.0f, 1.0f) * 3.0f);
            float next = vertex.height;
            switch (mode)
            {
            case TerrainSculptMode::Raise:
                next += weight;
                break;
            case TerrainSculptMode::Lower:
                next -= weight;
                break;
            case TerrainSculptMode::Smooth:
            {
                float sum = 0.0f;
                std::size_t count = 0;
                for (std::int64_t z = -1; z <= 1; ++z)
                {
                    for (std::int64_t x = -1; x <= 1; ++x)
                    {
                        const auto neighbour = grid.find(
                            {key.x + x, key.z + z});
                        if (neighbour == grid.end()) continue;
                        sum += neighbour->second.height;
                        ++count;
                    }
                }
                if (count > 0)
                {
                    const float mean = sum / static_cast<float>(count);
                    next += (mean - next) * std::min(weight, 1.0f);
                }
                break;
            }
            case TerrainSculptMode::Flatten:
                next += (flattenHeight - next) * std::min(weight, 1.0f);
                break;
            }
            if (!NearlyEqual(next, vertex.height))
            {
                nextHeights.emplace(key, next);
            }
        }
        if (nextHeights.empty()) return false;

        std::unordered_set<wi::terrain::ChunkData*> changedChunks;
        for (const auto& change : nextHeights)
        {
            auto& vertex = grid.at(change.first);
            vertex.height = change.second;
            for (std::size_t index = 0;
                index < vertex.referenceCount; ++index)
            {
                const auto& reference = vertex.references[index];
                reference.mesh->vertex_positions[reference.index].y =
                    change.second;
                changedChunks.insert(reference.chunk);
            }

            // Adjacent vertex normals depend on this height. Include every
            // chunk that owns the surrounding canonical grid points, which
            // also catches the opposite side of a shared edge or corner.
            for (std::int64_t z = -1; z <= 1; ++z)
            {
                for (std::int64_t x = -1; x <= 1; ++x)
                {
                    const auto neighbour = grid.find(
                        {change.first.x + x, change.first.z + z});
                    if (neighbour == grid.end()) continue;
                    for (std::size_t index = 0;
                        index < neighbour->second.referenceCount; ++index)
                    {
                        const auto& reference =
                            neighbour->second.references[index];
                        changedChunks.insert(reference.chunk);
                    }
                }
            }
        }
        RebuildTerrainChunks(terrain, grid, changedChunks, false);
        return true;
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
