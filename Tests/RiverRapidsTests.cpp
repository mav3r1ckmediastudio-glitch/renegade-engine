#include "RiverJunctionFixture.h"
#include "../EngineBridge/src/RiverRapidsTransport.h"
#include "../EngineBridge/src/RiverWaterwayTransport.h"
#include <iostream>
#include <cstring>
using namespace renegade::bridge;
using namespace river_rapids_transport;
static Surface Bed(bool reef){
 Surface s;s.terrain=true;
 auto point=[&](int x,int z){float px=-30+x*.25f,pz=-16+z*4.f;float depth=.8f-(reef?.6f*std::exp(-std::pow((px-12)/3.f,4.f)):0);return XMFLOAT3{px,-depth,pz};};
 for(int z=0;z<8;++z)for(int x=0;x<260;++x){auto a=point(x,z),b=point(x+1,z),c=point(x,z+1),d=point(x+1,z+1);s.triangles.push_back({a,c,b});s.triangles.push_back({b,c,d});}return s;
}
int main(){try{

 // Real editor bank regression: 42 micrometres of measured water is wet,
 // and must reach the explicitly documented 10cm resolved-layer clamp.
 Mask edge;edge.width=edge.height=1;edge.bounds={0,0,1,1};edge.cells={{float(Bank),0,1000,1}};
 for(float y:{-.000042f,0.f,.000005f}){
  Surface bed;bed.terrain=true;bed.triangles={{{-1,y,-1},{-1,y,2},{2,y,-1}},{{2,y,-1},{-1,y,2},{2,y,2}}};
  auto h=MeasureDepth(edge,{bed},0);Check(h[0]==.1f,"Wet bank epsilon lost measured shallow bed");
 }
 Surface dry;dry.terrain=true;dry.triangles={{{-1,.001f,-1},{-1,.001f,2},{2,.001f,-1}},{{2,.001f,-1},{-1,.001f,2},{2,.001f,2}}};
 bool dryRefused=false;try{MeasureDepth(edge,{dry},0);}catch(...){dryRefused=true;}Check(dryRefused,"Above-water surface invented bed depth");
 wi::scene::Scene scene;junction_proof::Create(scene);
 auto make=[&](float speed){auto inputs=junction_proof::Inputs(scene);for(auto& r:inputs)r.flow.baseSpeedMetersPerSecond=speed;std::shared_ptr<const RiverFlowNetwork> n;std::string e;Check(BuildRiverFlowNetwork(scene,inputs,{{"join",{"west","east"},"outlet"}},n,e),e);return n;};
 auto network=make(.35f);std::shared_ptr<const RiverJunctionSurface> surface;std::string error;Check(BuildRiverJunctionSurface(*network,surface,error),error);
 auto flat=Build(*network,*surface,{Bed(false)},256,128);
 auto rapid=Build(*network,*surface,{Bed(true)},256,128);
 auto original=river_waterway_transport::Build(*network,*surface,{Bed(false)},256,128);
 float constantError=0;for(int k:flat.active)constantError=std::max(constantError,std::abs(flat.velocity[k].z-original.velocity[k].z));
 Check(constantError<.0001f,"Constant real depth must recover accepted unit-depth velocity");
 Check(flat.rapidEmission==0,"Uniform bed must not emit procedural whitewater");
 Check(rapid.rapidEmission>0&&rapid.maximumFroude>1,"Shallow rough reach must produce supercritical aeration proxy");
 double shallow=0,deep=0;size_t ns=0,nd=0;
 for(int k:rapid.active){float x=rapid.mask.bounds.x+(k%256+.5f)*rapid.mask.bounds.z/256;
  if(x>10&&x<14){shallow+=rapid.velocity[k].z;++ns;}if(x>23&&x<27){deep+=rapid.velocity[k].z;++nd;}
  Check(rapid.depth[k]>=.10f&&std::isfinite(rapid.depth[k]),"Finite measured wet depth");}
 Check(ns&&nd&&shallow/ns>deep/nd*2,"Measured shallow sill must accelerate flow relative to downstream pool");
 Check(std::abs(rapid.inflow-rapid.outflow)<1e-8&&rapid.maximumDivergence<1e-4,"Depth-integrated discharge conservation");
 auto flatFastNetwork=make(1.8f);std::shared_ptr<const RiverJunctionSurface> flatFastSurface;Check(BuildRiverJunctionSurface(*flatFastNetwork,flatFastSurface,error),error);
 auto flatFast=Build(*flatFastNetwork,*flatFastSurface,{Bed(false)},256,128);Check(flatFast.maximumFroude>1&&flatFast.rapidEmission==0,"Fast flat-bed control must remain unaerated");
 auto submergedTop=[](float y){Surface s;s.triangles={{{20,y,-2.5f},{20,y,-1.3f},{21.2f,y,-2.5f}},{{21.2f,y,-2.5f},{20,y,-1.3f},{21.2f,y,-1.3f}}};return s;};
 auto shallowRock=Build(*network,*surface,{Bed(false),submergedTop(-.18f)},256,128);
 auto buriedRock=Build(*network,*surface,{Bed(false),submergedTop(-.95f)},256,128);
 auto aboveRock=Build(*network,*surface,{Bed(false),submergedTop(.3f)},256,128);
 Check(shallowRock.rapidEmission>0&&buriedRock.rapidEmission==0&&aboveRock.rapidEmission==0,"Actual submerged geometry only must drive depth aeration");
 auto deepBed=Bed(false);for(auto& t:deepBed.triangles)t.a.y=t.b.y=t.c.y=-5;
 bool depthRefused=false;try{Build(*network,*surface,{deepBed},256,128);}catch(...){depthRefused=true;}Check(depthRefused,"Over-limit real depth must fail closed");
 auto fasterNetwork=make(.7f);std::shared_ptr<const RiverJunctionSurface> fasterSurface;Check(BuildRiverJunctionSurface(*fasterNetwork,fasterSurface,error),error);
 auto faster=Build(*fasterNetwork,*fasterSurface,{Bed(true)},256,128);
 Check(std::abs(faster.inflow/rapid.inflow-2)<1e-5,"Inlet discharge follows authored speed");
 float velocityError=0;for(int k:rapid.active)velocityError=std::max(velocityError,std::abs(faster.velocity[k].z-2*rapid.velocity[k].z));Check(velocityError<.001f,"Projected depth-aware velocity scales with inflow");
 auto repeat=Build(*network,*surface,{Bed(true)},256,128);Check(repeat.depth==rapid.depth&&std::memcmp(repeat.faces.data(),rapid.faces.data(),rapid.faces.size()*sizeof(XMFLOAT4))==0,"Deterministic geometry-derived domain");
 bool refused=false;try{Build(*network,*surface,{},256,128);}catch(...){refused=true;}Check(refused,"Unmeasured bed must fail closed");
 auto bad=Bed(true);bad.triangles.front().a.y=NAN;refused=false;try{Build(*network,*surface,{bad},256,128);}catch(...){refused=true;}Check(refused,"NaN bathymetry must fail closed");
 std::vector<XMFLOAT4> zero(rapid.faces.size());auto state=zero;const float dt=std::min(.02f,.7f/rapid.maximumRate);
 for(int n=0;n<150;++n){state=Step(rapid,state,dt,n*dt,6,8);for(int k:rapid.active)Check(state[k].x>=-1e-7f&&std::isfinite(state[k].x),"Positive finite aerated foam transport");}
 double mass=0;for(auto x:state)mass+=x.x;Check(mass>0,"Rapid-only source reaches transported density");
 auto stopped=Step(rapid,zero,dt,dt,6,15,0);for(auto x:stopped)Check(x.x==0,"Zero transport speed cannot emit foam");
 std::cout<<"RAPIDS_CPU=PASS discharge="<<rapid.inflow<<" divergence="<<rapid.maximumDivergence<<" FrMax="<<rapid.maximumFroude<<" emission="<<rapid.rapidEmission<<" shallowSpeed="<<shallow/ns<<" poolSpeed="<<deep/nd<<" doubledVelocityError="<<velocityError<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"RAPIDS_CPU=FAIL "<<e.what()<<"\n";return 1;}}
