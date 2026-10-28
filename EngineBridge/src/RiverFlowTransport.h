#pragma once
#include "RiverFlowWaterline.h"
// G5 proof-local static transport preparation. No scene/renderer mutation.
namespace river_v2_transport {
using namespace river_v2_classification;
struct Transport {
    unsigned width=0,height=0;XMFLOAT4 bounds{};
    std::vector<XMFLOAT4> velocity; // signed unit X/Z, speed m/s, fluid validity.
    std::vector<XMFLOAT4> source;   // rock rate, bank rate, wake influence, signed lateral metres.
    double rockRate=0,bankRate=0;float maximumSpeed=0;
};
inline Transport Prepare(const renegade::bridge::RiverFlowField& flow,const Mask& mask,float speedScale=1,float sourceWidth=.04f){
    if(!std::isfinite(speedScale)||speedScale<0||speedScale>3||!std::isfinite(sourceWidth)||sourceWidth<.01f||sourceWidth>.08f||
       mask.width<8||mask.height<8||size_t(mask.width)*mask.height>1048576||mask.cells.size()!=size_t(mask.width)*mask.height||
       !std::isfinite(mask.bounds.z)||!std::isfinite(mask.bounds.w)||mask.bounds.z<=0||mask.bounds.w<=0)
        throw std::runtime_error("G5 invalid transport settings");
    Transport out;out.width=mask.width;out.height=mask.height;out.bounds=mask.bounds;
    const size_t size=mask.cells.size();out.velocity.resize(size);out.source.resize(size);
    std::vector<float> potential(size),speed(size);std::vector<XMFLOAT2> base(size);
    std::vector<unsigned char> fluid(size),fixed(size);
    const float dx=mask.bounds.z/mask.width,dz=mask.bounds.w/mask.height,wx=1/(dx*dx),wz=1/(dz*dz);
    auto p=[&](size_t k){return XMFLOAT2{mask.bounds.x+(k%mask.width+.5f)*dx,mask.bounds.y+(k/mask.width+.5f)*dz};};
    XMFLOAT2 centre{};float rockRadius=.25f;
    for(auto line:mask.rockLines){centre.x+=line.a.x+line.b.x;centre.y+=line.a.y+line.b.y;}
    if(!mask.rockLines.empty()){
        centre.x/=float(mask.rockLines.size()*2);centre.y/=float(mask.rockLines.size()*2);
        for(auto line:mask.rockLines)for(auto q:{line.a,line.b})rockRadius=std::max(rockRadius,std::hypot(q.x-centre.x,q.y-centre.y));
    }
    for(size_t k=0;k<size;++k){
        const auto c=mask.cells[k];
        if(!std::isfinite(c.x)||!std::isfinite(c.y)||!std::isfinite(c.z)||!std::isfinite(c.w)||c.x<0||c.x>6||std::floor(c.x)!=c.x)
            throw std::runtime_error("G5 malformed classification snapshot");
        auto q=flow.Evaluate(p(k));const int category=int(c.x);
        fluid[k]=q.valid&&category!=Invalid&&category!=SolidExcluded;
        potential[k]=q.alongMeters;base[k]=q.directionXZ;speed[k]=q.speedMetersPerSecond*speedScale;
        fixed[k]=q.alongMeters<.3f||q.alongMeters>flow.LengthMeters()-.3f;
    }
    // Harmonic potential with no-flux banks/solid boundaries; authored end potentials.
    // This is a bounded kinematic approximation, not Navier-Stokes hydrology.
    for(int iteration=0;iteration<240;++iteration)for(int parity=0;parity<2;++parity){
        for(unsigned z=1;z+1<mask.height;++z)for(unsigned x=1;x+1<mask.width;++x){
            size_t k=z*mask.width+x;if(((x+z)&1)!=parity||!fluid[k]||fixed[k])continue;
            float sum=0,weight=0;
            for(auto entry:{std::pair<size_t,float>{k-1,wx},{k+1,wx},{k-mask.width,wz},{k+mask.width,wz}})
                if(fluid[entry.first]){sum+=potential[entry.first]*entry.second;weight+=entry.second;}
            if(weight>0)potential[k]=sum/weight;
        }
    }
    for(unsigned z=0;z<mask.height;++z)for(unsigned x=0;x<mask.width;++x){
        size_t k=z*mask.width+x;if(!fluid[k])continue;
        auto phi=[&](int xx,int zz){if(xx<0||zz<0||xx>=int(mask.width)||zz>=int(mask.height))return potential[k];
            size_t j=size_t(zz)*mask.width+xx;return fluid[j]?potential[j]:potential[k];};
        float vx=(phi(int(x)+1,z)-phi(int(x)-1,z))/(2*dx),vz=(phi(x,int(z)+1)-phi(x,int(z)-1))/(2*dz);
        if((x==0||!fluid[k-1])&&vx<0)vx=0;
        if((x+1==mask.width||!fluid[k+1])&&vx>0)vx=0;
        if((z==0||!fluid[k-mask.width])&&vz<0)vz=0;
        if((z+1==mask.height||!fluid[k+mask.width])&&vz>0)vz=0;
        float magnitude=std::hypot(vx,vz);
        const float clamp=magnitude>2.2f?2.2f/magnitude:1;
        vx*=clamp*speed[k];vz*=clamp*speed[k];magnitude=std::hypot(vx,vz);
        out.maximumSpeed=std::max(out.maximumSpeed,magnitude);
        out.velocity[k]={magnitude>1e-7f?vx/magnitude:0,magnitude>1e-7f?vz/magnitude:0,magnitude,1};
        const auto point=p(k);float rockSource=0,bankSource=0;
        if(int(mask.cells[k].x)==RockContact&&!mask.rockLines.empty()){
            float nearest=INFINITY;XMFLOAT2 normal{};
            for(auto line:mask.rockLines){
                const float ax=line.b.x-line.a.x,az=line.b.y-line.a.y,len=ax*ax+az*az;
                float u=len>1e-12f?std::clamp(((point.x-line.a.x)*ax+(point.y-line.a.y)*az)/len,0.f,1.f):0;
                XMFLOAT2 delta{point.x-line.a.x-u*ax,point.y-line.a.y-u*az};float dist=std::hypot(delta.x,delta.y);
                if(dist<nearest){nearest=dist;normal=dist>1e-6f?XMFLOAT2{delta.x/dist,delta.y/dist}:XMFLOAT2{};}
            }
            const float incidence=normal.x*base[k].x+normal.y*base[k].y;
            const float leading=std::max(0.f,-incidence),shear=std::sqrt(std::max(0.f,1-incidence*incidence));
            rockSource=2.8f*std::exp(-nearest/sourceWidth)*(.015f+.985f*std::pow(leading,1.5f)+.075f*shear)*speed[k];
        }
        if(int(mask.cells[k].x)==Bank)bankSource=.035f*std::exp(-mask.cells[k].y/.05f)*speed[k];
        float wake=0,wakeLateral=0;
        if(!mask.rockLines.empty()){
            float a=(point.x-centre.x)*base[k].x+(point.y-centre.y)*base[k].y;
            float b=-(point.x-centre.x)*base[k].y+(point.y-centre.y)*base[k].x;
            wakeLateral=b;
            float width=rockRadius*.65f+std::max(0.f,a)*.12f;
            // Preserve the no-flux obstacle flow at contact; shed eddies only
            // beyond the actual footprint, where they cannot inject foam into rock.
            float downstream=a-rockRadius;
            wake=downstream>0?std::clamp(downstream/.7f,0.f,1.f)*std::exp(-downstream/5.f-b*b/(width*width)):0;
        }
        out.source[k]={rockSource,bankSource,wake*speedScale,wakeLateral};out.rockRate+=rockSource;out.bankRate+=bankSource;
    }
    return out;
}
}
