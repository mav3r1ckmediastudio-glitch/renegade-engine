#pragma once
#include "renegade/bridge/RiverJunctionSurfaceService.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
using namespace DirectX;
namespace junction_proof
{
    inline void Check(bool ok,const std::string& why) {if(!ok)throw std::runtime_error(why);}
    inline wi::ecs::Entity Reach(wi::scene::Scene& scene,const char* name,
        const std::vector<XMFLOAT3>& points,float halfWidth=2)
    {
        const auto root=scene.Entity_CreateTransform(name);scene.splines.Create(root);
        scene.materials.Create(root).shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
        for(auto p:points) {
            const auto node=scene.Entity_CreateTransform("Junction native control");
            auto* t=scene.transforms.GetComponent(node);t->translation_local=p;t->SetDirty();t->UpdateTransform();
            scene.Component_Attach(node,root);scene.splines.GetComponent(root)->spline_node_entities.push_back(node);
        }
        auto* s=scene.splines.GetComponent(root);s->width=halfWidth;s->mesh_generation_subdivision=12;s->SetDrawAligned(true);
        return root;
    }
    inline std::vector<renegade::bridge::RiverFlowNetworkReachInput> Inputs(wi::scene::Scene& scene)
    {
        using namespace renegade::bridge;
        RiverFlowSettings reverse;reverse.downstreamReversed=true;
        return {{"west",scene.Entity_FindByName("west"),reverse},{"east",scene.Entity_FindByName("east"),{}},
            {"outlet",scene.Entity_FindByName("outlet"),{}}};
    }
    inline void Create(wi::scene::Scene& scene,float width=2)
    {
        Reach(scene,"west",{{0,0,0},{-7,0,-4},{-15,0,-11},{-25,0,-11}},width);
        Reach(scene,"east",{{-25,0,11},{-15,0,11},{-7,0,4},{0,0,0}},width);
        Reach(scene,"outlet",{{0,0,0},{10,0,0},{20,0,-2},{30,0,-2}},width);
    }
    inline std::shared_ptr<const renegade::bridge::RiverFlowNetwork> Network(wi::scene::Scene& scene)
    {
        using namespace renegade::bridge;
        std::shared_ptr<const RiverFlowNetwork> n;std::string error;
        const bool ok=BuildRiverFlowNetwork(scene,Inputs(scene),{{"join",{"west","east"},"outlet"}},n,error);
        Check(ok,error);return n;
    }
    inline double Cross(XMFLOAT3 a,XMFLOAT3 b,XMFLOAT3 c)
    {return (double(b.x)-a.x)*(c.z-a.z)-(double(b.z)-a.z)*(c.x-a.x);}
    inline bool Inside(XMFLOAT2 p,XMFLOAT3 a,XMFLOAT3 b,XMFLOAT3 c,bool strict=false)
    {
        const double det=Cross(a,b,c);if(std::abs(det)<1e-12)return false;
        const XMFLOAT3 q{p.x,0,p.y};const double u=Cross(q,b,c)/det,v=Cross(a,q,c)/det,w=1-u-v;
        const double epsilon=strict?1e-7:-1e-6;return u>epsilon&&v>epsilon&&w>epsilon;
    }
}
