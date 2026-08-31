#include "renegade/bridge/VegetationService.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace
{
    constexpr const char* StableChunkKey =
        "renegade.vegetation.stable_distribution_v1";
    constexpr float MaskEpsilon = 0.0001f;
    constexpr float StableInactiveLength = 1.0f / 255.0f;

    bool HasStableMarker(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr &&
            metadata->bool_values.has(StableChunkKey) &&
            metadata->bool_values.get(StableChunkKey);
    }

    void SetStableMarker(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const bool value)
    {
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
            metadata = &scene.metadatas.Create(entity);
        metadata->bool_values.set(StableChunkKey, value);
    }

    std::uint32_t NativeTerrainStrandCount(
        const wi::terrain::Terrain& terrain,
        const wi::scene::MeshComponent& mesh) noexcept
    {
        if (mesh.vertex_positions.empty())
            return 0;
        const float nativeCount =
            static_cast<float>(mesh.vertex_positions.size()) * 3.0f *
            terrain.chunk_scale * terrain.chunk_scale;
        return static_cast<std::uint32_t>(nativeCount);
    }

    void CaptureBeforeIfNeeded(
        const wi::scene::Scene& scene,
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
        snapshot.stableDistribution = HasStableMarker(scene, chunk.entity);
        state->chunks.push_back(std::move(snapshot));
    }

    wi::HairParticleSystem* EnsureNativeLiveGrass(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        wi::terrain::ChunkData& chunk,
        const wi::scene::MeshComponent& mesh)
    {
        if (chunk.grass_entity == wi::ecs::INVALID_ENTITY)
            chunk.grass_entity = wi::ecs::CreateEntity();

        auto* live = scene.hairs.GetComponent(chunk.grass_entity);
        const bool created = live == nullptr;
        if (created)
            live = &scene.hairs.Create(chunk.grass_entity);

        if (created)
        {
            *live = chunk.grass;
            live->meshID = chunk.entity;
            live->strandCount = static_cast<std::uint32_t>(
                static_cast<float>(chunk.grass.strandCount) *
                std::max(0.0f, terrain.grass_density));
            live->CreateFromMesh(mesh);
            live->CreateRenderData();
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
        if (hierarchy == nullptr || hierarchy->parentID != chunk.entity)
            scene.Component_Attach(chunk.grass_entity, chunk.entity, true);

        return live;
    }

    wi::HairParticleSystem* ActivateNativeLengthDistribution(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        wi::terrain::ChunkData& chunk,
        const wi::scene::MeshComponent& mesh)
    {
        if (!HasStableMarker(scene, chunk.entity))
        {
            chunk.grass = terrain.grass_properties;
            chunk.grass.DeleteRenderData();
            chunk.grass.meshID = chunk.entity;
            chunk.grass.vertex_lengths.assign(
                mesh.vertex_positions.size(), StableInactiveLength);
            chunk.grass.strandCount = NativeTerrainStrandCount(terrain, mesh);
            chunk.grass.CreateFromMesh(mesh);
            chunk.grass_density_current = terrain.grass_density;
            SetStableMarker(scene, chunk.entity, true);
        }

        return EnsureNativeLiveGrass(scene, terrain, chunk, mesh);
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

            // Wicked PaintTool uses ceil(pointer travel / brush radius), capped
            // to 100 substeps. Studio supplies terrain world hits rather than
            // raw PaintTool screen coordinates, so preserve the same sampling
            // contract in world space.
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
        const float brushRadius = std::clamp(radius, 0.5f, 250.0f);
        const auto samples = BuildNativeStrokeSubsteps(
            beforeState, center, brushRadius);

        // This intentionally follows Wicked Editor PaintTool's
        // MODE_HAIRPARTICLE_LENGTH path: edit the live HairParticle component,
        // clamp retained emitters to 1/255 so distribution does not change,
        // set REBUILD_BUFFERS, then mirror vertex_lengths back to the terrain
        // chunk for persistence. No Renegade-side GPU upload or live-component
        // replacement is performed during drag painting.
        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;

            bool nearStroke = false;
            for (const auto& sample : samples)
            {
                const XMFLOAT3 delta(
                    chunk.sphere.center.x - sample.x,
                    chunk.sphere.center.y - sample.y,
                    chunk.sphere.center.z - sample.z);
                const float reach = brushRadius + chunk.sphere.radius;
                if (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z <=
                    reach * reach)
                {
                    nearStroke = true;
                    break;
                }
            }
            if (!nearStroke)
                continue;

            auto* mesh = scene.meshes.GetComponent(chunk.entity);
            if (mesh == nullptr || mesh->vertex_positions.empty())
                continue;

            const bool stable = HasStableMarker(scene, chunk.entity);
            if (!stable && mode == VegetationBrushMode::Delete)
                continue;

            CaptureBeforeIfNeeded(scene, beforeState, chunk);

            auto* live = ActivateNativeLengthDistribution(
                scene, terrain, chunk, *mesh);
            if (live == nullptr ||
                live->vertex_lengths.size() != mesh->vertex_positions.size())
            {
                continue;
            }

            // The mesh belongs to the terrain chunk, not the lazily-created
            // grass child. Wicked's stock painter can use the hair entity world
            // matrix because that hierarchy already exists before painting.
            // Renegade creates an empty chunk's grass child inside this call,
            // before RunHierarchyUpdateSystem has inherited the parent world
            // matrix. Use the mesh/chunk transform directly so first contact on
            // every neighbouring chunk is evaluated in the correct world space.
            const auto* transform = scene.transforms.GetComponent(chunk.entity);
            if (transform == nullptr)
                continue;
            const XMMATRIX world = XMLoadFloat4x4(&transform->world);

            bool chunkChanged = false;
            std::size_t changedVertices = 0;

            for (const auto& sample : samples)
            {
                const XMVECTOR centerVector = XMLoadFloat3(&sample);
                for (std::size_t vertex = 0;
                    vertex < mesh->vertex_positions.size(); ++vertex)
                {
                    XMVECTOR position = XMLoadFloat3(
                        &mesh->vertex_positions[vertex]);
                    position = XMVector3Transform(position, world);
                    const float distance = wi::math::Distance(
                        position, centerVector);
                    if (distance > brushRadius)
                        continue;

                    const float current = live->vertex_lengths[vertex];
                    if (current <= 0.0f)
                        continue;

                    const float target =
                        mode == VegetationBrushMode::Paint ? 1.0f : 0.0f;
                    const float affection = wi::math::SmoothStep(
                        0.0f,
                        1.0f,
                        1.0f - distance / brushRadius);
                    float next = wi::math::Lerp(
                        current, target, affection);
                    next = wi::math::Clamp(
                        next, StableInactiveLength, 1.0f);

                    if (std::abs(next - current) <= MaskEpsilon)
                        continue;

                    live->vertex_lengths[vertex] = next;
                    chunkChanged = true;
                    ++changedVertices;
                }
            }

            if (!chunkChanged)
                continue;

            live->_flags |= wi::HairParticleSystem::REBUILD_BUFFERS;

            // Exact Wicked terrain paint persistence seam: the live hair is
            // authoritative while painting; only its modified length mask is
            // copied back into ChunkData.
            chunk.grass.vertex_lengths = live->vertex_lengths;

            result.changed = true;
            ++result.affectedChunks;
            result.affectedVertices += changedVertices;
        }

        return result;
    }
}
