#include "../EngineBridge/src/PlaceableWaterfallAuthoring.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/PlaceableWaterfallRenderer.h"
#include <filesystem>
#include <iostream>
#include <string>
using namespace renegade::bridge;
using namespace renegade::bridge::placeable_waterfall;
static void Assert(bool yes,const char* msg){
    if(!yes)throw std::runtime_error(msg);
}
static void State(const wi::scene::Scene& s,const WaterfallId& id,
    const Definition& expected){
    auto e=FindWaterfallById(s,id);
    Assert(e!=wi::ecs::INVALID_ENTITY,"Stable waterfall identity not resolved");
    AuthoredWaterfall read;std::string error;
    Assert(ReadWaterfall(s,e,read,error),"Authored waterfall metadata unreadable");
    Assert(read.id==id,"Identity changed");
    Assert(read.settings.crest.x==expected.crest.x,"Crest X changed");
    Assert(read.settings.landing.z==expected.landing.z,"Landing Z changed");
    Assert(read.settings.width==expected.width,"Width changed");
    Assert(read.settings.impactSpray==expected.impactSpray,"Impact Spray changed");
    Assert(read.settings.mist==expected.mist,"Mist changed");
    Assert(read.settings.turbulence==expected.turbulence,"Turbulence changed");
    Assert(s.transforms.Contains(e)&&s.names.Contains(e),"Scene anchor missing");
    Assert(!s.objects.Contains(e)&&!s.emitters.Contains(e),"Derived visuals persisted in authoring anchor");
    Assert(EndpointsMatch(expected,BuildGeometry(read.settings)),"Saved handles lost");
    const auto landing=LandingHandle(s,e);Assert(landing!=wi::ecs::INVALID_ENTITY&&WaterfallOwner(s,landing)==e,"Persisted landing handle lost owner");
    const auto* handle=s.transforms.GetComponent(landing);Assert(std::abs(handle->translation_local.y-(expected.landing.y-expected.crest.y))<.001f,"Landing local transform diverges from authored value");

}
int main(int argc,char** argv){
 try{
    const auto destination=argc>1?std::filesystem::u8path(argv[1]):
        (std::filesystem::temp_directory_path()/"renegade-g13d-authoring-test.wiscene");
    std::filesystem::create_directories(destination.parent_path());
    if(argc>2&&std::string(argv[2])=="--read"){
        wi::Archive disk(destination.generic_string(),true);
        Assert(disk.IsOpen(),"Saved WISCENE archive absent");
        wi::scene::Scene loaded;
        loaded.Serialize(disk);
        std::vector<AuthoredWaterfall> fromDisk;std::string err;
        Assert(EnumerateWaterfalls(loaded,fromDisk,err)&&fromDisk.size()==2,
              "Separate-process WISCENE record enumeration failed");
        bool editedFound=false,originalFound=false;
        for(const auto& record:fromDisk){
            if(record.settings.width==8.25f&&record.settings.impactSpray==2.f
               &&record.settings.landing.z==-2.25f)editedFound=true;
            if(record.settings.width==5.7f&&record.settings.impactSpray==1.35f
               &&record.settings.crest.x==-3.f)originalFound=true;
        }
        Assert(editedFound&&originalFound,"Native file lost waterfall edits");
        std::cout<<"G13D_WATERFALL_SEPARATE_PROCESS_REOPEN_PASS instances=2 "
                   "native_wiscene=1"<<std::endl;
        return 0;
    }
    wi::scene::Scene scene;
    auto transient=scene.Entity_CreateTransform("__renegade_internal_waterfall_test");scene.metadatas.Create(transient).string_values.set("renegade.waterfall.generated.v1","test");
    auto unrelated=scene.Entity_CreateTransform("Unrelated");RemoveGeneratedWaterfallVisuals(scene);Assert(!scene.transforms.Contains(transient)&&scene.transforms.Contains(unrelated),"Generated cleanup damaged scene ownership");
    CommandService commands;
    Definition initial;
    initial.crest={-3,14,7};initial.landing={4,0,8};
    initial.width=5.7f;initial.mist=.8f;initial.impactSpray=1.35f;
    auto create=MakeCreateWaterfallCommand(scene,initial);
    const auto id=create->Id();
    Assert(commands.Execute(std::move(create)),"Create waterfall failed");
    State(scene,id,initial);
    std::vector<AuthoredWaterfall> list;std::string error;
    Assert(EnumerateWaterfalls(scene,list,error)&&list.size()==1,"Waterfall enumeration failed");
    Definition edited=initial;
    edited.crest.x=8.5f;edited.landing.z=-2.25f;
    edited.width=8.25f;edited.impactSpray=2.f;edited.mist=.5f;
    edited.turbulence=1.5f;
    Assert(commands.Execute(MakeEditWaterfallCommand(scene,id,edited)),
           "Editing selected waterfall failed");
    State(scene,id,edited);
    Assert(commands.Undo(),"Undo edit failed");
    State(scene,id,initial);
    Assert(commands.Redo(),"Redo edit failed");
    State(scene,id,edited);
    Definition invalid=edited;invalid.impactSpray=10.f;
    Assert(!commands.Execute(MakeEditWaterfallCommand(scene,id,invalid)),
           "Invalid edit should fail without altering stored state");
    State(scene,id,edited);
    Assert(commands.Execute(MakeRemoveWaterfallCommand(scene,id)),
           "Deleting waterfall failed");
    Assert(FindWaterfallById(scene,id)==wi::ecs::INVALID_ENTITY,"Remove leaves hidden waterfall");
    Assert(commands.Undo(),"Undo delete failed");
    State(scene,id,edited);
    Assert(commands.Redo(),"Redo delete failed");
    Assert(FindWaterfallById(scene,id)==wi::ecs::INVALID_ENTITY,"Redo delete failed");
    Assert(commands.Undo(),"Final restore failed");
    State(scene,id,edited);
    auto another=MakeCreateWaterfallCommand(scene,initial);
    auto anotherId=another->Id();
    Assert(anotherId!=id&&commands.Execute(std::move(another)),"Second independent waterfall failed");
    Assert(EnumerateWaterfalls(scene,list,error)&&list.size()==2,"Multiple waterfalls not enumerated");
    // Both independent records are in ordinary WISCENE metadata and transforms,
    // NOT a separate unsynchronised editor sidecar.
    // Exercise native in-memory archive before writing a real file; no rendering
    // context is needed for the editor's metadata-only transform anchor.
    wi::Archive memory;
    scene.Serialize(memory);
    Assert(memory.SaveFile(destination.generic_string()),"Unable to persist WISCENE archive");
    wi::scene::Scene fresh;
    memory.SetReadModeAndResetPos(true);
    fresh.Serialize(memory);
    // No GPU/device is attached to this headless archive test: authoring
    // metadata is validated before runtime scene hydration.
    State(fresh,id,edited);
    wi::scene::Scene diskScene;
    {wi::Archive disk(destination.generic_string(),true);
       Assert(disk.IsOpen(),"Unable to reopen native WISCENE from disk");
       diskScene.Serialize(disk);}
    State(diskScene,id,edited);
    State(diskScene,anotherId,initial);
    State(fresh,anotherId,initial);
    // The in-memory command history should not be reused for a newly loaded scene.
    CommandService reopenedCommands;
    auto reopenedEdit=edited;reopenedEdit.impactSpray=0;
    Assert(reopenedCommands.Execute(MakeEditWaterfallCommand(fresh,id,reopenedEdit)),
           "Freshly reopened waterfall cannot be edited");
    State(fresh,id,reopenedEdit);
    Assert(reopenedCommands.Undo(),"Fresh-scene Undo failed");
    State(fresh,id,edited);
    // Check unsupported future schema is rejected without corrupting caller.
    auto entity=FindWaterfallById(fresh,id);
    auto* m=fresh.metadatas.GetComponent(entity);
    const auto original=m->string_values.get(AuthoredMetadataKey);
    m->string_values.set(AuthoredMetadataKey,"RENEGADE_WATERFALL_V2 1 2 3");
    AuthoredWaterfall record;
    Assert(!ReadWaterfall(fresh,entity,record,error),"Unknown version silently accepted");
    m->string_values.set(AuthoredMetadataKey,original);
    Assert(EnumerateWaterfalls(fresh,list,error)&&list.size()==2,"Schema rejection corrupted records");
    // No duplicate persistent identifiers or render-derived components.
    wi::ecs::Entity duplicate=wi::ecs::INVALID_ENTITY;
    Assert(!CreateWaterfall(fresh,edited,id,duplicate,error),"Duplicate scene ID allowed");
    std::cout<<"G13D_WATERFALL_AUTHORING_PASS create=1 edit=1 delete=1 undo=1 redo=1 "
      <<"native_scene_save_reopen=1 handles=2 impact_slider=1 instances=2 "
      <<"no_sidecar=1 no_derived_gpu_assets=1"<<std::endl;
    return 0;
 }catch(const std::exception& ex){
    std::cerr<<"G13D_WATERFALL_AUTHORING_FAIL "<<ex.what()<<std::endl;
    return 1;
 }
}
