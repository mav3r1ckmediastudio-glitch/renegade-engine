#include "renegade/bridge/RiverFoamService.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace renegade::bridge;
void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main()
{
    try{
        wi::scene::Scene scene;const auto e=scene.Entity_CreateMaterial("Foam Test Water");
        scene.materials.GetComponent(e)->shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
        scene.metadatas.Create(e).string_values.set("unrelated","retained");
        const auto colour=scene.materials.GetComponent(e)->baseColor;
        std::string error;std::vector<RiverFoamRegion> got;
        Check(ReadRiverFoamRegions(scene,e,got,error)&&got.empty(),"absent foam defaults");
        RiverFoamRegion p;p.centre={12,24,-8};p.headingDegrees=90;
        CommandService history;
        Check(history.Execute(std::make_unique<SetRiverFoamRegionsCommand>(scene,e,std::vector<RiverFoamRegion>{p})),"create foam");
        const auto encoded=scene.metadatas.GetComponent(e)->string_values.get(RiverFoamMetadataKey);
        Check(history.Undo(),"undo creation");
        Check(!scene.metadatas.GetComponent(e)->string_values.has(RiverFoamMetadataKey),"undo leaves no foam metadata");
        Check(scene.metadatas.GetComponent(e)->string_values.get("unrelated")=="retained","undo preserves unrelated metadata");
        Check(history.Redo(),"redo creation");
        Check(scene.metadatas.GetComponent(e)->string_values.get(RiverFoamMetadataKey)==encoded,"redo exact");
        p.headingDegrees=-45;p.strength=.3f;
        Check(history.Execute(std::make_unique<SetRiverFoamRegionsCommand>(scene,e,std::vector<RiverFoamRegion>{p,p})),"two regions");
        Check(ReadRiverFoamRegions(scene,e,got,error)&&got.size()==2&&got[1].headingDegrees==-45,"read authored values");
        Check(history.Undo()&&history.Redo(),"edit history");
        auto packed=PackRiverFoamRegions(got);
        Check(packed.size()==8&&packed[0].x==12&&packed[0].y==24&&packed[1].x<-.7f,"GPU packing position and rotated flow");
        p.speed=std::numeric_limits<float>::quiet_NaN();
        Check(!history.Execute(std::make_unique<SetRiverFoamRegionsCommand>(scene,e,std::vector<RiverFoamRegion>{p})),"reject NaN");
        Check(!history.Execute(std::make_unique<SetRiverFoamRegionsCommand>(scene,e,std::vector<RiverFoamRegion>(17))),"reject overflow");
        const auto other=scene.Entity_CreateMaterial("Opaque");
        Check(!history.Execute(std::make_unique<SetRiverFoamRegionsCommand>(scene,other,std::vector<RiverFoamRegion>{})),"reject opaque");
        wi::Archive archive;wi::ecs::EntitySerializer serializer;scene.Serialize(archive);
        archive.SetReadModeAndResetPos(true);
        wi::scene::Scene reopened;reopened.Serialize(archive);
        const auto loaded=reopened.Entity_FindByName("Foam Test Water");
        Check(ReadRiverFoamRegions(reopened,loaded,got,error)&&got.size()==2&&got[0].strength==.3f,"WISCENE persistence");
        const auto* m=reopened.materials.GetComponent(loaded);
        Check(!m->IsCustomShader()&&m->userdata.x==0&&m->baseColor.x==colour.x,"authored foam leaves optics and transient fields unchanged");
        reopened.metadatas.GetComponent(loaded)->string_values.set(RiverFoamMetadataKey,"{broken");
        Check(!ReadRiverFoamRegions(reopened,loaded,got,error)&&got.empty(),"corrupt data fails closed");
        Check(!SetRiverFoamRegionsCommand(reopened,loaded,{}).Execute(),"do not overwrite corrupt existing data");
        std::cout<<"RIVER_FOAM_COMMAND_PERSISTENCE_TESTS=PASS\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
