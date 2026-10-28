#pragma once
// G12A free-flight presentation model. Separate from bed/pressure transport.
#include <WickedEngine.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
namespace renegade::bridge::river_waterfall {
using namespace DirectX;
inline void Check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
struct Settings {float height=10,width=4,inletSpeed=1,heading=0;XMFLOAT3 origin{};};
struct Row {XMFLOAT3 centre{},tangent{};float along=0,travel=0,speed=0,falling=0;};
struct Model {std::vector<Row> rows;float width=0,discharge=0,flightSeconds=0,totalTravel=0,landingAlong=0,landingX=0,impactPower=0;XMFLOAT3 lip{},landing{},direction{};};
inline float FlightTime(float drop,float downwardSpeed){Check(std::isfinite(drop)&&drop>=0&&std::isfinite(downwardSpeed)&&downwardSpeed>=0,"Invalid flight inputs");if(drop==0)return 0;return 2*drop/(downwardSpeed+std::sqrt(downwardSpeed*downwardSpeed+2*9.80665f*drop));}
inline Model Build(Settings s){
 Check(std::isfinite(s.height)&&s.height>=.5f&&s.height<=30&&std::isfinite(s.width)&&s.width>=.5f&&s.width<=12&&std::isfinite(s.inletSpeed)&&s.inletSpeed>=0&&s.inletSpeed<=3&&std::isfinite(s.heading),"Waterfall proof bounds: height0.5-30m width0.5-12m inlet0-3m/s");
 for(float f:{s.origin.x,s.origin.y,s.origin.z})Check(std::isfinite(f),"Nonfinite waterfall origin");
 Model m;m.width=s.width;m.discharge=s.width*.65f*s.inletSpeed;m.direction={std::cos(s.heading),0,std::sin(s.heading)};
 const float inlet=std::max(.1f,s.inletSpeed),lipDrop=std::min(.35f,s.height*.15f),toe=std::min(.65f,s.height*.25f),down=std::sqrt(2*9.80665f*lipDrop);
 const float time=FlightTime(s.height-lipDrop-toe,down),flightX=inlet*time,landingX=flightX+std::min(1.1f,s.height*.4f);m.landingX=landingX;m.flightSeconds=FlightTime(s.height-lipDrop,down);m.impactPower=1000*m.discharge*9.80665f*s.height;
 auto world=[&](XMFLOAT3 p){return XMFLOAT3{s.origin.x+p.x*m.direction.x,s.origin.y+p.y,s.origin.z+p.x*m.direction.z};};
 auto append=[&](XMFLOAT3 p,bool receiving=false){
  Row q;q.centre=world(p);float drop=std::max(0.f,s.height-p.y);q.speed=std::sqrt(inlet*inlet+2*9.80665f*drop);
  if(receiving){float t=std::clamp((p.x-landingX)/5.f,0.f,1.f);t=t*t*(3-2*t);q.speed+=t*(inlet-q.speed);}
  if(!m.rows.empty()){auto& old=m.rows.back();float ds=XMVectorGetX(XMVector3Length(XMLoadFloat3(&q.centre)-XMLoadFloat3(&old.centre)));if(ds<1e-6f)return;q.along=old.along+ds;q.travel=old.travel+2*ds/(old.speed+q.speed);if(q.travel<=old.travel)return; /* Collapse sub-ULP supports at analytic region joins. */}
  m.rows.push_back(q);
 };
 auto bezier=[&](XMFLOAT3 a,XMFLOAT3 b,XMFLOAT3 c,XMFLOAT3 d,int n){for(int k=0;k<=n;++k){float t=float(k)/n,u=1-t;append({u*u*u*a.x+3*u*u*t*b.x+3*u*t*t*c.x+t*t*t*d.x,u*u*u*a.y+3*u*u*t*b.y+3*u*t*t*c.y+t*t*t*d.y,0});}};
 for(int k=0;k<=80;++k)append({-12+(12-.65f)*k/80.f,s.height,0});
 const float n=std::hypot(inlet,down);bezier({-.65f,s.height,0},{-.25f,s.height,0},{-.25f*inlet/n,s.height-lipDrop+.25f*down/n,0},{0,s.height-lipDrop,0},32);
 for(int k=1;k<=192;++k){float t=time*k/192.f;append({inlet*t,s.height-lipDrop-down*t-.5f*9.80665f*t*t,0});}
 const float vy=down+9.80665f*time,n2=std::hypot(inlet,vy),toeHandle=std::min(.4f,toe*.75f);
 bezier({flightX,toe,0},{flightX+toeHandle*inlet/n2,toe-toeHandle*vy/n2,0},{landingX-std::min(.4f,(landingX-flightX)*.45f),0,0},{landingX,0,0},40);
 m.landingAlong=m.rows.back().along;
 for(int k=1;k<=300;++k)append({landingX+k*.1f,0,0},true);
 m.lip=world({0,s.height-lipDrop,0});m.landing=world({landingX,0,0});m.totalTravel=m.rows.back().travel;
 for(size_t k=0;k<m.rows.size();++k){auto a=m.rows[k?k-1:k].centre,b=m.rows[std::min(k+1,m.rows.size()-1)].centre;XMStoreFloat3(&m.rows[k].tangent,XMVector3Normalize(XMLoadFloat3(&b)-XMLoadFloat3(&a)));float grade=std::abs(m.rows[k].tangent.y);m.rows[k].falling=std::clamp((grade-.25f)/.75f,0.f,1.f)*(1-std::exp(-.8f*std::max(0.f,s.origin.y+s.height-m.rows[k].centre.y)));}
 return m;
}
inline void Mesh(const Model& m,wi::scene::MeshComponent& mesh,wi::ecs::Entity material){
 mesh=wi::scene::MeshComponent();mesh.SetDoubleSided(true);constexpr unsigned across=24;
 for(const auto& q:m.rows)for(unsigned j=0;j<=across;++j){float u=float(j)/across,side=(u-.5f)*m.width;mesh.vertex_positions.push_back({q.centre.x-m.direction.z*side,q.centre.y,q.centre.z+m.direction.x*side});mesh.vertex_uvset_0.push_back({u,q.along/m.width});mesh.vertex_uvset_1.push_back({u,q.travel});}
 for(size_t k=0;k+1<m.rows.size();++k)for(unsigned j=0;j<across;++j){uint32_t a=uint32_t(k*(across+1)+j),b=a+across+1;for(auto i:{a,a+1,b,b,a+1,b+1})mesh.indices.push_back(i);}
 auto& subset=mesh.subsets.emplace_back();subset.materialID=material;subset.indexCount=uint32_t(mesh.indices.size());
}
inline std::vector<XMFLOAT4> Path(const Model& m){std::vector<XMFLOAT4> out(2048);for(size_t k=0;k<out.size();++k){float travel=m.totalTravel*k/(out.size()-1);auto it=std::lower_bound(m.rows.begin(),m.rows.end(),travel,[](const Row& q,float t){return q.travel<t;});if(it==m.rows.end())--it;Row q=*it;if(it!=m.rows.begin()){auto& a=*(it-1);float t=std::clamp((travel-a.travel)/(it->travel-a.travel),0.f,1.f);q.along=a.along+t*(it->along-a.along);q.falling=a.falling+t*(it->falling-a.falling);q.speed=a.speed+t*(it->speed-a.speed);}out[k]={q.along-m.landingAlong,q.falling,m.width,q.speed};}return out;}
}
