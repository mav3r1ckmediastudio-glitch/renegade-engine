#pragma once
#include "RiverFlowNetworkService.h"
namespace renegade::bridge
{
    struct RiverJunctionQuery
    {
        XMFLOAT2 velocityXZ{};
        bool valid = false;
    };
    // One flat native-footprint union, NOT a mesh over existing water.
    // Positions/velocity are world space. Triangles face +Y; shared boundary
    // vertices are welded. No graphics handles, authored state or scene refs.
    class RiverJunctionSurface final
    {
    public:
        const std::vector<XMFLOAT3>& Positions() const noexcept { return positions_; }
        const std::vector<XMFLOAT2>& Velocities() const noexcept { return velocities_; }
        const std::vector<std::uint32_t>& Indices() const noexcept { return indices_; }
        RiverJunctionQuery Evaluate(XMFLOAT2 worldXZ) const noexcept;
        double AreaSquareMetres() const noexcept;
    private:
        std::vector<XMFLOAT3> positions_;
        std::vector<XMFLOAT2> velocities_;
        std::vector<std::uint32_t> indices_;
        friend bool BuildRiverJunctionSurface(const RiverFlowNetwork&,
            std::shared_ptr<const RiverJunctionSurface>&, std::string&);
    };
    // G7B diagnostic scope: exactly two tributaries / one outlet, one junction.
    // Exact flat plane/cap join within 0.1mm, <=1024 boundary edges, <=4096
    // sweep events, <=65536 vertices/triangles, <=16M bounded work operations.
    // Does not bridge gaps or flatten mismatched water. Original native boundary
    // is retained within a 10-micrometre weld tolerance. Conservative refusal
    // on degenerate or non-manifold output. Output unchanged on failure.
    // Velocity is a bounded kinematic blend, not discharge conservation.
    [[nodiscard]] bool BuildRiverJunctionSurface(const RiverFlowNetwork&,
        std::shared_ptr<const RiverJunctionSurface>& result, std::string& error);
}
