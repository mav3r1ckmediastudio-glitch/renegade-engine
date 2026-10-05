#pragma once
#include "renegade/bridge/PlayerViewHandBlend.h"
static bool TestPlayerViewHandBlends() {
 using namespace renegade::runtime;
 using Animation=wi::scene::AnimationComponent;
 wi::scene::Scene scene;
 const auto right=scene.Entity_CreateTransform("Right"),left=scene.Entity_CreateTransform("Left");
 scene.transforms.GetComponent(left)->translation_local={7,8,9};
 const auto entity=wi::ecs::CreateEntity();
 scene.animations.Create(entity).Pause();
 for(auto path:{Animation::AnimationChannel::Path::TRANSLATION,
   Animation::AnimationChannel::Path::ROTATION,Animation::AnimationChannel::Path::SCALE}) {
  const auto dataEntity=wi::ecs::CreateEntity();
  auto& data=scene.animation_datas.Create(dataEntity);data.keyframe_times={0,1};
  data.keyframe_data=path==Animation::AnimationChannel::Path::ROTATION?
   wi::vector<float>{0,0,0.707106781f,0.707106781f,0,0,0.707106781f,0.707106781f}:
   path==Animation::AnimationChannel::Path::SCALE?
   wi::vector<float>{3,3,3,3,3,3}:wi::vector<float>{10,0,0,10,0,0};
  auto* clip=scene.animations.GetComponent(entity);
  Animation::AnimationSampler sampler;sampler.data=dataEntity;
  Animation::AnimationChannel channel;channel.target=right;channel.path=path;
  channel.samplerIndex=int(clip->samplers.size());clip->samplers.push_back(sampler);clip->channels.push_back(channel);
 }
 RuntimePlayerHandBlend blend;blend.bones={right};
 blend.clip=wi::ecs::CreateEntity();
 auto evaluate=[&](float time,float dt) {
  auto* clip=scene.animations.GetComponent(entity);clip->end=1;clip->timer=0;
  clip->amount=PrepareRuntimePlayerHandBlend(scene,blend,entity,time,dt,0.1f);
  clip->last_update_time=-std::numeric_limits<float>::max();
  scene.dt=1.0f/60;scene.ScanAnimationDependencies();
  wi::jobsystem::context context;scene.RunAnimationUpdateSystem(context);wi::jobsystem::Wait(context);
 };
 wi::jobsystem::Initialize();
 evaluate(0,0.01f);
 if(std::abs(scene.transforms.GetComponent(right)->translation_local.x)>0.001f)return false;
 evaluate(0.05f,0.05f);
 const auto* tr=scene.transforms.GetComponent(right);
 if(std::abs(tr->translation_local.x-5)>0.001f || std::abs(tr->scale_local.x-2)>0.001f ||
    std::abs(tr->rotation_local.z-0.38268343f)>0.001f)return false;
 // A fixed origin is restored: evaluate at the same weight again without drift.
 evaluate(0.05f,0.000001f);
 if(std::abs(scene.transforms.GetComponent(right)->translation_local.x-5)>0.001f)return false;
 const auto before=scene.transforms.GetComponent(right)->translation_local.x;
 const float elapsed=blend.elapsed;
 PrepareRuntimePlayerHandBlend(scene,blend,entity,0.05f,0,0.1f);
 if(blend.elapsed!=elapsed || scene.transforms.GetComponent(right)->translation_local.x!=before)return false;
 // Interruption/restart snapshots the currently displayed midpoint, not old idle.
 evaluate(0,0.01f);
 if(std::abs(scene.transforms.GetComponent(right)->translation_local.x-before)>0.001f)return false;
 evaluate(0.1f,0.1f);
 if(std::abs(scene.transforms.GetComponent(right)->translation_local.x-10)>0.001f)return false;
 const auto held=scene.transforms.GetComponent(left)->translation_local;
 if(held.x!=7||held.y!=8||held.z!=9)return false;
 return true;
}
