#include "RiverJunctionFoamTransport.h"
#include "../EngineBridge/src/RiverWaterwayTransport.h"
#include <cstring>
#include <iostream>
namespace production=renegade::bridge::river_waterway_transport;
using namespace renegade::bridge;
using namespace junction_proof;
static junction_classification::Surface Box(float x,float z){
 junction_classification::Surface s;const XMFLOAT3 v[]={{x-.45f,-1,z-.45f},{x+.45f,-1,z-.45f},{x+.45f,-1,z+.45f},{x-.45f,-1,z+.45f},{x-.45f,1,z-.45f},{x+.45f,1,z-.45f},{x+.45f,1,z+.45f},{x-.45f,1,z+.45f}};
 const int ix[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
 for(int k=0;k<36;k+=3)s.triangles.push_back({v[ix[k]],v[ix[k+1]],v[ix[k+2]]});return s;
}
static float Difference(const std::vector<XMFLOAT4>& a,const std::vector<XMFLOAT4>& b){
 Check(a.size()==b.size(),"Domain array dimensions");float maximum=0;
 for(size_t k=0;k<a.size();++k)for(int c=0;c<4;++c)maximum=std::max(maximum,std::abs((&a[k].x)[c]-(&b[k].x)[c]));return maximum;
}
int main(){try{
 wi::scene::Scene scene;Create(scene);const auto network=Network(scene);std::string error;std::shared_ptr<const RiverJunctionSurface> surface;
 Check(BuildRiverJunctionSurface(*network,surface,error),error);
 const std::vector<junction_classification::Surface> obstacles{Box(-7,-4),Box(-7,4)};
 std::vector<river_v2_classification::Surface> adopted;
 for(const auto& old:obstacles){river_v2_classification::Surface s;s.terrain=old.terrain;for(auto t:old.triangles)s.triangles.push_back({t.a,t.b,t.c});adopted.push_back(std::move(s));}
 const auto oracle=junction_foam::Build(*network,*surface,obstacles,320,160);
 const auto domain=production::Build(*network,*surface,adopted,320,160);
 Check(oracle.active==domain.active&&oracle.index==domain.index,"Exact adopted fluid topology");
 Check(Difference(oracle.mask.cells,domain.mask.cells)==0&&Difference(oracle.faces,domain.faces)==0&&Difference(oracle.velocity,domain.velocity)==0,"Exact classifier and projected flux adoption");
 for(size_t k=0;k<domain.source.size();++k){auto a=oracle.source[k],b=domain.source[k];Check(a.x+a.y+a.z==b.x+b.y+b.z,"Exact aggregate emission independent of fixture channel names");}
 const auto narrow=production::Build(*network,*surface,adopted,320,160,.02f);
 const auto wide=production::Build(*network,*surface,adopted,320,160,.08f);
 Check(narrow.active==wide.active&&Difference(narrow.faces,wide.faces)==0,"Contact width preserves actual solid exclusion and projected flux");
 double narrowMass=0,wideMass=0;size_t narrowCells=0,wideCells=0;
 for(auto v:narrow.source){auto q=v.x+v.y+v.z;narrowMass+=q;narrowCells+=q>0;}
 for(auto v:wide.source){auto q=v.x+v.y+v.z;wideMass+=q;wideCells+=q>0;}
 Check(wideMass>narrowMass&&wideCells>narrowCells,"Wider contact band emits over more physical water cells");
 const float dt=std::min(.05f,.75f/domain.maximumRate);std::vector<XMFLOAT4> expected(domain.faces.size()),actual=expected;
 for(unsigned step=0;step<100;++step){expected=junction_foam::Step(oracle,expected,dt,step*dt);actual=production::Step(domain,actual,dt,step*dt);}
 Check(Difference(expected,actual)==0,"Exact 100-step conservative transport adoption");
 auto inputs=Inputs(scene);for(auto& in:inputs){if(in.id=="east")in.id="11111111-1111-4111-8111-111111111111";else if(in.id=="outlet")in.id="55555555-5555-4555-8555-555555555555";else in.id="99999999-9999-4999-8999-999999999999";}
 std::reverse(inputs.begin(),inputs.end());std::shared_ptr<const RiverFlowNetwork> renamed;
 Check(BuildRiverFlowNetwork(scene,inputs,{{"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",{"99999999-9999-4999-8999-999999999999","11111111-1111-4111-8111-111111111111"},"55555555-5555-4555-8555-555555555555"}},renamed,error),error);
 std::shared_ptr<const RiverJunctionSurface> renamedSurface;Check(BuildRiverJunctionSurface(*renamed,renamedSurface,error),error);
 const auto uuidDomain=production::Build(*renamed,*renamedSurface,adopted,320,160);
 Check(Difference(domain.faces,uuidDomain.faces)==0&&Difference(domain.velocity,uuidDomain.velocity)==0,"UUID names and input order preserve projected flow");
 double source0=0,source1=0;for(auto s:uuidDomain.source){source0+=s.x;source1+=s.y;}Check(source0>0&&source1>0,"Two UUID-addressed native inlets emit independently");
 std::vector<XMFLOAT4> uuidState(actual.size());for(unsigned step=0;step<100;++step)uuidState=production::Step(uuidDomain,uuidState,dt,step*dt);
 Check(Difference(actual,uuidState)==0,"UUID-addressed transport matches accepted aggregate state");
 auto fasterInputs=Inputs(scene);for(auto& input:fasterInputs)input.flow.baseSpeedMetersPerSecond*=2;
 std::shared_ptr<const RiverFlowNetwork> faster;
 Check(BuildRiverFlowNetwork(scene,fasterInputs,{{"join",{"west","east"},"outlet"}},faster,error),error);
 std::shared_ptr<const RiverJunctionSurface> fasterSurface;Check(BuildRiverJunctionSurface(*faster,fasterSurface,error),error);
 auto fastDomain=production::Build(*faster,*fasterSurface,adopted,320,160);
 Check(fastDomain.active==domain.active&&std::abs(fastDomain.inflow/domain.inflow-2)<1e-5&&
    fastDomain.inflow==fastDomain.outflow&&std::abs(fastDomain.maximumSpeed/domain.maximumSpeed-2)<1e-4,
    "Twice inlet speed doubles balanced flux and actual projected velocity without changing banks");
 std::shared_ptr<const RiverFlowNetwork> single;Check(BuildRiverFlowNetwork(scene,{inputs.front()},{},single,error),error);
 bool refused=false;try{production::Build(*single,*surface,adopted,320,160);}catch(const std::exception&){refused=true;}Check(refused,"Unsupported topology refuses before indexing junctions");
 std::cout<<"WATERWAY_TRANSPORT_ADOPTION=PASS fluid="<<domain.active.size()<<" steps=100 fluxMaxError=0 stateMaxError=0 uuidStateMaxError=0 inflow="<<domain.inflow<<" outflow="<<domain.outflow<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"WATERWAY_TRANSPORT_ADOPTION=FAIL "<<e.what()<<"\n";return 1;}}
