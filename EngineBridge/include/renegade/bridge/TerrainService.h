#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
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
        // World-space vertex spacing. One metre is Renegade's standard
        // authoring resolution; coarser values remain available for large
        // landscapes created deliberately by the user.
        float chunkScale = 1.0f;
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
    // Creates the native component and its four automatic material slots.
    // The base slot uses Renegade's bundled grass PBR material until the
    // asset-facing material workflow can replace it.
    [[nodiscard]] wi::ecs::Entity CreateTerrain(
        wi::scene::Scene& scene,
        const TerrainState& state,
        const char* name = "Terrain");

    // Rebinds only Renegade's bundled default terrain maps after a WISCENE
    // load. Wicked stores resource names relative to the scene archive, while
    // these read-only defaults live beside Studio and Runtime instead of in
    // each project.
    void RebindDefaultTerrainMaterials(wi::scene::Scene& scene);

    constexpr float DefaultGrassPackedTileCount = 32.0f;
    constexpr float DefaultGrassTextureScale = 8.0f;

    enum class TerrainMaterialPreset
    {
        Meadow,
        CoarseGrass,
        FineGroundCover,
    };

    struct TerrainMaterialSlotState
    {
        XMFLOAT4 baseColor = XMFLOAT4(1, 1, 1, 1);
        XMFLOAT4 texMulAdd = XMFLOAT4(1, 1, 0, 0);
        float roughness = 1.0f;
        float metalness = 0.0f;
        float reflectance = 0.02f;
        float normalMapStrength = 1.0f;
        bool primaryOcclusion = true;
        std::string baseColorMap;
        std::string normalMap;
        std::string surfaceMap;
    };

    struct TerrainMaterialState
    {
        std::array<TerrainMaterialSlotState, wi::terrain::MATERIAL_COUNT> slots;
    };

    [[nodiscard]] TerrainMaterialState CaptureTerrainMaterial(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain);
    void ApplyTerrainMaterial(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const TerrainMaterialState& state,
        bool restartGeneration = true);
    [[nodiscard]] float CaptureTerrainTextureScale(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain) noexcept;
    void SetTerrainTextureScale(TerrainMaterialState& state, float scale) noexcept;
    [[nodiscard]] float MakeTerrainMaterialPreset(
        TerrainMaterialPreset preset) noexcept;
    [[nodiscard]] TerrainMaterialState MakeDefaultGrassMaterial(
        float textureScale = DefaultGrassTextureScale);
    void ReloadDefaultTerrainMaterial(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain);

    class SetTerrainMaterialCommand final : public ICommand
    {
    public:
        SetTerrainMaterialCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity terrainEntity,
            TerrainMaterialState before,
            TerrainMaterialState after);
        bool Execute() override;
        void Undo() override;
    private:
        bool Apply(const TerrainMaterialState& state);
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity terrainEntity_ = wi::ecs::INVALID_ENTITY;
        TerrainMaterialState before_;
        TerrainMaterialState after_;
    };

    class CreateTerrainCommand final : public ICommand
    {
    public:
        CreateTerrainCommand(
            wi::scene::Scene& scene,
            const TerrainState& terrain,
            const char* name = "Terrain");

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;

    private:
        wi::scene::Scene* scene_ = nullptr;
        TerrainState terrain_;
        std::string name_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };

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

    enum class TerrainSculptMode { Raise, Lower, Smooth, Flatten };

    struct TerrainChunkHeights
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        std::vector<float> heights;
    };

    struct TerrainSculptState
    {
        std::vector<TerrainChunkHeights> chunks;
    };

    [[nodiscard]] TerrainSculptState CaptureTerrainSculpt(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain);
    // Removes identical chunks from both states and returns the number of
    // chunks that make up the completed stroke.
    std::size_t RetainChangedTerrainSculpt(
        TerrainSculptState& before,
        TerrainSculptState& after) noexcept;
    void RefreshTerrainSculptPhysics(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const TerrainSculptState& changedState);
    bool ApplyTerrainSculpt(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const TerrainSculptState& state);
    bool SculptTerrain(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const XMFLOAT3& center,
        float radius,
        float strength,
        float falloff,
        TerrainSculptMode mode,
        float flattenHeight);

    class SculptTerrainCommand final : public ICommand
    {
    public:
        SculptTerrainCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity terrainEntity,
            TerrainSculptState before,
            TerrainSculptState after);
        bool Execute() override;
        void Undo() override;
    private:
        bool Apply(const TerrainSculptState& state);
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity terrainEntity_ = wi::ecs::INVALID_ENTITY;
        TerrainSculptState before_;
        TerrainSculptState after_;
    };
}
