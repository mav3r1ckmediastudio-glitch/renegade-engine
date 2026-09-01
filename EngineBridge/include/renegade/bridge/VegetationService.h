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

    struct WickedVegetationSettings
    {
        float density = 1.0f;
        float length = 1.0f;
        float width = 1.0f;
        float stiffness = 0.5f;
        float drag = 0.1f;
        float gravityPower = 0.0f;
        float randomness = 0.2f;
        std::uint32_t randomSeed = 1;
        std::uint32_t segmentCount = 1;
        std::uint32_t billboardCount = 1;
        float viewDistance = 200.0f;
        float uniformity = 1.0f;
        bool cameraBend = true;
    };

    struct VegetationChunkState
    {
        wi::ecs::Entity chunkEntity = wi::ecs::INVALID_ENTITY;
        std::vector<float> vertexLengths;
    };

    struct VegetationStrokeState
    {
        std::vector<VegetationChunkState> chunks;
        bool hasLastCenter = false;
        XMFLOAT3 lastCenter = {};
    };

    struct VegetationPaintResult
    {
        bool changed = false;
        std::size_t affectedChunks = 0;
        std::size_t affectedVertices = 0;
        std::string error;
    };

    // Installs Wicked's pinned Content/terrain/grass.wiscene as the terrain's
    // native grass authority without restarting or replacing authored terrain.
    bool InitializeWickedVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        std::string* error = nullptr);

    [[nodiscard]] bool IsWickedVegetationInitialized(
        const wi::scene::Scene& scene,
        const wi::terrain::Terrain& terrain) noexcept;

    // Rebinds the bundled grass texture after Studio or Runtime adopts a
    // serialized scene at a different filesystem location.
    void RebindWickedVegetationResources(wi::scene::Scene& scene);

    // Clears native procedural masks only for newly generated chunks after the
    // terrain enters manual authoring. Existing painted chunks are untouched.
    std::size_t SynchronizeWickedVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain);

    VegetationPaintResult PaintWickedVegetation(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const XMFLOAT3& center,
        float radius,
        VegetationBrushMode mode,
        VegetationStrokeState* beforeState = nullptr);

    // Rebuilds the native per-chunk emitter distribution once when a completed
    // gesture ends, not once per pointer sample.
    void FinalizeWickedVegetationStroke(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const VegetationStrokeState& touchedState);

    [[nodiscard]] VegetationStrokeState CaptureWickedVegetationAfter(
        const wi::terrain::Terrain& terrain,
        const VegetationStrokeState& beforeState);

    bool ApplyWickedVegetationState(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const VegetationStrokeState& state);

    [[nodiscard]] WickedVegetationSettings CaptureWickedVegetationSettings(
        const wi::terrain::Terrain& terrain) noexcept;

    bool ApplyWickedVegetationSettings(
        wi::scene::Scene& scene,
        wi::terrain::Terrain& terrain,
        const WickedVegetationSettings& settings);

    class WickedVegetationStrokeCommand final : public ICommand
    {
    public:
        WickedVegetationStrokeCommand(
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

    class SetWickedVegetationSettingsCommand final : public ICommand
    {
    public:
        SetWickedVegetationSettingsCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity terrainEntity,
            WickedVegetationSettings before,
            WickedVegetationSettings after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const WickedVegetationSettings& settings);

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity terrainEntity_ = wi::ecs::INVALID_ENTITY;
        WickedVegetationSettings before_;
        WickedVegetationSettings after_;
    };
}
