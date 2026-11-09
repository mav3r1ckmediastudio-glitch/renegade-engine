#include "renegade/bridge/ParticleEffectRuntimeDependencies.h"
#include "renegade/bridge/ParticleEffectService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ScriptDocumentService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace renegade::bridge;
namespace fs=std::filesystem;
int main(int argc,char**argv){
 if(argc!=4)return 2;std::string error;fs::path target=fs::u8path(argv[3]);
 fs::create_directories(target/"Content/Scenes");fs::create_directories(target/"Content/Scripts");
 if(!SnapshotParticleEffectRuntimeInputs(argv[1],argv[2],target.generic_u8string(),error)){std::cerr<<error;return 3;}
 wi::scene::Scene scene;TransformState start;start.translation={0,0,0};
 CreatePlayerStartCommand player(scene,start);if(!player.Execute())return 4;
 auto settings=CapturePlayerControllerSettings(scene,player.CreatedEntity());settings.gravityFactor=0;
 SetPlayerControllerSettingsCommand gravity(scene,player.CreatedEntity(),settings);if(!gravity.Execute())return 5;
 auto owner=scene.Entity_CreateTransform("Particle Lua proof");
 if(!AssignNewPersistentEntityId(scene,owner,error)){std::cerr<<error;return 6;}
 auto path=(target/"Content/Scenes/LuaParticles.wiscene").generic_u8string();
 wi::Archive archive(path,false,false);scene.Serialize(archive);if(!archive.SaveFile(path))return 7;archive=wi::Archive();
 auto envelope=CreateDocumentEnvelope(argv[2],"scene","Content/Scenes/LuaParticles.wiscene","particle-lua-proof");
 if(!WriteDocumentEnvelope(path+".rmeta",envelope,error)){std::cerr<<error;return 8;}
 auto doc=CreateScriptDocument(argv[2],envelope.documentId,"Content/Scenes/LuaParticles.wiscene","particle-lua-proof");
 ScriptSourceBinding binding;binding.sourceId=GenerateStableId();binding.sourcePath="Content/Scripts/particle_proof.lua";
 doc.attachments.push_back(CreateScriptAttachment(ScriptScope::Entity,PersistentEntityId(scene,owner),binding));
 if(!WriteScriptDocument(path+".rscripts",doc,error)){std::cerr<<error;return 9;}
 std::ofstream(target/"LuaParticles.renegade")<<"format = renegade-project\nversion = 1\n\n[project]\nproject_id = "<<argv[2]<<"\nname = Particle Lua standalone proof\nstartup_scene = Content/Scenes/LuaParticles.wiscene\n";
 std::ofstream(target/"Content/Scripts/particle_proof.lua")<<R"LUA(
local p=renegade.particles
local effects={}
local elapsed=0
local phase=-1
return {
 on_start=function(self)
  local names={"QA Rising Embers","Candle Flame","QA Fire with Heat Shimmer"}
  for i,name in ipairs(names) do
   local e,err=p.spawn(name,{x=(i-2)*2,y=.2,z=5})
   assert(e,err);effects[i]=e
   assert(p.set_transform(e,{scale_x=2,scale_y=2,scale_z=2}))
   assert(p.get_layer(e,1))
  end
  assert(p.attach(effects[1],self.entity,"",true))
 end,
 on_update=function(self,dt)
  elapsed=elapsed+dt
  local nextPhase=math.floor(elapsed/5)%3
  if nextPhase~=phase then
   phase=nextPhase
   for _,e in ipairs(effects) do
    if phase==0 then assert(p.restart(e))
    elseif phase==1 then assert(p.pause(e))
    else assert(p.stop(e,true)) end
   end
  end
 end,
 on_stop=function(self)
  for _,e in ipairs(effects) do assert(p.destroy(e)) end
 end
}
)LUA";
 // Cold reopen confirms saved scene and script identity agree.
 wi::scene::Scene reopened;wi::Archive read(path,true,false);reopened.Serialize(read);
 if(!ValidateScriptDocumentAgainstScene(doc,reopened,error)){std::cerr<<error;return 10;}
 std::cout<<"PLAYER_PROOF_READY "<<(target/"LuaParticles.renegade").generic_u8string()<<'\n';return 0;
}
