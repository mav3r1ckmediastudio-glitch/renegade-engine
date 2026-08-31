#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    enum class VegetationBrushMode : std::uint8_t
    {
        Paint,
        Delete,
    };

    struct VegetationChunkMaskState
    {
        wi::ecs::Entity chunkEntity = wi::ecs::INVALID_ENTITY;
        std::vector<float> vertexLengths;
        std::uint32_t strandCount = 0;
    };

    struct VegetationStrokeState
    {
        std::vector<VegetationChunkMaskState> chunks;
    };

    struct VegetationPaintResult
    {
        bool changed = false;
        std::size_t affectedChunks = 0;
        std::size_t affectedVertices = 0;
        std::string error;
    };

    // Creator-facing subset of Wicked's native HairParticleSystem settings.
    // Mesh and raw strand count are deliberately not exposed: terrain chunks
    // are the emitter meshes and Renegade's Density control owns strand count.
    struct VegetationGrassSettings
    {
        float length = 0.35f;
        float width = 1.0f;
        float stiffness = 0.5f;
        float drag = 0.1f;
        float gravityPower = 0.0f;
        float randomness = 0.2f;
        std::uint32_t segmentCount = 1;
        std::uint32_t billboardCount = 1;
        std::uint32_t randomSeed = 1;
        float viewDistance = 200.0f;
        float uniformity = 1.0f;
        bool cameraBendEnabled = true;
        wi::vector<wi::HairParticleSystem::AtlasRect> atlasRects;
    };

    bool EnsureDefaultGrassVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        std::string* error = nullptr);

    [[nodiscard]] bool IsManualVegetationEnabled(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain) noexcept;

    std::size_t SynchronizeManualVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain);

    VegetationPaintResult PaintVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const XMFLOAT3& center,
        float radius,
        VegetationBrushMode mode,
        VegetationStrokeState* beforeState = nullptr);

    [[nodiscard]] VegetationStrokeState CaptureVegetationAfter(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain,
        const VegetationStrokeState& beforeState);

    bool ApplyVegetationState(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const VegetationStrokeState& state);

    [[nodiscard]] float CaptureVegetationDensity(
        const wi::terrain::Terrain& terrain) noexcept;
    bool SetVegetationDensity(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        float density);

    [[nodiscard]] VegetationGrassSettings CaptureVegetationGrassSettings(
        const wi::terrain::Terrain& terrain);
    [[nodiscard]] bool VegetationGrassSettingsEqual(
        const VegetationGrassSettings& left,
        const VegetationGrassSettings& right) noexcept;
    bool ApplyVegetationGrassSettings(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const VegetationGrassSettings& settings);

    class VegetationStrokeCommand final : public ICommand
    {
    public:
        VegetationStrokeCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity terrainEntity,
            VegetationStrokeState before,
            VegetationStrokeState after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const VegetationStrokeState& state);

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity terrainEntity_ = wi::ecs::INVALID_ENTITY;
        VegetationStrokeState before_;
        VegetationStrokeState after_;
    };

    class SetVegetationDensityCommand final : public ICommand
    {
    public:
        SetVegetationDensityCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity terrainEntity,
            float before,
            float after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(float density);

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity terrainEntity_ = wi::ecs::INVALID_ENTITY;
        float before_ = 1.0f;
        float after_ = 1.0f;
    };

    class SetVegetationGrassSettingsCommand final : public ICommand
    {
    public:
        SetVegetationGrassSettingsCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity terrainEntity,
            VegetationGrassSettings before,
            VegetationGrassSettings after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const VegetationGrassSettings& settings);

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity terrainEntity_ = wi::ecs::INVALID_ENTITY;
        VegetationGrassSettings before_;
        VegetationGrassSettings after_;
    };
}
