#include "renegade/bridge/RiverUnderwaterService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/IdentityService.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace renegade::bridge;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()
{
    try
    {
        RiverWaterPrism p;
        Require(MakeRiverWaterPrism({0,2,0},{4,2,0},{0,2,4},2,{.1f,.3f,.4f,.6f},{.8f,.3f,.1f},p),"flat volume creation");
        float enter,exit;
        Require(IntersectRiverWaterPrism(p,{1,1,1},{0,1,0},20,enter,exit)&&std::abs(exit-1)<.0001f,"upward ray exits actual surface");
        Require(IntersectRiverWaterPrism(p,{1,1,1},{1,0,0},20,enter,exit)&&std::abs(exit-2)<.0001f,"ray exits triangular bank");
        Require(IntersectRiverWaterPrism(p,{1,1,1},{0,-1,0},20,enter,exit)&&std::abs(exit-1)<.0001f,"ray exits authored bottom");
        Require(!IntersectRiverWaterPrism(p,{5,1,1},{0,1,0},20,enter,exit),"dry bank excluded");
        Require(!IntersectRiverWaterPrism(p,{1,-1,1},{1,0,0},20,enter,exit),"below-bed excluded");
        Require(IntersectRiverWaterPrism(p,{1,1,1},{1,0,0},.25f,enter,exit)&&exit==.25f,"opaque geometry limits optical distance");
        Require(MakeRiverWaterPrism({0,2,0},{4,3,0},{0,2,4},1,{.1f,.3f,.4f,.6f},{.8f,.3f,.1f},p),"sloped volume creation");
        Require(IntersectRiverWaterPrism(p,{1,2,1},{0,1,0},20,enter,exit)&&std::abs(exit-.25f)<.0001f,"sloped local surface");
        Require(!MakeRiverWaterPrism({0,0,0},{0,4,0},{0,0,4},1,{1,1,1,1},{1,1,1},p),"vertical falling sheet excluded");
        Require(!MakeRiverWaterPrism({0,0,0},{0,0,0},{0,0,4},1,{1,1,1,1},{1,1,1},p),"degenerate triangle rejected");
        wi::scene::Scene scene;
        auto river=scene.Entity_CreateTransform("Underwater test river");
        const auto id=GenerateStableId();scene.metadatas.Create(river).string_values.set(PersistentEntityIdMetadataKey,id);
        scene.splines.Create(river);
        scene.materials.Create(river).shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
        auto& mesh=scene.meshes.Create(river);mesh.vertex_positions={{0,2,0},{4,2,0},{0,2,4}};mesh.indices={0,1,2};
        scene.objects.Create(river).meshID=river;
        auto terrain=scene.Entity_CreateTransform("Linked terrain");scene.terrains.Create(terrain);
        auto setDepth=[&](const char* depth){scene.metadatas.Create(terrain).string_values.set(RiverTerrainLayersMetadataKey,
            std::string("{\"chunks\":[],\"layers\":[{\"bank\":2,\"depth\":")+depth+",\"river\":\""+id+"\"}],\"version\":1}");};
        Require(CaptureRiverWaterPrisms(scene).empty(),"unlinked water has no invented depth");
        setDepth("0.499");Require(CaptureRiverWaterPrisms(scene).empty(),"shallow stream cutoff");
        setDepth("0.5");Require(CaptureRiverWaterPrisms(scene).size()==1,"half metre river eligible");
        setDepth("2");auto captured=CaptureRiverWaterPrisms(scene);Require(captured.size()==1&&std::abs(captured[0].minimum.y)<.001f,"linked depth drives lower bound");
        scene.objects.GetComponent(river)->SetRenderable(false);Require(CaptureRiverWaterPrisms(scene).empty(),"hidden water excluded");scene.objects.GetComponent(river)->SetRenderable(true);
        // Native mesh GPU recreation belongs to the graphics proof, not a no-device unit test.
        scene.objects.Remove(river);scene.meshes.Remove(river);
        wi::Archive saved;scene.Serialize(saved);saved.SetReadModeAndResetPos(true);wi::scene::Scene reopened;reopened.Serialize(saved);
        const auto reopenedRiver=reopened.Entity_FindByName("Underwater test river");
        Require(PersistentEntityId(reopened,reopenedRiver)==id,"scene remap retains stable river identity");
        const auto reopenedTerrain=reopened.Entity_FindByName("Linked terrain");
        Require(reopened.metadatas.GetComponent(reopenedTerrain)->string_values.get(RiverTerrainLayersMetadataKey).find(id)!=std::string::npos,"scene reload retains channel depth link");
        std::cout<<"PASS: finite river volumes, local slopes, boundaries, cutoff, visibility, serialization\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
