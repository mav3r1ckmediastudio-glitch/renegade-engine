#include "../EngineBridge/src/PlaceableWaterfallEffect.h"
#include "../EngineBridge/src/WaterfallCollisionPaths.h"
#include <iostream>
#include <cmath>
using namespace renegade::bridge::placeable_waterfall;
int main(){
 try{
  unsigned count=0;
  for(float drop:{.5f,1.1f,3.f,12.f,30.f,60.f})
   for(float width:{.3f,1.2f,3.5f,12.f,25.f})
    for(float horizontal:{0.f,1.6f,8.f}){
      Definition d;d.crest={18,drop+2,-4};d.landing={18+horizontal,2,-4+horizontal*.6f};d.width=width;
      auto g=BuildGeometry(d);
      Require(EndpointsMatch(d,g),"Editor placement handles should match generated mesh");
      Require(g.supports.size()==101,"Height supports missing");
      float prev=INFINITY;
      for(const auto& s:g.supports){
       Require(std::isfinite(s.center.y)&&s.center.y<=prev+.001f,"Nonmonotonic fall");
       prev=s.center.y;
      }
      wi::scene::MeshComponent mesh;
      FillSheetMesh(d,g,mesh,wi::ecs::INVALID_ENTITY);
      Require(mesh.vertex_positions.size()==101*33,"Independent sheet continuity");
      Require(mesh.indices.size()==100*32*6,"Disconnected effect");
      Require(std::isfinite(g.supports.back().travelTime),"Nonnumeric gravity animation");
      // Every support row remains behind the permitted front spray origin.
      wi::scene::MeshComponent frontSheet;
      FillSheetMesh(d,g,frontSheet,wi::ecs::INVALID_ENTITY,3*.14f*VisualScale(d));
      for(size_t j=0;j<frontSheet.vertex_positions.size();++j){const auto& p=frontSheet.vertex_positions[j];const auto& centre=g.supports[j/33].center;float forward=(p.x-centre.x)*g.forward.x+(p.z-centre.z)*g.forward.z;Require(forward<FrontSprayOffset(d),"Mist spawn falls behind sheet");}
      const auto volumes=FrontSprayVolumes(d);Require(volumes.size()==12,"Missing shared-source breakup zones");
      for(size_t v=0;v<volumes.size();v+=2){const auto& core=volumes[v];const auto& outer=volumes[v+1];Require(core.halfSize.x>0&&outer.halfSize.x>0,"Empty breakup source");float separation=(outer.center.x-core.center.x)*g.forward.x+(outer.center.z-core.center.z)*g.forward.z;Require(separation<core.halfSize.x+outer.halfSize.x,"Detached spray source gap");Require(outer.density<core.density&&core.velocity.y<0,"Breakup must thin outward and fall");}
      ++count;
    }
  Definition edited;edited.crest={-4,14,7};edited.landing={-2,1,9};
  auto first=BuildGeometry(edited);wi::scene::MeshComponent before,after;
  FillSheetMesh(edited,first,before,wi::ecs::INVALID_ENTITY);
  edited.crest.x+=2;edited.landing.z+=3;edited.width=5;edited.turbulence=1.4f;
  auto rebuilt=BuildGeometry(edited);FillSheetMesh(edited,rebuilt,after,wi::ecs::INVALID_ENTITY,.28f);
  Require(EndpointsMatch(edited,rebuilt),"Rebuild lost moved handles");
  float change=0;for(size_t i=0;i<after.vertex_positions.size();++i){auto a=before.vertex_positions[i],b=after.vertex_positions[i];Require(std::isfinite(b.x)&&std::isfinite(b.y)&&std::isfinite(b.z),"Nonfinite edited mesh");change+=std::abs(a.x-b.x)+std::abs(a.z-b.z);}
  Require(change>1,"Moving placement did not rebuild geometry");
  Definition calm;calm.turbulence=0;auto calmGeometry=BuildGeometry(calm);FillSheetMesh(calm,calmGeometry,before,wi::ecs::INVALID_ENTITY);
  calm.turbulence=1.4f;FillSheetMesh(calm,calmGeometry,after,wi::ecs::INVALID_ENTITY);
  change=0;for(size_t i=0;i<after.vertex_positions.size();++i)change+=std::abs(before.vertex_positions[i].x-after.vertex_positions[i].x);
  Require(change>1,"Turbulence control has no geometric effect");
  // A finite shelf changes only intercepted streams; its edge releases flow again.
  Definition collision;collision.width=4;
  auto trace=[&](float edge){return TraceCollisionPaths(collision,[&](XMFLOAT3 a,XMFLOAT3 b){
   if(a.y>=6&&b.y<6){float t=(a.y-6)/(a.y-b.y);XMFLOAT3 p{a.x+(b.x-a.x)*t,6,a.z+(b.z-a.z)*t};if(p.x<edge&&p.z>-.8f&&p.z<.8f)return PathHit{true,p,{0,1,0}};}
   return PathHit{};
  });};
  auto clear=trace(-100),shelf=trace(2.4f),moved=trace(.1f);
  Require(clear.contacts==0&&shelf.contacts>0&&moved.contacts==0,"Obstacle movement did not invalidate collision paths");
  Require(shelf.paths.size()==32&&shelf.queries<=32*650,"Unbounded collision tracing");
  float narrow=INFINITY,broad=0,covered=0;for(const auto& path:shelf.paths){narrow=std::min(narrow,path.width);broad=std::max(broad,path.width);covered+=path.width;}
  Require(broad>narrow*4&&std::abs(covered-collision.width)<.001f,"Inlet flow partition must cover width without uniform stream sizes");

  unsigned landed=0;float deviation=0;
  for(size_t i=0;i<shelf.paths.size();++i){const auto& path=shelf.paths[i];Require(path.points.size()>=2,"Empty traced ribbon");if(path.points.back().position.y<.1f)++landed;deviation+=std::abs(path.points.back().position.x-clear.paths[i].points.back().position.x);for(const auto& q:path.points)Require(std::isfinite(q.position.x)&&std::isfinite(q.position.y),"Nonfinite traced trajectory");}
  Require(landed==32&&deviation>1,"Water did not leave the finite shelf and reach landing");
  BankTerminationSummary term;std::string termError;
  auto unchanged=shelf;
  Require(ResolveWaterfallBankTerminations(shelf,collision.landing.y,term,termError)
          &&term.waterLandings==32&&term.bankTerminations==0 &&
          shelf.paths[0].points.size()==unchanged.paths[0].points.size() &&
          shelf.impacts.size()==unchanged.impacts.size(),
          "Previously approved all-wet cascade must be unchanged");
  CollisionPaths mixed;
  WaterPath wet;wet.points={
      {{0,10,0},{0,-1,0},0,false},
      {{0,-.86f,0},{0,-1,0},1,false}
  };
  WaterPath blocked;blocked.points.resize(650);
  for(size_t i=0;i<650;++i){
      blocked.points[i].position={1,-.48f,0};
      blocked.points[i].time=float(i)*.0125f;
  }
  blocked.points[290].contact=true;
  mixed.paths={wet,blocked};
  mixed.impacts={{999,1,999}};
  Require(ResolveWaterfallBankTerminations(mixed,-.85f,term,termError)
          &&term.waterLandings==1&&term.bankTerminations==1
          &&mixed.paths[0].points.size()==2
          &&mixed.paths[1].points.size()==291
          &&mixed.paths[1].points.back().contact
          &&mixed.impacts.size()<=2,
          "Bank-blocked outer streams must end on bank without disabling wet waterfall");
  auto noWater=mixed;noWater.paths.erase(noWater.paths.begin());
  const auto oldSize=noWater.paths[0].points.size();
  Require(!ResolveWaterfallBankTerminations(noWater,-.85f,term,termError)
          &&noWater.paths[0].points.size()==oldSize,
          "All-dry waterfall must refuse without mutating input");
  auto noBank=mixed;
  noBank.paths[1].points.resize(650);
  for(size_t i=291;i<650;++i){
      noBank.paths[1].points[i]=noBank.paths[1].points.back();
      noBank.paths[1].points[i].contact=false;
  }
  noBank.paths[1].points[290].contact=false;
  Require(!ResolveWaterfallBankTerminations(noBank,-.85f,term,termError),
          "Uncontacted stream above water must not pretend to have a bank");
  std::cout<<"WATERFALL_MIXED_RIVER_BANK_TERMINATION=PASS wet=1 bank=1"<<std::endl;
  wi::scene::MeshComponent collisionMesh;FillCollisionPathMesh(collision,shelf,collisionMesh,wi::ecs::INVALID_ENTITY);
  Require(!collisionMesh.indices.empty()&&collisionMesh.vertex_uvset_1.size()==collisionMesh.vertex_positions.size(),"Collision ribbon cache incomplete");
  CollisionPaths isolated;WaterPath freeStream;freeStream.u=.5f;freeStream.width=.4f;
  freeStream.points={{{0,8,0},{0,-3,0},.2f,false},{{0,7,0},{0,-4,0},.4f,false},{{0,6,0},{0,-5,0},.6f,false}};isolated.paths.push_back(freeStream);
  wi::scene::MeshComponent roundMesh;FillCollisionPathMesh(collision,isolated,roundMesh,wi::ecs::INVALID_ENTITY);
  float depthLow=INFINITY,depthHigh=-INFINITY;for(auto p:roundMesh.vertex_positions){Require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"Rounded stream contains nonfinite vertices");depthLow=std::min(depthLow,p.x);depthHigh=std::max(depthHigh,p.x);}
  Require(depthHigh-depthLow>.05f,"Detached stream lost physical side-view thickness");
  isolated.paths[0].points[1].contact=true;FillCollisionPathMesh(collision,isolated,roundMesh,wi::ecs::INVALID_ENTITY);
  for(auto uv:roundMesh.vertex_uvset_1)Require(uv.x<1,"Rock contact neighbourhood permits vertex animation");
  // Impact spray only changes landing emissions, never source spray/mist.
  Definition impact;
  const auto baseline=EffectRate(impact,55.f);
  Require(std::abs(ImpactSprayRate(impact,55.f)-baseline)<.001f,
          "One-times impact slider changed baseline unit rate");
  impact.impactSpray=0;
  Require(ImpactSprayRate(impact,55.f)==0&&EffectRate(impact,55.f)==baseline,
          "Zero impact setting must not silence waterfall body");
  impact.impactSpray=2;
  Require(std::abs(ImpactSprayRate(impact,55.f)-2*baseline)<.001f,
          "Impact spray is not independent/linear");
  impact.impactSpray=3;
  Require(std::abs(ImpactSprayRate(impact,55.f)-3*baseline)<.001f,
          "Maximum slider not honoured");
  impact.impactSpray=3.01f;
  bool badImpact=false;try{BuildGeometry(impact);}catch(const std::runtime_error&){badImpact=true;}
  Require(badImpact,"Out of range impact slider not rejected");
  impact.impactSpray=std::numeric_limits<float>::quiet_NaN();
  badImpact=false;try{BuildGeometry(impact);}catch(const std::runtime_error&){badImpact=true;}
  Require(badImpact,"Non-finite impact slider not rejected");
  Definition zero;zero.intensity=0;
  Require(EffectRate(zero,350.f)==0,"Zero-flow scene still emits VFX");
  Definition invalid;invalid.width=30;
  bool caught=false;try{BuildGeometry(invalid);}catch(const std::runtime_error&){caught=true;}
  Require(caught,"Invalid width not rejected");
  invalid=Definition();invalid.crest.y=invalid.landing.y;
  caught=false;try{BuildGeometry(invalid);}catch(const std::runtime_error&){caught=true;}
  Require(caught,"No vertical waterfall accepted");
  std::cout<<"PLACEABLE_WATERFALL_GEOMETRY_PASS cases="<<count
            <<" no_river_dependencies=1 two_handles=1 zero_flow=1"<<std::endl;return 0;
 }catch(const std::exception& ex){std::cerr<<ex.what()<<std::endl;return 1;}
}
