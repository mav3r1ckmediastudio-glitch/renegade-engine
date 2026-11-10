#pragma once
#include "renegade/bridge/ParticleEffectService.h"
#include <unordered_map>
namespace renegade::bridge {
// Shared native setting application; resources and authoring history remain separate.
void ApplyParticleEffectLayerSettings(wi::scene::Scene&,wi::ecs::Entity root,wi::ecs::Entity layer,const ParticleEffectLayer&,const std::vector<ParticleMovementField>&,bool resetSprite);
void ApplyParticleEffectFieldMetadata(wi::scene::Scene&,wi::ecs::Entity root,const std::vector<ParticleMovementField>&);
class ParticleEffectRuntimeService {
public:
 ParticleEffectRuntimeService(wi::scene::Scene&,std::string root,StableId project);
 ~ParticleEffectRuntimeService();
 const std::vector<ParticleEffect>& Presets(std::string& error);
 wi::ecs::Entity Spawn(const std::string& preset,const XMFLOAT3&,std::string&);
bool GetTiming(wi::ecs::Entity,ParticleEffectTiming&,std::string&);
 bool SetTiming(wi::ecs::Entity,const ParticleEffectTiming&,std::string&);
  bool Layers(wi::ecs::Entity,std::vector<std::string>&,std::string&);
 bool Fields(wi::ecs::Entity,std::vector<std::string>&,std::string&);
 bool GetLayer(wi::ecs::Entity,size_t,ParticleEffectLayer&,std::string&);
 bool SetLayer(wi::ecs::Entity,size_t,const ParticleEffectLayer&,std::string&);
 bool GetField(wi::ecs::Entity,size_t,ParticleMovementField&,std::string&);
 bool SetField(wi::ecs::Entity,size_t,const ParticleMovementField&,std::string&);
 bool SetTexture(wi::ecs::Entity,size_t,size_t slot,const StableId&,std::string&);
 bool Control(wi::ecs::Entity,const std::string& operation,bool clear,std::string&);
 bool Burst(wi::ecs::Entity,int count,int layer,std::string&);
 bool Attach(wi::ecs::Entity,wi::ecs::Entity parent,const std::string& socket,bool keepWorld,std::string&);
 bool Detach(wi::ecs::Entity,std::string&);
 bool SetTransform(wi::ecs::Entity,const TransformState&,std::string&);
 bool GetTransform(wi::ecs::Entity,TransformState&,std::string&);
 bool Destroy(wi::ecs::Entity,std::string&);
 void Clear();
private:
 struct Live {StableId id;ParticleEffect effect;std::vector<wi::ecs::Entity> layers;std::vector<StableId> layerIds;bool spawned=false,paused=false,stopped=false;StableId attachedParent;wi::ecs::Entity parent=wi::ecs::INVALID_ENTITY;};
 Live* Track(wi::ecs::Entity,std::string&);
 void Apply(wi::ecs::Entity,Live&,bool restart=false);
 wi::scene::Scene& scene_;std::string root_;StableId project_;
 bool catalogLoaded_=false;std::vector<ParticleEffect> presets_;
 std::unordered_map<wi::ecs::Entity,Live> live_;
};
}
