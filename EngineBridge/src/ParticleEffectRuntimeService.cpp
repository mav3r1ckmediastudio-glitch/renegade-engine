#include "renegade/bridge/ParticleEffectRuntimeService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include <algorithm>
#include <cmath>
namespace renegade::bridge {
namespace { bool Finite(const XMFLOAT3& p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);} }
ParticleEffectRuntimeService::ParticleEffectRuntimeService(wi::scene::Scene& s,std::string r,StableId p):scene_(s),root_(std::move(r)),project_(std::move(p)){}
ParticleEffectRuntimeService::~ParticleEffectRuntimeService(){Clear();}
const std::vector<ParticleEffect>& ParticleEffectRuntimeService::Presets(std::string& error){
 error.clear();if(!catalogLoaded_){presets_=ListParticleEffects(root_,project_,error);if(error.empty())catalogLoaded_=true;}return presets_;
}
ParticleEffectRuntimeService::Live* ParticleEffectRuntimeService::Track(wi::ecs::Entity root,std::string& error){
 error.clear();auto id=PersistentEntityId(scene_,root);
 if(id.empty()||!scene_.transforms.Contains(root)){error="Particle effect no longer exists.";return nullptr;}
 auto i=live_.find(root);
 if(i!=live_.end()){
  auto& v=i->second;
  if(v.id!=id){live_.erase(i);error="Particle effect identity changed.";return nullptr;}
  for(size_t n=0;n<v.layers.size();++n)if(!scene_.emitters.Contains(v.layers[n])||PersistentEntityId(scene_,v.layers[n])!=v.layerIds[n]||(v.layers[n]!=root&&!scene_.Entity_IsDescendant(v.layers[n],root))){error="Particle effect layers were removed or detached.";return nullptr;}
  return &v;
 }
 auto owner=ParticleEffectRoot(scene_,root);
 if(owner!=wi::ecs::INVALID_ENTITY&&owner!=root){error="Use the complete particle effect root.";return nullptr;}
 if(owner==wi::ecs::INVALID_ENTITY&&!scene_.emitters.Contains(root)){error="Entity is not a particle effect.";return nullptr;}
 Live v;v.id=id;v.effect=CaptureParticleEffect(scene_,root);
 for(size_t n=0;n<scene_.emitters.GetCount();++n){auto e=scene_.emitters.GetEntity(n);if(e==root||scene_.Entity_IsDescendant(e,root)){v.layers.push_back(e);v.layerIds.push_back(PersistentEntityId(scene_,e));}}
 if(v.layers.empty()||v.layers.size()!=v.effect.layers.size()){error="Particle effect has no intact layers.";return nullptr;}
 auto h=scene_.hierarchy.GetComponent(root);if(h){v.parent=h->parentID;v.attachedParent=PersistentEntityId(scene_,v.parent);}
 return &live_.emplace(root,std::move(v)).first->second;
}
wi::ecs::Entity ParticleEffectRuntimeService::Spawn(const std::string& key,const XMFLOAT3& position,std::string& error){
 if(!Finite(position)){error="Particle position must be finite.";return wi::ecs::INVALID_ENTITY;}
 const auto& catalog=Presets(error);if(!error.empty())return wi::ecs::INVALID_ENTITY;
 const ParticleEffect* preset=nullptr;
 for(auto& p:catalog)if(p.id==key||p.name==key){if(preset&&preset->id!=p.id){error="Preset name is ambiguous; use its preset ID.";return wi::ecs::INVALID_ENTITY;}preset=&p;}
 if(!preset){error="Particle preset was not found in this project.";return wi::ecs::INVALID_ENTITY;}
 // Explicit spawn reclaims references removed by native parent/NPC deletion.
 for(auto i=live_.begin();i!=live_.end();)if(!scene_.transforms.Contains(i->first)||PersistentEntityId(scene_,i->first)!=i->second.id)i=live_.erase(i);else ++i;
 size_t spawned=0;for(auto& p:live_)spawned+=p.second.spawned;
 if(spawned>=1024){error="Runtime particle instance limit reached.";return wi::ecs::INVALID_ENTITY;}
 ParticleEffectCommand command(scene_,root_,project_,*preset,wi::ecs::INVALID_ENTITY,position);
 if(!command.Execute()){error=command.Error();return wi::ecs::INVALID_ENTITY;}
 const auto e=command.Entity();auto* v=Track(e,error);if(!v){scene_.Entity_Remove(e);return wi::ecs::INVALID_ENTITY;}v->spawned=true;return e;
}
void ParticleEffectRuntimeService::Apply(wi::ecs::Entity root,Live& v,bool restart){
 ApplyParticleEffectFieldMetadata(scene_,root,v.effect.fields);
 SetParticleTimingMetadata(scene_,root,v.effect.timing);
 for(size_t n=0;n<v.layers.size();++n){
  auto layer=v.effect.layers[n];if(v.stopped){layer.emitter.emitCount=layer.emitter.burstOnCreate=0;layer.emitter.paused=false;}
  if(v.paused)layer.emitter.paused=true;
  if(v.stopped&&layer.playback==ParticlePlayback::StationaryLoop)layer.enabled=false;
  ApplyParticleEffectLayerSettings(scene_,root,v.layers[n],layer,v.effect.fields,restart);
  if(restart)scene_.emitters.GetComponent(v.layers[n])->Restart();
 }
 HydrateParticleMovementFields(scene_);
}
bool ParticleEffectRuntimeService::GetTiming(wi::ecs::Entity e,ParticleEffectTiming& t,std::string& error){auto* v=Track(e,error);if(!v)return false;t=v->effect.timing;return true;}
bool ParticleEffectRuntimeService::SetTiming(wi::ecs::Entity e,const ParticleEffectTiming& t,std::string& error){auto* v=Track(e,error);if(!v||!ValidateParticleTiming(t,error))return false;v->effect.timing=t;ResetParticleTiming(scene_,e);Apply(e,*v);ControlParticleTiming(scene_,e,v->stopped?"stop":v->paused?"pause":"restart");return true;}
bool ParticleEffectRuntimeService::Layers(wi::ecs::Entity e,std::vector<std::string>& out,std::string& error){auto* v=Track(e,error);if(!v)return false;out.clear();for(auto& l:v->effect.layers)out.push_back(l.name);return true;}
bool ParticleEffectRuntimeService::Fields(wi::ecs::Entity e,std::vector<std::string>& out,std::string& error){auto* v=Track(e,error);if(!v)return false;out.clear();for(auto& f:v->effect.fields)out.push_back(f.name);return true;}
bool ParticleEffectRuntimeService::GetLayer(wi::ecs::Entity e,size_t n,ParticleEffectLayer& l,std::string& error){auto* v=Track(e,error);if(!v)return false;if(n>=v->effect.layers.size()){error="Layer index is out of range.";return false;}l=v->effect.layers[n];return true;}
bool ParticleEffectRuntimeService::SetLayer(wi::ecs::Entity e,size_t n,const ParticleEffectLayer& layer,std::string& error){
 auto* v=Track(e,error);if(!v)return false;if(n>=v->effect.layers.size()){error="Layer index is out of range.";return false;}
 auto candidate=v->effect;candidate.layers[n]=layer;
 if(layer.id!=v->effect.layers[n].id||layer.texture!=v->effect.layers[n].texture||layer.extraTextures!=v->effect.layers[n].extraTextures){error="Use set_texture for governed texture changes; layer identity is immutable.";return false;}
 if(!ValidateParticleEffect(candidate,error))return false;
 if((!candidate.fields.empty()||layer.sizeVariation>0||layer.speedVariation>0||layer.lifetime.enabled||int(layer.shape.type)>1)&&wi::graphics::GetDevice()&&!EnsureParticleMovementShaders(error))return false;
 candidate.layers[n].emitter=SanitizeParticleEmitterState(candidate.layers[n].emitter);
 v->effect=std::move(candidate);Apply(e,*v);return true;
}
bool ParticleEffectRuntimeService::GetField(wi::ecs::Entity e,size_t n,ParticleMovementField& f,std::string& error){auto* v=Track(e,error);if(!v)return false;if(n>=v->effect.fields.size()){error="Field index is out of range.";return false;}f=v->effect.fields[n];return true;}
bool ParticleEffectRuntimeService::SetField(wi::ecs::Entity e,size_t n,const ParticleMovementField& f,std::string& error){
 auto* v=Track(e,error);if(!v)return false;if(n>=v->effect.fields.size()){error="Field index is out of range.";return false;}
 if(f.id!=v->effect.fields[n].id){error="Field identity is immutable.";return false;}
 auto p=v->effect;p.fields[n]=f;if(!ValidateParticleEffect(p,error))return false;
 if(wi::graphics::GetDevice()&&!EnsureParticleMovementShaders(error))return false;
 v->effect=std::move(p);Apply(e,*v);return true;
}
bool ParticleEffectRuntimeService::SetTexture(wi::ecs::Entity e,size_t n,size_t slot,const StableId& id,std::string& error){
 auto* v=Track(e,error);if(!v)return false;if(n>=v->layers.size()||slot>4||!IsValidStableId(id)){error="Invalid layer, texture slot or asset ID.";return false;}
 PreparedMaterialTextureAsset prepared;if(!PrepareMaterialTextureAsset(root_,project_,id,prepared,error))return false;
 auto resource=LoadPreparedMaterialTextureAsset(prepared,error);if(!resource.IsValid()){if(error.empty())error="Texture could not be loaded.";return false;}
 const MaterialTextureSlot slots[]={MaterialTextureSlot::BaseColor,MaterialTextureSlot::Normal,MaterialTextureSlot::Surface,MaterialTextureSlot::Emissive,MaterialTextureSlot::Occlusion};
 if(!ApplyPreparedMaterialTextureAsset(scene_,v->layers[n],slots[slot],prepared,[resource](const auto&,std::string& err){err.clear();return resource;},error))return false;
 if(slot==0)v->effect.layers[n].texture=id;else v->effect.layers[n].extraTextures[slot-1]=id;
 if(auto* sp=scene_.sprites.GetComponent(v->layers[n]))sp->textureResource={};
 return true;
}
bool ParticleEffectRuntimeService::Control(wi::ecs::Entity e,const std::string& op,bool clear,std::string& error){
 auto* v=Track(e,error);if(!v)return false;
 if(op=="pause")v->paused=true;
 else if(op=="play"||op=="resume"){v->paused=false;v->stopped=false;}
 else if(op=="restart"){v->paused=false;v->stopped=false;}
 else if(op=="stop"){v->stopped=true;v->paused=false;}
 else {error="Unknown particle control operation.";return false;}
 ControlParticleTiming(scene_,e,op);Apply(e,*v,op=="restart"||(op=="stop"&&clear));return true;
}
bool ParticleEffectRuntimeService::Burst(wi::ecs::Entity e,int count,int chosen,std::string& error){
 auto* v=Track(e,error);if(!v)return false;if(count<1||count>1000000||chosen>=int(v->layers.size())||chosen< -1){error="Invalid burst count or layer.";return false;}
 bool emitted=false;
 for(size_t n=0;n<v->layers.size();++n)if((chosen<0||n==size_t(chosen))&&v->effect.layers[n].enabled&&v->effect.layers[n].playback==ParticlePlayback::Continuous){scene_.emitters.GetComponent(v->layers[n])->Burst(std::min(count,int(scene_.emitters.GetComponent(v->layers[n])->GetMaxParticleCount())));emitted=true;}
 if(!emitted){error="Burst needs an enabled continuous emitter layer.";return false;}return true;
}
bool ParticleEffectRuntimeService::Attach(wi::ecs::Entity e,wi::ecs::Entity parent,const std::string& socket,bool keepWorld,std::string& error){
 auto* v=Track(e,error);if(!v)return false;
 if(!scene_.transforms.Contains(parent)||parent==e||scene_.Entity_IsDescendant(parent,e)){error="Invalid particle attachment parent.";return false;}
 auto target=parent;
 if(!socket.empty()){target=wi::ecs::INVALID_ENTITY;for(size_t n=0;n<scene_.names.GetCount();++n){auto p=scene_.names.GetEntity(n);if(scene_.names[n].name==socket&&scene_.transforms.Contains(p)&&(p==parent||scene_.Entity_IsDescendant(p,parent))){if(target!=wi::ecs::INVALID_ENTITY){error="Attachment socket name is ambiguous.";return false;}target=p;}}if(target==wi::ecs::INVALID_ENTITY){error="Named socket was not found under the parent.";return false;}}
 if(target==e||scene_.Entity_IsDescendant(target,e)){error="Particle attachment would create a cycle.";return false;}
 scene_.Component_Attach(e,target,!keepWorld);v->parent=target;v->attachedParent=PersistentEntityId(scene_,target);return true;
}
bool ParticleEffectRuntimeService::Detach(wi::ecs::Entity e,std::string& error){auto* v=Track(e,error);if(!v)return false;scene_.Component_Detach(e);v->parent=wi::ecs::INVALID_ENTITY;v->attachedParent.clear();return true;}
bool ParticleEffectRuntimeService::GetTransform(wi::ecs::Entity e,TransformState& t,std::string& error){if(!Track(e,error))return false;auto* p=scene_.transforms.GetComponent(e);t.translation=p->translation_local;t.rotation=p->rotation_local;t.scale=p->scale_local;return true;}
bool ParticleEffectRuntimeService::SetTransform(wi::ecs::Entity e,const TransformState& t,std::string& error){
 if(!Track(e,error))return false;float q=t.rotation.x*t.rotation.x+t.rotation.y*t.rotation.y+t.rotation.z*t.rotation.z+t.rotation.w*t.rotation.w;
 if(!Finite(t.translation)||!Finite(t.scale)||!std::isfinite(q)||q<.000001f||t.scale.x<=0||t.scale.y<=0||t.scale.z<=0){error="Particle transform must be finite with positive scale and nonzero rotation.";return false;}
 auto* p=scene_.transforms.GetComponent(e);p->translation_local=t.translation;p->scale_local=t.scale;XMStoreFloat4(&p->rotation_local,XMQuaternionNormalize(XMLoadFloat4(&t.rotation)));p->SetDirty();p->UpdateTransform();return true;
}
bool ParticleEffectRuntimeService::Destroy(wi::ecs::Entity e,std::string& error){if(!Track(e,error))return false;scene_.Entity_Remove(e);live_.erase(e);return true;}
void ParticleEffectRuntimeService::Clear(){for(auto& pair:live_)if(pair.second.spawned&&PersistentEntityId(scene_,pair.first)==pair.second.id)scene_.Entity_Remove(pair.first);live_.clear();}
}
