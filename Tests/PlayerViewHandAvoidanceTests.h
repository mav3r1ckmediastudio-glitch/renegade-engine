#pragma once
#include <iostream>
#include "renegade/bridge/PlayerViewHandAvoidance.h"
static bool TestPlayerViewHandAvoidance() {
 std::cerr<<"avoidance begin\n";
 using namespace renegade::runtime;
 const XMFLOAT3 half={1,1,1};
 if(!PlayerBladeIntersectsBox({-2,0,0},{2,0,0},half) ||
    PlayerBladeIntersectsBox({-2,2,0},{2,2,0},half) ||
    !PlayerBladeIntersectsBox({0,0,0},{0,0,0},half))return false;
 const auto escape=PlayerBladeEscapeBox({-2,0,0},{2,0,0},half);
 if(PlayerBladeIntersectsBox({-2+escape.x,escape.y,escape.z},{2+escape.x,escape.y,escape.z},half))return false;
 // Proxy radius follows the relative authored sword/shield scale.
 RuntimePlayerHandAvoidance scaled;scaled.pose=std::make_unique<wi::scene::Scene>();
 scaled.blade=scaled.pose->Entity_CreateTransform("Scaled blade");
 scaled.shield=scaled.pose->Entity_CreateTransform("Scaled shield");
 scaled.radius=0.02f;scaled.halfExtents={0.1f,0.2f,0.3f};
 scaled.pose->transforms.GetComponent(scaled.blade)->scale_local={2,2,2};
 scaled.pose->transforms.GetComponent(scaled.shield)->scale_local={0.5f,0.5f,0.5f};
 UpdatePlayerHandPoseWorld(*scaled.pose);XMFLOAT3 sa,sb,se;
 RuntimePlayerBladeIntersection(scaled,sa,sb,se);
 if(std::abs(se.x-0.18f)>0.0001f||std::abs(se.y-0.28f)>0.0001f||std::abs(se.z-0.38f)>0.0001f)return false;
 wi::scene::Scene scene;
 const auto root=scene.Entity_CreateTransform("Root"),upper=scene.Entity_CreateTransform("Upper");
 const auto lower=scene.Entity_CreateTransform("Lower"),hand=scene.Entity_CreateTransform("Hand");
 const auto left=scene.Entity_CreateTransform("Left"),blade=scene.Entity_CreateTransform("Blade");
 const auto shield=scene.Entity_CreateTransform("Shield");
 scene.Component_Attach(upper,root,true);scene.Component_Attach(lower,upper,true);
 scene.Component_Attach(hand,lower,true);scene.Component_Attach(left,root,true);
 scene.Component_Attach(blade,hand,true);scene.Component_Attach(shield,left,true);
 scene.transforms.GetComponent(lower)->translation_local={0.3f,0,0};
 scene.transforms.GetComponent(hand)->translation_local={0.24f,0.18f,0};
 scene.transforms.GetComponent(left)->translation_local={0.6f,0.2f,0};
 auto& m=scene.metadatas.Create(root);m.bool_values.set("renegade.first_person.avoid_off_hand",true);
 const auto vector=[&](const char* name,const XMFLOAT3& value) {
  for(int axis=0;axis<3;++axis)m.float_values.set(std::string(name)+std::array<const char*,3>{"_x","_y","_z"}[axis],(&value.x)[axis]);
 };
 vector("blade_base",{0,0,0});vector("blade_tip",{0,0.4f,0});
 vector("shield_center",{});vector("shield_half_extents",{0.1f,0.1f,0.1f});
 m.float_values.set("blade_radius",0.015f);m.float_values.set("maximum_correction",0.35f);
 scene.metadatas.Create(blade).bool_values.set("renegade.first_person.blade_proxy",true);
 scene.metadatas.Create(shield).bool_values.set("renegade.first_person.shield_proxy",true);
 const auto clip=wi::ecs::CreateEntity(),data=wi::ecs::CreateEntity();
 scene.animation_datas.Create(data).keyframe_times={0,1};
 scene.animation_datas.GetComponent(data)->keyframe_data={0,0,0,0,0,0};
 auto& animation=scene.animations.Create(clip);animation.Pause();animation.end=1;animation.amount=1;
 wi::scene::AnimationComponent::AnimationSampler sampler;sampler.data=data;animation.samplers.push_back(sampler);
 wi::scene::AnimationComponent::AnimationChannel channel;channel.target=root;channel.samplerIndex=0;
 channel.path=wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION;animation.channels.push_back(channel);
 const renegade::bridge::PlayerViewBonePartition partition={{root},{upper,lower,hand},{left}};
 RuntimePlayerHandAvoidance state;std::string error;
 if(!InitializeRuntimePlayerHandAvoidance(scene,root,partition,{clip},state,error)||!state.enabled)return false;
 std::cerr<<"avoidance initialized\n";
 UpdatePlayerHandPoseWorld(scene);
 const auto originalLeft=scene.transforms.GetComponent(left)->world;
 EvaluateRuntimePlayerHandAvoidance(scene,state,{clip},1.0f/60);
 std::cerr<<"avoidance evaluated\n";
 XMFLOAT3 a,b,h;
 if(!state.corrections||state.unresolved||RuntimePlayerBladeIntersection(state,a,b,h)) {std::cerr<<"collision count "<<state.corrections<<" unresolved "<<state.unresolved<<" chain "<<state.chainLength<<" a="<<a.x<<","<<a.y<<","<<a.z<<" b="<<b.x<<","<<b.y<<","<<b.z<<" offset="<<state.offset.x<<","<<state.offset.y<<","<<state.offset.z<<"\n";return false;}
 UpdatePlayerHandPoseWorld(scene);
 const auto* corrected=scene.transforms.GetComponent(hand);
 // Wrist orientation and bone lengths preserved; off-hand world pose untouched.
 XMVECTOR rs,rq,rt; if(!XMMatrixDecompose(&rs,&rq,&rt,XMLoadFloat4x4(&corrected->world))) {std::cerr<<"invalid wrist matrix\n";return false;}
 XMFLOAT4 rotation;XMStoreFloat4(&rotation,rq);
 if(std::abs(rotation.x)>0.001f||std::abs(rotation.y)>0.001f||std::abs(rotation.z)>0.001f) {std::cerr<<"wrist orientation changed "<<rotation.x<<","<<rotation.y<<","<<rotation.z<<"\n";return false;}
 const auto length=[&](wi::ecs::Entity x,wi::ecs::Entity y) {
  return XMVectorGetX(XMVector3Length(scene.transforms.GetComponent(x)->GetPositionV()-scene.transforms.GetComponent(y)->GetPositionV()));
 };
 if(std::abs(length(upper,lower)-0.3f)>0.001f||std::abs(length(lower,hand)-0.3f)>0.001f) {std::cerr<<"bone lengths "<<length(upper,lower)<<","<<length(lower,hand)<<"\n";return false;}
 const float* original=&originalLeft._11;const float* after=&scene.transforms.GetComponent(left)->world._11;
 for(int k=0;k<16;++k)if(std::abs(original[k]-after[k])>0.0001f)return false;
 const auto count=state.corrections;const auto position=corrected->translation_local;
 EvaluateRuntimePlayerHandAvoidance(scene,state,{clip},0);
 if(state.corrections!=count||corrected->translation_local.x!=position.x||scene.inverse_kinematics.GetCount()!=0)return false;
 // A fully extended chain must never commit NaNs, even when a face target is unreachable.
 for(const auto e:{upper,lower,hand}) {
  auto* t=scene.transforms.GetComponent(e);t->rotation_local={0,0,0,1};t->scale_local={1,1,1};t->SetDirty();
 }
 scene.transforms.GetComponent(lower)->translation_local={0.3f,0,0};
 scene.transforms.GetComponent(hand)->translation_local={0.3f,0,0};
 scene.animations.GetComponent(clip)->amount=1;state.offset={};
 EvaluateRuntimePlayerHandAvoidance(scene,state,{clip},1.0f/60);
 UpdatePlayerHandPoseWorld(scene);
 for(const auto e:state.bones) {
  const auto& world=scene.transforms.GetComponent(e)->world;
  for(int k=0;k<16;++k)if(!std::isfinite((&world._11)[k])) {std::cerr<<"straight invalid "<<e<<" element "<<k<<"\n";return false;}
 }
 if(std::abs(length(upper,lower)-0.3f)>0.001f||std::abs(length(lower,hand)-0.3f)>0.001f) {std::cerr<<"straight lengths "<<length(upper,lower)<<","<<length(lower,hand)<<"\n";return false;}
 return true;
}
