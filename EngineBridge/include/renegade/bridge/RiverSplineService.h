#pragma once

#include <cstddef>
#include <utility>
#include <vector>
#include <WickedEngine.h>
#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    // A finite, connected stroke on the terrain. Y is the sampled terrain
    // elevation; the waterline defaults below the original ground so linked
    // riverbanks can conceal the ribbon edges.
    struct RiverSplineOptions
    {
        float widthMeters = 8.0f;
        float surfaceOffsetMeters = -0.15f;
        int subdivisionsPerSegment = 6;
    };

    [[nodiscard]] bool IsRiverSplineStrokeValid(
        const std::vector<XMFLOAT3>& points) noexcept;

    // Arc distance from the nearest curve point closes the bed INSIDE both
    // native ribbon ends. Returns a rounded cap distance, never a cut beyond
    // an open endpoint. Short rivers remain finite and taper at both ends.
    [[nodiscard]] float RiverBedEndDistance(float lateralDistanceSquared,
        float along, float length, float radius) noexcept;
    // One-time migration for existing Renegade water splines; authored native
    // shadow settings remain editable after migration.
    void RestoreRiverCaustics(wi::scene::Scene& scene);
    // Test-only shader IDs are process-local and must NEVER enter WISCENE.
    // Masks just the opt-in diagnostic ID for the synchronous archive write,
    // restoring the exact material ID on scope exit, including exceptions.
    class ScopedRiverLocalDepthArchiveMask final
    {
    public:
        explicit ScopedRiverLocalDepthArchiveMask(wi::scene::Scene& scene);
        ~ScopedRiverLocalDepthArchiveMask() noexcept;
        ScopedRiverLocalDepthArchiveMask(const ScopedRiverLocalDepthArchiveMask&) = delete;
        ScopedRiverLocalDepthArchiveMask& operator=(const ScopedRiverLocalDepthArchiveMask&) = delete;
    private:
        wi::scene::Scene& scene_;
        std::vector<std::pair<wi::ecs::Entity,int>> originals_;
    };

    // Restore the native WATER pipeline; clear rejected experimental custom IDs.
    [[nodiscard]] bool BindRiverSurfaceShader(wi::scene::Scene& scene, wi::ecs::Entity river);

    // Native river WATER controls only: never route through general material
    // sanitation, which can clamp wide-river UV tiling or reset other optics.
    // These values persist in Wicked's MaterialComponent WISCENE archive.
    // Stock river look approved by the owner (pale blue, 2026-10-21).
    // The inspector calls 1 - material alpha 'clarity'; Wicked WATER uses
    // alpha for water depth attenuation, not transparent surface blending.
    inline constexpr float DefaultRiverBedDepthMeters = 1.35f;
    inline constexpr float DefaultRiverBankWidthMeters = 2.0f;
    struct RiverWaterMaterialState
    {
        XMFLOAT4 tintOpacity{0.031f, 0.044f, 0.047f, 0.126f}; // 0.874 clarity
        float flowUvV = -0.001f;
        float flowUvU = 0;
        float normalStrength = 1.0f;
    };
    [[nodiscard]] bool CaptureRiverWaterMaterialState(
        const wi::scene::Scene& scene, wi::ecs::Entity river,
        RiverWaterMaterialState& state) noexcept;

    // Restricts commands to native WATER splines. Other material properties,
    // normal texture, extinction and riverbed composition are never overwritten.
    class SetRiverWaterMaterialCommand final : public ICommand
    {
    public:
        SetRiverWaterMaterialCommand(wi::scene::Scene& scene,
            wi::ecs::Entity river, RiverWaterMaterialState after) noexcept;
        bool Execute() override;
        void Undo() override;
    private:
        bool Apply(const RiverWaterMaterialState& state) noexcept;
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity river_ = wi::ecs::INVALID_ENTITY;
        RiverWaterMaterialState before_{}, after_{};
        bool captured_ = false;
    };

    // EXPLICIT OPT-IN ONLY, not called by new-river creation or scene load.
    // Re-express a near-planar, consistently graded river in a tilted root's local
    // frame, preserving every authored world control point. Wicked obtains
    // native WATER's reflection plane from the root, not mesh vertex normals.
    // Reject rivers whose height cannot be fitted within a tight plane tolerance. No terrain, mesh shader, materials,
    // identities or saved scenes are directly edited.
    class OrientNearPlanarRiverWaterPlaneCommand final : public ICommand
    {
    public:
        OrientNearPlanarRiverWaterPlaneCommand(wi::scene::Scene& scene,
            wi::ecs::Entity river) noexcept;
        bool Execute() override;
        void Undo() override;

    private:
        [[nodiscard]] bool ApplySaved(
            const TransformState& root,
            const std::vector<TransformState>& nodes) noexcept;
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity river_ = wi::ecs::INVALID_ENTITY;
        std::vector<wi::ecs::Entity> nodes_;
        TransformState beforeRoot_{}, afterRoot_{};
        std::vector<TransformState> beforeNodes_, afterNodes_;
        bool captured_ = false;
    };

    // One Undo/Redo transaction owns the river spline and its node hierarchy.
    // Wicked generates the ribbon mesh during Scene::RunSplineUpdateSystem.
    class CreateRiverSplineCommand final : public ICommand
    {
    public:
        CreateRiverSplineCommand(
            wi::scene::Scene& scene,
            std::vector<XMFLOAT3> points,
            RiverSplineOptions options = {});

        bool Execute() override;
        void Undo() override;
        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept
        {
            return entity_;
        }

    private:
        [[nodiscard]] bool Restore();
        wi::scene::Scene* scene_ = nullptr;
        std::vector<XMFLOAT3> points_;
        RiverSplineOptions options_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };
}
