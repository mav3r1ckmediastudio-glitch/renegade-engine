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

    // Loads Wicked's pinned Content/terrain/grass.wiscene into the native
    // Terrain grass property/material seam, then switches the terrain to
    // Renegade's explicit manual-paint mode. Existing generated grass is
    // cleared only on this first transition; creator-authored masks are not.
    bool EnsureDefaultGrassVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        std::string* error = nullptr);

    [[nodiscard]] bool IsManualVegetationEnabled(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain) noexcept;

    // Newly generated terrain chunks have Wicked's procedural grass mask.
    // Once a terrain is in manual mode, initialize only unmarked new chunks to
    // empty grass so expanding terrain never invents creator vegetation.
    std::size_t SynchronizeManualVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain);

    // Applies one live brush sample. When beforeState is supplied, the original
    // state of each touched chunk is captured lazily before its first mutation,
    // keeping completed-stroke Undo/Redo bounded to changed chunks.
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
}
