#include "renegade/bridge/ParticleEffectService.h"
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
namespace renegade::bridge {
namespace {
struct Base {std::string source;float rate=0;int burst=0;bool enabled=true,paused=false;};
struct Clock {
 StableId identity;std::string source;ParticleEffectTiming timing;
 double elapsed=0,previous=-1;bool started=false,paused=false,stopped=false;
 uint64_t frame=0;ParticleTimingSample sample;std::unordered_map<wi::ecs::Entity,Base> layers;
};
struct Context {uint64_t frame=0;float dt=0;std::unordered_map<wi::ecs::Entity,Clock> roots;};
std::unordered_map<const wi::scene::Scene*,Context> contexts;
constexpr auto Key="renegade.particle.timing";
constexpr auto BaseKey="renegade.particle.timing.authored";
}
bool ValidateParticleTiming(const ParticleEffectTiming& t,std::string& error){
 if(!std::isfinite(t.duration)||!std::isfinite(t.delay)||t.duration<.01f||t.duration>3600||t.delay<0||t.delay>3600||t.burst>1000000){error="Timing needs duration 0.01-3600 s, delay 0-3600 s and a bounded burst.";return false;}error.clear();return true;
}
ParticleTimingSample SampleParticleTiming(const ParticleEffectTiming& t,double elapsed,double previous){
 if(!t.enabled)return {true,false,false};
 if(elapsed<0)return {};
 double cycle=double(t.duration)+t.delay;
 bool active=t.loop?std::fmod(elapsed,cycle)>=double(t.delay):(elapsed>=double(t.delay)&&elapsed<cycle);
 bool burst=t.loop?(elapsed>=double(t.delay)&&std::floor((elapsed-t.delay)/cycle)>std::floor((previous-t.delay)/cycle)):(previous<double(t.delay)&&elapsed>=double(t.delay));
 return {active,burst,!t.loop&&elapsed>=cycle};
}
void WriteParticleTiming(std::ostream& out,const ParticleEffectTiming& t){out<<t.enabled<<' '<<t.loop<<' '<<t.duration<<' '<<t.delay<<' '<<t.burst<<'\n';}
bool ReadParticleTiming(std::istream& in,ParticleEffectTiming& result){ParticleEffectTiming t;if(!(in>>t.enabled>>t.loop>>t.duration>>t.delay>>t.burst))return false;std::string error;if(!ValidateParticleTiming(t,error))return false;result=t;return true;}
void SetParticleTimingMetadata(wi::scene::Scene& scene,wi::ecs::Entity root,const ParticleEffectTiming& timing){
 auto* meta=scene.metadatas.GetComponent(root);if(!meta)meta=&scene.metadatas.Create(root);std::ostringstream data;data<<std::setprecision(9);WriteParticleTiming(data,timing);meta->string_values.set(Key,data.str());
}
void ResetParticleTiming(wi::scene::Scene& scene,wi::ecs::Entity root){auto c=contexts.find(&scene);if(c!=contexts.end())c->second.roots.erase(root);}
void BeginParticleTimingFrame(wi::scene::Scene& scene,float dt){
 auto& c=contexts[&scene];++c.frame;c.dt=std::isfinite(dt)&&dt>0?std::min(dt,60.0f):0;
 for(auto i=c.roots.begin();i!=c.roots.end();)if(!scene.transforms.Contains(i->first)||PersistentEntityId(scene,i->first)!=i->second.identity)i=c.roots.erase(i);else ++i;
}
void ControlParticleTiming(wi::scene::Scene& scene,wi::ecs::Entity root,const std::string& op){
 auto& c=contexts[&scene].roots[root];auto id=PersistentEntityId(scene,root);if(c.identity!=id){c={};c.identity=id;}
 if(op=="pause")c.paused=true;
 else if(op=="stop"){c.paused=false;c.stopped=true;}
 else {
  bool reset=op=="restart"||c.stopped||(!c.timing.loop&&c.elapsed>=double(c.timing.delay)+c.timing.duration);
  c.paused=c.stopped=false;if(reset){c.elapsed=0;c.previous=-1;c.started=false;c.frame=0;}
 }
}
void ApplyParticleTimingLayer(wi::scene::Scene& scene,wi::ecs::Entity entity){
 auto* emitter=scene.emitters.GetComponent(entity);auto* meta=scene.metadatas.GetComponent(entity);if(!emitter||!meta)return;
 auto root=ParticleEffectRoot(scene,entity);if(root==wi::ecs::INVALID_ENTITY)return;
 auto* rootMeta=scene.metadatas.GetComponent(root);if(!rootMeta||!rootMeta->string_values.has(Key)||!meta->string_values.has(BaseKey))return;
 const auto& source=rootMeta->string_values.get(Key);auto& ctx=contexts[&scene];
 auto found=ctx.roots.find(root);if(source.rfind("0 ",0)==0&&found==ctx.roots.end())return;
 auto& clock=ctx.roots[root];auto identity=PersistentEntityId(scene,root);
 if(clock.identity!=identity){clock={};clock.identity=identity;}
 if(clock.source!=source){
  ParticleEffectTiming timing;std::istringstream input(source);if(!ReadParticleTiming(input,timing)){wi::backlog::post("Invalid particle effect timing.",wi::backlog::LogLevel::Error);return;}
  clock.timing=timing;clock.source=source;clock.elapsed=0;clock.previous=-1;clock.started=false;clock.frame=0;
 }
 if(clock.frame!=ctx.frame){
  clock.frame=ctx.frame;clock.sample={};
  if(ctx.dt>0&&!clock.paused&&!clock.stopped){
   if(!clock.started){clock.started=true;clock.previous=-1;clock.elapsed=0;}
   else{clock.previous=clock.elapsed;clock.elapsed+=ctx.dt;}
   clock.sample=SampleParticleTiming(clock.timing,clock.elapsed,clock.previous);
  }else if(clock.started)clock.sample=SampleParticleTiming(clock.timing,clock.elapsed,clock.elapsed);
  if(!clock.timing.enabled)clock.sample={true,false,false};
  if(clock.stopped)clock.sample={false,false,false};
 }
 auto& base=clock.layers[entity];const auto& authored=meta->string_values.get(BaseKey);
 if(base.source!=authored){std::istringstream input(authored);if(!(input>>base.rate>>base.burst>>base.enabled>>base.paused))return;base.source=authored;}
 bool stationary=meta->string_values.has("renegade.particle.stationary.authored");
 bool active=clock.sample.active&&base.enabled&&!clock.stopped;
 emitter->count=active&&!stationary?base.rate:0;
 emitter->burst_on_create=clock.timing.enabled?0:base.burst;
 emitter->SetPaused(stationary||base.paused||clock.paused);
 meta->bool_values.set("renegade.particle.timing.hidden",!active);
 if(stationary)meta->bool_values.set("renegade.particle.sprite.paused",base.paused||clock.paused);
 if(clock.timing.enabled&&clock.sample.active&&clock.sample.burst&&base.enabled&&!base.paused&&!clock.paused&&!clock.stopped){
  if(stationary){if(auto* sp=scene.sprites.GetComponent(entity)){sp->anim.drawRectAnim._elapsedTime=0;sp->anim.drawRectAnim._currentFrame=meta->int_values.get("renegade.particle.sprite.start");}}
  else {uint64_t count=uint64_t(clock.timing.burst)+uint64_t(std::max(0,base.burst));if(count)emitter->Burst(int(std::min(count,uint64_t(emitter->GetMaxParticleCount()))));}
 }
}
}
