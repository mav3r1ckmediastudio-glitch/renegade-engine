#pragma once
#include "renegade/bridge/CommandService.h"
#include <memory>
namespace renegade::bridge {
inline constexpr const char* RiverFlowProfileMetadataKey="renegade.river.flow.v1";
struct RiverFlowProfile {
    bool enabled=false,foam=true,reversed=false,rapids=false;
    // Optional native-head emission; old projects default off, no waterfall linkage.
    bool startWhitewater=false;
    float startWhitewaterAmount=.65f;
    // -1 = upstream first point, otherwise a selected authored spline node.
    // Index is bounded and persisted; moving that node moves the foam source.
    int startWhitewaterNodeIndex=-1; // legacy v4 fallback only
    // v5 stable source anchor: survives insertion, deletion elsewhere, and
    // native WISCENE save/reopen. If this node is removed, source is absent.
    std::string startWhitewaterNodeId;
    float rapidIntensity=1;
    float speed=1, intensity=1, lifetime=3.2f, turbulence=1, sourceWidth=.04f, threshold=0;
};
bool ValidateRiverFlowProfile(const RiverFlowProfile&,std::string&);
bool ReadRiverFlowProfile(const wi::scene::Scene&,wi::ecs::Entity,RiverFlowProfile&,std::string&);
// Resolve only nodes actually belonging to this spline. A v5 ID always takes
// priority; never fall back to an index when an anchored node was removed.
wi::ecs::Entity ResolveRiverWhitewaterSourceNode(
    const wi::scene::Scene&,wi::ecs::Entity,const RiverFlowProfile&);
class SetRiverFlowProfileCommand final:public ICommand {
public:
    SetRiverFlowProfileCommand(wi::scene::Scene&,wi::ecs::Entity,RiverFlowProfile);
    bool Execute()override;void Undo()override;
private:
    wi::scene::Scene* scene_;wi::ecs::Entity river_;RiverFlowProfile profile_;
    std::string before_,after_,id_;bool captured_=false,hadKey_=false,hadMetadata_=false;
};
// Native half-width transaction, wrapped with linked terrain recomposition.
std::unique_ptr<ICommand> MakeSetRiverWidthCommand(wi::scene::Scene&,wi::ecs::Entity,float fullWidth);
struct RiverFlowDiagnostics {
    std::uint64_t rebuilds=0;double lastBuildMilliseconds=0;
    double foamMass=0;float maximumDensity=0;std::size_t cells=0;
    float maximumDetail=0;std::uint64_t detailSignature=0;
};
// Per-path, opt-in transient resources. CPU material bindings exist ONLY inside
// a native Update or Render scope; authoring, duplication and save see originals.
class RiverFlowRenderer {
public:
    explicit RiverFlowRenderer(std::string waterwayShaderDirectory="Content/river");~RiverFlowRenderer();
    RiverFlowRenderer(const RiverFlowRenderer&)=delete;
    RiverFlowRenderer& operator=(const RiverFlowRenderer&)=delete;
    void Prepare(wi::scene::Scene&,std::uint64_t revision,float dt,std::uint64_t editRevision=0);
    void Dispatch()const;
    void DrawArrows(wi::ecs::Entity)const;
    void Invalidate();
    void SetSuspended(bool suspended);
    const std::string& Status(wi::ecs::Entity)const;
    std::size_t ActiveCount()const;
    bool Diagnostics(wi::ecs::Entity,RiverFlowDiagnostics&,std::string&,bool readback=false)const;
    class BindingScope {
    public:
        explicit BindingScope(const RiverFlowRenderer&);~BindingScope();
        BindingScope(const BindingScope&)=delete;
    private:const RiverFlowRenderer& owner_;
    };
    BindingScope Bind()const {return BindingScope(*this);}
private:
    struct Impl;std::unique_ptr<Impl> impl_;
    void Apply()const;void Restore()const;
};
}
