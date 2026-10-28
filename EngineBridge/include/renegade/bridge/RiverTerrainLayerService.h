#pragma once
#include "renegade/bridge/TerrainService.h"
#include "renegade/bridge/RiverSplineService.h"
#include <string>

namespace renegade::bridge
{
    inline constexpr const char* RiverTerrainLayersMetadataKey =
        "renegade.terrain.river-layers.v1";

    // Read-only per-river UI state. Does not cancel terrain generation, touch
    // native meshes or recompose sculpt layers. Validates the saved UUID layer
    // document and fails closed on malformed or ambiguous metadata.
    struct RiverTerrainLayerSettings
    {
        bool linked = false;
        float depthMeters = DefaultRiverBedDepthMeters;
        float bankWidthMeters = DefaultRiverBankWidthMeters;
    };
    [[nodiscard]] bool ReadRiverTerrainLayerSettings(
        const wi::scene::Scene& scene, wi::ecs::Entity terrainEntity,
        wi::ecs::Entity riverEntity, RiverTerrainLayerSettings& settings,
        std::string& error);

    [[nodiscard]] bool SceneHasRiverTerrainLayers(const wi::scene::Scene& scene);
    [[nodiscard]] bool HasRiverTerrainLayers(
        const wi::scene::Scene& scene, wi::ecs::Entity terrainEntity);
    // All mutating calls require the editor scene-safe resource phase.
    // River references use existing persistent UUIDs, never raw ECS IDs.
    [[nodiscard]] bool SetRiverTerrainLayer(wi::scene::Scene& scene,
        wi::ecs::Entity terrainEntity, wi::ecs::Entity riverEntity,
        float depth, float bankWidth, bool enabled, std::string& error);
    [[nodiscard]] bool RecomposeRiverTerrainLayers(wi::scene::Scene& scene,
        wi::ecs::Entity terrainEntity, std::string& error);
    [[nodiscard]] bool ReadRiverTerrainBaseline(wi::scene::Scene& scene,
        wi::ecs::Entity terrainEntity, TerrainSculptState& baseline,
        std::string& error);
    [[nodiscard]] bool SetRiverTerrainBaseline(wi::scene::Scene& scene,
        wi::ecs::Entity terrainEntity, const TerrainSculptState& baseline,
        std::string& error, bool refreshPhysics = true);

    // Ordinary obstacle movement must not snapshot/recompose linked terrain.
    // River controls/roots/ancestors and linked terrain transforms still do.
    [[nodiscard]] bool RiverTransformNeedsTerrainTransaction(const wi::scene::Scene& scene,
        wi::ecs::Entity entity);
    [[nodiscard]] bool IsRiverSplineControlNode(const wi::scene::Scene& scene,
        wi::ecs::Entity entity);
    // Bounded live mesh preview only; linked terrain is recomposed once on commit.
    // Explicit Restore also cancels every neighbouring point displacement.
    class RiverNodeMovePreview
    {
    public:
        RiverNodeMovePreview(wi::scene::Scene& scene,wi::ecs::Entity entity,
            const TransformState& before);
        bool Apply(const TransformState& after);
        void Restore();
    private:
        struct Node {wi::ecs::Entity entity;TransformState before;float along;};
        wi::scene::Scene* scene_;
        wi::ecs::Entity selected_,root_=wi::ecs::INVALID_ENTITY;
        std::vector<Node> nodes_;
        size_t selectedIndex_=0;
        float width_=0;
    };

    // One transaction owns both the native transform and every affected
    // composed terrain snapshot. Execute only at a scene-safe resource point.
    class WatercourseSceneCommand : public ICommand
    {
    public:
        WatercourseSceneCommand(wi::scene::Scene& scene, std::unique_ptr<ICommand> command);
        bool Execute() override;
        void Undo() override;
    private:
        struct TerrainEdit
        {
            wi::ecs::Entity entity;
            TerrainSculptState before, after;
        };
        wi::scene::Scene* scene_;
        std::unique_ptr<ICommand> command_;
        std::vector<TerrainEdit> terrains_;
        bool captured_ = false;
    };

    class SetWatercourseTransformCommand final : public WatercourseSceneCommand
    {
    public:
        SetWatercourseTransformCommand(wi::scene::Scene& scene, wi::ecs::Entity entity,
            const TransformState& before, const TransformState& after);
    };

    class SetRiverTerrainLayerCommand final : public ICommand
    {
    public:
        SetRiverTerrainLayerCommand(wi::scene::Scene& scene,
            wi::ecs::Entity terrainEntity, wi::ecs::Entity riverEntity,
            float depth, float bankWidth, bool enabled = true);
        bool Execute() override;
        void Undo() override;
        const std::string& Error() const noexcept { return error_; }
    private:
        wi::scene::Scene* scene_;
        wi::ecs::Entity terrain_, river_;
        float depth_, bank_;
        bool enabled_, captured_ = false;
        TerrainSculptState before_, after_;
        std::string error_;
    };

    // Explicit bounded authoring transaction. Reads the authored baseline, fits
    // a downhill native spline below both banks, then links its channel. Never
    // invoked on scene load or implicitly by drawing. Execute at safe point.
    struct MountainStreamFit
    {
        std::vector<XMFLOAT3> points;
        float maximumCutMeters = 0;
        float lengthMeters = 0;
    };
    [[nodiscard]] bool PlanMountainStream(wi::scene::Scene& scene,
        wi::ecs::Entity terrain, wi::ecs::Entity river,
        MountainStreamFit& result, std::string& error);

    class FitMountainStreamCommand final : public ICommand
    {
    public:
        FitMountainStreamCommand(wi::scene::Scene& scene, wi::ecs::Entity terrain,
            wi::ecs::Entity river, float depth, float bank);
        bool Execute() override;
        void Undo() override;
        const std::string& Error() const noexcept { return error_; }
    private:
        struct Node { wi::ecs::Entity entity; TransformState pose; };
        bool ApplyNodes(bool fitted);
        wi::scene::Scene* scene_;
        wi::ecs::Entity terrain_, river_;
        float depth_, bank_;
        std::vector<Node> beforeNodes_, afterNodes_;
        TerrainSculptState beforeTerrain_, afterTerrain_;
        int beforeSubdivisions_ = 6;
        bool captured_ = false;
        std::string error_;
    };

}
