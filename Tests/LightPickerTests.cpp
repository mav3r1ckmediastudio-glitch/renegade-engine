#include "renegade/bridge/LightPresetService.h"
#include "renegade/bridge/ScriptMetadataService.h"
#include "RuntimeScriptRuntime.h"
#include "RuntimeScriptEntityApi.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <cmath>
using namespace renegade;
namespace fs=std::filesystem;
int failures=0;
void Check(bool v,const char* text){if(!v){++failures;std::cerr<<"FAIL: "<<text<<'\n';}}
bool Near(float a,float b){return std::abs(a-b)<.0001f;}
int main() {
 using namespace bridge;
 const auto root=fs::temp_directory_path()/("renegade-light-picker-"+GenerateStableId());
 fs::create_directories(root);const auto project=GenerateStableId();std::string error;
 LightPreset p;p.name="Brazier Fire";p.light=MakeNewLightState(wi::scene::LightComponent::POINT);
 p.light.color={1,.7f,.3f};p.light.intensity=24;p.light.castShadow=true;p.light.range=8;
 p.behaviour=LightBehaviour::Flame;
 Check(SaveLightPreset(root.string(),project,p,true,error),"save complete project light preset");
 auto presets=ListLightPresets(root.string(),project,error);
 Check(presets.size()==1&&presets[0].name==p.name&&!HasLightStateChange(p.light,presets[0].light)&&presets[0].behaviour==p.behaviour,"cold read preserves settings");
 p.speed=2.8f;p.movement=.1f;Check(SaveLightPreset(root.string(),project,p,false,error),"explicit update");
 presets=ListLightPresets(root.string(),project,error);Check(presets.size()==1&&Near(presets[0].speed,2.8f),"update retains preset identity");
 Check(ListLightPresets(root.string(),GenerateStableId(),error).empty(),"project identity is checked");
 Check(SaveLightColours(root.string(),{{1,.5f,.2f}},error),"save colour");
 Check(LoadLightColours(root.string()).size()==1,"cold read saved colour");
 Check(EnsureLightBehaviourSource(root.string(),error),"adopt stock script");
 auto meta=EvaluateScriptMetadata(root.string(),LightBehaviourSourcePath);
 if(!meta.succeeded)for(auto& d:meta.diagnostics)std::cerr<<d.message<<'\n';
 Check(meta.succeeded&&meta.descriptor.properties.size()==5,"stock metadata evaluates");
 wi::scene::Scene scene;
 auto doc=CreateScriptDocument(project,GenerateStableId(),"Content/Scenes/LightQA.wiscene","test");
 LightSetupCommand create(scene,&doc,p,wi::ecs::INVALID_ENTITY,{2,3,4});
 Check(create.Execute(),"configured light creation");auto entity=create.Entity();auto id=PersistentEntityId(scene,entity);
 Check(scene.names.GetComponent(entity)->name=="Brazier Fire"&&doc.attachments.size()==1,"light and behaviour one command");
 Check(ValidateScriptDocumentAgainstScene(doc,scene,error),"attachment resolves to light");
 Check(CaptureLightPreset(scene,entity,&doc).id==p.id,"instance remembers preset for explicit update");
 create.Undo();Check(scene.lights.GetCount()==0&&doc.attachments.empty(),"undo removes light and behaviour");
 Check(create.Execute()&&PersistentEntityId(scene,create.Entity())==id&&doc.attachments.size()==1,"redo retains stable identity");
 LightSetupCommand second(scene,&doc,p);Check(second.Execute(),"second light");
 Check(scene.names.GetComponent(second.Entity())->name=="Brazier Fire 2","clean repeated-placement names");
 second.Undo();Check(create.Entity()==entity,"first light retained");
 DuplicateEntityCommand duplicate(scene,entity);
 Check(duplicate.Execute(),"native duplicate");
 Check(scene.names.GetComponent(duplicate.DuplicatedEntity())->name=="Brazier Fire 2","native duplicate uses numbered name");
 DuplicateEntityCommand next(scene,duplicate.DuplicatedEntity());
 Check(next.Execute()&&scene.names.GetComponent(next.DuplicatedEntity())->name=="Brazier Fire 3","duplicate of duplicate avoids Copy chains");
 next.Undo();duplicate.Undo();
 // Explicit script round-trip and native scene round-trip.
 auto scriptsPath=root/"LightQA.wiscene.rscripts";
 Check(WriteScriptDocument(scriptsPath.string(),doc,error),"write scripting companion");
 ScriptDocument loaded;Check(ReadScriptDocument(scriptsPath.string(),project,doc.sceneDocumentId,loaded,error),"cold read scripting companion");
 {wi::Archive a((root/"LightQA.wiscene").string(),false);scene.Serialize(a);}
 wi::scene::Scene reopened;{wi::Archive a((root/"LightQA.wiscene").string(),true);reopened.Serialize(a);}
 Check(ValidateScriptDocumentAgainstScene(loaded,reopened,error),"reopened attachment matches native entity");
 Check(CaptureLightPreset(scene,entity,&loaded).behaviour==LightBehaviour::Flame,"capture behaviour for edit picker");
 // Each actual Lua mode runs against a real light in governed Lua.
 for(int mode=1;mode<=5;++mode) {
  auto setup=p;setup.behaviour=static_cast<LightBehaviour>(mode);setup.strength=1;
  LightSetupCommand edit(scene,&doc,setup,entity);Check(edit.Execute(),"change light behaviour");
  Check(doc.attachments.size()==1,"behaviour switching replaces stock script");
  runtime::RuntimeScriptRuntime lua;
  Check(lua.StartScene(doc,scene,root.string(),error),"start governed light script");
  const auto baseline=scene.lights.GetComponent(entity)->intensity;
  const auto position=scene.transforms.GetComponent(entity)->translation_local;
  const auto rotation=scene.transforms.GetComponent(entity)->rotation_local;
  bool changed=false;
  for(int frame=0;frame<120;++frame) {
   lua.Update(1.0f/60);
   auto* l=scene.lights.GetComponent(entity);auto* t=scene.transforms.GetComponent(entity);
   changed|=!Near(l->intensity,baseline)||!Near(t->rotation_local.y,rotation.y);
  }
  for(const auto& d:lua.Diagnostics())std::cerr<<d.sourcePath<<": "<<d.message<<'\n';
  Check(lua.DisabledInstanceCount()==0&&changed,"live Lua behaviour animates");
  const float paused=scene.lights.GetComponent(entity)->intensity;
  lua.Pause();lua.Update(.5f);Check(Near(paused,scene.lights.GetComponent(entity)->intensity),"pause freezes behaviour");
  lua.Resume();lua.StopScene();
  Check(Near(baseline,scene.lights.GetComponent(entity)->intensity)&&Near(position.x,scene.transforms.GetComponent(entity)->translation_local.x),"stop restores authored light");
 }
 LightPreset constant=p;constant.behaviour=LightBehaviour::Constant;
 LightSetupCommand steady(scene,&doc,constant,entity);
 Check(steady.Execute()&&doc.attachments.empty(),"constant removes stock behaviour");
 steady.Undo();Check(doc.attachments.size()==1,"undo constant restores prior behaviour");
 auto remove=MakeDeleteLightSetupCommand(scene,&doc,entity);
 Check(remove->Execute()&&doc.attachments.empty()&&!scene.lights.Contains(entity),"delete removes behaviour with light");
 remove->Undo();Check(scene.lights.Contains(entity)&&doc.attachments.size()==1,"undo delete restores behaviour");
 // Generation safety and rejected invalid writes must preserve the source.
 std::unordered_map<StableId,wi::ecs::Entity> ids{{id,entity}};
 runtime::RuntimeScriptEntityApi api(scene,ids,4);runtime::RuntimeLightAnimationState state;
 Check(api.GetLightState({id,4},state,error),"read governed light");
 auto invalid=state;invalid.intensity=std::numeric_limits<float>::quiet_NaN();
 Check(!api.SetLightState({id,4},invalid,error),"reject NaN");
 Check(!api.SetLightState({id,3},state,error),"reject stale generation");
 Check(Near(state.intensity,scene.lights.GetComponent(entity)->intensity),"rejected writes preserve light");
 fs::remove_all(root);
 std::cout<<"Light picker checks: "<<failures<<" failures\n";return failures?1:0;
}
