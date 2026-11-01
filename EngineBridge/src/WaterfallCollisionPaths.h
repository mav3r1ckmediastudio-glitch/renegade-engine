#pragma once
#include "PlaceableWaterfallEffect.h"
namespace renegade::bridge::placeable_waterfall {
struct PathHit { bool hit=false;XMFLOAT3 point{},normal{}; };
struct WaterPathPoint { XMFLOAT3 position{},velocity{};float time=0;bool contact=false; };
struct WaterPath { std::vector<WaterPathPoint> points;float u=0,width=0; };
struct CollisionPaths {std::vector<WaterPath> paths;std::vector<XMFLOAT3> impacts;unsigned queries=0,contacts=0;};
// Bounded edit-time trajectory cache. Query must use immutable opaque obstacle geometry.
// Presentation model: no fluid pressure, pooling or conservation claim.
template<class Query> CollisionPaths TraceCollisionPaths(const Definition& d,Query query){
 const auto g=BuildGeometry(d);CollisionPaths result;
 constexpr int lanes=32,steps=650;constexpr float dt=.0125f,clearance=.065f;
 std::array<float,lanes> shares{};float total=0,cumulative=0;
 for(int lane=0;lane<lanes;++lane){float n=.5f+.5f*std::sin((lane+1)*2.399963f);shares[lane]=.28f+2.5f*n*n;total+=shares[lane];}
 for(int lane=0;lane<lanes;++lane){
  WaterPath path;float fraction=shares[lane]/total;path.u=cumulative+fraction*.5f;path.width=d.width*fraction;cumulative+=fraction;
  XMVECTOR p=XMLoadFloat3(&d.crest)+XMLoadFloat3(&g.lateral)*((path.u-.5f)*d.width);
  XMVECTOR v=XMLoadFloat3(&g.forward)*.85f;bool onSurface=false;int flightSteps=100;
  for(int step=0;step<steps;++step){
   WaterPathPoint q;XMStoreFloat3(&q.position,p);XMStoreFloat3(&q.velocity,v);q.time=step*dt;q.contact=onSurface;path.points.push_back(q);
   if(q.position.y<=d.landing.y+.02f)break;
   v+=XMVectorSet(0,-9.80665f*dt,0,0);
   XMVECTOR delta=v*dt;float length=XMVectorGetX(XMVector3Length(delta));
   XMFLOAT3 from,to;XMStoreFloat3(&from,p);XMStoreFloat3(&to,p+delta);
   ++result.queries;auto hit=query(from,to);
   if(hit.hit){
    XMVECTOR n=XMVector3Normalize(XMLoadFloat3(&hit.normal));if(XMVectorGetX(XMVector3Dot(n,v))>0)n=-n;
    float inward=XMVectorGetX(XMVector3Dot(v,n));if(inward<0)v-=n*inward;
    // A small retained downstream push stops stationary pools, which this model cannot solve.
    XMVECTOR push=XMLoadFloat3(&g.forward);push-=n*XMVectorGetX(XMVector3Dot(push,n));
    v+=push*.025f;
    // Impact loses transverse energy: the core is runoff, not a reflected spray jet.
    auto lateral=XMLoadFloat3(&g.lateral),forward=XMLoadFloat3(&g.forward);
    float sideSpeed=XMVectorGetX(XMVector3Dot(v,lateral));v+=lateral*(std::clamp(sideSpeed,-1.2f,1.2f)-sideSpeed);
    float forwardSpeed=XMVectorGetX(XMVector3Dot(v,forward));v+=forward*(std::clamp(forwardSpeed,.65f,2.4f)-forwardSpeed);
    p=XMLoadFloat3(&hit.point)+n*clearance;
    if(flightSteps>8){XMFLOAT3 impact;XMStoreFloat3(&impact,p);result.impacts.push_back(impact);}
    ++result.contacts;flightSteps=0;onSurface=true;
   }else{
    p+=delta;onSurface=false;++flightSteps;
   }
   float speed=XMVectorGetX(XMVector3Length(v));if(speed>18)v*=18/speed;
   // Keep trace finite even on awkward rock bowls.
   XMFLOAT3 current;XMStoreFloat3(&current,p);
   if(!std::isfinite(current.x)||!std::isfinite(current.y)||!std::isfinite(current.z))throw std::runtime_error("Nonfinite collision path");
  }
  result.paths.push_back(std::move(path));
 }
 return result;
}
// G13 bank-to-water junction: some outer streams finish against solid terrain
// ABOVE a lower receiving waterline. Do not reject the entire waterfall
// because these bounded lanes cannot flow through the bank. Keep all wet lanes
// unchanged and terminate only exhausted, physically contacting bank lanes
// at their first low-elevation collision. Refuse uncontacted/open-air gaps
// and scenes where no stream reaches the actual water.
// An invalid attempt never modifies the input paths.
struct BankTerminationSummary {size_t waterLandings=0,bankTerminations=0;};
inline bool ResolveWaterfallBankTerminations(CollisionPaths& paths,float waterY,
    BankTerminationSummary& result,std::string& error) {
    result={};error.clear();
    if(!std::isfinite(waterY)||paths.paths.empty()){
        error="Invalid receiving waterline or empty waterfall paths";return false;
    }
    std::vector<size_t> truncate(paths.paths.size(),0);
    for(size_t i=0;i<paths.paths.size();++i) {
        const auto& lane=paths.paths[i];
        if(lane.points.empty()){error="Empty water lane";return false;}
        if(lane.points.back().position.y<=waterY+.02f){
            ++result.waterLandings;continue;
        }
        // TraceCollisionPaths has a fixed 650-step budget. Only completed
        // trajectories stalled on a real nearby bank can terminate short.
        if(lane.points.size()!=650){
            error="Non-exhausted falling stream ends above water";return false;
        }
        size_t first=lane.points.size();
        for(size_t j=1;j<lane.points.size();++j) {
            const auto& point=lane.points[j];
            if(point.contact &&
                point.position.y>=waterY+.07f &&
                point.position.y<=waterY+.60f){
                first=j;break;
            }
        }
        if(first>=lane.points.size()){
            error="Falling stream could not reach either water or a physical bank";return false;
        }
        truncate[i]=first+1;
        ++result.bankTerminations;
    }
    if(result.waterLandings==0){
        error="No receiving-water contact from waterfall streams";return false;
    }
    if(result.bankTerminations==0)return true;
    for(size_t i=0;i<paths.paths.size();++i)
        if(truncate[i])paths.paths[i].points.resize(truncate[i]);
    // The original impact cache includes the now discarded dry-bank runoff
    // tail. Derive retained impact sites only from the retained trajectories;
    // preserve initial airborne-to-rock impacts and genuine water landing.
    paths.impacts.clear();
    paths.contacts=0;
    for(const auto& lane:paths.paths) {
        size_t airborne=0;
        bool previousContact=false;
        for(const auto& point:lane.points) {
            if(point.contact) {
                ++paths.contacts;
                if(!previousContact && airborne>8)
                    paths.impacts.push_back(point.position);
                airborne=0;
            } else ++airborne;
            previousContact=point.contact;
        }
    }
    return true;
}
inline void FillCollisionPathMesh(const Definition& d,const CollisionPaths& paths,wi::scene::MeshComponent& mesh,wi::ecs::Entity material){
 mesh=wi::scene::MeshComponent();mesh.SetDoubleSided(true);auto g=BuildGeometry(d);
 constexpr uint32_t ring=8,stride=ring+1;
 for(size_t lane=0;lane<paths.paths.size();++lane){const auto& path=paths.paths[lane];
  uint32_t base=uint32_t(mesh.vertex_positions.size());
  for(size_t row=0;row<path.points.size();++row){const auto& point=path.points[row];
   auto prior=path.points[row?row-1:row].position,next=path.points[std::min(row+1,path.points.size()-1)].position;
   XMVECTOR tangent=XMVector3Normalize(XMLoadFloat3(&next)-XMLoadFloat3(&prior));
   XMVECTOR across=XMLoadFloat3(&g.lateral);across=XMVector3Normalize(across-tangent*XMVectorGetX(XMVector3Dot(across,tangent)));
   if(!std::isfinite(XMVectorGetX(across)))across=XMLoadFloat3(&g.lateral);
   XMVECTOR depth=XMVector3Normalize(XMVector3Cross(tangent,across));
   float radius=path.width*(.48f+.17f*std::sin(point.time*7.7f+lane*1.73f)+.10f*std::sin(point.time*19.1f-lane*2.4f));
   // Crest R6: make seven source channels materially different in thickness
   // while keeping neighbouring ribbon boundaries welded. Each joint is
   // weighted by BOTH channel widths and therefore agrees from either lane.
   // This modifies only rendered cross-sections, never collision or landing.
   const auto smooth=[](float a,float b,float x){auto t=std::clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);};
   const float crestOnly=smooth(.12f,.42f,point.time)*(1-smooth(1.55f,2.40f,point.time));
   constexpr float groupWidths[7]={.59f,1.43f,.72f,1.26f,.58f,1.56f,.76f};
   constexpr float groupEdges[8]={0.f,.223f,.287f,.471f,.550f,.801f,.882f,1.f};
   const auto visualWidth=[&](const WaterPath& stream){
       unsigned group=0;
       while(group<6&&stream.u>=groupEdges[group+1])++group;
       float varied=groupWidths[group]+.08f*std::sin(point.time*(4.1f+.23f*group)+group*1.91f);
       return stream.width*(1.f+(varied-1.f)*crestOnly);
   };
   const float streamRadius=radius*visualWidth(path)/std::max(path.width,.001f);
   bool joinedLeft=false,joinedRight=false;XMFLOAT3 left=point.position,right=point.position;
   auto join=[&](size_t neighbor,XMFLOAT3& edge){
       const auto& other=paths.paths[neighbor];
       if(row>=other.points.size())return false;
       auto q=other.points[row].position;
       float distance=XMVectorGetX(XMVector3Length(XMLoadFloat3(&q)-XMLoadFloat3(&point.position)));
       if(distance>(path.width+other.width)*1.3f||std::abs(q.y-point.position.y)>.25f)return false;
       const float selfWidth=visualWidth(path),otherWidth=visualWidth(other);
       const float shared=std::clamp(selfWidth/(selfWidth+otherWidth),.20f,.80f);
       XMStoreFloat3(&edge,XMLoadFloat3(&point.position)+(XMLoadFloat3(&q)-XMLoadFloat3(&point.position))*shared);
       return true;
   };
   if(lane>0)joinedLeft=join(lane-1,left);if(lane+1<paths.paths.size())joinedRight=join(lane+1,right);
   float leftRadius=joinedLeft?XMVectorGetX(XMVector3Length(XMLoadFloat3(&left)-XMLoadFloat3(&point.position))):streamRadius;
   float rightRadius=joinedRight?XMVectorGetX(XMVector3Length(XMLoadFloat3(&right)-XMLoadFloat3(&point.position))):streamRadius;
   // A rounded section keeps detached streams visible around the orbit. Surface runoff stays thin.
   float thickness=(joinedLeft&&joinedRight)?.012f:std::max(.025f,radius*.48f);
   if(point.contact)thickness=.009f;
   // Suppress movement near any rock contact, not merely on the impact sample.
   float clearance=1;for(int k=-6;k<=6;++k){int at=int(row)+k;if(at>=0&&at<int(path.points.size())&&path.points[at].contact)clearance=0;}
   for(uint32_t side=0;side<=ring;++side){float angle=float(side)/ring*6.2831853f,c=std::cos(angle),sn=std::sin(angle);
    XMFLOAT3 position;XMStoreFloat3(&position,XMLoadFloat3(&point.position)+across*(c*(c<0?leftRadius:rightRadius))+depth*(sn*thickness));
    mesh.vertex_positions.push_back(position);
    mesh.vertex_uvset_0.push_back({path.u+c*path.width/(2*d.width),std::clamp((d.crest.y-position.y)/g.drop,0.f,1.f)});
    mesh.vertex_uvset_1.push_back({.5f+2*clearance,point.time});
   }
  }
  for(uint32_t row=0;row+1<path.points.size();++row)for(uint32_t side=0;side<ring;++side){auto a=base+row*stride+side,b=a+stride;for(auto k:{a,a+1,b,b,a+1,b+1})mesh.indices.push_back(k);}
 }
 auto& subset=mesh.subsets.emplace_back();subset.materialID=material;subset.indexCount=uint32_t(mesh.indices.size());
}
}
