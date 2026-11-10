#include "RuntimeScriptRuntime.h"
#include "renegade/bridge/ParticleEffectRuntimeService.h"
#include "renegade/bridge/ParticleEffectRuntimeDependencies.h"
#include "renegade/bridge/ParticleTextureLibraryService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace renegade::bridge;
using namespace renegade::runtime;
namespace fs=std::filesystem;
int failures=0;
void Check(bool ok,const std::string& msg){if(!ok){++failures;std::cerr<<"FAIL "<<msg<<'\n';}}
std::string Read(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void Write(const fs::path& p,const std::string& text){fs::create_directories(p.parent_path());std::ofstream(p,std::ios::binary)<<text;}
int main(int argc,char** argv){
 auto folder=fs::temp_directory_path()/("particle-lua-"+GenerateStableId());fs::create_directories(folder);
 auto project=GenerateStableId();std::string error;
 ParticleEffect effect;effect.name="Lua test effect";
 for(auto name:{"Sparks","Smoke","Candle"}){ParticleEffectLayer l;l.id=GenerateStableId();l.name=name;l.emitter=MakeNewParticleEmitterState();l.emitter.maxParticles=100;l.emitter.emitCount=12;l.emitter.size=.2f;l.emitter.life=2;if(l.name=="Smoke")l.enabled=false;if(l.name=="Candle")l.playback=ParticlePlayback::StationaryLoop;effect.layers.push_back(l);}
 ParticleMovementField field;field.id=GenerateStableId();field.name="Swirl";effect.fields.push_back(field);
 Check(SaveParticleEffect(folder.string(),project,effect,true,error),"save fixture: "+error);
 auto presetPath=folder/"Content/Particles"/(effect.id+".rparticle");const auto original=Read(presetPath);
 wi::scene::Scene scene;auto owner=scene.Entity_CreateTransform("Grinder");Check(AssignNewPersistentEntityId(scene,owner,error),"owner identity");
 auto socket=scene.Entity_CreateTransform("cut");Check(AssignNewPersistentEntityId(scene,socket,error),"socket identity");scene.Component_Attach(socket,owner,true);
 auto ownerId=PersistentEntityId(scene,owner);
 const char* source=R"LUA(
local p=renegade.particles
local first
return {
 on_start=function(self)
  assert(p.contract_version==1 and #p.presets()==1)
  local presets=p.presets()
  assert(presets[1].name=="Lua test effect")
  first=assert(p.spawn(presets[1].id,{x=1,y=2,z=3}))
  assert(renegade.entity.is_valid(first))
  assert(tostring(first)=="EntityRef(valid)")
  local second=assert(p.spawn("Lua test effect"))
  assert(#p.layers(first)==3 and #p.fields(first)==1)
  assert(p.set_layer(first,"Sparks",{emit_rate=0,size=.25,opacity=.65,field_1_weight=.3,size_variation=.4,velocity_y=2}))
  assert(math.abs(p.get_layer(first,"Sparks").opacity-.65)<.0001)
  assert(p.get_layer(second,1).emit_rate==12)
  assert(p.set_layer(first,1,{size=.0001225}))
  assert(math.abs(p.get_layer(first,1).size-.0001225)<.00000001)
  assert(p.set_layer(first,1,{size=.25}))
  assert(p.set_layer(first,1,{shape_type=3,shape_radius=.4,shape_angle=35,normal_factor=2}))
  assert(p.get_layer(first,1).shape_type==3 and p.get_layer(second,1).shape_type==0)
  assert(not p.set_layer(first,1,{shape_type=7}))
  assert(not p.set_layer(first,1,{shape_radius=-1}))
  assert(not p.get_timing(first).enabled)
  assert(p.set_timing(first,{enabled=true,loop=false,duration=2,delay=.5,burst=12}))
  assert(p.get_timing(first).burst==12 and not p.get_timing(second).enabled)
  assert(not p.set_timing(first,{duration=0,burst=3}))
  assert(p.get_timing(first).burst==12)
  assert(not p.set_timing(first,{enabled=1}))
  assert(not p.set_timing(first,{burst=1.5}))
  assert(not p.set_timing(first,{delay=0/0}))
  assert(not p.set_timing(first,{typo=true}))
  assert(p.set_timing(first,{enabled=false}))
  assert(not p.get_lifetime(first,"Sparks").enabled)
  assert(p.set_lifetime(first,"Sparks",{enabled=true,keys={{time=0,size=.2,red=1,green=0,blue=0,opacity=0},{time=.4,size=2,red=0,green=1,blue=0,opacity=1},{time=1,size=0,red=0,green=0,blue=1,opacity=0}}}))
  assert(p.get_lifetime(first,1).keys[2].size==2)
  assert(not p.get_lifetime(second,1).enabled)
  assert(not p.set_lifetime(first,1,{keys={{time=0},{time=0}}}))
  assert(not p.set_lifetime(first,1,{keys={{time=0},{time=1,size=-1}}}))
  assert(not p.set_lifetime(first,1,{keys={{time=0},{time=1,typo=1}}}))
  assert(not p.set_lifetime(first,1,{enabled=1}))
  assert(not p.set_lifetime(first,1,{keys={[1]={time=0},[3]={time=1}}}))
  assert(not p.set_lifetime(first,1,{keys={{time=0},{time=1,red=0/0}}}))
  assert(not p.set_lifetime({},1,{enabled=true}))
  assert(p.get_lifetime(first,1).keys[2].size==2)
  assert(p.set_field(first,"Swirl",{strength=3,lift=.5,clockwise=true}))
  assert(p.get_field(first,1).strength==3 and p.get_field(second,1).strength==1)
  assert(not p.set_layer(first,"Sparks",{opacity=.1,size=-1}))
  assert(math.abs(p.get_layer(first,"Sparks").opacity-.65)<.0001)
  assert(not p.set_layer(first,1,{size=0/0}))
  assert(not p.set_layer(first,1,{missing_setting=5}))
  assert(not p.set_layer(first,1,{enabled=1}))
  assert(not p.set_layer(first,1,{columns=2,rows=2,frames=8}))
  assert(not p.set_field(first,1,{strength=21}))
  assert(not p.set_field(first,1,{qx=0,qy=0,qz=0,qw=0}))
  assert(not p.set_layer({},1,{size=1}))
  assert(p.set_layer(first,"Smoke",{enabled=true,opacity=.15}))
  assert(p.stop(first,true))
  assert(p.burst(first,5,"Sparks"))
  assert(not p.burst(first,5,"Candle"))
  assert(p.pause(first) and p.resume(first) and p.restart(first))
  assert(p.attach(first,self.entity,"cut"))
  assert(p.set_transform(first,{x=0,y=1,z=0,qx=0,qy=0,qz=0,qw=1,scale_x=2,scale_y=2,scale_z=2}))
  assert(p.get_transform(first).y==1)
  assert(not p.attach(first,first))
  assert(not p.attach(first,self.entity,"missing_socket"))
  assert(p.destroy(second))
  assert(not renegade.entity.is_valid(second))
  assert(p.get_layer(second,1)==nil)
  assert(p.spawn("unknown effect")==nil)
  assert(p.spawn("../../outside")==nil)
 end,
 on_update=function(self,dt)
  if first and renegade.entity.is_valid(first) then
   assert(p.pause(first));assert(p.get_layer(first,"Sparks").size==.25)
  end
 end,
 on_stop=function(self)
  if first and renegade.entity.is_valid(first) then assert(p.stop(first,true)) end
 end
}
)LUA";
 Write(folder/"Content/Scripts/particle_test.lua",source);
 auto doc=CreateScriptDocument(project,GenerateStableId(),"Content/Scenes/Proof.wiscene","particle-lua-tests");
 ScriptSourceBinding binding;binding.sourceId=GenerateStableId();binding.sourcePath="Content/Scripts/particle_test.lua";binding.apiVersion=1;
 doc.attachments.push_back(CreateScriptAttachment(ScriptScope::Entity,ownerId,binding));
 RuntimeScriptRuntime runtime;Check(runtime.StartScene(doc,scene,folder.string(),error),"start governed Lua: "+error);
 for(const auto& d:runtime.Diagnostics())std::cerr<<d.callback<<": "<<d.message<<'\n';
 Check(runtime.DisabledInstanceCount()==0,"all Lua assertions succeeded");
 Check(scene.emitters.GetCount()==3,"one complete spawned instance remains");
 wi::ecs::Entity spawned=wi::ecs::INVALID_ENTITY;
 for(size_t n=0;n<scene.emitters.GetCount();++n)spawned=ParticleEffectRoot(scene,scene.emitters.GetEntity(n));
 if(spawned!=wi::ecs::INVALID_ENTITY){
  auto* h=scene.hierarchy.GetComponent(spawned);Check(h&&h->parentID==socket,"named socket attachment");
  auto captured=CaptureParticleEffect(scene,spawned);Check(captured.layers.size()==3&&captured.fields.size()==1&&captured.fields[0].strength==3,"runtime field changes reach native metadata");
  Check(scene.transforms.GetComponent(spawned)->translation_local.y==1,"root transform reaches native scene");
 }
 runtime.Update(.02f);Check(runtime.DisabledInstanceCount()==0,"update controls");
 runtime.StopScene();Check(scene.emitters.GetCount()==0&&scene.transforms.Contains(owner),"scene stop removes spawned effects and preserves owner");
 Check(Read(presetPath)==original,"Lua never writes the saved preset");
 // Placed effects are controllable and survive runtime cleanup.
 ParticleEffectCommand place(scene,folder.string(),project,effect);Check(place.Execute(),"place fixture");
 auto placed=place.Entity();ParticleEffectRuntimeService service(scene,folder.string(),project);ParticleEffectLayer layer;
 Check(service.GetLayer(placed,0,layer,error),"adopt placed instance");layer.emitter.color.w=.4f;Check(service.SetLayer(placed,0,layer,error),"edit placed instance");
 service.Clear();Check(scene.emitters.GetCount()==3,"placed instance survives service cleanup");
 // Parent deletion makes child references fail safely; native root remains a unit.
 auto parent=scene.Entity_CreateTransform("temporary parent");Check(AssignNewPersistentEntityId(scene,parent,error),"temporary parent identity");
 Check(service.Attach(placed,parent,"",false,error),"attach placed effect");scene.Entity_Remove(parent);
 Check(!service.GetLayer(placed,0,layer,error),"deleted parent invalidates effect access");
 Check(Read(presetPath)==original,"placed changes preserve preset");
 // NPC/native deletion must not exhaust the live-instance budget over time.
 for(int i=0;i<1025;++i){
  auto e=service.Spawn(effect.id,{},error);
  if(e==wi::ecs::INVALID_ENTITY){Check(false,"native deletion reclaims spawn capacity: "+error);break;}
  scene.Entity_Remove(e);
 }
 service.Clear();
 // Cross-project catalog does not expose this project's presets.
 ParticleEffectRuntimeService foreign(scene,folder.string(),GenerateStableId());
 Check(foreign.Spawn(effect.id,{},error)==wi::ecs::INVALID_ENTITY,"cross-project preset rejection");
 // Governed catalog dependency closure works after originals are unavailable.
 if(argc>1){
  std::vector<ParticleTextureEntry> textures;Check(ReadIncludedParticleTextures(argv[1],textures,error),"read texture fixture catalog");
  if(!textures.empty()){
   auto texture=textures.front();Check(AdoptParticleTexture(folder.string(),project,texture,error),"adopt governed fixture texture");
   auto textured=effect;textured.layers[0].texture=texture.assetId;
   Check(SaveParticleEffect(folder.string(),project,textured,true,error),"save textured runtime preset");
   std::vector<DependencyCandidate> deps;Check(CollectParticleEffectRuntimeDependencies(folder.string(),project,deps,error),"collect runtime dependencies: "+error);
   bool hasPreset=false,hasTexture=false;for(auto& d:deps){hasPreset|=d.declaredPath.find(textured.id)!=std::string::npos;hasTexture|=d.dependencyClass==DependencyClass::Texture;}
   Check(hasPreset&&hasTexture,"catalog closure contains preset and governed product");
   auto staged=folder.parent_path()/(folder.filename().string()+"-staged");
   Check(SnapshotParticleEffectRuntimeInputs(folder.string(),project,staged.string(),error),"snapshot catalog: "+error);
   auto hidden=folder.parent_path()/(folder.filename().string()+"-hidden");fs::rename(folder,hidden);
   PreparedMaterialTextureAsset product;Check(PrepareMaterialTextureAsset(staged.string(),project,texture.assetId,product,error),"staged texture independent of source project: "+error);
   auto cold=ListParticleEffects(staged.string(),project,error);Check(cold.size()==2,"staged presets cold read");
   fs::rename(hidden,folder);fs::remove_all(staged);
  }
 }
 fs::remove_all(folder);std::cout<<"ParticleLuaTests failures="<<failures<<'\n';return failures?1:0;
}
