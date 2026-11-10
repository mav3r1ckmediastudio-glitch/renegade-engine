#include "renegade/bridge/ParticleEffectService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <unordered_map>
using namespace renegade::bridge;
LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcW(h,m,w,l);}
int main(int argc,char**argv){
 if(argc==4&&std::string(argv[1])=="--heat-fire-test"){
  std::string error;auto presets=ListParticleEffects(argv[2],argv[3],error);ParticleEffect effect;
  for(auto& p:presets)if(p.name=="Brazier Fire")effect=p;
  if(effect.layers.empty())return 36;
  effect.id.clear();effect.name="QA Fire with Heat Shimmer";
  bool heat=false;
  for(auto& l:effect.layers)if(l.emitter.shaderType==wi::EmittedParticleSystem::SOFT_DISTORTION){
   l.name="Heat shimmer";l.emitter.color={1,1,1,.08f};l.material.baseColor=l.emitter.color;l.emitter.emitCount=10;l.emitter.size=.7f;l.emitter.life=2.8f;l.emitter.velocity={0,.5f,0};l.emitter.rotation=.5f;l.emitter.randomFactor=.2f;l.emitter.randomLife=.5f;l.emitter.opacityPeakStart=.15f;l.emitter.opacityPeakEnd=.6f;l.transform.translation={0,1.3f,0};l.sizeVariation=.3f;l.speedVariation=.25f;heat=true;
  }
  if(!heat)return 37;
  if(!SaveParticleEffect(argv[2],argv[3],effect,true,error)){std::cerr<<error;return 38;}
  std::cout<<"HEAT_FIRE_REVIEW "<<effect.id<<'\n';return 0;
 }
 if((argc==5||argc==6)&&std::string(argv[1])=="--ember-field-test"){
  std::string error;CreatorTextureWorkflowService workflow;auto imported=workflow.ImportTexture(argv[2],argv[3],argv[4]);if(!imported.succeeded){std::cerr<<imported.error;return 34;}
  ParticleEffect effect;effect.name="QA Rising Embers";
  for(int n=0;n<3;++n){
   ParticleEffectLayer l;l.id=GenerateStableId();l.name=n==0?"Fine embers":n==1?"Brighter flecks":"Drifting specks";l.texture=imported.assetId;l.emitter=MakeNewParticleEmitterState();auto& e=l.emitter;
   e.maxParticles=256;e.emitCount=n==0?20:n==1?6:12;e.size=n==0?.035f:n==1?.055f:.018f;e.life=n==0?5.2f:n==1?6:4;e.randomLife=1;e.randomFactor=.2f;e.normalFactor=0;e.velocity={0,.6f,0};e.gravity={0,0,0};e.drag=1;e.volume=true;
   // Original GGMAX sheet packs four different masks, not four animation frames.
   // Its lower-right ember is frame 3. Lifetime animation over [3,4) holds that cell.
   e.framesX=e.framesY=2;e.frameCount=4;e.frameStart=3;e.frameRate=0;e.frameBlending=false;
   e.opacityPeakStart=.03f;e.opacityPeakEnd=n==1?.45f:.65f;e.color=n==1?XMFLOAT4(1,.95f,.85f,.9f):XMFLOAT4(1,1,1,.9f);
   e.emissiveColor={1,1,1};e.emissiveStrength=n==0?3.5f:n==1?5:2.5f;e.rotation=1.8f;
   l.sizeVariation=n==0?.65f:n==1?.5f:.7f;l.speedVariation=n==0?.55f:n==1?.65f:.45f;
   l.transform.translation={n==1?.04f:n==2?-.06f:0,.08f,0};l.transform.scale={n==0?.45f:n==1?.3f:.5f,.035f,n==0?.45f:n==1?.3f:.5f};
   l.material.blendMode=wi::enums::BLENDMODE_ADDITIVE;l.material.baseColor=e.color;l.fieldInfluence={.4f,n==0?.85f:n==1?1:.6f,1,1};effect.layers.push_back(l);
  }
  ParticleMovementField rise;rise.id=GenerateStableId();rise.name="Warm rising air";rise.type=ParticleFieldType::Updraft;rise.transform.translation={0,1.5f,0};rise.transform.scale={2,3,2};rise.strength=.8f;rise.lift=0;rise.falloff=.2f;
  auto noise=rise;noise.id=GenerateStableId();noise.name="Uneven air";noise.type=ParticleFieldType::Turbulence;noise.strength=1.8f;noise.noiseScale=2.5f;noise.animationSpeed=.9f;effect.fields={rise,noise};
  if(argc==6)effect.id=argv[5];
  if(!SaveParticleEffect(argv[2],argv[3],effect,argc==5,error)){std::cerr<<error;return 35;}std::cout<<"EMBER_FIELD_TEST_PRESET "<<effect.id<<'\n';return 0;
 }
 if(argc==5&&std::string(argv[1])=="--mixed-leaf-test"){
  std::string error;ParticleEffect effect;effect.name="QA Mixed Leaves";
  for(int n=0;n<3;++n){
   CreatorTextureWorkflowService workflow;auto imported=workflow.ImportTexture(argv[2],argv[3],std::string(argv[4])+"/Leaf"+std::to_string(n+1)+".PNG");if(!imported.succeeded){std::cerr<<imported.error;return 32;}
   ParticleEffectLayer l;l.id=GenerateStableId();l.name="Leaf "+std::to_string(n+1);l.texture=imported.assetId;l.emitter=MakeNewParticleEmitterState();auto& e=l.emitter;
   e.emitCount=n==0?2:n==1?2.5f:1.5f;e.size=n==0?.09f:n==1?.11f:.08f;e.life=n==0?5.5f:n==1?4.8f:6.3f;e.randomLife=.8f;e.randomFactor=0;e.normalFactor=0;e.velocity={0,.04f,0};e.gravity={0,0,0};e.drag=1;e.volume=true;e.framesX=e.framesY=4;e.frameCount=16;e.frameRate=n==0?11:n==1?8:13;e.frameBlending=false;e.opacityPeakStart=.06f;e.opacityPeakEnd=.75f;e.color={1,1,1,1};
   l.sizeVariation=n==0?.5f:n==1?.65f:.45f;l.speedVariation=n==0?.35f:n==1?.45f:.25f;
   l.fieldInfluence={n==0?.85f:n==1?.7f:1,n==0?.65f:n==1?1:.8f,1,1};
   l.transform.translation={n==0?-.45f:n==1?.5f:0,n==0?1:n==1?1.65f:.7f,n==0?.3f:n==1?-.4f:.5f};l.transform.scale={1.5f,n==0?.65f:n==1?.8f:.5f,1.5f};l.material.blendMode=wi::enums::BLENDMODE_ALPHA;effect.layers.push_back(l);
  }
  ParticleMovementField swirl;swirl.id=GenerateStableId();swirl.name="Gentle circulation";swirl.transform.translation={0,1.4f,0};swirl.transform.scale={3.5f,2.4f,3.5f};swirl.strength=.7f;swirl.lift=.04f;swirl.falloff=.35f;
  auto noise=swirl;noise.id=GenerateStableId();noise.name="Changing air";noise.type=ParticleFieldType::Turbulence;noise.strength=.35f;noise.lift=0;noise.noiseScale=1.4f;noise.animationSpeed=.8f;effect.fields={swirl,noise};
  if(!SaveParticleEffect(argv[2],argv[3],effect,true,error)){std::cerr<<error;return 33;}std::cout<<"MIXED_LEAF_TEST_PRESET "<<effect.id<<'\n';return 0;
 }
 if(argc==5&&std::string(argv[1])=="--leaf-field-test"){
  std::string error;CreatorTextureWorkflowService workflow;auto imported=workflow.ImportTexture(argv[2],argv[3],argv[4]);if(!imported.succeeded){std::cerr<<imported.error;return 30;}
  ParticleEffect effect;effect.name="QA Leaf Swirl";
  ParticleEffectLayer layer;layer.id=GenerateStableId();layer.name="Leaf flipbook";layer.texture=imported.assetId;layer.emitter=MakeNewParticleEmitterState();
  auto& e=layer.emitter;e.emitCount=16;e.size=.12f;e.life=5;e.randomLife=.4f;e.normalFactor=0;e.randomFactor=.3f;e.velocity={0,.1f,0};e.gravity={0,0,0};e.drag=1;e.volume=true;e.framesX=e.framesY=4;e.frameCount=16;e.frameRate=12;e.frameBlending=false;e.color={1,1,1,1};e.opacityPeakStart=.05f;e.opacityPeakEnd=.85f;
  layer.transform.translation={0,.6f,0};layer.transform.scale={.8f,.25f,.8f};layer.material.blendMode=wi::enums::BLENDMODE_ALPHA;effect.layers={layer};
  ParticleMovementField field;field.id=GenerateStableId();field.name="Leaf circulation";field.transform.scale={2,1.3f,2};field.transform.translation={0,.65f,0};field.strength=1.2f;field.falloff=.25f;effect.fields={field};
  if(!SaveParticleEffect(argv[2],argv[3],effect,true,error)){std::cerr<<error;return 31;}std::cout<<"LEAF_FIELD_TEST_PRESET "<<effect.id<<'\n';return 0;
 }
 if(argc==6&&std::string(argv[1])=="--copy-presets"){
  const std::string source=argv[2],target=argv[3],project=argv[4];std::string error,line;
  std::ifstream plan(argv[5]);if(!plan)return 20;
  std::unordered_map<StableId,StableId> textures;
  while(std::getline(plan,line)){
   if(!line.empty()&&line.back()=='\r')line.pop_back();
   auto split=line.find('\t');if(split==std::string::npos)return 21;
   auto id=line.substr(0,split),path=line.substr(split+1);
   if(path.empty())return 21;
   if(path.front()=='@'){textures[id]=path.substr(1);continue;}
   CreatorTextureWorkflowService workflow;auto imported=workflow.ImportTexture(target,project,path);
   if(!imported.succeeded){std::cerr<<imported.error<<'\n';return 22;}
   textures[id]=imported.assetId;std::cout<<"IMPORTED "<<id<<" "<<imported.assetId<<'\n';
  }
  auto presets=ListParticleEffects(source,project,error);if(!error.empty()||presets.empty()){std::cerr<<error;return 23;}
  // Resolve the entire transfer before writing preset documents.
  for(auto& effect:presets)for(auto& layer:effect.layers){
   auto remap=[&](StableId& id){if(id.empty())return true;auto it=textures.find(id);if(it==textures.end())return false;id=it->second;return true;};
   if(!remap(layer.texture))return 24;for(auto& id:layer.extraTextures)if(!remap(id))return 24;
  }
  for(auto& effect:presets){if(!SaveParticleEffect(target,project,effect,true,error)){std::cerr<<error;return 25;}std::cout<<"COPIED "<<effect.name<<" "<<effect.id<<'\n';}
  std::cout<<"PRESET_TRANSFER_PASS "<<presets.size()<<'\n';return 0;
 }
 if(argc!=4)return 2;std::string root=argv[1],project=argv[2],textures=argv[3],error;
 ParticleEffect p;p.name="Brazier Flame";
 ParticleEffectLayer flame;flame.id=GenerateStableId();flame.name="Flame";flame.texture="83e9258b-e909-4ef2-a4ce-d027fd815ccd";
 flame.emitter=MakeNewParticleEmitterState();auto& f=flame.emitter;
 f.emitCount=51;f.maxParticles=800;f.size=.7f;f.life=1.248f;f.normalFactor=0;f.randomFactor=.35f;f.randomLife=.25f;
 f.velocity={0,.75f,0};f.gravity={0,0,0};f.framesX=f.framesY=8;f.frameCount=64;f.frameRate=24;f.frameBlending=true;f.color={1,1,1,.9f};
 flame.material.blendMode=wi::enums::BLENDMODE_ADDITIVE;flame.material.baseColor=f.color;p.layers={flame};
 auto save=[&](ParticleEffect setup){
  if(!SaveParticleEffect(root,project,setup,true,error)){std::cerr<<error<<'\n';return false;}
  std::cout<<"SAVED "<<setup.name<<" "<<setup.id<<'\n';return true;
 };
 if(!save(p))return 7;
 auto import=[&](const std::string& name){CreatorTextureWorkflowService w;auto r=w.ImportTexture(root,project,textures+"/"+name);if(!r.succeeded){std::cerr<<r.error<<'\n';return StableId{};}return r.assetId;};
 const auto normal=import("T_Refraction_N.png");
 const auto smoke=import("Smoke.TGA"),spark=import("T_Spark_1.TGA"),candle=import("Candle.png"),mist=import("Mist.png"),steam=import("Steam.TGA");
 if(smoke.empty()||spark.empty()||candle.empty()||mist.empty()||steam.empty())return 8;
 auto layer=[&](const char*name,const StableId&texture){ParticleEffectLayer l;l.id=GenerateStableId();l.name=name;l.texture=texture;l.emitter=MakeNewParticleEmitterState();l.emitter.normalFactor=0;l.emitter.gravity={0,0,0};l.emitter.randomFactor=.1f;l.emitter.randomLife=.2f;l.emitter.maxParticles=512;l.material.blendMode=wi::enums::BLENDMODE_ALPHA;l.emitter.color={1,1,1,1};l.material.baseColor=l.emitter.color;return l;};
 auto s=layer("Smoke",smoke);s.emitter.framesX=s.emitter.framesY=8;s.emitter.frameCount=64;s.emitter.frameRate=16;s.emitter.frameBlending=true;
 s.emitter.emitCount=5;s.emitter.size=.55f;s.emitter.life=3.2f;s.emitter.velocity={.02f,.42f,.015f};s.emitter.color={.4f,.4f,.4f,.22f};s.transform.translation={0,.45f,0};s.emitter.opacityPeakStart=.18f;s.emitter.opacityPeakEnd=.45f;
 auto e=layer("Embers",spark);e.emitter.emitCount=4;e.emitter.size=.025f;e.emitter.life=2.5f;e.emitter.velocity={0,.9f,0};e.emitter.randomFactor=.35f;e.emitter.color={1,.42f,.07f,1};e.emitter.emissiveColor={1,.3f,.025f};e.emitter.emissiveStrength=3;e.material.blendMode=wi::enums::BLENDMODE_ADDITIVE;e.emitter.opacityPeakStart=.04f;e.emitter.opacityPeakEnd=.65f;e.transform.translation={0,.2f,0};
 auto haze=layer("Heat haze",{});haze.extraTextures[0]=normal;haze.emitter.shaderType=wi::EmittedParticleSystem::SOFT_DISTORTION;haze.emitter.size=.65f;haze.emitter.emitCount=5;haze.emitter.life=1.5f;haze.emitter.velocity={0,.15f,0};haze.emitter.randomFactor=.04f;haze.emitter.color={1,1,1,.035f};haze.transform.translation={0,.7f,0};haze.emitter.opacityPeakStart=.2f;haze.emitter.opacityPeakEnd=.5f;
 p.name="Heat Haze";p.layers={haze};if(!save(p))return 9;
 p.name="Brazier Fire";p.layers={flame,s,e,haze};if(!save(p))return 9;
 p.name="Smoke";p.layers={s};if(!save(p))return 9;
 p.name="Embers";p.layers={e};if(!save(p))return 9;
 auto c=layer("Candle flame",candle);c.playback=ParticlePlayback::StationaryLoop;c.emitter.framesX=c.emitter.framesY=6;c.emitter.frameCount=36;c.emitter.frameRate=24;c.emitter.frameBlending=true;c.emitter.emitCount=12;c.emitter.size=.08f;c.emitter.life=.35f;c.emitter.randomFactor=0;c.emitter.randomLife=.05f;c.emitter.velocity={0,.01f,0};c.material.blendMode=wi::enums::BLENDMODE_ADDITIVE;
 c.transform.translation={0,.08f,0};p.name="Candle Flame";p.layers={c};if(!save(p))return 9;
 auto m=layer("Mist",mist);m.emitter.framesX=4;m.emitter.framesY=16;m.emitter.frameCount=64;m.emitter.frameRate=12;m.emitter.frameBlending=true;m.emitter.emitCount=3;m.emitter.size=2;m.emitter.scaleX=2;m.emitter.scaleY=.25f;m.emitter.life=6;m.emitter.velocity={.12f,0,0};m.emitter.color={.75f,.8f,.85f,.18f};m.emitter.opacityPeakStart=.2f;m.emitter.opacityPeakEnd=.55f;
 p.name="Ground Mist";p.layers={m};if(!save(p))return 9;
 auto t=s;t.id=GenerateStableId();t.name="Steam";t.texture=steam;t.emitter.color={.8f,.85f,.9f,.35f};t.emitter.emitCount=8;t.emitter.size=.4f;t.emitter.life=2;t.emitter.velocity={0,.6f,0};
 p.name="Steam";p.layers={t};if(!save(p))return 9;
 std::cout<<"SOURCE_SCENE_UNCHANGED\n";
 return 0;
}
