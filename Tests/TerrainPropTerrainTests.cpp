#include "renegade/bridge/TerrainPropTerrain.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace renegade::bridge;
namespace
{
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
wi::ecs::Entity AddGrid(wi::scene::Scene& scene,wi::terrain::Terrain& terrain,int cell,float offset)
{
    const auto id=wi::ecs::CreateEntity();
    auto& mesh=scene.meshes.Create(id);
    constexpr int side=wi::terrain::chunk_width;
    for(int z=0;z<side;++z) for(int x=0;x<side;++x)
        mesh.vertex_positions.push_back(XMFLOAT3(float(x),x==1 && z==1 ? 4.f:0.f,float(z)));
    auto& transform=scene.transforms.Create(id);
    transform.Translate(XMFLOAT3(offset,5,0));transform.UpdateTransform();
    wi::terrain::Chunk chunk{};chunk.x=cell;chunk.z=0;
    terrain.chunks[chunk].entity=id;
    return id;
}
}
int main()
{
    try
    {
        wi::scene::Scene scene;
        auto root=wi::ecs::CreateEntity();
        auto& terrain=scene.terrains.Create(root);
        auto first=AddGrid(scene,terrain,0,0);
        TerrainPropTerrainSnapshot old;
        std::string error;
        Check(CaptureTerrainPropTerrain(scene,root,old,error),error.c_str());
        float y=0;
        Check(old.sample(.25f,.25f,y)&&std::abs(y-5)<1e-5f,"LOD0 lower triangle");
        Check(old.sample(.75f,.75f,y)&&std::abs(y-7)<1e-5f,"LOD0 upper triangle");
        Check(!old.sample(-.01f,0,y),"finite west edge");
        Check(old.sample(66,66,y)&&std::abs(y-5)<1e-5f,"inclusive far corner");
        Check(!old.sample(66.01f,66,y),"finite east edge");
        scene.meshes.GetComponent(first)->vertex_positions[68].y=8;
        Check(old.sample(.75f,.75f,y)&&std::abs(y-7)<1e-5f,"worker snapshot owns heights");
        AddGrid(scene,terrain,1,66);
        TerrainPropTerrainSnapshot expanded;
        Check(CaptureTerrainPropTerrain(scene,root,expanded,error),error.c_str());
        Check(!old.sample(80,4,y)&&expanded.sample(80,4,y),"expansion captured deliberately");
        Check(expanded.bounds.maxX==132,"expanded authored bounds");
        auto* transform=scene.transforms.GetComponent(first);
        transform->RotateRollPitchYaw(XMFLOAT3(.2f,0,0));transform->UpdateTransform();
        TerrainPropTerrainSnapshot unchanged=expanded;
        Check(!CaptureTerrainPropTerrain(scene,root,unchanged,error),"tilted terrain rejected");
        Check(unchanged.sample(80,4,y),"failed capture preserves previous output");
        transform->ClearTransform();
        transform->Translate(XMFLOAT3(std::numeric_limits<float>::quiet_NaN(),0,0));
        transform->UpdateTransform();
        Check(!CaptureTerrainPropTerrain(scene,root,unchanged,error),"nonfinite transform rejected");
        std::cout<<"Terrain snapshot triangle, edge, isolation, expansion and failure checks PASS\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
