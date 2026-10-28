#pragma once
// G7C isolated transport oracle; no production call site or authored state.
#include "RiverJunctionFixture.h"
#include "RiverJunctionClassifier.h"
#include <array>
#include <numeric>
namespace junction_foam
{
using namespace renegade::bridge;
using namespace junction_classification;
struct Adapter
{
    const RiverJunctionSurface& surface;
    std::vector<RiverFlowRibbonRow> ribbon;
    const std::vector<RiverFlowRibbonRow>& NativeRibbon() const{return ribbon;}
    RiverFlowQuery Evaluate(XMFLOAT2 p) const {
        const auto q=surface.Evaluate(p);RiverFlowQuery out;out.valid=q.valid;return out;
    }
};
struct Domain
{
    Mask mask;
    std::vector<XMFLOAT4> faces,velocity,source; // outward R/T/L/B face flux m2/s; RG dir/B speed/A fluid; west/east/outlet emission
    std::vector<int> active,index;
    float maximumRate=0,maximumSpeed=0;
    size_t disconnectedCells=0;
    double inflow=0,outflow=0,maximumDivergence=0,projectionResidual=0;
};
inline Domain Build(const RiverFlowNetwork& network,const RiverJunctionSurface& surface,
    const std::vector<Surface>& obstacles,unsigned width=640,unsigned height=320)
{
    using junction_proof::Check;
    Adapter adapter{surface,{}};
    for(const auto& reach:network.Reaches())adapter.ribbon.insert(adapter.ribbon.end(),reach.field->NativeRibbon().begin(),reach.field->NativeRibbon().end());
    Domain d;d.mask=junction_classification::Build(adapter,obstacles,width,height,.14f);
    const auto size=d.mask.cells.size();d.faces.resize(size);d.velocity.resize(size);d.source.resize(size);d.index.assign(size,-1);
    const float dx=d.mask.bounds.z/width,dz=d.mask.bounds.w/height;
    const std::array<int,4> offsets{1,int(width),-1,-int(width)};
    const std::array<XMFLOAT2,4> normals{{{1,0},{0,1},{-1,0},{0,-1}}};
    auto point=[&](std::size_t k){return XMFLOAT2{d.mask.bounds.x+(k%width+.5f)*dx,d.mask.bounds.y+(k/width+.5f)*dz};};
    auto neighbour=[&](std::size_t k,int side) {
        const int x=int(k%width),z=int(k/width);
        if((side==0&&x+1==int(width))||(side==2&&x==0)||(side==1&&z+1==int(height))||(side==3&&z==0))return -1;
        return int(k)+offsets[side];
    };
    std::vector<XMFLOAT2> base(size);
    for(std::size_t k=0;k<size;++k) {
        const int category=int(d.mask.cells[k].x);if(category==Invalid||category==SolidExcluded)continue;
        const auto q=surface.Evaluate(point(k));if(!q.valid)continue;
        d.index[k]=int(d.active.size());d.active.push_back(int(k));base[k]=q.velocityXZ;
    }
    Check(!d.active.empty(),"Empty junction fluid raster");
    // Raster slivers separated by actual banks are stagnant, not connected flow.
    // Keep the largest four-face-connected wet component; never bridge a rock.
    std::vector<unsigned char> visited(size);
    std::vector<int> largest,component;
    for(int seed:d.active)if(!visited[seed]) {
        component.clear();component.push_back(seed);visited[seed]=1;
        for(size_t head=0;head<component.size();++head)for(int side=0;side<4;++side) {
            int n=neighbour(component[head],side);
            if(n>=0&&d.index[n]>=0&&!visited[n]){visited[n]=1;component.push_back(n);}
        }
        if(component.size()>largest.size())largest=component;
    }
    d.disconnectedCells=d.active.size()-largest.size();
    Check(d.disconnectedCells<=d.active.size()/100,"Native bank raster disconnects a substantial flow region");
    d.active=std::move(largest);std::sort(d.active.begin(),d.active.end());
    std::fill(d.index.begin(),d.index.end(),-1);
    for(size_t j=0;j<d.active.size();++j)d.index[d.active[j]]=int(j);

    struct Port{RiverFlowNetworkPort port;bool outlet;};
    std::vector<Port> ports;
    for(const auto& reach:network.Reaches()) {
        if(reach.id==network.Junctions()[0].outgoing)ports.push_back({reach.downstream,true});
        else ports.push_back({reach.upstream,false});
    }
    std::vector<std::pair<int,int>> outletFaces;int gauge=-1;
    std::vector<unsigned> portFaces(ports.size());
    for(int k:d.active)for(int side=0;side<4;++side) {
        const auto normal=normals[side];const float faceLength=(side%2==0?dz:dx);
        const int n=neighbour(k,side);auto* face=&d.faces[k].x;
        if(n>=0&&d.index[n]>=0) {
            face[side]=((base[k].x+base[n].x)*normal.x+(base[k].y+base[n].y)*normal.y)*.5f*faceLength;
            continue;
        }
        const auto p=point(k);const XMFLOAT2 centre{p.x+normal.x*dx*.5f,p.y+normal.y*dz*.5f};
        for(size_t portIndex=0;portIndex<ports.size();++portIndex) {
            const auto& port=ports[portIndex];
            const auto& a=port.port.rightBank;const auto& b=port.port.leftBank;
            const float alignment=(normal.x*port.port.directionXZ.x+normal.y*port.port.directionXZ.y)*(port.outlet?1:-1);
            if(alignment<=.2f||Distance(centre,{{a.x,a.z},{b.x,b.z}})>std::max(dx,dz)*1.1f)continue;
            face[side]=(port.port.directionXZ.x*normal.x+port.port.directionXZ.y*normal.y)*port.port.speedMetersPerSecond*faceLength;
            ++portFaces[portIndex];
            if(port.outlet){d.outflow+=face[side];outletFaces.push_back({k,side});gauge=d.index[k];}
            else d.inflow-=face[side];
            break;
        }
    }
    for(auto faces:portFaces)Check(faces>0,"A native cap is disconnected from the shared fluid domain");
    Check(d.inflow>0&&d.outflow>0&&gauge>=0,"Native open caps not represented on fluid grid");
    // Unit-depth kinematic boundary budget: two inlets feed the single outlet.
    // No depth/cross-section hydrodynamics is claimed.
    const float outletScale=float(d.inflow/d.outflow);
    for(auto entry:outletFaces)(&d.faces[entry.first].x)[entry.second]*=outletScale;
    d.outflow=d.inflow;
    const std::size_t count=d.active.size();
    std::vector<std::array<int,4>> adjacent(count);std::vector<double> diagonal(count),rhs(count),pressure(count),r(count),z(count),p(count),ap(count);
    const double weights[]={double(dz)/dx,double(dx)/dz,double(dz)/dx,double(dx)/dz};
    for(std::size_t j=0;j<count;++j) {
        const int k=d.active[j];for(int side=0;side<4;++side) {
            const int n=neighbour(k,side);adjacent[j][side]=n>=0?d.index[n]:-1;
            if(adjacent[j][side]>=0)diagonal[j]+=weights[side];
            rhs[j]-=(&d.faces[k].x)[side];
        }
        Check(diagonal[j]>0,"Isolated fluid raster cell");
    }
    diagonal[gauge]=1;rhs[gauge]=0;
    auto dot=[](const auto& a,const auto& b){double sum=0;for(std::size_t n=0;n<a.size();++n)sum+=a[n]*b[n];return sum;};
    auto multiply=[&](const auto& input,auto& output) {
        for(std::size_t j=0;j<count;++j) {
            if(int(j)==gauge){output[j]=input[j];continue;}
            double value=diagonal[j]*input[j];
            for(int side=0;side<4;++side)if(adjacent[j][side]>=0&&adjacent[j][side]!=gauge)value-=weights[side]*input[adjacent[j][side]];
            output[j]=value;
        }
    };
    r=rhs;for(std::size_t j=0;j<count;++j)z[j]=p[j]=r[j]/diagonal[j];
    double rz=dot(r,z),initial=std::max(dot(rhs,rhs),1e-30);
    unsigned iterations=0;
    for(;iterations<3000&&dot(r,r)>initial*1e-16;++iterations) {
        multiply(p,ap);const double divisor=dot(p,ap);Check(divisor>0,"Projection matrix lost positive definiteness");
        const double alpha=rz/divisor;
        for(std::size_t j=0;j<count;++j){pressure[j]+=alpha*p[j];r[j]-=alpha*ap[j];z[j]=r[j]/diagonal[j];}
        const double next=dot(r,z),beta=next/rz;
        for(std::size_t j=0;j<count;++j)p[j]=z[j]+beta*p[j];rz=next;
    }
    d.projectionResidual=std::sqrt(dot(r,r)/initial);
    Check(d.projectionResidual<1e-6,"Bounded pressure projection did not converge");
    for(std::size_t j=0;j<count;++j) {
        const int k=d.active[j];auto* f=&d.faces[k].x;
        for(int side=0;side<4;++side)if(adjacent[j][side]>=0)f[side]+=float((pressure[j]-pressure[adjacent[j][side]])*weights[side]);
        d.maximumDivergence=std::max(d.maximumDivergence,std::abs(double(f[0])+f[1]+f[2]+f[3]));
        const float vx=(f[0]-f[2])/(2*dz),vz=(f[1]-f[3])/(2*dx),speed=std::hypot(vx,vz);
        Check(std::isfinite(speed)&&speed<10,"Unsafe projected fluid velocity");
        d.velocity[k]={speed>1e-7f?vx/speed:0,speed>1e-7f?vz/speed:0,speed,1};
        d.maximumSpeed=std::max(d.maximumSpeed,speed);
        float outward=0;for(int side=0;side<4;++side)outward+=std::max(f[side],0.f);
        d.maximumRate=std::max(d.maximumRate,outward/(dx*dz));
        if(int(d.mask.cells[k].x)!=RockContact)continue;
        float nearest=INFINITY;XMFLOAT2 outwardNormal{};const auto world=point(k);
        for(const auto& line:d.mask.rockLines) {
            const float ax=line.b.x-line.a.x,az=line.b.y-line.a.y,len=ax*ax+az*az;
            const float t=len>1e-12f?std::clamp(((world.x-line.a.x)*ax+(world.y-line.a.y)*az)/len,0.f,1.f):0;
            const XMFLOAT2 delta{world.x-line.a.x-t*ax,world.y-line.a.y-t*az};const float distance=std::hypot(delta.x,delta.y);
            if(distance<nearest){nearest=distance;outwardNormal=distance>1e-7f?XMFLOAT2{delta.x/distance,delta.y/distance}:XMFLOAT2{};}
        }
        // Impact is supplied by incident reach flow, before no-penetration deflects it.
        // Sampling the projected wall-tangent velocity would erase leading impact.
        const float incidentSpeed=std::hypot(base[k].x,base[k].y);
        const float incidence=incidentSpeed>1e-7f?-(outwardNormal.x*base[k].x+outwardNormal.y*base[k].y)/incidentSpeed:0;
        const float rate=2.8f*std::exp(-nearest/.08f)*(.015f+.985f*std::pow(std::max(0.f,incidence),1.5f))*incidentSpeed;
        int origin=2;float best=INFINITY;
        for(int n=0;n<2;++n) {
            const auto* reach=network.FindReach(n==0?"west":"east");Check(reach!=nullptr,"Proof reach identities missing");
            const auto q=reach->field->Evaluate(world);
            if(q.valid&&std::abs(q.lateralFraction)<best){best=std::abs(q.lateralFraction);origin=n;}
        }
        (&d.source[k].x)[origin]=rate;
    }
    Check(d.maximumDivergence<1e-4,"Projected face flux is not locally balanced");
    return d;
}
inline std::vector<XMFLOAT4> Step(const Domain& d,const std::vector<XMFLOAT4>& state,float dt,float time,
    float lifetime=6,unsigned sourceMask=7,float speedScale=1)
{
    using junction_proof::Check;
    Check(state.size()==d.faces.size()&&std::isfinite(dt)&&dt>=0&&dt<=.1f&&std::isfinite(time)&&
        std::isfinite(lifetime)&&lifetime>0&&std::isfinite(speedScale)&&speedScale>=0&&speedScale<=2,"Invalid transport step");
    Check(dt*d.maximumRate*speedScale<=.8f,"Transport step exceeds positivity CFL");
    auto next=state;const auto w=d.mask.width;const float area=d.mask.bounds.z/w*d.mask.bounds.w/d.mask.height;
    const float decay=std::exp(-dt/lifetime);
    for(int k:d.active) {
        auto value=state[k];const int neighbours[]={k+1,k+int(w),k-1,k-int(w)};
        for(int side=0;side<4;++side) {
            const float f=(&d.faces[k].x)[side]*speedScale;
            const int x=k%w,z=k/w;
            const bool outside=(side==0&&x+1==int(w))||(side==2&&x==0)||(side==1&&z+1==int(d.mask.height))||(side==3&&z==0);
            const int n=outside?-1:neighbours[side];
            const XMFLOAT4 other=n>=0&&n<int(state.size())&&d.index[n]>=0?state[n]:XMFLOAT4{};
            const auto upstream=f>=0?state[k]:other;
            for(int c=0;c<4;++c)(&value.x)[c]-=dt*f/area*(&upstream.x)[c];
        }
        const auto pos=XMFLOAT2{d.mask.bounds.x+(k%w+.5f)/w*d.mask.bounds.z,d.mask.bounds.y+(k/w+.5f)/d.mask.height*d.mask.bounds.w};
        const float pulse=.35f+.65f*(.5f+.5f*std::sin(pos.x*8.3f+pos.y*5.7f-time*2));
        float emission=0;for(int c=0;c<3;++c)if(sourceMask&(1u<<c))emission+=(&d.source[k].x)[c];
        const float added=emission*pulse*speedScale*dt;
        value.y=(value.y+value.x*dt)*decay;value.x=value.x*decay+added;
        value.z=value.z*decay+(pos.x-d.velocity[k].x*d.velocity[k].z*time)*added;
        value.w=value.w*decay+(pos.y-d.velocity[k].y*d.velocity[k].z*time)*added;
        if(value.x<1e-7f||value.y/std::max(value.x,1e-7f)>18)value={};
        next[k]=value;
    }
    return next;
}
}
