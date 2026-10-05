#pragma once
#include "renegade/bridge/PlayerViewAnimationMask.h"
#include "renegade/bridge/PlayerViewRig.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include <array>

namespace renegade::runtime {
struct RuntimePlayerHandAnimationState {
 bool enabled=false,attacking=false;
 std::array<std::vector<wi::ecs::Entity>,16> primary;
 std::array<wi::ecs::Entity,3> offMovement={wi::ecs::INVALID_ENTITY,wi::ecs::INVALID_ENTITY,wi::ecs::INVALID_ENTITY};
 std::array<wi::ecs::Entity,3> block={wi::ecs::INVALID_ENTITY,wi::ecs::INVALID_ENTITY,wi::ecs::INVALID_ENTITY};
 std::vector<wi::ecs::Entity> generated,sourceClips;
 wi::ecs::Entity base=wi::ecs::INVALID_ENTITY,right=wi::ecs::INVALID_ENTITY,left=wi::ecs::INVALID_ENTITY;
 float baseTime=0,rightTime=0,leftTime=0;
 unsigned blockPhase=0,attackVariant=0;
 PlayerViewAction action=PlayerViewAction::Idle;
};
inline void ResetRuntimePlayerHandAnimations(wi::scene::Scene& scene,RuntimePlayerHandAnimationState& state) {
 for(const auto entity:state.generated)scene.Entity_Remove(entity);
 state={};
}
inline int PlayerHandSemanticIndex(const std::string& action) {
 static const std::array<const char*,16> names={"Idle","Walk","Run","Attack","Reload","ReloadPartial",
  "AimIn","AimOut","AimAttack","Equip","Unequip","JumpStart","JumpLoop","JumpLand","Charge","Release"};
 for(size_t i=0;i<names.size();++i)if(action==names[i])return int(i);
 return -1;
}
inline bool InitializeRuntimePlayerHandAnimations(wi::scene::Scene& scene,wi::ecs::Entity viewRoot,
 RuntimePlayerHandAnimationState& state,std::string& error) {
 RuntimePlayerHandAnimationState prepared;
 wi::ecs::Entity primaryRoot=wi::ecs::INVALID_ENTITY,offRoot=wi::ecs::INVALID_ENTITY,armature=wi::ecs::INVALID_ENTITY;
 const auto within=[&](wi::ecs::Entity e){return e==viewRoot||scene.Entity_IsDescendant(e,viewRoot);};
 for(size_t i=0;i<scene.metadatas.GetCount();++i) {
  const auto e=scene.metadatas.GetEntity(i);if(!within(e))continue;
  const auto& m=scene.metadatas[i];
  if(m.bool_values.has("renegade.first_person.independent_hands")&&m.bool_values.get("renegade.first_person.independent_hands"))
   prepared.enabled=true;
 }
 if(!prepared.enabled)return true;
 for(size_t i=0;i<scene.armatures.GetCount();++i)if(within(scene.armatures.GetEntity(i))) {
  if(armature!=wi::ecs::INVALID_ENTITY){error="Independent hands require one view armature.";return false;}
  armature=scene.armatures.GetEntity(i);
 }
 for(size_t i=0;i<scene.metadatas.GetCount();++i) {
  const auto e=scene.metadatas.GetEntity(i);if(!within(e))continue;
  const auto& m=scene.metadatas[i];
  for(const auto& role:std::array<std::pair<const char*,wi::ecs::Entity*>,2>{{
   {"renegade.first_person.primary_layer_root",&primaryRoot},{"renegade.first_person.off_hand_layer_root",&offRoot}}})
   if(m.bool_values.has(role.first)&&m.bool_values.get(role.first)) {
    if(*role.second!=wi::ecs::INVALID_ENTITY){error="Ambiguous hand layer root.";return false;}
    *role.second=e;
   }
 }
 bridge::PlayerViewBonePartition partition;
 if(!bridge::CollectPlayerViewBonePartition(scene,armature,primaryRoot,offRoot,partition,error))return false;
 const auto cleanup=[&](){for(const auto e:prepared.generated)scene.Entity_Remove(e);};
 const auto create=[&](wi::ecs::Entity source,const std::vector<wi::ecs::Entity>& targets) {
  wi::ecs::Entity e=wi::ecs::INVALID_ENTITY;
  if(bridge::CreatePlayerViewMaskedAnimationClip(scene,source,targets,e,error))prepared.generated.push_back(e);
  return e;
 };
 struct Source {wi::ecs::Entity entity;std::string action;};
 std::vector<Source> sources;
 const size_t count=scene.animations.GetCount();
 for(size_t i=0;i<count;++i) {
  const auto e=scene.animations.GetEntity(i);
  const auto& clip=scene.animations[i];
  if(std::none_of(clip.channels.begin(),clip.channels.end(),[&](const auto& c){return within(c.target);}))continue;
  const auto* m=scene.metadatas.GetComponent(e);
  if(!m||!m->string_values.has(bridge::CreatorCharacterAnimationActionMetadataKey))continue;
  sources.push_back({e,m->string_values.get(bridge::CreatorCharacterAnimationActionMetadataKey)});
  prepared.sourceClips.push_back(e);
 }
 std::stable_sort(sources.begin(),sources.end(),[&](const auto& a,const auto& b) {
  if(a.action!="Attack" || b.action!="Attack")return a.action=="Attack" && b.action!="Attack";
  const auto order=[&](auto e) {
   const auto* m=scene.metadatas.GetComponent(e);
   return m&&m->int_values.has("renegade.first_person.attack_order")?m->int_values.get("renegade.first_person.attack_order"):0;
  };
  return order(a.entity)<order(b.entity);
 });
 for(const auto& source:sources) {
  const int index=PlayerHandSemanticIndex(source.action);
  if(index>=0) {
   const auto right=create(source.entity,partition.primary);
   if(right==wi::ecs::INVALID_ENTITY){cleanup();return false;}
   prepared.primary[index].push_back(right);
   if(index<3) {
    if(prepared.offMovement[index]!=wi::ecs::INVALID_ENTITY){error="Duplicate movement hand binding.";cleanup();return false;}
    prepared.offMovement[index]=create(source.entity,partition.offHand);
    if(prepared.offMovement[index]==wi::ecs::INVALID_ENTITY){cleanup();return false;}
   }
   if(index==0)prepared.base=create(source.entity,partition.base);
  } else {
   const int block=source.action=="BlockStart"?0:source.action=="BlockLoop"?1:source.action=="BlockEnd"?2:-1;
   if(block>=0) {
    if(prepared.block[block]!=wi::ecs::INVALID_ENTITY){error="Duplicate shield binding.";cleanup();return false;}
    prepared.block[block]=create(source.entity,partition.offHand);
    if(prepared.block[block]==wi::ecs::INVALID_ENTITY){cleanup();return false;}
   }
  }
 }
 if(prepared.base==wi::ecs::INVALID_ENTITY||prepared.primary[0].size()!=1||prepared.primary[3].empty()||
    std::any_of(prepared.block.begin(),prepared.block.end(),[](auto e){return e==wi::ecs::INVALID_ENTITY;})) {
  error="Independent hands require Idle, Attack and shield start/loop/end clips.";cleanup();return false;
 }
 for(const auto e:prepared.sourceClips){auto& clip=*scene.animations.GetComponent(e);clip.Pause();clip.RootMotionOff();clip.amount=0;}
 state=std::move(prepared);error.clear();return true;
}
inline void UpdateRuntimePlayerHandAnimations(wi::scene::Scene& scene,RuntimePlayerHandAnimationState& state,
 PlayerViewAction movement,float dt,bool attackPressed,bool blockHeld) {
 if(!state.enabled)return;
 const bool advancing=std::isfinite(dt)&&dt>0;
 const unsigned move=movement==PlayerViewAction::Sprint?2:movement==PlayerViewAction::Walk?1:0;
 const unsigned available=state.primary[move].empty()?0:move;
 if(advancing&&!state.attacking&&attackPressed) {
  state.attacking=true;state.action=PlayerViewAction::Attack;state.rightTime=0;
  const auto& attacks=state.primary[3];state.right=attacks[state.attackVariant++%attacks.size()];
 }
 if(!state.attacking) {
  const auto next=state.primary[available].front();
  if(state.right!=next){state.right=next;state.rightTime=0;}
  state.action=available==2?PlayerViewAction::Sprint:available==1?PlayerViewAction::Walk:PlayerViewAction::Idle;
 }
 if(advancing) {
  if(blockHeld&&state.blockPhase==0){state.blockPhase=1;state.leftTime=0;}
  if(!blockHeld&&(state.blockPhase==1||state.blockPhase==2)){state.blockPhase=3;state.leftTime=0;}
 }
 const auto nextLeft=state.blockPhase?state.block[state.blockPhase-1]:
  state.offMovement[available]==wi::ecs::INVALID_ENTITY?state.offMovement[0]:state.offMovement[available];
 if(nextLeft!=state.left){state.left=nextLeft;state.leftTime=0;}
 const auto advance=[&](wi::ecs::Entity entity,float& time,bool loop) {
  const auto* clip=scene.animations.GetComponent(entity);
  const float duration=clip->end-clip->start;
  if(advancing&&duration>0)time=loop?std::fmod(time+dt,duration):std::min(time+dt,duration);
  return duration<=0||time>=duration;
 };
 advance(state.base,state.baseTime,true);
 const bool attackEnded=advance(state.right,state.rightTime,!state.attacking);
 const bool leftEnded=advance(state.left,state.leftTime,state.blockPhase==0||state.blockPhase==2);
 for(const auto e:state.generated){auto& c=*scene.animations.GetComponent(e);c.Pause();c.RootMotionOff();c.amount=0;}
 const auto pose=[&](wi::ecs::Entity entity,float time) {
  auto& c=*scene.animations.GetComponent(entity);c.amount=1;c.timer=std::clamp(c.start+time,c.start,c.end);
  c.last_update_time=-std::numeric_limits<float>::max();
 };
 pose(state.base,state.baseTime);pose(state.right,state.rightTime);pose(state.left,state.leftTime);
 if(advancing&&state.attacking&&attackEnded)state.attacking=false;
 if(advancing&&leftEnded&&state.blockPhase==1){state.blockPhase=2;state.leftTime=0;}
 else if(advancing&&leftEnded&&state.blockPhase==3){state.blockPhase=0;state.leftTime=0;}
}
}
