#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    enum class SpecialistComponentKind
    {
        Hair,
        ForceField,
        Video,
        Spline,
    };

    class EnsureSpecialistComponentCommand final : public ICommand
    {
    public:
        EnsureSpecialistComponentCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            SpecialistComponentKind kind);
        bool Execute() override;
        void Undo() override;

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        SpecialistComponentKind kind_ = SpecialistComponentKind::Hair;
        bool created_ = false;
    };

    struct HairParticleState
    {
        wi::ecs::Entity mesh = wi::ecs::INVALID_ENTITY;
        bool cameraBend = false;
        std::uint32_t strandCount = 1000;
        float length = 1.0f;
        float width = 1.0f;
        float stiffness = 0.5f;
        float drag = 0.5f;
        float gravityPower = 0.5f;
        float randomness = 0.2f;
        std::uint32_t segments = 1;
        std::uint32_t billboards = 1;
        std::uint32_t randomSeed = 1;
        float viewDistance = 100.0f;
        float uniformity = 0.1f;
    };

    [[nodiscard]] HairParticleState CaptureHairParticle(
        const wi::HairParticleSystem& hair) noexcept;
    [[nodiscard]] HairParticleState SanitizeHairParticle(
        const wi::scene::Scene& scene,
        const HairParticleState& state) noexcept;
    void ApplyHairParticle(
        wi::HairParticleSystem& hair,
        const HairParticleState& state) noexcept;

    class SetHairParticleCommand final : public ICommand
    {
    public:
        SetHairParticleCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            HairParticleState state);
        bool Execute() override;
        void Undo() override;
    private:
        bool Apply(const HairParticleState& state) noexcept;
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        HairParticleState before_;
        HairParticleState after_;
    };

    struct ForceFieldState
    {
        wi::scene::ForceFieldComponent::Type type =
            wi::scene::ForceFieldComponent::Type::Point;
        float gravity = 0.0f;
        float range = 10.0f;
    };

    [[nodiscard]] ForceFieldState CaptureForceField(
        const wi::scene::ForceFieldComponent& force) noexcept;

    class SetForceFieldCommand final : public ICommand
    {
    public:
        SetForceFieldCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            ForceFieldState state);
        bool Execute() override;
        void Undo() override;
    private:
        bool Apply(const ForceFieldState& state) noexcept;
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        ForceFieldState before_;
        ForceFieldState after_;
    };

    struct VideoAuthoringState
    {
        std::string filename;
        bool looped = false;
    };

    [[nodiscard]] VideoAuthoringState CaptureVideoAuthoring(
        const wi::scene::VideoComponent& video);
    [[nodiscard]] bool ApplyVideoAuthoring(
        wi::scene::VideoComponent& video,
        const VideoAuthoringState& state,
        std::string& error);
    [[nodiscard]] bool PreviewVideo(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        bool play) noexcept;
    [[nodiscard]] bool StopVideoPreview(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;
    [[nodiscard]] bool SeekVideoPreview(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        float seconds) noexcept;

    class SetVideoAuthoringCommand final : public ICommand
    {
    public:
        SetVideoAuthoringCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            VideoAuthoringState state);
        bool Execute() override;
        void Undo() override;
        [[nodiscard]] const std::string& LastError() const noexcept;
    private:
        bool Apply(const VideoAuthoringState& state) noexcept;
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        VideoAuthoringState before_;
        VideoAuthoringState after_;
        std::string lastError_;
    };

    struct SplineAuthoringState
    {
        bool looped = false;
        bool filled = false;
        bool drawAligned = false;
        float width = 1.0f;
        float rotationDegrees = 0.0f;
        int meshSubdivision = 0;
        int verticalSubdivision = 0;
        float terrainModifier = 0.0f;
        float terrainTextureFalloff = 0.0f;
        float terrainPushdown = 0.0f;
        wi::scene::MeshComponent::COMPUTE_NORMALS fillNormals =
            wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH;
    };

    [[nodiscard]] SplineAuthoringState CaptureSplineAuthoring(
        const wi::scene::SplineComponent& spline) noexcept;
    void ApplySplineAuthoring(
        wi::scene::SplineComponent& spline,
        const SplineAuthoringState& state) noexcept;

    class SetSplineAuthoringCommand final : public ICommand
    {
    public:
        SetSplineAuthoringCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            SplineAuthoringState state);
        bool Execute() override;
        void Undo() override;
    private:
        bool Apply(const SplineAuthoringState& state) noexcept;
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        SplineAuthoringState before_;
        SplineAuthoringState after_;
    };

    class AddSplineNodeCommand final : public ICommand
    {
    public:
        AddSplineNodeCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity splineEntity,
            XMFLOAT3 localPosition);
        bool Execute() override;
        void Undo() override;
        [[nodiscard]] wi::ecs::Entity CreatedNode() const noexcept;
    private:
        bool RestoreNode();
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity spline_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity node_ = wi::ecs::INVALID_ENTITY;
        XMFLOAT3 localPosition_ = XMFLOAT3(0, 0, 0);
        wi::Archive snapshot_;
        std::size_t insertionIndex_ = 0;
        bool snapshotReady_ = false;
    };

    struct GaussianSplatInfo
    {
        std::size_t splatCount = 0;
        int sphericalHarmonicsDegree = 0;
        std::size_t cpuMemoryBytes = 0;
        std::size_t gpuMemoryBytes = 0;
    };

    [[nodiscard]] GaussianSplatInfo InspectGaussianSplat(
        const wi::GaussianSplatModel& splat) noexcept;

    class ImportGaussianSplatCommand final : public ICommand
    {
    public:
        ImportGaussianSplatCommand(
            wi::scene::Scene& scene,
            std::string sourcePath);
        bool Execute() override;
        void Undo() override;
        [[nodiscard]] const std::vector<wi::ecs::Entity>& CreatedEntities()
            const noexcept;
        [[nodiscard]] const std::string& LastError() const noexcept;
    private:
        bool ImportFirstTime();
        bool Restore();
        wi::scene::Scene* scene_ = nullptr;
        std::string sourcePath_;
        std::vector<wi::ecs::Entity> entities_;
        std::vector<wi::Archive> snapshots_;
        std::string lastError_;
        bool snapshotsReady_ = false;
    };

    // Phase 7E does not pretend Wicked virtual-texture residency is authored
    // content. This is an inspectable status surface over the native Terrain
    // runtime so creators can see what exists without serializing transient GPU
    // residency/page state into Renegade metadata.
    struct TerrainVirtualTextureStatus
    {
        bool atlasValid = false;
        std::size_t chunkCount = 0;
        std::size_t virtualTextureChunkCount = 0;
        std::size_t activeVirtualTextureCount = 0;
        std::uint32_t physicalTilesX = 0;
        std::uint32_t physicalTilesY = 0;
    };

    [[nodiscard]] TerrainVirtualTextureStatus InspectTerrainVirtualTextures(
        const wi::terrain::Terrain& terrain) noexcept;

    struct TerrainPaintChunkState
    {
        wi::terrain::Chunk chunk;
        std::size_t materialIndex = 0;
        std::vector<std::uint8_t> pixels;
    };

    struct TerrainPaintState
    {
        std::vector<TerrainPaintChunkState> chunks;
    };

    [[nodiscard]] TerrainPaintState CaptureTerrainPaint(
        const wi::terrain::Terrain& terrain,
        std::size_t materialIndex);
    [[nodiscard]] bool PaintTerrainMaterial(
        wi::terrain::Terrain& terrain,
        const XMFLOAT3& worldCenter,
        float radius,
        float strength,
        std::size_t materialIndex,
        bool erase,
        TerrainPaintState& before,
        TerrainPaintState& after,
        std::string& error);
    [[nodiscard]] bool ApplyTerrainPaint(
        wi::terrain::Terrain& terrain,
        const TerrainPaintState& state) noexcept;

    class PaintTerrainMaterialCommand final : public ICommand
    {
    public:
        PaintTerrainMaterialCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity terrainEntity,
            TerrainPaintState before,
            TerrainPaintState after);
        bool Execute() override;
        void Undo() override;
    private:
        bool Apply(const TerrainPaintState& state) noexcept;
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity terrain_ = wi::ecs::INVALID_ENTITY;
        TerrainPaintState before_;
        TerrainPaintState after_;
    };
}
