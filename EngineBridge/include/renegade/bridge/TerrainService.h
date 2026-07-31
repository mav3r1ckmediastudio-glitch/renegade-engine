#pragma once

#include <cstdint>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    enum class TerrainPreset
    {
        FlatWorld,
        Island,
        Coastline,
        Highlands,
    };

    // Curated, serializable authoring view of Wicked's streamed terrain.
    // Material assets and painted chunk data deliberately remain owned by the
    // native Terrain component and survive state edits untouched.
    struct TerrainState
    {
        bool centerToCamera = false;
        bool removeDistantChunks = false;
        bool physics = true;
        bool tessellation = false;
        int visibleChunkRadius = 6;
        int propChunkRadius = 4;
        int physicsChunkRadius = 3;
        float chunkScale = 2.0f;
        std::uint32_t seed = 3926;
        float minimumHeight = -20.0f;
        float maximumHeight = 120.0f;
        float lowAltitudeBlend = 0.12f;
        float baseBlend = 0.42f;
        float slopeBlend = 0.72f;
        float lodBias = 0.0f;
    };

    [[nodiscard]] TerrainState CaptureTerrain(
        const wi::terrain::Terrain& terrain) noexcept;
    void ApplyTerrain(
        wi::terrain::Terrain& terrain,
        const TerrainState& state,
        bool restartGeneration = true) noexcept;
    [[nodiscard]] TerrainState MakeTerrainPreset(
        const TerrainState& current,
        TerrainPreset preset) noexcept;

    // Creates the native component and its four automatic material slots.
    // Texture assignment is intentionally asset-pipeline work; the generated
    // neutral colours make the terrain usable before imported assets exist.
    [[nodiscard]] wi::ecs::Entity CreateTerrain(
        wi::scene::Scene& scene,
        const TerrainState& state,
        const char* name = "Terrain");

    class SetTerrainCommand final : public ICommand
    {
    public:
        SetTerrainCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const TerrainState& terrain);
        SetTerrainCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const TerrainState& before,
            const TerrainState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const TerrainState& state);

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        TerrainState before_;
        TerrainState after_;
    };
}
