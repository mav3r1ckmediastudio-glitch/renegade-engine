#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/TerrainService.h"

namespace renegade::bridge
{
    struct TerrainVirtualTextureInfo
    {
        std::size_t chunkCount = 0;
        std::size_t chunksWithVirtualTexture = 0;
        int virtualTexturesInUse = 0;
        std::uint64_t tileAllocations = 0;
        std::uint32_t tileWidth = 0;
        std::uint32_t tileHeight = 0;
    };

    [[nodiscard]] TerrainVirtualTextureInfo CaptureTerrainVirtualTextureInfo(
        const wi::terrain::Terrain& terrain) noexcept;

    struct TerrainBlendmapChunkState
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::vector<std::vector<std::uint8_t>> layers;
    };

    struct TerrainBlendmapState
    {
        std::vector<TerrainBlendmapChunkState> chunks;
    };

    [[nodiscard]] TerrainBlendmapState CaptureTerrainBlendmaps(
        const wi::terrain::Terrain& terrain);
    bool ApplyTerrainBlendmaps(
        wi::terrain::Terrain& terrain,
        const TerrainBlendmapState& state);
    bool PaintTerrainMaterial(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const XMFLOAT3& center,
        float radius,
        float amount,
        float smoothness,
        std::size_t materialLayer);

    class TerrainMaterialPaintCommand final : public ICommand
    {
    public:
        TerrainMaterialPaintCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity terrainEntity,
            XMFLOAT3 center,
            float radius,
            float amount,
            float smoothness,
            std::size_t materialLayer);
        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const TerrainBlendmapState& state);
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity terrainEntity_ = wi::ecs::INVALID_ENTITY;
        XMFLOAT3 center_ = {};
        float radius_ = 8.0f;
        float amount_ = 1.0f;
        float smoothness_ = 1.0f;
        std::size_t materialLayer_ = 0;
        TerrainBlendmapState before_;
        TerrainBlendmapState after_;
        bool captured_ = false;
    };

    struct TerrainHeightmapInfo
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        float minimumHeight = 0.0f;
        float maximumHeight = 0.0f;
    };

    // Renegade uses a deterministic square little-endian R16 heightmap for
    // lossless round-tripping. Pixel 0 maps to terrain.bottomLevel and 65535
    // maps to terrain.topLevel. The dimensions are inferred from file size.
    bool ExportTerrainHeightmapR16(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain,
        const std::string& filename,
        TerrainHeightmapInfo* info = nullptr,
        std::string* error = nullptr);

    bool BuildTerrainHeightmapImportStateR16(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain,
        const std::string& filename,
        TerrainSculptState& state,
        TerrainHeightmapInfo* info = nullptr,
        std::string* error = nullptr);

    class ImportTerrainHeightmapR16Command final : public ICommand
    {
    public:
        ImportTerrainHeightmapR16Command(
            wi::scene::Scene& scene,
            wi::ecs::Entity terrainEntity,
            std::string filename);
        bool Execute() override;
        void Undo() override;
        [[nodiscard]] const std::string& Error() const noexcept { return error_; }
        [[nodiscard]] const TerrainHeightmapInfo& Info() const noexcept { return info_; }

    private:
        bool Apply(const TerrainSculptState& state);
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity terrainEntity_ = wi::ecs::INVALID_ENTITY;
        std::string filename_;
        std::string error_;
        TerrainHeightmapInfo info_;
        TerrainSculptState before_;
        TerrainSculptState after_;
        bool captured_ = false;
    };
}
