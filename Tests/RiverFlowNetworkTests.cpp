#include "renegade/bridge/RiverFlowNetworkService.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <set>
using namespace renegade::bridge;
using namespace DirectX;

namespace
{
    int checks = 0;
    void Check(bool ok, const std::string& message)
    {
        ++checks; if (!ok) throw std::runtime_error(message);
    }
    wi::ecs::Entity River(wi::scene::Scene& scene, XMFLOAT3 a, XMFLOAT3 b)
    {
        const auto root = scene.Entity_CreateTransform("Network native reach");
        scene.splines.Create(root);
        scene.materials.Create(root).shaderType = wi::scene::MaterialComponent::SHADERTYPE_WATER;
        for (const auto p : {a,b}) {
            const auto node = scene.Entity_CreateTransform("Network control");
            auto* t = scene.transforms.GetComponent(node);
            t->translation_local = p; t->SetDirty(); t->UpdateTransform();
            scene.Component_Attach(node, root);
            scene.splines.GetComponent(root)->spline_node_entities.push_back(node);
        }
        auto* s = scene.splines.GetComponent(root);
        s->width = 1; s->mesh_generation_subdivision = 16; s->SetDrawAligned(true);
        return root;
    }
    struct Fixture
    {
        wi::scene::Scene scene;
        std::vector<RiverFlowNetworkReachInput> inputs;
        std::vector<RiverFlowNetworkJunctionInput> joins{{"confluence", {"west","east"}, "outlet"}};
        Fixture()
        {
            inputs = {{"west", River(scene, {-20,3,-10}, {0,3,0}), {}},
                {"east", River(scene, {-20,3,10}, {0,3,0}), {}},
                {"outlet", River(scene, {0,3,0}, {25,3,0}), {}}};
        }
    };
    std::shared_ptr<const RiverFlowNetwork> Build(Fixture& f)
    {
        std::shared_ptr<const RiverFlowNetwork> n; std::string error;
        Check(BuildRiverFlowNetwork(f.scene, f.inputs, f.joins, n, error), error);
        Check(n && error.empty(), "Success contract");
        return n;
    }
    void Refuse(Fixture& f, const std::shared_ptr<const RiverFlowNetwork>& good, const char* expected)
    {
        auto output = good; std::string error;
        Check(!BuildRiverFlowNetwork(f.scene, f.inputs, f.joins, output, error),
            std::string("Expected refusal: ")+expected);
        Check(output == good, "Failure replaced immutable prior output");
        Check(error.find(expected) != std::string::npos, "Unexpected reason: "+error);
        std::cout << "REFUSED=" << error << "\n";
    }
    void Move(Fixture& f, std::size_t reach, std::size_t node, XMFLOAT3 p)
    {
        auto* t = f.scene.transforms.GetComponent(
            f.scene.splines.GetComponent(f.inputs[reach].river)->spline_node_entities[node]);
        t->translation_local = p; t->SetDirty(); t->UpdateTransform();
    }
}
int main()
{
    try {
        Check(wi::graphics::GetDevice() == nullptr, "CPU graph tests must not create graphics");
        Fixture f;
        const auto materialCount = f.scene.materials.GetCount();
        const auto good = Build(f); const auto hash = good->DeterministicHash();
        Check(good->Reaches().size()==3 && good->Junctions().size()==1, "Confluence topology");
        Check(good->UpstreamFirstOrder()==std::vector<std::string>({"east","west","outlet"}), "Canonical DAG order");
        Check(good->FindReach("missing")==nullptr, "Unknown reach lookup");
        const auto* west = good->FindReach("west");
        Check(west && std::hypot(west->downstream.centre.x, west->downstream.centre.z)<1e-5f, "Endpoint anchored");
        Check(west->downstream.directionXZ.x>.8f && west->downstream.directionXZ.y>0, "Incoming forward flow");
        Check(std::abs(west->downstream.widthMeters-2)<1e-4f, "Actual native width");
        Check(f.scene.meshes.GetCount()==0 && f.scene.materials.GetCount()==materialCount, "CPU graph does not generate scene geometry");
        std::reverse(f.inputs.begin(), f.inputs.end());
        std::reverse(f.joins[0].incoming.begin(), f.joins[0].incoming.end());
        Check(Build(f)->DeterministicHash()==hash, "Input order independent hash");
        Check(Build(f)->DeterministicHash()==hash, "Repeatability");

        { Fixture x; x.joins.clear(); auto n=Build(x); Check(n->UpstreamFirstOrder().size()==3, "Explicit isolated reaches"); Check(n->DeterministicHash()!=hash, "Topology affects hash"); }
        { Fixture x; x.inputs[0].id=""; Refuse(x,good,"identity"); }
        { Fixture x; x.inputs[0].id="bad id"; Refuse(x,good,"identity"); }
        { Fixture x; x.inputs[0].id=std::string(129,'a'); Refuse(x,good,"identity"); }
        { Fixture x; x.inputs[0].id="east"; Refuse(x,good,"Duplicate reach"); }
        { Fixture x; x.inputs[0].river=x.inputs[1].river; Refuse(x,good,"duplicate native"); }
        { Fixture x; x.inputs[0].river=wi::ecs::INVALID_ENTITY; Refuse(x,good,"native river"); }
        { Fixture x; x.inputs.clear(); Refuse(x,good,"1..16"); }
        { Fixture x; x.inputs.resize(17); Refuse(x,good,"1..16"); }
        { Fixture x; x.joins.resize(16); Refuse(x,good,"limit"); }
        { Fixture x; x.joins[0].id=""; Refuse(x,good,"junction identity"); }
        { Fixture x; x.joins.push_back(x.joins[0]); Refuse(x,good,"junction identity"); }
        { Fixture x; x.joins[0].incoming.clear(); Refuse(x,good,"1..8"); }
        { Fixture x; x.joins[0].incoming.resize(9,"west"); Refuse(x,good,"1..8"); }
        { Fixture x; x.joins[0].outgoing="unknown"; Refuse(x,good,"unknown outgoing"); }
        { Fixture x; x.joins[0].incoming={"unknown"}; Refuse(x,good,"unknown incoming"); }
        { Fixture x; x.joins[0].incoming={"west","west"}; Refuse(x,good,"duplicate incoming"); }
        { Fixture x; x.joins[0].incoming={"outlet"}; Refuse(x,good,"self connection"); }
        { Fixture x; x.joins.push_back({"second",{"west"},"east"}); Refuse(x,good,"downstream port"); }
        { Fixture x; x.joins.push_back({"second",{"outlet"},"outlet"}); Refuse(x,good,"upstream port"); }
        { Fixture x; x.joins.push_back({"return",{"outlet"},"west"}); Refuse(x,good,"cycle"); }
        { Fixture x; Move(x,0,0,{-20,3.02f,-10}); Move(x,0,1,{0,3.02f,0}); Refuse(x,good,"water levels"); }
        { Fixture x; Move(x,0,1,{.1f,3,0}); Refuse(x,good,"cap centres"); }
        { Fixture x; Move(x,0,0,{-20,4,-10}); Refuse(x,good,"non-flat"); }
        { Fixture x; x.inputs[0].flow.downstreamReversed=true; Refuse(x,good,"opposes"); }
        { Fixture x; x.inputs[2].flow.downstreamReversed=true; Refuse(x,good,"opposes"); }
        { Fixture x; x.inputs[0].flow.baseSpeedMetersPerSecond=0; Refuse(x,good,"zero-speed"); }
        { Fixture x; x.inputs[2].flow.baseSpeedMetersPerSecond=0; Refuse(x,good,"zero-speed"); }
        { Fixture x; x.inputs[0].flow.baseSpeedMetersPerSecond=std::numeric_limits<float>::quiet_NaN(); Refuse(x,good,"Reach west:"); }
        { Fixture x; Move(x,0,0,{-20,3,10}); Refuse(x,good,"parallel incoming"); }
        { Fixture x; Move(x,0,0,{0,3,-20}); Refuse(x,good,"perpendicular"); }
        // Reversed authoring must use the actual opposite cap, not just negate a vector.
        { Fixture x; Move(x,0,0,{0,3,0}); Move(x,0,1,{-20,3,-10});
          x.inputs[0].flow.downstreamReversed=true;
          auto n=Build(x); Check(n->FindReach("west")->downstream.directionXZ.x>.8f, "Reverse cap semantics");
          Check(n->DeterministicHash()!=hash, "Reversed authored field identity"); }
        // Common parent world transform must be respected by all endpoint ports.
        { Fixture x; const auto parent=x.scene.Entity_CreateTransform("World parent");
          for (auto& input:x.inputs) x.scene.Component_Attach(input.river,parent);
          auto* t=x.scene.transforms.GetComponent(parent);
          t->translation_local={40,5,-30}; t->scale_local={1.5f,1,.8f};
          XMStoreFloat4(&t->rotation_local,XMQuaternionRotationRollPitchYaw(0,.7f,0)); t->SetDirty(); t->UpdateTransform();
          auto n=Build(x); auto p=n->Junctions()[0].anchor;
          Check(std::abs(p.x-40)<1e-4f && std::abs(p.y-8)<1e-4f && std::abs(p.z+30)<1e-4f,"World-space junction anchor"); }
        // A chain of confluences and continuations has a deterministic complete order.
        { Fixture x; auto r=River(x.scene,{25,3,0},{50,3,0}); x.inputs.push_back({"tail",r,{}});
          x.joins.push_back({"next",{"outlet"},"tail"}); auto n=Build(x);
          Check(n->UpstreamFirstOrder().back()=="tail", "Multi-junction graph order");
          auto p=std::find(n->UpstreamFirstOrder().begin(),n->UpstreamFirstOrder().end(),"outlet");
          Check(p+1==n->UpstreamFirstOrder().end()-1,"Outlet precedes tail"); }
        // Full cap and fan-in budget is accepted, rather than merely checking refusal.
        { Fixture x; x.inputs.clear(); x.joins.clear();
          for(int n=0;n<16;++n) x.inputs.push_back({"r"+std::to_string(n),River(x.scene,{float(n*10),3,0},{float((n+1)*10),3,0}),{}});
          for(int n=0;n<15;++n) x.joins.push_back({"j"+std::to_string(n),{"r"+std::to_string(n)},"r"+std::to_string(n+1)});
          auto n=Build(x); Check(n->Reaches().size()==16 && n->Junctions().size()==15,"Maximum chain accepted"); }
        { Fixture x; Move(x,0,1,{.01f,3,0}); auto n=Build(x);
          Check(std::abs(n->FindReach("west")->downstream.centre.x-.01f)<1e-4f,"Tolerance must not snap authored cap");
          Check(std::abs(n->Junctions()[0].anchor.x)<1e-4f,"Anchor is actual outlet cap"); }
        { Fixture x; Move(x,0,0,{-20,3.002f,-10}); Move(x,0,1,{0,3.002f,0});
          Check(bool(Build(x)),"Sub-tolerance flat level accepted"); }
        { Fixture x; x.inputs.clear(); x.joins={ {"fan",{},"out"} };
          for(int k=0;k<8;++k) {
            const float angle=-.8f+float(k)*1.6f/7;
            const auto id="in"+std::to_string(k);
            x.inputs.push_back({id,River(x.scene,{-20*std::cos(angle),3,-20*std::sin(angle)},{0,3,0}),{}});
            x.joins[0].incoming.push_back(id);
          }
          x.inputs.push_back({"out",River(x.scene,{0,3,0},{20,3,0}),{}});
          auto n=Build(x);Check(n->Junctions()[0].incoming.size()==8,"Maximum fan-in accepted"); }
        Check(good->DeterministicHash()==hash, "Immutable previous graph after failures");
        std::cout << "RIVER_FLOW_NETWORK_CPU=PASS checks=" << checks << " hash=" << hash << "\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << "\n"; return 1; }
}
