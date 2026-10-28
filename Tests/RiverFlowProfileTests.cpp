#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/IdentityService.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace renegade::bridge;
static int count=0;
static void Check(bool ok,const char* why){++count;if(!ok)throw std::runtime_error(why);}
int main(){try{
 wi::scene::Scene scene;auto e=scene.Entity_CreateTransform("Profile River");
 scene.splines.Create(e);auto& m=scene.materials.Create(e);m.shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
 std::string error;Check(AssignNewPersistentEntityId(scene,e,error),"identity");
 for(XMFLOAT3 p: {XMFLOAT3{-5,0,0},XMFLOAT3{5,0,0}}){auto n=scene.Entity_CreateTransform("Node");scene.transforms.GetComponent(n)->Translate(p);scene.Component_Attach(n,e);scene.splines.GetComponent(e)->spline_node_entities.push_back(n);}
 scene.splines.GetComponent(e)->width=1;scene.splines.GetComponent(e)->mesh_generation_subdivision=12;
 auto* metadata=scene.metadatas.GetComponent(e);metadata->string_values.set("unrelated","retained");
 const auto id=PersistentEntityId(scene,e);const auto optics=m.baseColor;
 RiverFlowProfile p;Check(ReadRiverFlowProfile(scene,e,p,error)&&!p.enabled,"opt-in defaults");
 p.enabled=true;p.intensity=1.8f;p.turbulence=1.5f;
 p.startWhitewater=true;p.startWhitewaterAmount=1.35f;p.startWhitewaterNodeIndex=1;
 CommandService commands;auto epoch=commands.Revision();Check(commands.Execute(std::make_unique<SetRiverFlowProfileCommand>(scene,e,p)),"enable command");
 auto bytes=scene.metadatas.GetComponent(e)->string_values.get(RiverFlowProfileMetadataKey);
 RiverFlowProfile anchored;
 Check(ReadRiverFlowProfile(scene,e,anchored,error)&&
       IsValidStableId(anchored.startWhitewaterNodeId)&&
       ResolveRiverWhitewaterSourceNode(scene,e,anchored)==
          scene.splines.GetComponent(e)->spline_node_entities[1],
       "v5 command must anchor original Point 2 by persistent ID");
 const auto actualSelected=scene.splines.GetComponent(e)->spline_node_entities[1];
 const auto actualId=PersistentEntityId(scene,actualSelected);
 auto prefix=scene.Entity_CreateTransform("Inserted Before Selected Source");
 Check(AssignNewPersistentEntityId(scene,prefix,error),"insert test node stable identity");
 auto& nodeList=scene.splines.GetComponent(e)->spline_node_entities;
 nodeList.insert(nodeList.begin(),prefix);
 Check(ResolveRiverWhitewaterSourceNode(scene,e,anchored)==actualSelected,
       "Source anchor moved when an earlier node was inserted");
 Check(PersistentEntityId(scene,actualSelected)==actualId,
       "Inserted node corrupted source ID");
 const auto toRemove=std::find(nodeList.begin(),nodeList.end(),actualSelected);
 Check(toRemove!=nodeList.end(),"Test source node not found before deletion");
 nodeList.erase(toRemove);
 Check(ResolveRiverWhitewaterSourceNode(scene,e,anchored)==wi::ecs::INVALID_ENTITY,
       "Deleted selected node silently reassigned source to next index");
 RiverFlowProfile disabled=anchored;disabled.startWhitewater=false;
 SetRiverFlowProfileCommand disableMissing(scene,e,disabled);
 Check(disableMissing.Execute(),"Could not turn whitewater off with missing source node");
 disableMissing.Undo();
 Check(ReadRiverFlowProfile(scene,e,disabled,error)&&disabled.startWhitewater,
       "Missing-source off/Undo did not restore original settings");
 nodeList.insert(nodeList.begin()+2,actualSelected);
 Check(ResolveRiverWhitewaterSourceNode(scene,e,anchored)==actualSelected,
       "Undo deletion failed to restore original source anchor");
 nodeList.erase(nodeList.begin());
 scene.Entity_Remove(prefix);
 Check(ResolveRiverWhitewaterSourceNode(scene,e,anchored)==actualSelected,
       "Undo insertion lost source identity");
 std::cout<<"G13E9_STABLE_SOURCE_INSERT_DELETE_UNDO_PASS"<<std::endl;
 Check(commands.Revision()>epoch,"successful edit epoch");epoch=commands.Revision();
 Check(commands.Undo(),"undo");Check(commands.Revision()>epoch,"undo epoch");Check(!scene.metadatas.GetComponent(e)->string_values.has(RiverFlowProfileMetadataKey),"undo removes absent profile");
 Check(commands.Redo(),"redo");Check(scene.metadatas.GetComponent(e)->string_values.get(RiverFlowProfileMetadataKey)==bytes,"redo exact");
 p.reversed=true;p.speed=2;
 Check(commands.Execute(std::make_unique<SetRiverFlowProfileCommand>(scene,e,p)),"reverse/speed");
 Check(commands.Undo()&&commands.Redo(),"edit undo redo");
 Check(commands.Execute(MakeSetRiverWidthCommand(scene,e,3)),"width transaction");
 Check(scene.splines.GetComponent(e)->width==1.5f,"width changed");
 Check(commands.Undo()&&scene.splines.GetComponent(e)->width==1,"width undo");
 Check(commands.Redo()&&scene.splines.GetComponent(e)->width==1.5f,"width redo");
 Check(!commands.Execute(MakeSetRiverWidthCommand(scene,e,std::numeric_limits<float>::quiet_NaN())),"NaN width refused");
 wi::Archive archive;scene.Serialize(archive);archive.SetReadModeAndResetPos(true);
 wi::scene::Scene loaded;loaded.Serialize(archive);auto river=loaded.Entity_FindByName("Profile River");RiverFlowProfile got;
 Check(ReadRiverFlowProfile(loaded,river,got,error)&&got.enabled&&got.reversed&&got.speed==2&&got.intensity==1.8f&&got.turbulence==1.5f&&got.startWhitewater&&got.startWhitewaterAmount==1.35f&&
       got.startWhitewaterNodeIndex==1 &&
       got.startWhitewaterNodeId==actualId &&
       ResolveRiverWhitewaterSourceNode(loaded,river,got)!=wi::ecs::INVALID_ENTITY,
       "native v5 stable selected-node whitewater persistence");
 Check(PersistentEntityId(loaded,river)==id,"stable UUID roundtrip");
 Check(!loaded.materials.GetComponent(river)->IsCustomShader()&&loaded.materials.GetComponent(river)->userdata.y==0,"no transient bindings");
 Check(loaded.materials.GetComponent(river)->baseColor.x==optics.x,"native optics preserved");
 p.speed=std::numeric_limits<float>::quiet_NaN();Check(!SetRiverFlowProfileCommand(scene,e,p).Execute(),"NaN profile refused");
 for(float value:{-.01f,3.01f}){p={};p.speed=value;Check(!ValidateRiverFlowProfile(p,error),"speed bounds");}
 p={};p.sourceWidth=.001f;Check(!ValidateRiverFlowProfile(p,error),"width bounds");
 p={};p.startWhitewaterAmount=2.1f;Check(!ValidateRiverFlowProfile(p,error),"whitewater bounds");
 p={};p.startWhitewaterNodeIndex=512;Check(!ValidateRiverFlowProfile(p,error),"whitewater source node range");
 p={};p.startWhitewaterNodeIndex=4;
 Check(!SetRiverFlowProfileCommand(scene,e,p).Execute(),"nonexistent source node refused");
 p={};p.startWhitewaterNodeIndex=1;
 Check(ValidateRiverFlowProfile(p,error),"valid selected river node index");
 p={};Check(!p.startWhitewater,"legacy river sources must remain opt-in");
 // Legacy v4 index anchors upgrade to v5 UUID on the next profile edit.
 const std::string legacyV4=R"({"version":4,"enabled":true,"foam":true,"reversed":false,"rapids":false,"rapidIntensity":1.0,"startWhitewater":true,"startWhitewaterAmount":0.65,"startWhitewaterNodeIndex":1,"speed":1.0,"intensity":1.0,"lifetime":3.2,"turbulence":1.0,"sourceWidth":0.04,"threshold":0.0})";
 loaded.metadatas.GetComponent(river)->string_values.set(RiverFlowProfileMetadataKey,legacyV4);
 Check(ReadRiverFlowProfile(loaded,river,got,error)&&
       got.startWhitewaterNodeIndex==1&&got.startWhitewaterNodeId.empty(),
       "v4 legacy index migration decode");
 got.startWhitewaterAmount=.72f;
 Check(SetRiverFlowProfileCommand(loaded,river,got).Execute(),
       "v4 legacy source failed safe v5 ID promotion on edit");
 Check(ReadRiverFlowProfile(loaded,river,got,error)&&
       IsValidStableId(got.startWhitewaterNodeId) &&
       ResolveRiverWhitewaterSourceNode(loaded,river,got)!=wi::ecs::INVALID_ENTITY,
       "v4 legacy edit did not convert to stable v5 node anchor");
 // Existing v1-v3 authored rivers must not unexpectedly acquire upstream
 // whitewater when opened by the v5 reader.
 const std::string legacyV3=R"({"version":3,"enabled":true,"foam":true,"reversed":false,"rapids":false,"rapidIntensity":1.0,"startWhitewater":true,"startWhitewaterAmount":0.65,"speed":1.0,"intensity":1.0,"lifetime":3.2,"turbulence":1.0,"sourceWidth":0.04,"threshold":0.0})";
 loaded.metadatas.GetComponent(river)->string_values.set(RiverFlowProfileMetadataKey,legacyV3);
 Check(ReadRiverFlowProfile(loaded,river,got,error)&&got.startWhitewater&&got.startWhitewaterNodeIndex==-1,
    "v3 auto-upstream source migration");

 const std::string legacyV2=R"({"version":2,"enabled":true,"foam":true,"reversed":false,"rapids":false,"rapidIntensity":1.0,"speed":1.0,"intensity":1.0,"lifetime":3.2,"turbulence":1.0,"sourceWidth":0.04,"threshold":0.0})";
 loaded.metadatas.GetComponent(river)->string_values.set(RiverFlowProfileMetadataKey,legacyV2);
 Check(ReadRiverFlowProfile(loaded,river,got,error)&&got.enabled&&!got.startWhitewater&&got.startWhitewaterAmount==.65f,
   "v2 migrated old river with unrequested start foam");
 const std::string legacyV1=R"({"version":1,"enabled":true,"foam":true,"reversed":false,"speed":1.0,"intensity":1.0,"lifetime":3.2,"turbulence":1.0,"sourceWidth":0.04,"threshold":0.0})";
 loaded.metadatas.GetComponent(river)->string_values.set(RiverFlowProfileMetadataKey,legacyV1);
 Check(ReadRiverFlowProfile(loaded,river,got,error)&&got.enabled&&!got.startWhitewater&&!got.rapids,
   "v1 migrated old river with unrequested foam source");
 loaded.metadatas.GetComponent(river)->string_values.set(RiverFlowProfileMetadataKey,"{broken");
 Check(!ReadRiverFlowProfile(loaded,river,got,error)&&!got.enabled,"malformed fail closed");
 Check(!SetRiverFlowProfileCommand(loaded,river,{}).Execute(),"malformed not overwritten");
 Check(scene.metadatas.GetComponent(e)->string_values.get("unrelated")=="retained","unrelated metadata preserved");
 std::cout<<"G6_PROFILE_COMMAND_PERSISTENCE=PASS assertions="<<count<<"\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<"\n";return 1;}}
