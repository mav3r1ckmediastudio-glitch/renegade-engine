#pragma once
// Automatic derived support points: creator nodes remain the source of truth.
// Native Wicked interpolation/generation, not this cubic, is the final geometry.
#include "RiverCascadeFlow.h"
namespace renegade::bridge::river_smooth_drop {
using namespace DirectX;
using river_cascade::Require;
inline std::vector<XMFLOAT3> Controls(XMFLOAT3 top,XMFLOAT3 bottom,XMFLOAT3 forward){
 auto finite=[](XMFLOAT3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
 Require(finite(top)&&finite(bottom)&&finite(forward),"Nonfinite smooth drop input");
 const float h=top.y-bottom.y;
 Require(std::isfinite(h)&&h>=.15f&&h<=2,"Smooth drop height bounds");
 forward.y=0;const float length=std::hypot(forward.x,forward.z);
 Require(std::isfinite(length)&&length>.001f,"Smooth drop requires a horizontal heading");
 forward.x/=length;forward.z/=length;
 // Radius grows with drop, bounded internally; no creator mode/angle controls.
 const float radius=std::clamp(h*.5f,.2f,.8f);
 auto offset=[&](XMFLOAT3 p,float d){return XMFLOAT3{p.x+forward.x*d,p.y,p.z+forward.z*d};};
 const auto a=offset(top,-radius),b=top,c=bottom,d=offset(bottom,radius);
 std::vector<XMFLOAT3> out;
 // Horizontal shoulders supply the native Catmull tangent at either end.
 out.push_back(offset(a,-radius));
 for(int k=0;k<=16;++k){float t=.5f-.5f*std::cos(XM_PI*k/16.f),u=1-t;
  out.push_back({u*u*u*a.x+3*u*u*t*b.x+3*u*t*t*c.x+t*t*t*d.x,
   u*u*u*a.y+3*u*u*t*b.y+3*u*t*t*c.y+t*t*t*d.y,
   u*u*u*a.z+3*u*u*t*b.z+3*u*t*t*c.z+t*t*t*d.z});
 }
 out.push_back(offset(d,radius));return out;
}
inline XMFLOAT4 Frame(XMFLOAT3 before,XMFLOAT3 after){
 auto t=XMVector3Normalize(XMLoadFloat3(&after)-XMLoadFloat3(&before));
 XMFLOAT3 tangent;XMStoreFloat3(&tangent,t);
 Require(std::isfinite(tangent.x)&&std::isfinite(tangent.y)&&std::isfinite(tangent.z),"Smooth native frame finite");
 auto bank=XMVector3Normalize(XMVectorSet(-tangent.z,0,tangent.x,0));
 auto normal=XMVector3Normalize(XMVector3Cross(bank,t));
 auto up=XMVectorSet(0,1,0,0),axis=XMVector3Cross(up,normal);
 XMFLOAT4 q;
 if(XMVectorGetX(XMVector3LengthSq(axis))<1e-12f)XMStoreFloat4(&q,XMQuaternionIdentity());
 else XMStoreFloat4(&q,XMQuaternionRotationAxis(axis,std::acos(std::clamp(XMVectorGetY(normal),-1.f,1.f))));
 return q;
}
inline void Refine(wi::scene::MeshComponent& mesh,const river_cascade::Field& field){
 const auto positions=mesh.vertex_positions;const auto uv=mesh.vertex_uvset_0;
 Require(positions.size()==field.rows.size()*2&&uv.size()==positions.size(),"Native cascade refinement input");
 mesh.vertex_positions.clear();mesh.vertex_uvset_0.clear();mesh.vertex_uvset_1.clear();mesh.vertex_normals.clear();mesh.vertex_tangents.clear();mesh.indices.clear();
 constexpr int across=16;
 for(size_t k=0;k<field.rows.size();++k)for(int j=0;j<=across;++j){float t=float(j)/across;XMFLOAT3 p;XMStoreFloat3(&p,XMVectorLerp(XMLoadFloat3(&positions[k*2]),XMLoadFloat3(&positions[k*2+1]),t));mesh.vertex_positions.push_back(p);
  mesh.vertex_uvset_0.push_back({uv[k*2].x+(uv[k*2+1].x-uv[k*2].x)*t,uv[k*2].y});mesh.vertex_uvset_1.push_back({t,field.rows[k].travel});}
 for(size_t k=0;k+1<field.rows.size();++k)for(int j=0;j<across;++j){uint32_t a=uint32_t(k*(across+1)+j),c=a+across+1;for(auto i:{a,a+1,c,c,a+1,c+1})mesh.indices.push_back(i);}
 mesh.subsets[0].indexOffset=0;mesh.subsets[0].indexCount=uint32_t(mesh.indices.size());mesh.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
 mesh.CreateRenderData();mesh.aabb._min.x-=.03f;mesh.aabb._max.x+=.03f;
}
inline river_cascade::Field Build(const wi::scene::MeshComponent& mesh,const XMMATRIX& world,float inletSpeed=1,float discharge=2.6f,bool requireFall=true){
 Require(mesh.vertex_positions.size()>=4&&mesh.vertex_positions.size()%2==0&&mesh.vertex_positions.size()<=16384,"Cascade native ribbon bounds");
 Require(std::isfinite(inletSpeed)&&inletSpeed>.05f&&inletSpeed<=3&&std::isfinite(discharge)&&discharge>0&&discharge<=20,"Cascade hydraulic inputs");
 river_cascade::Field out;out.discharge=discharge;float top=0,distance=0,travel=0,previousSpeed=inletSpeed;XMFLOAT3 previous{};
 for(size_t k=0;k<mesh.vertex_positions.size();k+=2){
  auto a=XMVector3TransformCoord(XMLoadFloat3(&mesh.vertex_positions[k]),world),b=XMVector3TransformCoord(XMLoadFloat3(&mesh.vertex_positions[k+1]),world);
  XMFLOAT3 p;XMStoreFloat3(&p,(a+b)*.5f);float width=XMVectorGetX(XMVector3Length(a-b));
  Require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&width>=.5f&&width<=12,"Invalid actual native cascade frame");
  if(k==0)top=p.y;
  const float drop=top-p.y;Require(drop>=-.005f&&drop<=2.5f,"Cascade drop exceeds2.5m total path or rises");
  const float speed=std::sqrt(inletSpeed*inletSpeed+2*9.80665f*std::max(0.f,drop));
  if(k){float ds=std::hypot(std::hypot(p.x-previous.x,p.z-previous.z),p.y-previous.y);Require(ds>1e-6f,"Collapsed native cascade rows");distance+=ds;travel+=2*ds/(previousSpeed+speed);}
  out.rows.push_back({p,{},distance,travel,speed,discharge/(width*speed),1-std::exp(-.8f*std::max(0.f,drop))});previous=p;previousSpeed=speed;
 }
 out.drop=top-out.rows.back().centre.y;Require(out.drop>=.15f&&out.drop<=2.5f,"Cascade supported total path drop .15–2m");
 const auto first=out.rows.front().centre,last=out.rows.back().centre;Require(!requireFall||std::hypot(last.x-first.x,last.z-first.z)<=out.drop,"Cascade requires a steep falling reach");
 for(size_t k=0;k<out.rows.size();++k){const auto a=out.rows[k?k-1:k].centre,b=out.rows[std::min(k+1,out.rows.size()-1)].centre;auto v=XMVector3Normalize(XMLoadFloat3(&b)-XMLoadFloat3(&a))*out.rows[k].speed;XMStoreFloat3(&out.rows[k].velocity,v);}
 out.impactPower=1000*discharge*9.80665f*out.drop;return out;
}
}
