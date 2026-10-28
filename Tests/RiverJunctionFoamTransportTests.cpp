#include "RiverJunctionFoamTransport.h"
#include <iostream>
using namespace junction_proof;using namespace junction_foam;
static Surface Box(float x,float z) {
    Surface s;const XMFLOAT3 v[]={{x-.45f,-1,z-.45f},{x+.45f,-1,z-.45f},{x+.45f,-1,z+.45f},{x-.45f,-1,z+.45f},
        {x-.45f,1,z-.45f},{x+.45f,1,z-.45f},{x+.45f,1,z+.45f},{x-.45f,1,z+.45f}};
    const int ix[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
    for(int n=0;n<36;n+=3)s.triangles.push_back({v[ix[n]],v[ix[n+1]],v[ix[n+2]]});return s;
}
static double Mass(const Domain& d,const std::vector<XMFLOAT4>& a,float minX=-INFINITY) {
    double sum=0;for(int k:d.active)if(d.mask.bounds.x+(k%d.mask.width+.5f)/d.mask.width*d.mask.bounds.z>minX)sum+=a[k].x;
    return sum*d.mask.bounds.z/d.mask.width*d.mask.bounds.w/d.mask.height;
}
int main(){try{
    wi::scene::Scene scene;Create(scene);auto network=Network(scene);
    std::shared_ptr<const RiverJunctionSurface> surface;std::string why;
    Check(BuildRiverJunctionSurface(*network,surface,why),why);
    auto d=Build(*network,*surface,{Box(-7,-4),Box(-7,4)},320,160);
    Check(d.mask.counts[SolidExcluded]>0&&d.mask.counts[RockContact]>0,"Actual sliced solids/contact cells");
    double west=0,east=0;for(auto s:d.source){west+=s.x;east+=s.y;}
    Check(west>0&&east>0,"Independent native branch sources");
    const float dt=std::min(.05f,.75f/d.maximumRate);
    std::vector<XMFLOAT4> state(d.faces.size());
    const float area=d.mask.bounds.z/d.mask.width*d.mask.bounds.w/d.mask.height;
    // Exact one-step global budget before age/cutoff can discard any mass.
    for(int k:d.active)state[k]={.5f,0,0,0};
    double escaping=0;for(int k:d.active)for(int side=0;side<4;++side){
        const int n=k+(side==0?1:side==1?int(d.mask.width):side==2?-1:-int(d.mask.width));
        if(n<0||n>=int(state.size())||d.index[n]<0)escaping+=std::max(0.f,(&d.faces[k].x)[side])*state[k].x;
    }
    const double before=Mass(d,state);auto next=Step(d,state,dt,0,6,0);
    Check(std::abs(Mass(d,next)-(before-dt*escaping)*std::exp(-dt/6))<before*2e-6,"Conservative transport/decay/outflow budget");
    auto paused=Step(d,state,dt,0,6,7,0);
    Check(std::abs(Mass(d,paused)-before*std::exp(-dt/6))<before*2e-6,"Zero-flow pause only decays existing foam");
    for(unsigned origin:{1u,2u}){
        state.assign(state.size(),{});double peakOutlet=0;
        for(float t=0;t<32;t+=dt){
            state=Step(d,state,dt,t,6,t<8?origin:0);
            peakOutlet=std::max(peakOutlet,Mass(d,state,5));
            for(std::size_t k=0;k<state.size();++k){
                Check(std::isfinite(state[k].x)&&state[k].x>=-1e-7f,"Finite nonnegative density");
                Check(d.index[k]>=0||state[k].x==0,"No foam inside rock/bank/outside native union");
            }
        }
        Check(peakOutlet>.00001,"Tributary source reaches common downstream outlet");
        for(float t=32;t<60;t+=dt)state=Step(d,state,dt,t,6,0);
        Check(Mass(d,state)<1e-7,"Emission-off foam expires");
        std::cout<<"ORIGIN="<<origin<<" OUTLET_PEAK="<<peakOutlet<<" FINAL_MASS="<<Mass(d,state)<<"\n";
    }
    bool refused=false;try{Step(d,state,1,0);}catch(const std::exception&){refused=true;}Check(refused,"Unsafe step refused");
    std::cout<<"JUNCTION_FOAM_CPU=PASS fluid="<<d.active.size()<<" inflow="<<d.inflow<<" outflow="<<d.outflow
        <<" maxDivergence="<<d.maximumDivergence<<" projectionResidual="<<d.projectionResidual<<" maxSpeed="<<d.maximumSpeed<<" dt="<<dt<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
