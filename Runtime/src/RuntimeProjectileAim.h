#pragma once
#include "RuntimeProjectileSession.h"
namespace renegade::runtime {
// Camera picks the intended target; muzzle sweep owns actual cover contact.
inline bool ResolveProjectileMuzzleAim(const bridge::ProjectileAssetDocument& asset,
 const bridge::ProjectileSource& source,const XMFLOAT3 eye,const XMFLOAT3 aim,
 const XMFLOAT3& muzzle,const XMFLOAT3& socketForward,
 const bridge::ProjectileSegmentQuery& query,XMFLOAT3& origin,XMFLOAT3& direction,
 std::string& error) {
 if(!query||!bridge::FiniteProjectileVector(ProjectileBridgeVector(eye))||
    !bridge::FiniteProjectileVector(ProjectileBridgeVector(muzzle))) {
  error="Invalid muzzle query or position.";return false;
 }
 const auto forward=XMLoadFloat3(&aim);
 const float length=XMVectorGetX(XMVector3Length(forward));
 if(!std::isfinite(length)||length<0.000001f){error="Invalid camera aim.";return false;}
 const auto unit=forward/length;
 bridge::ProjectileRecord probe;probe.launch.source=source;
 probe.launch.definition=bridge::ProjectileSimulationDefinition(asset);
 origin=muzzle;
 const auto cover=query(probe,ProjectileBridgeVector(eye),ProjectileBridgeVector(muzzle));
 if(cover.status==bridge::ProjectileQueryStatus::Blocked){error="Muzzle cover query was blocked.";return false;}
 if(cover.status==bridge::ProjectileQueryStatus::Hit) {
  const auto travel=XMLoadFloat3(&muzzle)-XMLoadFloat3(&eye);
  const float distance=XMVectorGetX(XMVector3Length(travel));
  XMStoreFloat3(&direction,distance>0.000001f?travel/distance:unit);
  origin=ProjectileNativeVector(cover.contact.position);
  // Just before the obstruction, so the normal simulation records its impact.
  origin.x-=direction.x*0.0001f;origin.y-=direction.y*0.0001f;origin.z-=direction.z*0.0001f;
  error.clear();return true;
 }
 XMFLOAT3 target;
 XMStoreFloat3(&target,XMLoadFloat3(&eye)+unit*1000);
 const auto aimed=query(probe,ProjectileBridgeVector(eye),ProjectileBridgeVector(target));
 if(aimed.status==bridge::ProjectileQueryStatus::Blocked){error="Aim query was blocked.";return false;}
 if(aimed.status==bridge::ProjectileQueryStatus::Hit)target=ProjectileNativeVector(aimed.contact.position);
 const auto delta=XMLoadFloat3(&target)-XMLoadFloat3(&origin);
 const float distance=XMVectorGetX(XMVector3Length(delta));
 if(!std::isfinite(distance)||distance<0.000001f){error="Aim target coincides with muzzle.";return false;}
 const auto launch=delta/distance;
 if(XMVectorGetX(XMVector3Dot(launch,XMLoadFloat3(&socketForward)))<=0) {
  error="Launch socket points away from the aim target. Adjust its direction.";return false;
 }
 XMStoreFloat3(&direction,launch);error.clear();return true;
}
}
