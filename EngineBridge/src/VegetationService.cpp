#include "renegade/bridge/GroundPaint.h"
#include "renegade/bridge/VegetationService.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>
#include <array>
#include <fstream>
#include <iterator>
#include "renegade/bridge/PayloadDigest.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"

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

    struct StockGrassStyle
    {
        const char* source;
        const char* digest;
        const char* projectFile;
        std::array<std::array<unsigned,4>,6> crops;
        XMFLOAT4 colour;
    };
    const StockGrassStyle MeadowOpenStyle{
        "meadow_open.png",
        "c083bbb89d84d7df9e7ac353dcbd4b6ed131f81dc9b51afc51d0d50321b77113",
        "meadow-open-c083bbb89d84d7df.png",
        {{{39,21,420,484},{565,15,977,480},{66,660,462,986},
          {601,526,955,993},{71,1063,474,1508},{590,1026,914,1507}}},
        {1,1,1,1}};
    const StockGrassStyle MeadowFullStyle{
        "meadow_full.png",
        "d863e86a0a4cf17fdc714982f7778fd29b3efd9bebe53aaea6af56534a28c562",
        "meadow-full-d863e86a0a4cf17f.png",
        {{{24,123,484,475},{521,129,983,473},{28,619,492,943},
          {531,560,998,943},{40,1070,498,1426},{528,1086,994,1425}}},
        {.670896f,.660796f,.778305f,1}};
    const StockGrassStyle* StockGrassStyleFor(const std::string& path)
    {
        const auto file=fs::u8path(path).filename().u8string();
        if(file==MeadowOpenStyle.projectFile)return &MeadowOpenStyle;
        if(file==MeadowFullStyle.projectFile)return &MeadowFullStyle;
        return nullptr;
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
        renegade::bridge::RefreshGroundGrass(scene,terrain,chunk);
        return true;
    }

    constexpr const char* PaletteKey = "renegade.grass.atlas_frames.v1";
    using renegade::bridge::GrassAtlas;
    using renegade::bridge::VegetationChunkState;
    using renegade::bridge::VegetationStrokeState;

    // Non-negative uniformity controls Wicked's spatial noise for ordinary hair.
    // Painted mixes opt into the per-strand atlas mode in the simulation shader.
    constexpr float MixedPaletteUniformity = -1.f;

    GrassAtlas LayerAtlas(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* material = scene.materials.GetComponent(entity);
        const auto* metadata = scene.metadatas.GetComponent(entity);
        if (!material || !metadata || !metadata->int_values.has(PaletteKey)) return {};
        return {material->textures[wi::scene::MaterialComponent::BASECOLORMAP].name,
            static_cast<unsigned>(metadata->int_values.get(PaletteKey))};
    }

    void IndexLayers(const wi::scene::Scene& scene, VegetationStrokeState& stroke)
    {
        if (stroke.indexed) return;
        stroke.indexed = true;
        for (size_t i=0; i<scene.hairs.GetCount(); ++i)
        {
            const auto entity = scene.hairs.GetEntity(i);
            const auto* metadata = scene.metadatas.GetComponent(entity);
            if (metadata && metadata->int_values.has(PaletteKey))
                stroke.layers[scene.hairs[i].meshID].push_back(entity);
        }
    }

    wi::HairParticleSystem& CreateLayer(wi::scene::Scene& scene, const wi::terrain::Terrain& terrain,
        wi::ecs::Entity chunk, wi::ecs::Entity entity, const GrassAtlas& atlas, size_t vertices)
    {
        auto& hair = scene.hairs.Create(entity);
        hair = terrain.grass_properties;
        hair.DeleteRenderData();
        hair.meshID = chunk;
        hair.strandCount = 0;
        hair.vertex_lengths.assign(vertices, 0.f);
        hair.indices.clear();
        const auto side = static_cast<unsigned>(std::ceil(std::sqrt(double(atlas.frames))));
        const float scale = 1.f / side;
        if (atlas.frames > 1)
            hair.uniformity = MixedPaletteUniformity;
        hair.atlas_rects.resize(atlas.frames);
        for (unsigned i=0; i<atlas.frames; ++i)
        {
            hair.atlas_rects[i].texMulAdd = {scale, scale, (i%side)*scale, (i/side)*scale};
            hair.atlas_rects[i].size = 1.f;
        }
        const auto* style=StockGrassStyleFor(atlas.path);
        if(style && atlas.frames==6)
        {
            for(unsigned i=0;i<6;++i)
            {
                const auto& c=style->crops[i];
                hair.atlas_rects[i].texMulAdd={
                    float(c[2]-c[0])/1024, float(c[3]-c[1])/1536,
                    float(c[0])/1024, float(c[1])/1536};
            }
        }
        auto& material = scene.materials.Create(entity);
        material = terrain.grass_material;
        material.baseColor = {1,1,1,1};
        if(style && atlas.frames==6)
        {
            material.baseColor=style->colour;
            material.SetRoughness(.9f);
            material.SetReflectance(.02f);
            material.SetAlphaRef(.5f);
        }
        auto& slot = material.textures[wi::scene::MaterialComponent::BASECOLORMAP];
        slot.name = atlas.path;
        slot.resource = wi::resourcemanager::Load(slot.name);
        material.SetDirty();
        material.CreateRenderData();
        scene.names.Create(entity) = "Painted grass";
        scene.transforms.Create(entity);
        scene.metadatas.Create(entity).int_values.set(PaletteKey, int(atlas.frames));
        scene.Component_Attach(entity,chunk,true);
        return hair;
    }

    void RefreshLayer(wi::HairParticleSystem& hair, const wi::terrain::Terrain& terrain)
    {
        hair.strandCount = static_cast<uint32_t>(NativeStrandCount(terrain, ActiveVertexCount(hair))
            * terrain.grass_density);
        // Wicked rebuilds the masked index distribution on the next native update.
        hair._flags |= wi::HairParticleSystem::REBUILD_BUFFERS;
        hair.SetDirty();
    }

    void PaintLayer(wi::scene::Scene& scene, const wi::terrain::Terrain& terrain,
        const wi::terrain::ChunkData& chunk, const wi::scene::MeshComponent& mesh,
        VegetationStrokeState& stroke, wi::ecs::Entity entity, const std::vector<size_t>& vertices,
        float target, renegade::bridge::VegetationPaintResult& result)
    {
        auto& hair = *scene.hairs.GetComponent(entity);
        if (hair.vertex_lengths.size()!=mesh.vertex_positions.size()) return;
        auto authored=renegade::bridge::AuthoredPlantLengths(scene,entity,hair);
        if (std::none_of(vertices.begin(),vertices.end(),[&](size_t v){return authored[v]!=target;}))
            return;
        auto found = std::find_if(stroke.chunks.begin(),stroke.chunks.end(),
            [&](const auto& snapshot){return snapshot.layer==entity;});
        if (found==stroke.chunks.end())
            stroke.chunks.push_back({chunk.entity,authored,entity,LayerAtlas(scene,entity)});
        for (auto v: vertices)
        {
            if (authored[v]!=target) ++result.affectedVertices;
            authored[v]=target;
        }
        renegade::bridge::SetAuthoredPlantLengths(scene,entity,authored);
        hair.vertex_lengths=std::move(authored);
        RefreshLayer(hair,terrain);
        result.changed=true;
    }

    void CaptureBefore(
        renegade::bridge::VegetationStrokeState* state,
        const wi::terrain::ChunkData& chunk)
    {
        if (state == nullptr)
            return;
        for (const auto& existing : state->chunks)
        {
            if (existing.chunkEntity == chunk.entity && existing.layer == wi::ecs::INVALID_ENTITY)
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
    bool BuildStockGrassAtlas(const fs::path& project, StockGrassPreset preset,
        GrassAtlas& atlas, std::string& error)
    {
        const auto& style=preset==StockGrassPreset::MeadowOpen?MeadowOpenStyle:MeadowFullStyle;
        const auto source=BundledGrassRoot()/"foliage"/"Presets"/style.source;
        std::ifstream file(source,std::ios::binary);
        if(!file){error="Bundled meadow grass is missing.";return false;}
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
        std::string digest;
        if(!ComputePayloadSha256(bytes,digest,error))return false;
        if(digest!=style.digest){error="Bundled meadow grass failed its content check.";return false;}
        const auto target=project/"Content/Vegetation/Mixes"/style.projectFile;
        std::ifstream existing(target,std::ios::binary);
        std::vector<uint8_t> current((std::istreambuf_iterator<char>(existing)),{});
        if(current!=bytes)
        {
            ProjectDocumentTransactionOptions options;
            options.allowedRoot=project.generic_u8string();
            options.journalDirectory=(project/"Intermediate/Transactions").generic_u8string();
            const auto result=ProjectDocumentTransaction{}.Execute(
                {{target.generic_u8string(),std::move(bytes)}},options);
            if(!result.success){error=result.message;return false;}
        }
        atlas={target.generic_u8string(),6};
        return true;
    }

    WickedVegetationSettings StockGrassPresetSettings(StockGrassPreset preset,
        const WickedVegetationSettings& current) noexcept
    {
        auto settings=current;
        settings.density=(preset==StockGrassPreset::MeadowFull?25.f:50.f)/3.f;
        settings.length=.5f;settings.width=.9f;
        settings.segmentCount=2;settings.billboardCount=2;
        settings.randomness=.25f;settings.randomSeed=17;
        settings.uniformity=MixedPaletteUniformity;settings.viewDistance=100.f;
        return settings;
    }

    std::vector<fs::path> GrassPaletteEntries(const fs::path& project, PlantGroup group)
    {
        std::vector<fs::path> entries;
        const auto custom = project / CustomPlantDirectory;
        const auto append = [&](PlantGroup category, const fs::path& folder)
        {
            if (group != PlantGroup::All && group != category) return;
            std::error_code ec;
            for (fs::directory_iterator it(folder,ec), end; !ec && it != end; it.increment(ec))
            {
                auto ext = it->path().extension().wstring();
                for (auto& c : ext) if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
                if (ext == L".png" && it->is_regular_file(ec)) entries.push_back(it->path());
            }
        };
        // Retired bundled categories stay empty even in an older, uncleared install.
        // Existing project-owned PNGs and painted atlases remain independent.
        append(PlantGroup::Custom, custom);
        append(PlantGroup::Custom, custom.parent_path()); // Imports from earlier projects.
        std::sort(entries.begin(),entries.end());
        return entries;
    }
    bool IsWickedVegetationInitialized(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain) noexcept
    {
        return terrain.terrainEntity != wi::ecs::INVALID_ENTITY &&
            HasMarker(scene, terrain.terrainEntity, ManualTerrainKey);
    }

    void RebindWickedVegetationResources(
        wi::scene::Scene& scene)
    {
        // Previously saved palette layers carry their frame count in metadata.
        for (size_t i = 0; i < scene.hairs.GetCount(); ++i)
        {
            auto& hair = scene.hairs[i];
            const auto* meta = scene.metadatas.GetComponent(scene.hairs.GetEntity(i));
            if (hair.atlas_rects.size() > 1 && meta && meta->int_values.has(PaletteKey))
                hair.uniformity = MixedPaletteUniformity;
        }
        const std::string texturePath =
            (BundledGrassRoot() / "grassparticle.png")
                .generic_u8string();
        if (!wi::helper::FileExists(texturePath))
            return;

        const auto rebind = [&texturePath](
            wi::scene::MaterialComponent& material)
        {
            auto& slot = material.textures[
                wi::scene::MaterialComponent::BASECOLORMAP];
            slot.name = texturePath;
            slot.resource = wi::resourcemanager::Load(texturePath);
            material.SetDirty();
            material.CreateRenderData();
        };

        for (std::size_t index = 0;
            index < scene.terrains.GetCount(); ++index)
        {
            auto& terrain = scene.terrains[index];
            if (!IsWickedVegetationInitialized(scene, terrain))
                continue;

            rebind(terrain.grass_material);
            if (terrain.grassEntity !=
                wi::ecs::INVALID_ENTITY)
            {
                if (auto* material = scene.materials.GetComponent(
                        terrain.grassEntity))
                {
                    rebind(*material);
                }
            }
            for (auto& entry : terrain.chunks)
            {
                const auto entity =
                    entry.second.grass_entity;
                if (entity == wi::ecs::INVALID_ENTITY)
                    continue;
                if (auto* material =
                        scene.materials.GetComponent(entity))
                {
                    rebind(*material);
                }
            }
        }
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
        bool allChunksReady = false;
        return SynchronizeWickedVegetation(scene, terrain, allChunksReady);
    }

    std::size_t SynchronizeWickedVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        bool& allChunksReady)
    {
        allChunksReady = false;
        if (!IsWickedVegetationInitialized(scene, terrain))
            return 0;

        allChunksReady = true;
        std::size_t initialized = 0;
        for (auto& entry : terrain.chunks)
        {
            if (PrepareManualChunk(scene, terrain, entry.second))
                ++initialized;
            if (!HasMarker(scene, entry.second.entity, ManualChunkKey))
                allChunksReady = false;
        }
        return initialized;
    }

    void UpdateGrassInteraction(wi::scene::Scene& scene, wi::ecs::Entity& entity,
        const XMFLOAT3& position, const XMFLOAT3& normal)
    {
        if (!scene.forces.Contains(entity))
        {
            entity = wi::ecs::CreateEntity();
            auto& force = scene.forces.Create(entity);
            force.type = wi::scene::ForceFieldComponent::Type::Point;
            force.gravity = -40.f;
            force.range = 2.f;
            scene.transforms.Create(entity);
            scene.metadatas.Create(entity).runtimeProjectionOnly = true;
        }
        auto& transform = *scene.transforms.GetComponent(entity);
        XMStoreFloat3(&transform.translation_local, XMLoadFloat3(&position) + XMLoadFloat3(&normal)*2.f);
        transform.SetDirty();
    }

    void ClearGrassInteraction(wi::scene::Scene& scene, wi::ecs::Entity& entity)
    {
        if (entity != wi::ecs::INVALID_ENTITY && scene.forces.Contains(entity))
            scene.Entity_Remove(entity);
        entity = wi::ecs::INVALID_ENTITY;
    }

    VegetationPaintResult PaintWickedVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const XMFLOAT3& center,
        const float radius,
        const VegetationBrushMode mode,
        VegetationStrokeState* beforeState,
        const GrassAtlas& atlas)
    {
        VegetationStrokeState local;
        auto& stroke = beforeState ? *beforeState : local;
        VegetationPaintResult result;
        if (!IsWickedVegetationInitialized(scene, terrain))
        {
            result.error = "Initialize Wicked grass before painting.";
            return result;
        }

        if (!stroke.indexed && mode == VegetationBrushMode::Paint &&
            (atlas.frames > 64 || (atlas.frames && (atlas.path.empty() || !wi::helper::FileExists(atlas.path)))))
        { result.error="Invalid grass palette atlas."; return result; }
        IndexLayers(scene,stroke);
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
            std::vector<std::size_t> footprint;
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

                footprint.push_back(index);
                const float current =
                    chunk.grass.vertex_lengths[index];
                const float target =
                    mode == VegetationBrushMode::Paint && !atlas.frames ? 1.0f : 0.0f;
                if (std::abs(current - target) <= MaskEpsilon)
                    continue;
                changedVertices.push_back(index);
            }

            if (footprint.empty()) continue;
            auto& layers = stroke.layers[chunk.entity];
            wi::ecs::Entity selected = wi::ecs::INVALID_ENTITY;
            if (mode == VegetationBrushMode::Paint && atlas.frames)
            {
                for (auto entity: layers)
                {
                    auto existing = LayerAtlas(scene,entity);
                    if (existing.frames == atlas.frames &&
                        fs::u8path(existing.path).filename() == fs::u8path(atlas.path).filename())
                    { selected=entity; break; }
                }
                if (selected==wi::ecs::INVALID_ENTITY)
                {
                    selected=wi::ecs::CreateEntity();
                    CreateLayer(scene,terrain,chunk.entity,selected,atlas,mesh->vertex_positions.size());
                    layers.push_back(selected);
                }
            }
            const auto previous = result.affectedVertices;
            for (auto entity: layers)
                PaintLayer(scene,terrain,chunk,*mesh,stroke,entity,footprint,entity==selected ? 1.f : 0.f,result);
            if (changedVertices.empty() && result.affectedVertices!=previous) ++result.affectedChunks;
            if (changedVertices.empty())
                continue;

            CaptureBefore(beforeState, chunk);
            const float target =
                mode == VegetationBrushMode::Paint && !atlas.frames ? 1.0f : 0.0f;
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
            if (snapshot.layer!=wi::ecs::INVALID_ENTITY)
            {
                auto* hair=scene.hairs.GetComponent(snapshot.layer);
                if(hair)
                {
                    auto authored=AuthoredPlantLengths(scene,snapshot.layer,*hair);
                    if(std::none_of(authored.begin(),authored.end(),[](float x){return x>MaskEpsilon;}))scene.Entity_Remove(snapshot.layer);
                    else if(auto* chunk=FindChunk(terrain,snapshot.chunkEntity))RefreshGroundGrass(scene,terrain,*chunk);
                }
                continue;
            }
            auto* chunk = FindChunk(terrain, snapshot.chunkEntity);
            if (chunk != nullptr)
                SynchronizeLiveGrass(scene, terrain, *chunk, true);
        }
    }

    VegetationStrokeState CaptureWickedVegetationAfter(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain,
        const VegetationStrokeState& beforeState)
    {
        VegetationStrokeState after;
        for (const auto& before : beforeState.chunks)
        {
            if (before.layer!=wi::ecs::INVALID_ENTITY)
            {
                auto snapshot=before;
                const auto* hair=scene.hairs.GetComponent(before.layer);
                if (hair) snapshot.vertexLengths=AuthoredPlantLengths(scene,before.layer,*hair);
                else snapshot.vertexLengths.clear();
                if (snapshot.vertexLengths!=before.vertexLengths) after.chunks.push_back(std::move(snapshot));
                continue;
            }
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
            if (snapshot.layer!=wi::ecs::INVALID_ENTITY && chunk && mesh)
            {
                if (std::none_of(snapshot.vertexLengths.begin(),snapshot.vertexLengths.end(),[](float v){return v>MaskEpsilon;}))
                {
                    if (scene.hairs.Contains(snapshot.layer)) scene.Entity_Remove(snapshot.layer);
                }
                else
                {
                    if (snapshot.vertexLengths.size()!=mesh->vertex_positions.size()) return false;
                    auto* hair=scene.hairs.GetComponent(snapshot.layer);
                    if (!hair) hair=&CreateLayer(scene,terrain,chunk->entity,snapshot.layer,snapshot.atlas,mesh->vertex_positions.size());
                    SetAuthoredPlantLengths(scene,snapshot.layer,snapshot.vertexLengths);
                    hair->vertex_lengths=snapshot.vertexLengths;
                    RefreshLayer(*hair,terrain);
                    RefreshGroundGrass(scene,terrain,*chunk);
                }
                applied=true;
                continue;
            }
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
            std::clamp(settings.density, 0.0f, 20.0f);
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
        for (size_t i=0;i<scene.hairs.GetCount();++i)
        {
            auto& hair=scene.hairs[i];
            const auto* metadata=scene.metadatas.GetComponent(scene.hairs.GetEntity(i));
            if (!metadata || !metadata->int_values.has(PaletteKey) ||
                !scene.Entity_IsDescendant(hair.meshID,terrain.terrainEntity)) continue;
            ApplySettings(hair,settings,rebuildTopology);
            if (hair.atlas_rects.size() > 1)
                hair.uniformity = MixedPaletteUniformity;
            if (settings.density != before.density)
                hair.strandCount=static_cast<uint32_t>(NativeStrandCount(terrain,ActiveVertexCount(hair))*settings.density);
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
        before_.layers.clear();
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
