#pragma once
#include "RiverFlowFieldService.h"

namespace renegade::bridge
{
    // G7A: explicit, transient endpoint topology only. No renderer/scene edits,
    // persistence, implicit proximity snapping, hydraulic flux or junction mesh.
    struct RiverFlowNetworkReachInput
    {
        std::string id; // Caller-supplied stable identity, never an ECS index.
        wi::ecs::Entity river = wi::ecs::INVALID_ENTITY;
        RiverFlowSettings flow{};
        float firstNativeParameter=0,lastNativeParameter=1; // G7D nonoverlapping logical spans.
    };
    struct RiverFlowNetworkJunctionInput
    {
        std::string id;
        std::vector<std::string> incoming; // One or more downstream ends.
        std::string outgoing; // Exactly one upstream end; splits deferred.
    };
    struct RiverFlowNetworkPort
    {
        XMFLOAT3 centre{}, rightBank{}, leftBank{};
        XMFLOAT2 directionXZ{}; // Flow direction, including reverse.
        float widthMeters = 0;
        float speedMetersPerSecond = 0;
    };
    struct RiverFlowNetworkReach
    {
        std::string id;
        std::shared_ptr<const RiverFlowField> field;
        RiverFlowNetworkPort upstream, downstream;
    };
    struct RiverFlowNetworkJunction
    {
        std::string id;
        std::vector<std::string> incoming; // Canonical sorted stable IDs.
        std::string outgoing;
        XMFLOAT3 anchor{}; // Actual downstream reach upstream native cap centre.
    };
    class RiverFlowNetwork final
    {
    public:
        const std::vector<RiverFlowNetworkReach>& Reaches() const noexcept { return reaches_; }
        const std::vector<RiverFlowNetworkJunction>& Junctions() const noexcept { return junctions_; }
        const std::vector<std::string>& UpstreamFirstOrder() const noexcept { return order_; }
        const RiverFlowNetworkReach* FindReach(const std::string& id) const noexcept;
        std::uint64_t DeterministicHash() const noexcept;
    private:
        std::vector<RiverFlowNetworkReach> reaches_;
        std::vector<RiverFlowNetworkJunction> junctions_;
        std::vector<std::string> order_;
        friend bool BuildRiverFlowNetwork(const wi::scene::Scene&,
            const std::vector<RiverFlowNetworkReachInput>&,
            const std::vector<RiverFlowNetworkJunctionInput>&,
            std::shared_ptr<const RiverFlowNetwork>&, std::string&);
    };
    // Stable read point required. 1..16 logical native WATER reaches, <=15
    // junctions, <=8 incoming per junction; flat native banks within 5 mm of
    // the reach's first cap, joining cap centres within 20 mm XZ / 5 mm Y.
    // Disjoint spans may share a native entity. One connection per port; no cycles, duplicate IDs, zero flow at
    // joins, opposed downstream directions or same-side collinear tributaries.
    // Disconnected reaches allowed explicitly. Output is replaced ONLY on
    // success. Topology-valid does NOT certify watertight/tessellated geometry:
    // native ribbons can overlap at a confluence; renderer adoption is deferred.
    [[nodiscard]] bool BuildRiverFlowNetwork(
        const wi::scene::Scene& scene,
        const std::vector<RiverFlowNetworkReachInput>& reaches,
        const std::vector<RiverFlowNetworkJunctionInput>& junctions,
        std::shared_ptr<const RiverFlowNetwork>& result, std::string& error);
}
