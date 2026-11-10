#include "renegade/bridge/ParticleEffectService.h"
#include "renegade/bridge/SelectionService.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <limits>
#include <cmath>
using namespace renegade::bridge;
namespace fs=std::filesystem;
int failures=0;
void Check(bool value,const std::string& name){if(!value){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
bool Near(float a,float b){return std::abs(a-b)<.00001f;}
int main(int argc,char**argv){
 std::string error;
 ParticleEmissionShape shape;shape.radius=2;shape.height=3;shape.angle=25;shape.thickness=.4f;
 for(int type=0;type<7;++type){shape.type=ParticleShapeType(type);Check(ValidateParticleShape(shape,error),"shape validation");
  for(int i=0;i<1000;++i){XMFLOAT3 u={float((i*17)%997)/996,float((i*31)%991)/990,float((i*67)%983)/982};auto sample=SampleParticleShape(shape,u);
   float radial=std::sqrt(sample.position.x*sample.position.x+sample.position.y*sample.position.y);float length=std::sqrt(radial*radial+sample.position.z*sample.position.z);
   if(type==0)Check(length==0,"point anchored");
   if(type==1)Check(std::abs(sample.position.x)<=1&&std::abs(sample.position.y)<=1&&std::abs(sample.position.z)<=1,"box bounds");
   if(type==2)Check(length<=2.00001f,"sphere volume bounds");
   if(type==3)Check(radial<=2.00001f&&sample.normal.z>=std::cos(XMConvertToRadians(25.0f))-.00001f,"cone angle bounds");
   if(type==4)Check(radial<=2.00001f&&sample.position.z==0,"disc plane");
   if(type==5)Check(radial>=1.79999f&&radial<=2.20001f,"ring band");
   if(type==6)Check(radial<=2.00001f&&std::abs(sample.position.z)<=1.50001f,"cylinder bounds");
  }
 }
 shape.type=ParticleShapeType::Sphere;shape.surface=true;auto shell=SampleParticleShape(shape,{.3f,.7f,.1f});Check(Near(shell.position.x*shell.position.x+shell.position.y*shell.position.y+shell.position.z*shell.position.z,4),"sphere surface");
 shape.angle=91;Check(!ValidateParticleShape(shape,error),"invalid shape rejected");shape.angle=25;
 ParticleEffectTiming timing;timing.enabled=true;timing.duration=2;timing.delay=1;timing.burst=7;
 Check(!SampleParticleTiming(timing,0,-1).active,"timing initial delay");
 Check(SampleParticleTiming(timing,1,.9).active&&SampleParticleTiming(timing,1,.9).burst,"timing begins with burst");
 Check(!SampleParticleTiming(timing,3,2.9).active,"loop delay repeats");
 Check(SampleParticleTiming(timing,4,3.9).burst,"loop next burst");
 timing.loop=false;Check(SampleParticleTiming(timing,3,2.9).completed&&!SampleParticleTiming(timing,3,2.9).active,"one shot finishes");
 Check(!SampleParticleTiming(timing,1,1).burst,"paused clock no repeat burst");
 timing.delay=0;Check(SampleParticleTiming(timing,0,-1).burst,"immediate burst");
 timing.duration=0;Check(!ValidateParticleTiming(timing,error),"zero duration rejected");timing.duration=2;timing.delay=1;
 ParticleLifetimeCurve curve;
 Check(!curve.enabled&&curve.keys.size()==2,"neutral defaults");
 Check(ValidateParticleLifetime(curve,error),"default validation");
 Check(SampleParticleLifetime(curve,.5f).size==1,"disabled identity");
 curve.enabled=true;curve.keys={{0,.2f,{1,0,0,0}},{.4f,2,{0,1,0,1}},{1,0,{0,0,1,0}}};
 auto k=SampleParticleLifetime(curve,.2f);Check(Near(k.size,1.1f)&&Near(k.colour.x,.5f)&&Near(k.colour.y,.5f)&&Near(k.colour.w,.5f),"piecewise interpolation");
 k=SampleParticleLifetime(curve,.7f);Check(Near(k.size,1)&&Near(k.colour.y,.5f)&&Near(k.colour.z,.5f),"late interpolation");
 Check(SampleParticleLifetime(curve,-1).size==.2f&&SampleParticleLifetime(curve,2).size==0,"endpoint clamps");
 auto bad=curve;bad.keys[1].time=0;Check(!ValidateParticleLifetime(bad,error),"reject duplicate times");
 bad=curve;bad.keys[1].time=1.2f;Check(!ValidateParticleLifetime(bad,error),"reject unordered times");
 bad=curve;bad.keys.back().time=.9f;Check(!ValidateParticleLifetime(bad,error),"require final endpoint");
 bad=curve;bad.keys[1].size=std::numeric_limits<float>::quiet_NaN();Check(!ValidateParticleLifetime(bad,error),"reject NaN");
 bad=curve;bad.keys[1].colour.w=2;Check(!ValidateParticleLifetime(bad,error),"reject alpha range");
 bad=curve;bad.keys.resize(9);Check(!ValidateParticleLifetime(bad,error),"bounded point count");
 std::ostringstream data;WriteParticleLifetime(data,curve);std::istringstream input(data.str());ParticleLifetimeCurve read;
 Check(ReadParticleLifetime(input,read)&&read.enabled&&read.keys.size()==3&&read.keys[1].size==2,"curve codec");
 auto root=fs::temp_directory_path()/("particle-lifetime-"+GenerateStableId());fs::create_directories(root);auto project=GenerateStableId();
 ParticleEffect effect;effect.name="Lifetime fixture";effect.timing=timing;
 for(int i=0;i<2;++i){ParticleEffectLayer l;l.id=GenerateStableId();l.name="Layer "+std::to_string(i);l.emitter=MakeNewParticleEmitterState();l.shape=shape;l.lifetime=curve;if(i==1)l.lifetime.enabled=false;effect.layers.push_back(l);}
 Check(SaveParticleEffect(root.string(),project,effect,true,error),"save schema6: "+error);
 auto presets=ListParticleEffects(root.string(),project,error);Check(presets.size()==1&&presets[0].layers[0].lifetime.keys.size()==3&&presets[0].layers[0].lifetime.keys[1].size==2&&!presets[0].layers[1].lifetime.enabled,"preset cold read");
 auto path=root/"Content/Particles"/(effect.id+".rparticle");
 // Schema 5 has the same records with no lifetime line per layer.
 std::ifstream original(path);std::vector<std::string> lines;std::string line;while(std::getline(original,line))lines.push_back(line);original.close();
 Check(lines.size()>=12,"known fixture records");
 if(lines.size()>=12){std::vector<std::string> legacy;for(int index:{0,1,2,3,4,5,8,9,12,13})legacy.push_back(lines[index]);lines=legacy;lines[0]="renegade-particle-effect 5";auto id=GenerateStableId();lines[2]=id;std::ofstream old(root/"Content/Particles"/(id+".rparticle"));for(auto& text:lines)old<<text<<'\n';old.close();auto all=ListParticleEffects(root.string(),project,error);bool neutral=false;for(auto& p:all)if(p.id==id)neutral=!p.layers[0].lifetime.enabled&&p.layers[0].lifetime.keys.size()==2;Check(neutral,"schema5 migrates neutral");}
 wi::scene::Scene scene;ParticleEffectCommand create(scene,root.string(),project,effect);Check(create.Execute(),"create curved composite");
 auto entity=create.Entity();
 auto native=CaptureParticleEffect(scene,entity);Check(native.timing.enabled&&native.timing.burst==7&&native.layers[0].shape.type==ParticleShapeType::Sphere,"shape and timing capture");
 UpdateParticleEffectSprites(scene,.1f);Check(scene.emitters[0].count==0,"delay gates emission");
 UpdateParticleEffectSprites(scene,1);Check(scene.emitters[0].count==effect.layers[0].emitter.emitCount,"duration enables authored emission");
 UpdateParticleEffectSprites(scene,2);Check(scene.emitters[0].count==0,"one shot stops emission");
 Check(CaptureParticleEffect(scene,entity).layers[0].emitter.emitCount==effect.layers[0].emitter.emitCount,"capture retains authored rate after stop");
 ControlParticleTiming(scene,entity,"restart");UpdateParticleEffectSprites(scene,.1f);Check(scene.emitters[0].count==0,"restart repeats delay");
 ControlParticleTiming(scene,entity,"pause");UpdateParticleEffectSprites(scene,2);Check(scene.emitters[0].IsPaused(),"pause freezes simulation");
 ControlParticleTiming(scene,entity,"resume");UpdateParticleEffectSprites(scene,1);Check(!scene.emitters[0].IsPaused()&&scene.emitters[0].count>0,"resume continues timeline");
 auto captured=CaptureParticleEffect(scene,entity);Check(captured.layers.size()==2&&captured.layers[0].lifetime.enabled&&captured.layers[0].lifetime.keys[1].size==2,"scene metadata capture");
 {wi::Archive a((root/"Curves.wiscene").string(),false);scene.Serialize(a);}
 wi::scene::Scene reopened;{wi::Archive a((root/"Curves.wiscene").string(),true);reopened.Serialize(a);}auto cold=CaptureParticleEffect(reopened,reopened.emitters.GetEntity(0));
 Check(cold.timing.enabled&&cold.timing.burst==7&&cold.layers[0].shape.surface,"native shape and timing cold read");
 Check(cold.layers.size()==2&&cold.layers[0].lifetime.keys[1].colour.y==1,"native archive cold read");
 auto changed=captured;changed.layers[0].lifetime.keys[1].size=3;
 ParticleEffectCommand edit(scene,root.string(),project,changed,entity);Check(edit.Execute(),"composite curve edit");
 Check(CaptureParticleEffect(scene,entity).layers[0].lifetime.keys[1].size==3,"native edit capture");
 edit.Undo();Check(CaptureParticleEffect(scene,entity).layers[0].lifetime.keys[1].size==2,"curve undo");Check(edit.Execute(),"curve redo");
 DuplicateEntityCommand copy(scene,entity);Check(copy.Execute(),"duplicate curved composite");
 Check(scene.emitters.GetCount()==4,"duplicate retains whole effect");
 copy.Undo();Check(scene.emitters.GetCount()==2,"duplicate undo");
 auto isolated=effect;isolated.layers[0].lifetime.keys[1].size=4;Check(effect.layers[0].lifetime.keys[1].size==2,"independent copied curve storage");
 // Existing owner catalog must remain readable with neutral migration.
 if(argc>2){auto old=ListParticleEffects(argv[1],argv[2],error);Check(!old.empty()&&error.empty(),"existing project catalog compatibility");for(auto& p:old)for(auto& l:p.layers)Check(!l.lifetime.enabled,"old preset neutral curve");}
 fs::remove_all(root);std::cout<<"ParticleLifetimeTests failures="<<failures<<'\n';return failures?1:0;
}
