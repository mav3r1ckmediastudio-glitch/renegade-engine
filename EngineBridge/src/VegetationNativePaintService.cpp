#include "renegade/bridge/VegetationService.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace
{
    // Legacy marker from the abandoned "whole chunk at 1/255" experiment.
    // It is read only so old WD01 test scenes can be migrated back to real
    // Wicked Add/Remove coverage. New painting never sets it true.
    constexpr const char* LegacyStableChunkKey =
        "renegade.vegetation.stable_distribution_v1";
    constexpr float MaskEpsilon = 0.0001f;
    constexpr float LegacyInvisibleLength = 1.0f / 255.0f;

    bool HasLegacyStableMarker(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr &&
            metadata->bool_values.has(LegacyStableChunkKey) &&
            metadata->bool_values.get(LegacyStableChunkKey);
    }

    void ClearLegacyStableMarker(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata != nullptr &&
            metadata->bool_values.has(LegacyStableChunkKey))
        {
            metadata->bool_values.set(LegacyStableChunkKey, false);
        }
    }

    std::size_t CountActiveGrassVertices(
        const wi::HairParticleSystem& grass) noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            grass.vertex_lengths.begin(),
            grass.vertex_lengths.end(),
            [](const float value) { return value > MaskEpsilon; }));
    }

    std::uint32_t NativeTerrainStrandCount(
        const wi::terrain::Terrain& terrain,
        const std::size_t activeVertexCount) noexcept
    {
        if (activeVertexCount == 0)
            return 0;
        const float nativeCount =
            static_cast<float>(activeVertexCount) * 3.0f *
            terrain.chunk_scale * terrain.chunk_scale;
        return static_cast<std::uint32_t>(nativeCount);
    }

    void PrepareAuthoredChunkGrass(
        wi::terrain::Terrain& terrain,
        wi::terrain::ChunkData& chunk,
        const wi::scene::MeshComponent& mesh)
    {
        if (chunk.grass.vertex_lengths.size() != mesh.vertex_positions.size())
        {
            chunk.grass = terrain.grass_properties;
            chunk.grass.DeleteRenderData();
            chunk.grass.vertex_lengths.assign(
                mesh.vertex_positions.size(), 0.0f);
            chunk.grass.strandCount = 0;
        }

        chunk.grass.meshID = chunk.entity;

        // Values at or below 1/255 packed to zero in Wicked's R8 length mask.
        // Collapse any residue from older WD01 builds to a genuine zero so it
        // cannot keep invisible emitter triangles alive or block Add painting.
        for (float& value : chunk.grass.vertex_lengths)
        {
            if (value <= LegacyInvisibleLength + MaskEpsilon)
                value = 0.0f;
        }
    }

    bool SynchronizeNativeLiveGrass(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        wi::terrain::ChunkData& chunk)
    {
        const std::uint32_t authoredStrandCount = chunk.grass.strandCount;
        if (authoredStrandCount == 0)
        {
            if (chunk.grass_entity != wi::ecs::INVALID_ENTITY)
            {
                scene.Entity_Remove(chunk.grass_entity);
                chunk.grass_entity = wi::ecs::INVALID_ENTITY;
            }
            chunk.grass_density_current = terrain.grass_density;
            return true;
        }

        bool createdEntity = false;
        if (chunk.grass_entity == wi::ecs::INVALID_ENTITY)
        {
            chunk.grass_entity = wi::ecs::CreateEntity();
            createdEntity = true;
        }

        auto* live = scene.hairs.GetComponent(chunk.grass_entity);
        const bool createdLive = live == nullptr;
        if (createdLive)
            live = &scene.hairs.Create(chunk.grass_entity);

        chunk.grass_density_current = terrain.grass_density;
        const std::uint32_t liveStrandCount = static_cast<std::uint32_t>(
            static_cast<float>(authoredStrandCount) *
            std::max(0.0f, terrain.grass_density));

        if (createdLive)
        {
            // Match Wicked Terrain::place_chunk(): the authored ChunkData grass
            // already owns the correct mesh distribution, so copy it once and
            // create render data. Do not rebuild/copy the component per sample.
            *live = chunk.grass;
            live->strandCount = liveStrandCount;
            live->CreateRenderData();
        }
        else
        {
            // Match Wicked PaintTool: edit the existing live HairParticle and
            // let its normal UpdateCPU path rebuild after the mask changed.
            live->meshID = chunk.entity;
            live->vertex_lengths = chunk.grass.vertex_lengths;
            live->strandCount = liveStrandCount;
            live->_flags |= wi::HairParticleSystem::REBUILD_BUFFERS;
        }

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

        const auto* hierarchy = scene.hierarchy.GetComponent(chunk.grass_entity);
        if (createdEntity || hierarchy == nullptr || hierarchy->parentID != chunk.entity)
            scene.Component_Attach(chunk.grass_entity, chunk.entity, true);

        return true;
    }

    void RebuildAuthoredChunkDistribution(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        wi::terrain::ChunkData& chunk,
        const wi::scene::MeshComponent& mesh)
    {
        const std::size_t activeVertexCount = CountActiveGrassVertices(chunk.grass);
        chunk.grass.meshID = chunk.entity;
        chunk.grass.strandCount = NativeTerrainStrandCount(
            terrain, activeVertexCount);
        chunk.grass.CreateFromMesh(mesh);
        SynchronizeNativeLiveGrass(scene, terrain, chunk);
    }

    void MigrateLegacyStableChunks(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain)
    {
        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;
            if (!HasLegacyStableMarker(scene, chunk.entity))
                continue;

            auto* mesh = scene.meshes.GetComponent(chunk.entity);
            if (mesh == nullptr || mesh->vertex_positions.empty())
                continue;

            PrepareAuthoredChunkGrass(terrain, chunk, *mesh);
            ClearLegacyStableMarker(scene, chunk.entity);
            RebuildAuthoredChunkDistribution(scene, terrain, chunk, *mesh);
        }
    }

    void CaptureBeforeIfNeeded(
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

        renegade::bridge::VegetationChunkMaskState snapshot;
        snapshot.chunkEntity = chunk.entity;
        snapshot.vertexLengths = chunk.grass.vertex_lengths;
        snapshot.strandCount = chunk.grass.strandCount;
        snapshot.stableDistribution = false;
        state->chunks.push_back(std::move(snapshot));
    }

    std::vector<XMFLOAT3> BuildNativeStrokeSubsteps(
        renegade::bridge::VegetationStrokeState* state,
        const XMFLOAT3& center,
        const float radius)
    {
        std::vector<XMFLOAT3> samples;
        const bool hasPrevious =
            state != nullptr && state->hasLastCenter && !state->chunks.empty();

        if (!hasPrevious)
        {
            samples.push_back(center);
        }
        else
        {
            const float dx = center.x - state->lastCenter.x;
            const float dy = center.y - state->lastCenter.y;
            const float dz = center.z - state->lastCenter.z;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            // Wicked PaintTool subdivides pointer travel by brush radius and
            // caps the work at 100 samples. Studio supplies world-space terrain
            // hits, so keep the same bounded sampling contract in world space.
            const int substepCount = std::clamp(
                static_cast<int>(std::ceil(distance / std::max(0.1f, radius))),
                1,
                100);
            samples.reserve(static_cast<std::size_t>(substepCount));
            for (int substep = 1; substep <= substepCount; ++substep)
            {
                const float t = static_cast<float>(substep) /
                    static_cast<float>(substepCount);
                samples.push_back(XMFLOAT3(
                    state->lastCenter.x + dx * t,
                    state->lastCenter.y + dy * t,
                    state->lastCenter.z + dz * t));
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

        // One-time compatibility repair for scenes authored by the discarded
        // stable-distribution builds. This removes invisible 1/255 support and
        // recalculates native strand counts from genuinely painted vertices.
        MigrateLegacyStableChunks(scene, terrain);

        const float brushRadius = std::clamp(radius, 0.5f, 250.0f);
        const auto samples = BuildNativeStrokeSubsteps(
            beforeState, center, brushRadius);

        // Wicked HairParticle Add/Remove semantics over terrain ChunkData:
        // Paint = vertex length 1, Delete = 0. The live scene HairParticle is
        // synchronized only once per changed chunk after all stroke substeps.
        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;

            auto* mesh = scene.meshes.GetComponent(chunk.entity);
            const auto* transform = scene.transforms.GetComponent(chunk.entity);
            if (mesh == nullptr || transform == nullptr ||
                mesh->vertex_positions.empty())
            {
                continue;
            }

            const XMMATRIX world = XMLoadFloat4x4(&transform->world);
            const wi::primitive::AABB worldAabb = mesh->aabb.transform(world);

            // Match Wicked PaintTool's broad phase: brush sphere vs the actual
            // rendered object's AABB. Do not depend on Terrain::ChunkData::sphere
            // bookkeeping, which can be stale across serialized/regenerated
            // chunks and was rejecting otherwise valid paint targets.
            bool nearStroke = false;
            for (const auto& sample : samples)
            {
                const wi::primitive::Sphere brushSphere(sample, brushRadius);
                if (brushSphere.intersects(worldAabb))
                {
                    nearStroke = true;
                    break;
                }
            }
            if (!nearStroke)
                continue;

            PrepareAuthoredChunkGrass(terrain, chunk, *mesh);

            bool chunkChanged = false;
            std::size_t changedVertices = 0;
            bool capturedBefore = false;
            const float target =
                mode == VegetationBrushMode::Paint ? 1.0f : 0.0f;

            for (const auto& sample : samples)
            {
                const XMVECTOR centerVector = XMLoadFloat3(&sample);
                for (std::size_t vertex = 0;
                    vertex < mesh->vertex_positions.size(); ++vertex)
                {
                    XMVECTOR position = XMLoadFloat3(
                        &mesh->vertex_positions[vertex]);
                    position = XMVector3TransformCoord(position, world);
                    if (wi::math::Distance(position, centerVector) > brushRadius)
                        continue;

                    const float current = chunk.grass.vertex_lengths[vertex];
                    if (std::abs(current - target) <= MaskEpsilon)
                        continue;

                    if (!capturedBefore)
                    {
                        CaptureBeforeIfNeeded(beforeState, chunk);
                        capturedBefore = true;
                    }

                    chunk.grass.vertex_lengths[vertex] = target;
                    chunkChanged = true;
                    ++changedVertices;
                }
            }

            if (!chunkChanged)
                continue;

            // This is Wicked terrain's own density model: only vertices that
            // actually carry grass contribute to strand capacity. It avoids the
            // old 53k-strands-per-touched-chunk cost for tiny painted patches.
            RebuildAuthoredChunkDistribution(scene, terrain, chunk, *mesh);
            ClearLegacyStableMarker(scene, chunk.entity);

            result.changed = true;
            ++result.affectedChunks;
            result.affectedVertices += changedVertices;
        }

        return result;
    }
}
