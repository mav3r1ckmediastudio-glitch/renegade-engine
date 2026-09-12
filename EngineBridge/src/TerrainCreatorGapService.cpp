#include "renegade/bridge/TerrainCreatorGapService.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <utility>

namespace
{
    constexpr std::int64_t TerrainChunkStride = wi::terrain::chunk_width - 1;
    constexpr std::int64_t TerrainChunkHalfWidth = TerrainChunkStride / 2;

    struct GridKey
    {
        std::int64_t x = 0;
        std::int64_t z = 0;
    };

    GridKey KeyFor(const wi::terrain::Chunk& chunk, std::size_t vertexIndex) noexcept
    {
        const auto localX = static_cast<std::int64_t>(vertexIndex % wi::terrain::chunk_width);
        const auto localZ = static_cast<std::int64_t>(vertexIndex / wi::terrain::chunk_width);
        return {
            static_cast<std::int64_t>(chunk.x) * TerrainChunkStride + localX - TerrainChunkHalfWidth,
            static_cast<std::int64_t>(chunk.z) * TerrainChunkStride + localZ - TerrainChunkHalfWidth,
        };
    }

    struct GridBounds
    {
        std::int64_t minX = std::numeric_limits<std::int64_t>::max();
        std::int64_t maxX = std::numeric_limits<std::int64_t>::min();
        std::int64_t minZ = std::numeric_limits<std::int64_t>::max();
        std::int64_t maxZ = std::numeric_limits<std::int64_t>::min();

        [[nodiscard]] bool Valid() const noexcept { return minX <= maxX && minZ <= maxZ; }
        [[nodiscard]] std::uint32_t Width() const noexcept
        {
            return Valid() ? static_cast<std::uint32_t>(maxX - minX + 1) : 0;
        }
        [[nodiscard]] std::uint32_t Height() const noexcept
        {
            return Valid() ? static_cast<std::uint32_t>(maxZ - minZ + 1) : 0;
        }
    };

    GridBounds BoundsFor(const wi::terrain::Terrain& terrain)
    {
        GridBounds bounds;
        for (const auto& entry : terrain.chunks)
        {
            const auto first = KeyFor(entry.first, 0);
            const auto last = KeyFor(entry.first, wi::terrain::vertexCount - 1);
            bounds.minX = std::min(bounds.minX, first.x);
            bounds.maxX = std::max(bounds.maxX, last.x);
            bounds.minZ = std::min(bounds.minZ, first.z);
            bounds.maxZ = std::max(bounds.maxZ, last.z);
        }
        return bounds;
    }

    wi::terrain::ChunkData* FindChunkByEntity(
        wi::terrain::Terrain& terrain,
        wi::ecs::Entity entity)
    {
        for (auto& entry : terrain.chunks)
            if (entry.second.entity == entity) return &entry.second;
        return nullptr;
    }

    const wi::terrain::ChunkData* FindChunkByEntity(
        const wi::terrain::Terrain& terrain,
        wi::ecs::Entity entity)
    {
        for (const auto& entry : terrain.chunks)
            if (entry.second.entity == entity) return &entry.second;
        return nullptr;
    }

    bool SameBlendmaps(
        const renegade::bridge::TerrainBlendmapState& a,
        const renegade::bridge::TerrainBlendmapState& b)
    {
        if (a.chunks.size() != b.chunks.size()) return false;
        for (std::size_t i = 0; i < a.chunks.size(); ++i)
        {
            if (a.chunks[i].entity != b.chunks[i].entity || a.chunks[i].layers != b.chunks[i].layers)
                return false;
        }
        return true;
    }

    float Smooth01(float value) noexcept
    {
        value = std::clamp(value, 0.0f, 1.0f);
        return value * value * (3.0f - 2.0f * value);
    }

    bool ReadSquareR16(
        const std::string& filename,
        std::vector<std::uint16_t>& pixels,
        std::uint32_t& side,
        std::string* error)
    {
        std::ifstream stream(filename, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            if (error) *error = "could not open R16 heightmap";
            return false;
        }
        const auto bytes = stream.tellg();
        if (bytes <= 0 || (static_cast<std::uint64_t>(bytes) % 2ull) != 0ull)
        {
            if (error) *error = "R16 heightmap byte count is invalid";
            return false;
        }
        const auto samples = static_cast<std::uint64_t>(bytes) / 2ull;
        const auto root = static_cast<std::uint64_t>(std::llround(std::sqrt(static_cast<double>(samples))));
        if (root == 0 || root * root != samples || root > std::numeric_limits<std::uint32_t>::max())
        {
            if (error) *error = "R16 heightmap must be square; dimensions are inferred from file size";
            return false;
        }
        side = static_cast<std::uint32_t>(root);
        pixels.resize(static_cast<std::size_t>(samples));
        stream.seekg(0, std::ios::beg);
        for (auto& pixel : pixels)
        {
            std::uint8_t lo = 0, hi = 0;
            stream.read(reinterpret_cast<char*>(&lo), 1);
            stream.read(reinterpret_cast<char*>(&hi), 1);
            if (!stream)
            {
                if (error) *error = "R16 heightmap ended unexpectedly";
                return false;
            }
            pixel = static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8u));
        }
        return true;
    }
}

namespace renegade::bridge
{
    TerrainVirtualTextureInfo CaptureTerrainVirtualTextureInfo(
        const wi::terrain::Terrain& terrain) noexcept
    {
        TerrainVirtualTextureInfo info;
        info.chunkCount = terrain.chunks.size();
        info.virtualTexturesInUse = static_cast<int>(terrain.virtual_textures_in_use.size());
        info.tileWidth = terrain.atlas.physical_tile_count_x;
        info.tileHeight = terrain.atlas.physical_tile_count_y;
        for (const auto& entry : terrain.chunks)
        {
            const auto& chunk = entry.second;
            if (chunk.vt == nullptr) continue;
            ++info.chunksWithVirtualTexture;
            info.tileAllocations += chunk.vt->allocation_requests.size();
        }
        return info;
    }

    TerrainBlendmapState CaptureTerrainBlendmaps(const wi::terrain::Terrain& terrain)
    {
        TerrainBlendmapState state;
        state.chunks.reserve(terrain.chunks.size());
        for (const auto& entry : terrain.chunks)
        {
            TerrainBlendmapChunkState chunk;
            chunk.entity = entry.second.entity;
            chunk.layers.reserve(entry.second.blendmap_layers.size());
            for (const auto& layer : entry.second.blendmap_layers)
                chunk.layers.push_back(layer.pixels);
            state.chunks.push_back(std::move(chunk));
        }
        std::sort(state.chunks.begin(), state.chunks.end(), [](const auto& left, const auto& right)
        {
            return left.entity < right.entity;
        });
        return state;
    }

    bool ApplyTerrainBlendmaps(
        wi::terrain::Terrain& terrain,
        const TerrainBlendmapState& state)
    {
        bool changed = false;
        for (const auto& authored : state.chunks)
        {
            auto* chunk = FindChunkByEntity(terrain, authored.entity);
            if (chunk == nullptr) continue;
            chunk->blendmap_layers.resize(authored.layers.size());
            for (std::size_t layer = 0; layer < authored.layers.size(); ++layer)
                chunk->blendmap_layers[layer].pixels = authored.layers[layer];
            chunk->blendmap = {};
            terrain.CreateChunkRegionTexture(*chunk);
            if (chunk->vt != nullptr) chunk->vt->invalidate();
            changed = true;
        }
        return changed;
    }

    bool PaintTerrainMaterial(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const XMFLOAT3& center,
        float radius,
        float amount,
        float smoothness,
        std::size_t materialLayer)
    {
        if (materialLayer >= wi::terrain::MATERIAL_COUNT) return false;
        radius = std::max(0.001f, radius);
        amount = std::clamp(amount, 0.0f, 1.0f);
        smoothness = std::max(0.001f, smoothness);
        bool any = false;

        for (auto& entry : terrain.chunks)
        {
            auto& chunk = entry.second;
            auto* mesh = scene.meshes.GetComponent(chunk.entity);
            const auto* transform = scene.transforms.GetComponent(chunk.entity);
            if (mesh == nullptr || transform == nullptr ||
                mesh->vertex_positions.size() != wi::terrain::vertexCount)
            {
                continue;
            }

            bool rebuild = false;
            const XMMATRIX world = transform->GetWorldMatrix();
            chunk.enable_blendmap_layer(materialLayer);
            auto& target = chunk.blendmap_layers[materialLayer].pixels;
            if (target.size() != wi::terrain::vertexCount)
                target.resize(wi::terrain::vertexCount);

            for (std::size_t i = 0; i < mesh->vertex_positions.size(); ++i)
            {
                XMFLOAT3 position;
                XMStoreFloat3(&position,
                    XMVector3TransformCoord(XMLoadFloat3(&mesh->vertex_positions[i]), world));
                const float dx = position.x - center.x;
                const float dy = position.y - center.y;
                const float dz = position.z - center.z;
                const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (distance > radius) continue;

                const float edge = 1.0f - distance / radius;
                const float affection = amount * Smooth01(std::min(1.0f, edge / smoothness));
                const float current = static_cast<float>(target[i]) / 255.0f;
                target[i] = static_cast<std::uint8_t>(
                    std::clamp(current + (1.0f - current) * affection, 0.0f, 1.0f) * 255.0f);

                for (std::size_t layer = materialLayer + 1; layer < chunk.blendmap_layers.size(); ++layer)
                {
                    auto& pixels = chunk.blendmap_layers[layer].pixels;
                    if (pixels.size() != wi::terrain::vertexCount)
                        pixels.resize(wi::terrain::vertexCount);
                    const float upper = static_cast<float>(pixels[i]) / 255.0f;
                    pixels[i] = static_cast<std::uint8_t>(
                        std::clamp(upper * (1.0f - affection), 0.0f, 1.0f) * 255.0f);
                }
                rebuild = true;
            }

            if (rebuild)
            {
                chunk.blendmap = {};
                terrain.CreateChunkRegionTexture(chunk);
                if (chunk.vt != nullptr) chunk.vt->invalidate();
                any = true;
            }
        }
        return any;
    }

    TerrainMaterialPaintCommand::TerrainMaterialPaintCommand(
        wi::scene::Scene& scene,
        wi::ecs::Entity terrainEntity,
        XMFLOAT3 center,
        float radius,
        float amount,
        float smoothness,
        std::size_t materialLayer)
        : scene_(&scene), terrainEntity_(terrainEntity), center_(center), radius_(radius),
          amount_(amount), smoothness_(smoothness), materialLayer_(materialLayer)
    {
    }

    bool TerrainMaterialPaintCommand::Execute()
    {
        if (scene_ == nullptr) return false;
        auto* terrain = scene_->terrains.GetComponent(terrainEntity_);
        if (terrain == nullptr) return false;
        if (captured_) return Apply(after_);

        before_ = CaptureTerrainBlendmaps(*terrain);
        if (!PaintTerrainMaterial(
                *scene_, *terrain, center_, radius_, amount_, smoothness_, materialLayer_))
        {
            return false;
        }
        after_ = CaptureTerrainBlendmaps(*terrain);
        if (SameBlendmaps(before_, after_)) return false;
        captured_ = true;
        return true;
    }

    void TerrainMaterialPaintCommand::Undo()
    {
        (void)Apply(before_);
    }

    bool TerrainMaterialPaintCommand::Apply(const TerrainBlendmapState& state)
    {
        if (scene_ == nullptr) return false;
        auto* terrain = scene_->terrains.GetComponent(terrainEntity_);
        return terrain != nullptr && ApplyTerrainBlendmaps(*terrain, state);
    }

    bool ExportTerrainHeightmapR16(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain,
        const std::string& filename,
        TerrainHeightmapInfo* info,
        std::string* error)
    {
        const GridBounds bounds = BoundsFor(terrain);
        if (!bounds.Valid())
        {
            if (error) *error = "terrain has no generated chunks";
            return false;
        }
        const std::uint32_t width = bounds.Width();
        const std::uint32_t height = bounds.Height();
        if (width != height)
        {
            if (error) *error = "terrain grid is not square";
            return false;
        }
        std::vector<std::uint16_t> pixels(static_cast<std::size_t>(width) * height, 0);
        std::vector<std::uint8_t> written(pixels.size(), 0);
        const float range = std::max(0.001f, terrain.topLevel - terrain.bottomLevel);

        for (const auto& entry : terrain.chunks)
        {
            const auto* mesh = scene.meshes.GetComponent(entry.second.entity);
            if (mesh == nullptr || mesh->vertex_positions.size() != wi::terrain::vertexCount) continue;
            for (std::size_t i = 0; i < mesh->vertex_positions.size(); ++i)
            {
                const GridKey key = KeyFor(entry.first, i);
                const auto x = static_cast<std::uint32_t>(key.x - bounds.minX);
                const auto z = static_cast<std::uint32_t>(key.z - bounds.minZ);
                const std::size_t index = static_cast<std::size_t>(z) * width + x;
                if (written[index]) continue;
                const float normalized = std::clamp(
                    (mesh->vertex_positions[i].y - terrain.bottomLevel) / range, 0.0f, 1.0f);
                pixels[index] = static_cast<std::uint16_t>(normalized * 65535.0f + 0.5f);
                written[index] = 1;
            }
        }

        std::ofstream stream(filename, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            if (error) *error = "could not create R16 heightmap";
            return false;
        }
        for (const auto pixel : pixels)
        {
            const std::uint8_t bytes[2] = {
                static_cast<std::uint8_t>(pixel & 0xFFu),
                static_cast<std::uint8_t>((pixel >> 8u) & 0xFFu),
            };
            stream.write(reinterpret_cast<const char*>(bytes), 2);
        }
        if (!stream)
        {
            if (error) *error = "failed while writing R16 heightmap";
            return false;
        }
        if (info)
        {
            info->width = width;
            info->height = height;
            info->minimumHeight = terrain.bottomLevel;
            info->maximumHeight = terrain.topLevel;
        }
        if (error) error->clear();
        return true;
    }

    bool BuildTerrainHeightmapImportStateR16(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain,
        const std::string& filename,
        TerrainSculptState& state,
        TerrainHeightmapInfo* info,
        std::string* error)
    {
        std::vector<std::uint16_t> pixels;
        std::uint32_t side = 0;
        if (!ReadSquareR16(filename, pixels, side, error)) return false;
        const GridBounds bounds = BoundsFor(terrain);
        if (!bounds.Valid())
        {
            if (error) *error = "terrain has no generated chunks";
            return false;
        }

        state.chunks.clear();
        state.chunks.reserve(terrain.chunks.size());
        const float range = std::max(0.001f, terrain.topLevel - terrain.bottomLevel);
        const double gridWidth = std::max<std::int64_t>(1, bounds.maxX - bounds.minX);
        const double gridHeight = std::max<std::int64_t>(1, bounds.maxZ - bounds.minZ);

        for (const auto& entry : terrain.chunks)
        {
            const auto* mesh = scene.meshes.GetComponent(entry.second.entity);
            if (mesh == nullptr || mesh->vertex_positions.size() != wi::terrain::vertexCount) continue;
            TerrainChunkHeights chunk;
            chunk.entity = entry.second.entity;
            chunk.heights.resize(mesh->vertex_positions.size());
            for (std::size_t i = 0; i < mesh->vertex_positions.size(); ++i)
            {
                const GridKey key = KeyFor(entry.first, i);
                const double u = static_cast<double>(key.x - bounds.minX) / gridWidth;
                const double v = static_cast<double>(key.z - bounds.minZ) / gridHeight;
                const auto x = static_cast<std::uint32_t>(std::clamp(
                    std::llround(u * static_cast<double>(side - 1)), 0ll,
                    static_cast<long long>(side - 1)));
                const auto z = static_cast<std::uint32_t>(std::clamp(
                    std::llround(v * static_cast<double>(side - 1)), 0ll,
                    static_cast<long long>(side - 1)));
                const float normalized = static_cast<float>(
                    pixels[static_cast<std::size_t>(z) * side + x]) / 65535.0f;
                chunk.heights[i] = terrain.bottomLevel + normalized * range;
            }
            state.chunks.push_back(std::move(chunk));
        }
        if (state.chunks.empty())
        {
            if (error) *error = "terrain contains no editable chunk meshes";
            return false;
        }
        if (info)
        {
            info->width = side;
            info->height = side;
            info->minimumHeight = terrain.bottomLevel;
            info->maximumHeight = terrain.topLevel;
        }
        if (error) error->clear();
        return true;
    }

    ImportTerrainHeightmapR16Command::ImportTerrainHeightmapR16Command(
        wi::scene::Scene& scene,
        wi::ecs::Entity terrainEntity,
        std::string filename)
        : scene_(&scene), terrainEntity_(terrainEntity), filename_(std::move(filename))
    {
    }

    bool ImportTerrainHeightmapR16Command::Execute()
    {
        error_.clear();
        if (scene_ == nullptr) return false;
        auto* terrain = scene_->terrains.GetComponent(terrainEntity_);
        if (terrain == nullptr)
        {
            error_ = "selected entity has no native terrain";
            return false;
        }
        if (captured_) return Apply(after_);

        before_ = CaptureTerrainSculpt(*scene_, *terrain);
        if (!BuildTerrainHeightmapImportStateR16(
                *scene_, *terrain, filename_, after_, &info_, &error_))
        {
            return false;
        }
        if (!Apply(after_))
        {
            error_ = "native terrain rejected heightmap state";
            return false;
        }
        captured_ = true;
        return true;
    }

    void ImportTerrainHeightmapR16Command::Undo()
    {
        (void)Apply(before_);
    }

    bool ImportTerrainHeightmapR16Command::Apply(const TerrainSculptState& state)
    {
        if (scene_ == nullptr) return false;
        auto* terrain = scene_->terrains.GetComponent(terrainEntity_);
        return terrain != nullptr && ApplyTerrainSculpt(*scene_, *terrain, state);
    }
}
