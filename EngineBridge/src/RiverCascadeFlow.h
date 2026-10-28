#pragma once
// G10B bounded free-falling reach. Surface coordinates are 3D native ribbon
// distance/travel time, never world-XZ. Not a volumetric or hydraulic-jump solver.
#include <WickedEngine.h>
#include <vector>
#include <stdexcept>
#include <cmath>
namespace renegade::bridge::river_cascade {
using namespace DirectX;
inline void Require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
struct Sample {XMFLOAT3 centre{},velocity{};float distance=0,travel=0,speed=0,thickness=0,entrainment=0;};
struct Field {std::vector<Sample> rows;float discharge=0,drop=0,impactPower=0;};
inline Field Build(const wi::scene::MeshComponent& mesh,const XMMATRIX& world,float inletSpeed=1,float discharge=2.6f,bool requireFall=true){
 Require(mesh.vertex_positions.size()>=4&&mesh.vertex_positions.size()%2==0&&mesh.vertex_positions.size()<=16384,"Cascade native ribbon bounds");
 Require(std::isfinite(inletSpeed)&&inletSpeed>.05f&&inletSpeed<=3&&std::isfinite(discharge)&&discharge>0&&discharge<=20,"Cascade hydraulic inputs");
 Field out;out.discharge=discharge;float top=0,distance=0,travel=0,previousSpeed=inletSpeed;XMFLOAT3 previous{};
 for(size_t k=0;k<mesh.vertex_positions.size();k+=2){
  auto a=XMVector3TransformCoord(XMLoadFloat3(&mesh.vertex_positions[k]),world),b=XMVector3TransformCoord(XMLoadFloat3(&mesh.vertex_positions[k+1]),world);
  XMFLOAT3 p;XMStoreFloat3(&p,(a+b)*.5f);float width=XMVectorGetX(XMVector3Length(a-b));
  Require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&width>=.5f&&width<=12,"Invalid actual native cascade frame");
  if(k==0)top=p.y;
  const float drop=top-p.y;Require(drop>=-.005f&&drop<=2,"Cascade drop exceeds2m or rises");
  const float speed=std::sqrt(inletSpeed*inletSpeed+2*9.80665f*std::max(0.f,drop));
  if(k){float ds=std::hypot(std::hypot(p.x-previous.x,p.z-previous.z),p.y-previous.y);Require(ds>1e-6f,"Collapsed native cascade rows");distance+=ds;travel+=2*ds/(previousSpeed+speed);}
  out.rows.push_back({p,{},distance,travel,speed,discharge/(width*speed),1-std::exp(-.8f*std::max(0.f,drop))});previous=p;previousSpeed=speed;
 }
 out.drop=top-out.rows.back().centre.y;Require(out.drop>=.15f&&out.drop<=2,"Cascade supported drop .15–2m");
 const auto first=out.rows.front().centre,last=out.rows.back().centre;Require(!requireFall||std::hypot(last.x-first.x,last.z-first.z)<=out.drop,"Cascade requires a steep falling reach");
 for(size_t k=0;k<out.rows.size();++k){const auto a=out.rows[k?k-1:k].centre,b=out.rows[std::min(k+1,out.rows.size()-1)].centre;auto v=XMVector3Normalize(XMLoadFloat3(&b)-XMLoadFloat3(&a))*out.rows[k].speed;XMStoreFloat3(&out.rows[k].velocity,v);}
 out.impactPower=1000*discharge*9.80665f*out.drop;return out;
}
inline void SetTravelUV(wi::scene::MeshComponent& mesh,const Field& field){
 Require(mesh.vertex_positions.size()==field.rows.size()*2,"Cascade field/mesh parity");mesh.vertex_uvset_1.clear();
 for(auto& s:field.rows){mesh.vertex_uvset_1.push_back({0,s.travel});mesh.vertex_uvset_1.push_back({1,s.travel});}
}
}

namespace renegade::bridge::river_cascade {
inline void RefineAcross(wi::scene::MeshComponent& mesh,const Field& field){
 const auto positions=mesh.vertex_positions;const auto uv=mesh.vertex_uvset_0;
 Require(positions.size()==field.rows.size()*2&&uv.size()==positions.size(),"Native cascade refinement input");
 mesh.vertex_positions.clear();mesh.vertex_uvset_0.clear();mesh.vertex_uvset_1.clear();mesh.vertex_normals.clear();mesh.vertex_tangents.clear();mesh.indices.clear();
 constexpr int across=32;
 for(size_t k=0;k<field.rows.size();++k)for(int j=0;j<=across;++j){float t=float(j)/across;XMFLOAT3 p;XMStoreFloat3(&p,XMVectorLerp(XMLoadFloat3(&positions[k*2]),XMLoadFloat3(&positions[k*2+1]),t));mesh.vertex_positions.push_back(p);
  mesh.vertex_uvset_0.push_back({uv[k*2].x+(uv[k*2+1].x-uv[k*2].x)*t,uv[k*2].y});mesh.vertex_uvset_1.push_back({t,field.rows[k].travel});}
 for(size_t k=0;k+1<field.rows.size();++k)for(int j=0;j<across;++j){uint32_t a=uint32_t(k*(across+1)+j),c=a+across+1;for(auto i:{a,a+1,c,c,a+1,c+1})mesh.indices.push_back(i);}
 mesh.subsets[0].indexOffset=0;mesh.subsets[0].indexCount=uint32_t(mesh.indices.size());mesh.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
 mesh.CreateRenderData();mesh.aabb._min.x-=.03f;mesh.aabb._max.x+=.03f;
}
}
