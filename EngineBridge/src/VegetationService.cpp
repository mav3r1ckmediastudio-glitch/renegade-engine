#include "renegade/bridge/VegetationService.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    constexpr const char* ManualTerrainKey =
        "renegade.vegetation.manual_terrain";
    constexpr const char* ManualChunkKey =
        "renegade.vegetation.manual_chunk";
    constexpr const char* SettingsVersionKey =
        "renegade.vegetation.hair_settings_v2";
    constexpr float MaskEpsilon = 0.0001f;

    // Wicked's CreateFromMesh() keeps only triangles containing at least one
    // positive vertex length. A tiny CPU-side sentinel therefore keeps the
    // emitter triangle list stable, while Wicked's R8_UNORM upload truncates
    // 0.001 * 255 to zero so no visible grass is rendered at those vertices.
    // This prevents already-painted strands from being redistributed whenever
    // the brush expands or deletes part of a patch.
    constexpr float InactiveDistributionMask = 0.001f;
    constexpr float PaintedMaskThreshold = 0.5f;
    constexpr float RenegadeDefaultGrassLength = 0.35f;

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

    bool NearlyEqual(const float left, const float right) noexcept
    {
        return std::abs(left - right) <= MaskEpsilon;
    }

    bool NormalizeInactiveMask(std::vector<float>& lengths) noexcept
    {
        bool changed = false;
        for (float& value : lengths)
        {
            if (value <= MaskEpsilon)
            {
                value = InactiveDistributionMask;
                changed = true;
            }
        }
        return changed;
    }

    bool HasPaintedVertices(const wi::HairParticleSystem& grass) noexcept
    {
        return std::any_of(
            grass.vertex_lengths.begin(),
            grass.vertex_lengths.end(),
            [](const float value)
            {
                return value >= PaintedMaskThreshold;
            });
    }

    std::uint32_t StableActiveStrandCount(
        const wi::terrain::Terrain& terrain,
        const wi::scene::MeshComponent& mesh) noexcept
    {
        if (mesh.vertex_positions.empty())
            return 0;

        // Wicked terrain's native density formula for a full chunk. The stable
        // full-chunk emitter capacity stays constant while the length mask
        // determines what fraction is visible, preserving the accepted density
        // without changing the strand index space during a stroke.
        const double count =
            static_cast<double>(mesh.vertex_positions.size()) * 3.0 *
            static_cast<double>(terrain.chunk_scale) *
            static_cast<double>(terrain.chunk_scale);
        return static_cast<std::uint32_t>(std::min(
            count,
            static_cast<double>(std::numeric_limits<std::uint32_t>::max())));
    }

    renegade::bridge::VegetationGrassSettings ClampSettings(
        renegade::bridge::VegetationGrassSettings settings)
    {
        settings.length = std::clamp(settings.length, 0.0f, 4.0f);
        settings.width = std::clamp(settings.width, 0.0f, 2.0f);
        settings.stiffness = std::clamp(settings.stiffness, 0.0f, 10.0f);
        settings.drag = std::clamp(settings.drag, 0.0f, 1.0f);
        settings.gravityPower = std::clamp(settings.gravityPower, 0.0f, 1.0f);
        settings.randomness = std::clamp(settings.randomness, 0.0f, 1.0f);
        settings.segmentCount = std::clamp<std::uint32_t>(
            settings.segmentCount, 1u, 10u);
        settings.billboardCount = std::clamp<std::uint32_t>(
            settings.billboardCount, 1u, 10u);
        settings.randomSeed = std::clamp<std::uint32_t>(
            settings.randomSeed, 1u, 12345u);
        settings.viewDistance = std::clamp(settings.viewDistance, 0.0f, 1000.0f);
        settings.uniformity = std::clamp(settings.uniformity, 0.01f, 2.0f);

        for (auto& rect : settings.atlasRects)
        {
            rect.texMulAdd.z = std::clamp(rect.texMulAdd.z, 0.0f, 1.0f);
            rect.texMulAdd.w = std::clamp(rect.texMulAdd.w, 0.0f, 1.0f);
            rect.texMulAdd.x = std::clamp(
                rect.texMulAdd.x,
                0.001f,
                std::max(0.001f, 1.0f - rect.texMulAdd.z));
            rect.texMulAdd.y = std::clamp(
                rect.texMulAdd.y,
                0.001f,
                std::max(0.001f, 1.0f - rect.texMulAdd.w));
            rect.size = std::clamp(rect.size, 0.0f, 2.0f);
        }
        return settings;
    }

    void ApplySettingsToHair(
        wi::HairParticleSystem& hair,
        const renegade::bridge::VegetationGrassSettings& settings)
    {
        const bool topologyChanged =
            hair.segmentCount != settings.segmentCount ||
            hair.billboardCount != settings.billboardCount;

        hair.length = settings.length;
        hair.width = settings.width;
        hair.stiffness = settings.stiffness;
        hair.drag = settings.drag;
        hair.gravityPower = settings.gravityPower;
        hair.randomness = settings.randomness;
        hair.segmentCount = settings.segmentCount;
        hair.billboardCount = settings.billboardCount;
        hair.randomSeed = settings.randomSeed;
        hair.viewDistance = settings.viewDistance;
        hair.uniformity = settings.uniformity;
        hair.atlas_rects = settings.atlasRects;
        hair.SetCameraBendEnabled(settings.cameraBendEnabled);
        hair.regenerate_frame = true;
        hair.SetDirty();
        if (topologyChanged)
            hair._flags |= wi::HairParticleSystem::REBUILD_BUFFERS;
    }

    void CopyAuthoredGrassState(
        wi::HairParticleSystem& live,
        const wi::HairParticleSystem& authored)
    {
        const bool topologyChanged =
            live.segmentCount != authored.segmentCount ||
            live.billboardCount != authored.billboardCount;

        live.meshID = authored.meshID;
        live.segmentCount = authored.segmentCount;
        live.billboardCount = authored.billboardCount;
        live.randomSeed = authored.randomSeed;
        live.length = authored.length;
        live.stiffness = authored.stiffness;
        live.drag = authored.drag;
        live.gravityPower = authored.gravityPower;
        live.randomness = authored.randomness;
        live.viewDistance = authored.viewDistance;
        live.vertex_lengths = authored.vertex_lengths;
        live.width = authored.width;
        live.uniformity = authored.uniformity;
        live.atlas_rects = authored.atlas_rects;
        live.layerMask = authored.layerMask;
        live.SetCameraBendEnabled(authored.IsCameraBendEnabled());
        live.regenerate_frame = true;
        live.SetDirty();
        if (topologyChanged)
            live._flags |= wi::HairParticleSystem::REBUILD_BUFFERS;
    }

    bool EnsureLiveChunkGrass(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        wi::terrain::ChunkData& chunk)
    {
        auto* mesh = scene.meshes.GetComponent(chunk.entity);
        if (mesh == nullptr)
            return false;

        if (chunk.grass.vertex_lengths.size() != mesh->vertex_positions.size())
        {
            chunk.grass.vertex_lengths.assign(
                mesh->vertex_positions.size(),
                InactiveDistributionMask);
        }
        else
        {
            NormalizeInactiveMask(chunk.grass.vertex_lengths);
        }

        chunk.grass.meshID = chunk.entity;
        chunk.grass.CreateFromMesh(*mesh);
        chunk.grass_density_current = terrain.grass_density;

        if (chunk.grass_entity == wi::ecs::INVALID_ENTITY &&
            chunk.grass.strandCount == 0)
        {
            return true;
        }

        bool createdEntity = false;
        if (chunk.grass_entity == wi::ecs::INVALID_ENTITY)
        {
            chunk.grass_entity = wi::ecs::CreateEntity();
            createdEntity = true;
        }

        auto* live = scene.hairs.GetComponent(chunk.grass_entity);
        if (live == nullptr)
        {
            live = &scene.hairs.Create(chunk.grass_entity);
            *live = chunk.grass;
        }
        else
        {
            CopyAuthoredGrassState(*live, chunk.grass);
        }

        live->strandCount = static_cast<std::uint32_t>(
            static_cast<float>(chunk.grass.strandCount) *
            std::max(0.0f, terrain.grass_density));
        live->CreateFromMesh(*mesh);

        auto* material = scene.materials.GetComponent(chunk.grass_entity);
        if (material == nullptr)
        {
            material = &scene.materials.Create(chunk.grass_entity);
            *material = terrain.grass_material;
            material->SetDirty();
            material->CreateRenderData();
        }

        if (scene.transforms.GetComponent(chunk.grass_entity) == nullptr)
            scene.transforms.Create(chunk.grass_entity);
        if (scene.names.GetComponent(chunk.grass_entity) == nullptr)
            scene.names.Create(chunk.grass_entity).name = "grass";

        if (createdEntity)
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
        chunk.grass.vertex_lengths.assign(
            mesh->vertex_positions.size(),
            InactiveDistributionMask);
        chunk.grass.strandCount = 0;
        chunk.grass.CreateFromMesh(*mesh);
        EnsureLiveChunkGrass(scene, terrain, chunk);
        SetBoolMarker(scene, chunk.entity, ManualChunkKey);
        return true;
    }

    bool StabilizeManualChunk(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        wi::terrain::ChunkData& chunk)
    {
        if (!HasBoolMarker(scene, chunk.entity, ManualChunkKey))
            return false;

        auto* mesh = scene.meshes.GetComponent(chunk.entity);
        if (mesh == nullptr || mesh->vertex_positions.empty())
            return false;

        bool changed = false;
        if (chunk.grass.vertex_lengths.size() != mesh->vertex_positions.size())
        {
            chunk.grass.vertex_lengths.assign(
                mesh->vertex_positions.size(),
                InactiveDistributionMask);
            changed = true;
        }
        else
        {
            changed |= NormalizeInactiveMask(chunk.grass.vertex_lengths);
        }

        const std::uint32_t stableCount = HasPaintedVertices(chunk.grass)
            ? StableActiveStrandCount(terrain, *mesh)
            : 0;
        if (chunk.grass.strandCount != stableCount)
        {
            chunk.grass.strandCount = stableCount;
            changed = true;
        }

        const auto settings =
            renegade::bridge::CaptureVegetationGrassSettings(terrain);
        const auto chunkSettings = [&]()
        {
            renegade::bridge::VegetationGrassSettings value;
            value.length = chunk.grass.length;
            value.width = chunk.grass.width;
            value.stiffness = chunk.grass.stiffness;
            value.drag = chunk.grass.drag;
            value.gravityPower = chunk.grass.gravityPower;
            value.randomness = chunk.grass.randomness;
            value.segmentCount = chunk.grass.segmentCount;
            value.billboardCount = chunk.grass.billboardCount;
            value.randomSeed = chunk.grass.randomSeed;
            value.viewDistance = chunk.grass.viewDistance;
            value.uniformity = chunk.grass.uniformity;
            value.cameraBendEnabled = chunk.grass.IsCameraBendEnabled();
            value.atlasRects = chunk.grass.atlas_rects;
            return value;
        }();
        if (!renegade::bridge::VegetationGrassSettingsEqual(
                settings, chunkSettings))
        {
            ApplySettingsToHair(chunk.grass, settings);
            changed = true;
        }

        if (changed)
            EnsureLiveChunkGrass(scene, terrain, chunk);
        return changed;
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
            if (!NearlyEqual(left.vertexLengths[index], right.vertexLengths[index]))
                return false;
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
            // One-time migration from the first WD01 owner build, which copied
            // Wicked's oversized authored length verbatim. After the marker is
            // present the creator owns Length and it is never overwritten.
            if (!HasBoolMarker(scene, terrain.terrainEntity, SettingsVersionKey))
            {
                auto settings = CaptureVegetationGrassSettings(terrain);
                settings.length = RenegadeDefaultGrassLength;
                ApplyVegetationGrassSettings(scene, terrain, settings);
                SetBoolMarker(scene, terrain.terrainEntity, SettingsVersionKey);
            }
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
        terrain.grass_properties.length = RenegadeDefaultGrassLength;
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
        SetBoolMarker(scene, terrain.terrainEntity, SettingsVersionKey);
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

        std::size_t changed = 0;
        for (auto& entry : terrain.chunks)
        {
            if (InitializeManualChunk(scene, terrain, entry.second))
            {
                ++changed;
                continue;
            }
            if (StabilizeManualChunk(scene, terrain, entry.second))
                ++changed;
        }
        return changed;
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
        const float target = mode == VegetationBrushMode::Paint
            ? 1.0f
            : InactiveDistributionMask;

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
                    mesh->vertex_positions.size(),
                    InactiveDistributionMask);
            }
            else
            {
                NormalizeInactiveMask(chunk.grass.vertex_lengths);
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
                if (NearlyEqual(chunk.grass.vertex_lengths[index], target))
                    continue;
                changedIndices.push_back(index);
            }

            if (changedIndices.empty())
                continue;

            CaptureChunkBeforeIfNeeded(beforeState, chunk);
            for (const std::size_t index : changedIndices)
                chunk.grass.vertex_lengths[index] = target;

            chunk.grass.strandCount = HasPaintedVertices(chunk.grass)
                ? StableActiveStrandCount(terrain, *mesh)
                : 0;
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

            const auto settings = CaptureVegetationGrassSettings(terrain);
            chunk->grass = terrain.grass_properties;
            chunk->grass.DeleteRenderData();
            chunk->grass.meshID = chunk->entity;
            chunk->grass.vertex_lengths = snapshot.vertexLengths;
            NormalizeInactiveMask(chunk->grass.vertex_lengths);
            chunk->grass.strandCount = HasPaintedVertices(chunk->grass)
                ? StableActiveStrandCount(terrain, *mesh)
                : 0;
            ApplySettingsToHair(chunk->grass, settings);
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
        if (NearlyEqual(terrain.grass_density, clamped))
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
            live->regenerate_frame = true;
        }
        return true;
    }

    VegetationGrassSettings CaptureVegetationGrassSettings(
        const wi::terrain::Terrain& terrain)
    {
        VegetationGrassSettings settings;
        settings.length = terrain.grass_properties.length;
        settings.width = terrain.grass_properties.width;
        settings.stiffness = terrain.grass_properties.stiffness;
        settings.drag = terrain.grass_properties.drag;
        settings.gravityPower = terrain.grass_properties.gravityPower;
        settings.randomness = terrain.grass_properties.randomness;
        settings.segmentCount = terrain.grass_properties.segmentCount;
        settings.billboardCount = terrain.grass_properties.billboardCount;
        settings.randomSeed = terrain.grass_properties.randomSeed;
        settings.viewDistance = terrain.grass_properties.viewDistance;
        settings.uniformity = terrain.grass_properties.uniformity;
        settings.cameraBendEnabled =
            terrain.grass_properties.IsCameraBendEnabled();
        settings.atlasRects = terrain.grass_properties.atlas_rects;
        return settings;
    }

    bool VegetationGrassSettingsEqual(
        const VegetationGrassSettings& left,
        const VegetationGrassSettings& right) noexcept
    {
        if (!NearlyEqual(left.length, right.length) ||
            !NearlyEqual(left.width, right.width) ||
            !NearlyEqual(left.stiffness, right.stiffness) ||
            !NearlyEqual(left.drag, right.drag) ||
            !NearlyEqual(left.gravityPower, right.gravityPower) ||
            !NearlyEqual(left.randomness, right.randomness) ||
            left.segmentCount != right.segmentCount ||
            left.billboardCount != right.billboardCount ||
            left.randomSeed != right.randomSeed ||
            !NearlyEqual(left.viewDistance, right.viewDistance) ||
            !NearlyEqual(left.uniformity, right.uniformity) ||
            left.cameraBendEnabled != right.cameraBendEnabled ||
            left.atlasRects.size() != right.atlasRects.size())
        {
            return false;
        }

        for (std::size_t i = 0; i < left.atlasRects.size(); ++i)
        {
            const auto& a = left.atlasRects[i];
            const auto& b = right.atlasRects[i];
            if (!NearlyEqual(a.texMulAdd.x, b.texMulAdd.x) ||
                !NearlyEqual(a.texMulAdd.y, b.texMulAdd.y) ||
                !NearlyEqual(a.texMulAdd.z, b.texMulAdd.z) ||
                !NearlyEqual(a.texMulAdd.w, b.texMulAdd.w) ||
                !NearlyEqual(a.size, b.size))
            {
                return false;
            }
        }
        return true;
    }

    bool ApplyVegetationGrassSettings(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const VegetationGrassSettings& requested)
    {
        const VegetationGrassSettings settings = ClampSettings(requested);
        const VegetationGrassSettings before =
            CaptureVegetationGrassSettings(terrain);
        if (VegetationGrassSettingsEqual(before, settings))
            return false;

        ApplySettingsToHair(terrain.grass_properties, settings);

        if (terrain.grassEntity != wi::ecs::INVALID_ENTITY)
        {
            if (auto* master = scene.hairs.GetComponent(terrain.grassEntity))
                ApplySettingsToHair(*master, settings);
        }

        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;
            ApplySettingsToHair(chunk.grass, settings);
            if (chunk.grass_entity == wi::ecs::INVALID_ENTITY)
                continue;
            if (auto* live = scene.hairs.GetComponent(chunk.grass_entity))
                ApplySettingsToHair(*live, settings);
        }

        SetBoolMarker(scene, terrain.terrainEntity, SettingsVersionKey);
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
        return terrain != nullptr &&
            ApplyVegetationState(*scene_, *terrain, state);
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

    SetVegetationGrassSettingsCommand::SetVegetationGrassSettingsCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity terrainEntity,
        VegetationGrassSettings before,
        VegetationGrassSettings after)
        : scene_(&scene)
        , terrainEntity_(terrainEntity)
        , before_(std::move(before))
        , after_(std::move(after))
    {
    }

    bool SetVegetationGrassSettingsCommand::Apply(
        const VegetationGrassSettings& settings)
    {
        if (scene_ == nullptr)
            return false;
        auto* terrain = scene_->terrains.GetComponent(terrainEntity_);
        return terrain != nullptr &&
            ApplyVegetationGrassSettings(*scene_, *terrain, settings);
    }

    bool SetVegetationGrassSettingsCommand::Execute()
    {
        return Apply(after_);
    }

    void SetVegetationGrassSettingsCommand::Undo()
    {
        Apply(before_);
    }
}
