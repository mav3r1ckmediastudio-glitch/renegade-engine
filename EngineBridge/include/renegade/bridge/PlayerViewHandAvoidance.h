#pragma once
#include "renegade/bridge/PlayerViewAnimationMask.h"
#include <memory>

namespace renegade::runtime {
struct RuntimePlayerHandAvoidance {
 bool enabled=false,intersecting=false;
 unsigned corrections=0,unresolved=0,chainLength=0;
 wi::ecs::Entity root=0,blade=0,shield=0,hand=0;
 XMFLOAT3 bladeBase={},bladeTip={},shieldCenter={},halfExtents={},offset={};
 float radius=0.015f,maximum=0.35f,residual=0;
 std::vector<wi::ecs::Entity> bones;
 std::shared_ptr<wi::scene::Scene> pose;
};
// Conservative segment against an expanded shield box in authored shield space.
// The segment is the blade capsule centreline; radius expands each box axis.
inline bool PlayerBladeIntersectsBox(const XMFLOAT3& a,const XMFLOAT3& b,const XMFLOAT3& half) {
 float enter=0,leave=1;
 const float* av=&a.x;const float* bv=&b.x;const float* hv=&half.x;
 for(int axis=0;axis<3;++axis) {
  const float d=bv[axis]-av[axis];
  if(std::abs(d)<0.000001f) {if(av[axis]<-hv[axis]||av[axis]>hv[axis])return false;}
  else {
   float lo=(-hv[axis]-av[axis])/d,hi=(hv[axis]-av[axis])/d;
   if(lo>hi)std::swap(lo,hi);enter=std::max(enter,lo);leave=std::min(leave,hi);
   if(enter>leave)return false;
  }
 }
 return true;
}
inline XMFLOAT3 PlayerBladeEscapeBox(const XMFLOAT3& a,const XMFLOAT3& b,const XMFLOAT3& half) {
 XMFLOAT3 best={};float distance=std::numeric_limits<float>::max();
 for(int axis=0;axis<3;++axis) {
  const float minimum=std::min((&a.x)[axis],(&b.x)[axis]);
  const float maximum=std::max((&a.x)[axis],(&b.x)[axis]);
  for(const float delta:{(&half.x)[axis]-minimum+0.003f,-(&half.x)[axis]-maximum-0.003f})
   if(std::abs(delta)<distance) {best={};(&best.x)[axis]=delta;distance=std::abs(delta);}
 }
 return best;
}
inline XMMATRIX PlayerHandWorld(const wi::scene::Scene& scene,wi::ecs::Entity e) {
 XMMATRIX matrix=XMMatrixIdentity();
 for(size_t n=0;n<=scene.hierarchy.GetCount();++n) {
  const auto* t=scene.transforms.GetComponent(e);if(!t)break;
  matrix*=t->GetLocalMatrix();
  const auto* h=scene.hierarchy.GetComponent(e);if(!h)break;e=h->parentID;
 }
 return matrix;
}
inline void UpdatePlayerHandPoseWorld(wi::scene::Scene& pose) {
 wi::jobsystem::context ctx;
 pose.RunTransformUpdateSystem(ctx);wi::jobsystem::Wait(ctx);
 pose.RunHierarchyUpdateSystem(ctx);wi::jobsystem::Wait(ctx);
}
inline bool InitializeRuntimePlayerHandAvoidance(wi::scene::Scene& scene,
 wi::ecs::Entity viewRoot,const bridge::PlayerViewBonePartition& partition,
 const std::vector<wi::ecs::Entity>& generated,RuntimePlayerHandAvoidance& out,std::string& error) {
 const wi::scene::MetadataComponent* settings=nullptr;
 for(size_t i=0;i<scene.metadatas.GetCount();++i) {
  const auto e=scene.metadatas.GetEntity(i);
  if(e!=viewRoot&&!scene.Entity_IsDescendant(e,viewRoot))continue;
  const auto& m=scene.metadatas[i];
  if(m.bool_values.has("renegade.first_person.avoid_off_hand")&&m.bool_values.get("renegade.first_person.avoid_off_hand")) {
   if(settings){error="Ambiguous hand avoidance settings.";return false;}settings=&m;out.root=e;
  }
  if(m.bool_values.has("renegade.first_person.blade_proxy")&&m.bool_values.get("renegade.first_person.blade_proxy")) {
   if(out.blade){error="Ambiguous blade proxy.";return false;}out.blade=e;
  }
  if(m.bool_values.has("renegade.first_person.shield_proxy")&&m.bool_values.get("renegade.first_person.shield_proxy")) {
   if(out.shield){error="Ambiguous shield proxy.";return false;}out.shield=e;
  }
 }
 if(!settings)return true;
 if(!out.blade||!out.shield||!scene.hierarchy.Contains(out.blade)) {error="Missing authored hand avoidance proxies.";return false;}
 out.hand=scene.hierarchy.GetComponent(out.blade)->parentID;
 if(std::find(partition.primary.begin(),partition.primary.end(),out.hand)==partition.primary.end()) {
  error="Blade proxy must be attached to the primary hand partition.";return false;
 }
 for(auto e=out.hand;out.chainLength<32;) {
  const auto* parent=scene.hierarchy.GetComponent(e);if(!parent)break;e=parent->parentID;
  if(std::find(partition.primary.begin(),partition.primary.end(),e)==partition.primary.end())break;
  ++out.chainLength;
 }
 if(out.chainLength==0 || out.chainLength>=32){error="Invalid primary IK chain.";return false;}
 const auto vector=[&](const char* name,XMFLOAT3& v) {
  for(int axis=0;axis<3;++axis) {
   const auto key=std::string(name)+std::array<const char*,3>{"_x","_y","_z"}[axis];
   if(!settings->float_values.has(key))return false;
   (&v.x)[axis]=settings->float_values.get(key);if(!std::isfinite((&v.x)[axis]))return false;
  }
  return true;
 };
 if(!vector("blade_base",out.bladeBase)||!vector("blade_tip",out.bladeTip)||
    !vector("shield_center",out.shieldCenter)||!vector("shield_half_extents",out.halfExtents)||
    !settings->float_values.has("blade_radius")||!settings->float_values.has("maximum_correction")) {
  error="Malformed authored hand avoidance proxies.";return false;
 }
 out.radius=settings->float_values.get("blade_radius");out.maximum=settings->float_values.get("maximum_correction");
 if(!std::isfinite(out.radius)||out.radius<=0||out.radius>0.1f||!std::isfinite(out.maximum)||out.maximum<=0||out.maximum>0.5f||
    out.halfExtents.x<=0||out.halfExtents.y<=0||out.halfExtents.z<=0) {error="Invalid hand avoidance dimensions.";return false;}
 out.bones=partition.base;out.bones.insert(out.bones.end(),partition.primary.begin(),partition.primary.end());
 out.bones.insert(out.bones.end(),partition.offHand.begin(),partition.offHand.end());
 out.pose=std::make_shared<wi::scene::Scene>();
 auto& pose=*out.pose;
 for(size_t i=0;i<scene.transforms.GetCount();++i) {
  const auto e=scene.transforms.GetEntity(i);
  if(e!=out.root&&!scene.Entity_IsDescendant(e,out.root))continue;
  pose.transforms.Create(e)=scene.transforms[i];
  if(e!=out.root&&scene.hierarchy.Contains(e))pose.hierarchy.Create(e)=*scene.hierarchy.GetComponent(e);
 }
 for(const auto e:generated) {
  const auto* clip=scene.animations.GetComponent(e);pose.animations.Create(e)=*clip;
  for(const auto& sampler:clip->samplers)
   if(!pose.animation_datas.Contains(sampler.data))pose.animation_datas.Create(sampler.data)=*scene.animation_datas.GetComponent(sampler.data);
 }
 out.enabled=true;return true;
}
inline bool RuntimePlayerBladeIntersection(RuntimePlayerHandAvoidance& state,XMFLOAT3& a,XMFLOAT3& b,XMFLOAT3& extents) {
 const auto& pose=*state.pose;
 const auto inverse=XMMatrixInverse(nullptr,XMLoadFloat4x4(&pose.transforms.GetComponent(state.shield)->world));
 const auto blade=XMLoadFloat4x4(&pose.transforms.GetComponent(state.blade)->world);
 const auto matrix=blade*inverse;
 XMStoreFloat3(&a,XMVector3TransformCoord(XMLoadFloat3(&state.bladeBase),matrix)-XMLoadFloat3(&state.shieldCenter));
 XMStoreFloat3(&b,XMVector3TransformCoord(XMLoadFloat3(&state.bladeTip),matrix)-XMLoadFloat3(&state.shieldCenter));
 // Capsule radius, like endpoints, is authored before the uniform mesh scale.
 const float radius=state.radius*std::max({XMVectorGetX(XMVector3Length(matrix.r[0])),
  XMVectorGetX(XMVector3Length(matrix.r[1])),XMVectorGetX(XMVector3Length(matrix.r[2]))});
 extents={state.halfExtents.x+radius,state.halfExtents.y+radius,state.halfExtents.z+radius};
 return PlayerBladeIntersectsBox(a,b,extents);
}
// Native evaluation happens once on this small private pose scene. Its resulting
// locals are committed before the world scene's normal hierarchy/skinning update.
// World animations are never evaluated here. No IK component enters saved assets.
inline void EvaluateRuntimePlayerHandAvoidance(wi::scene::Scene& scene,
 RuntimePlayerHandAvoidance& state,const std::vector<wi::ecs::Entity>& generated,float dt) {
 if(!state.enabled||!std::isfinite(dt)||dt<=0)return;
 auto& pose=*state.pose;pose.dt=dt;
 for(size_t i=0;i<pose.transforms.GetCount();++i) {
  const auto e=pose.transforms.GetEntity(i);pose.transforms[i]=*scene.transforms.GetComponent(e);
 }
 auto* root=pose.transforms.GetComponent(state.root);
 XMStoreFloat4x4(&root->world,PlayerHandWorld(scene,state.root));root->ApplyTransform();root->SetDirty();
 for(const auto e:generated) {
  pose.animations.GetComponent(e)->amount=scene.animations.GetComponent(e)->amount;
  pose.animations.GetComponent(e)->timer=scene.animations.GetComponent(e)->timer;
  pose.animations.GetComponent(e)->last_update_time=-std::numeric_limits<float>::max();
 }
 wi::jobsystem::context ctx;
 pose.ScanAnimationDependencies();pose.RunAnimationUpdateSystem(ctx);wi::jobsystem::Wait(ctx);
 UpdatePlayerHandPoseWorld(pose);
 // Hold the authored world wrist orientation while native CCD moves the chain.
 XMVECTOR scale,wristRotation,position;
 XMMatrixDecompose(&scale,&wristRotation,&position,XMLoadFloat4x4(&pose.transforms.GetComponent(state.hand)->world));
 const XMVECTOR authoredHand=position;
 XMFLOAT3 a,b,extents;bool hit=RuntimePlayerBladeIntersection(state,a,b,extents);
 XMVECTOR displacement=XMVectorZero();
 // Immediate separation, eased return. Easing entry would leave penetration.
 if(!hit) {
  displacement=XMVector3TransformNormal(XMLoadFloat3(&state.offset)*std::exp(-dt/0.10f),
    XMLoadFloat4x4(&pose.transforms.GetComponent(state.shield)->world));
 }
 state.intersecting=hit;
 const auto authored=pose.transforms.GetComponentArray();
 auto closest=authored;
 const auto initialEscape=PlayerBladeEscapeBox(a,b,extents);
 float closestDistance=XMVectorGetX(XMVector3Length(XMLoadFloat3(&initialEscape)));
 const auto restore=[&]() {
  for(size_t i=0;i<authored.size();++i)pose.transforms[i]=authored[i];
  UpdatePlayerHandPoseWorld(pose);
  return true;
 };
 const auto preserveWrist=[&]() -> bool {
  const auto parent=pose.hierarchy.GetComponent(state.hand)->parentID;
  XMVECTOR ps,pq,pt;
  if(!XMMatrixDecompose(&ps,&pq,&pt,XMLoadFloat4x4(&pose.transforms.GetComponent(parent)->world)))return false;
  XMVECTOR ls,lq,lt;
  if(!XMMatrixDecompose(&ls,&lq,&lt,XMMatrixRotationQuaternion(wristRotation)*XMMatrixInverse(nullptr,XMMatrixRotationQuaternion(pq))))return false;
  auto* hand=pose.transforms.GetComponent(state.hand);XMStoreFloat4(&hand->rotation_local,XMQuaternionNormalize(lq));hand->SetDirty();
  UpdatePlayerHandPoseWorld(pose);
  for(const auto e:state.bones) {
   const auto& world=pose.transforms.GetComponent(e)->world;
   for(int k=0;k<16;++k)if(!std::isfinite((&world._11)[k]))return false;
  }
  return true;
 };
 const auto solve=[&](XMVECTOR target) -> bool {
  auto& ik=pose.inverse_kinematics.Create(state.hand);ik.chain_length=state.chainLength;ik.iteration_count=4;
  ik.use_target_position=true;XMStoreFloat3(&ik.target_position,target);
  pose.RunProceduralAnimationUpdateSystem(ctx);wi::jobsystem::Wait(ctx);
  for(const auto e:state.bones) {
   const auto& t=pose.transforms_temp[pose.transforms.GetIndex(e)];
   for(float v:{t.translation_local.x,t.translation_local.y,t.translation_local.z,
       t.rotation_local.x,t.rotation_local.y,t.rotation_local.z,t.rotation_local.w,
       t.scale_local.x,t.scale_local.y,t.scale_local.z})if(!std::isfinite(v))return false;
  }
  // Wicked IK is deliberately a world-pose modifier. Retain its private solved
  // locals before recomputing attachments and committing the pose to Runtime.
  for(const auto e:state.bones) {
   const auto& solved=pose.transforms_temp[pose.transforms.GetIndex(e)];
   auto* t=pose.transforms.GetComponent(e);
   // CCD owns rotations only; keep authored lengths and scale even at singular targets.
   t->rotation_local=solved.rotation_local;t->SetDirty();
  }
  UpdatePlayerHandPoseWorld(pose);
  return preserveWrist();
 };
 if(!hit&&XMVectorGetX(XMVector3Length(displacement))>0.0001f) {
  if(!solve(authoredHand+displacement)){restore();displacement=XMVectorZero();}
  hit=RuntimePlayerBladeIntersection(state,a,b,extents);
 }
 if(hit && !state.intersecting) {
  restore();displacement=XMVectorZero();hit=RuntimePlayerBladeIntersection(state,a,b,extents);
 }
 if(hit) {
  restore();RuntimePlayerBladeIntersection(state,a,b,extents);
  std::array<XMFLOAT3,6> candidates;
  for(int axis=0;axis<3;++axis) {
   candidates[axis*2]={};candidates[axis*2+1]={};
   (&candidates[axis*2].x)[axis]=(&extents.x)[axis]-std::min((&a.x)[axis],(&b.x)[axis])+0.006f;
   (&candidates[axis*2+1].x)[axis]=-(&extents.x)[axis]-std::max((&a.x)[axis],(&b.x)[axis])-0.006f;
  }
  std::stable_sort(candidates.begin(),candidates.end(),[](const auto& x,const auto& y) {
   return XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&x)))<XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&y)));
  });
  for(const auto& candidate:candidates) {
   restore();
   displacement=XMVector3TransformNormal(XMLoadFloat3(&candidate),XMLoadFloat4x4(&pose.transforms.GetComponent(state.shield)->world));
   if(XMVectorGetX(XMVector3Length(displacement))>state.maximum)continue;
   for(unsigned iteration=0;iteration<4;++iteration) {
    if(!solve(authoredHand+displacement)){hit=true;break;}
    hit=RuntimePlayerBladeIntersection(state,a,b,extents);
    if(!hit)break;
    const auto escape=PlayerBladeEscapeBox(a,b,extents);
    const float remaining=XMVectorGetX(XMVector3Length(XMLoadFloat3(&escape)));
    if(remaining<closestDistance){closestDistance=remaining;closest=pose.transforms.GetComponentArray();}
    displacement+=XMVector3TransformNormal(XMLoadFloat3(&escape),XMLoadFloat4x4(&pose.transforms.GetComponent(state.shield)->world));
    if(XMVectorGetX(XMVector3Length(displacement))>state.maximum)break;
   }
   if(!hit)break;
  }
 }

 // Small scapular translation completes near-contact IK without stretching limbs.
 // It is capped independently at 4cm and by the authored total correction bound.
 if(hit && closestDistance<=0.04f) {
  for(size_t i=0;i<closest.size();++i)pose.transforms[i]=closest[i];
  UpdatePlayerHandPoseWorld(pose);
  RuntimePlayerBladeIntersection(state,a,b,extents);
  const auto escape=PlayerBladeEscapeBox(a,b,extents);
  const auto worldDelta=XMVector3TransformNormal(XMLoadFloat3(&escape),XMLoadFloat4x4(&pose.transforms.GetComponent(state.shield)->world));
  auto anchor=state.hand;
  for(unsigned n=0;n<state.chainLength;++n)anchor=pose.hierarchy.GetComponent(anchor)->parentID;
  const auto parent=pose.hierarchy.GetComponent(anchor)->parentID;
  const auto localDelta=XMVector3TransformNormal(worldDelta,XMMatrixInverse(nullptr,XMLoadFloat4x4(&pose.transforms.GetComponent(parent)->world)));
  auto* t=pose.transforms.GetComponent(anchor);
  XMStoreFloat3(&t->translation_local,XMLoadFloat3(&t->translation_local)+localDelta);t->SetDirty();
  UpdatePlayerHandPoseWorld(pose);
  hit=RuntimePlayerBladeIntersection(state,a,b,extents);
  if(XMVectorGetX(XMVector3Length(worldDelta))>0.04f ||
     XMVectorGetX(XMVector3Length(pose.transforms.GetComponent(state.hand)->GetPositionV()-authoredHand))>state.maximum)hit=true;
 }
 if(hit){restore();displacement=XMVectorZero();}
 if(state.intersecting)++state.corrections;
 if(hit){++state.unresolved;state.residual=closestDistance;}
 // Store actual offset in moving shield coordinates for stable relaxation.
 const auto actual=pose.transforms.GetComponent(state.hand)->GetPositionV()-authoredHand;
 const auto inverse=XMMatrixInverse(nullptr,XMLoadFloat4x4(&pose.transforms.GetComponent(state.shield)->world));
 XMStoreFloat3(&state.offset,XMVector3TransformNormal(actual,inverse));
 pose.inverse_kinematics.Clear();
 for(const auto e:state.bones) {
  auto* target=scene.transforms.GetComponent(e);const auto* solved=pose.transforms.GetComponent(e);
  target->translation_local=solved->translation_local;target->rotation_local=solved->rotation_local;
  target->scale_local=solved->scale_local;target->SetDirty();
 }
 for(const auto e:generated)scene.animations.GetComponent(e)->amount=0;
}
}
