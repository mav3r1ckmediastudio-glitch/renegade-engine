#pragma once
#include <WickedEngine.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace renegade::runtime {
// Transition from the last evaluated local pose. Wicked still samples the
// destination clip and interpolates T/R/S through AnimationComponent::amount.
// Restoring this fixed origin each frame avoids accumulated, frame-rate-dependent
// lerps; restarting from the visible pose makes interrupted fades continuous.
struct RuntimePlayerHandBlend {
 struct Pose { XMFLOAT3 position,scale; XMFLOAT4 rotation; };
 std::vector<wi::ecs::Entity> bones;
 std::vector<Pose> origin;
 wi::ecs::Entity clip=wi::ecs::INVALID_ENTITY;
 float time=0,elapsed=0,duration=0;
};
inline float PrepareRuntimePlayerHandBlend(wi::scene::Scene& scene,
 RuntimePlayerHandBlend& blend,wi::ecs::Entity clip,float time,float dt,float duration) {
 if(blend.clip==wi::ecs::INVALID_ENTITY) {
  blend.clip=clip;blend.time=time;return 1;
 }
 const bool advancing=std::isfinite(dt)&&dt>0;
 if(!advancing) {
  const float f=blend.duration>0?std::clamp(blend.elapsed/blend.duration,0.0f,1.0f):1;
  return f*f*(3-2*f);
 }
 if(clip!=blend.clip || time+0.00001f<blend.time) {
  blend.origin.clear();
  for(const auto bone:blend.bones) {
   const auto* t=scene.transforms.GetComponent(bone);
   blend.origin.push_back({t->translation_local,t->scale_local,t->rotation_local});
  }
  blend.elapsed=0;blend.duration=std::isfinite(duration)?std::max(0.0f,duration):0;
 } else blend.elapsed=std::min(blend.elapsed+dt,blend.duration);
 blend.clip=clip;blend.time=time;
 const float fraction=blend.duration>0?std::clamp(blend.elapsed/blend.duration,0.0f,1.0f):1;
 const float weight=fraction*fraction*(3-2*fraction);
 if(weight<1 && blend.origin.size()==blend.bones.size()) {
  for(size_t i=0;i<blend.bones.size();++i) {
   auto* t=scene.transforms.GetComponent(blend.bones[i]);
   t->translation_local=blend.origin[i].position;
   t->rotation_local=blend.origin[i].rotation;
   t->scale_local=blend.origin[i].scale;t->SetDirty();
  }
 }
 return weight;
}
}
