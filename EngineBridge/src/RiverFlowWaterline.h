#pragma once
// Bounded static geometry classifier adopted from the accepted G4 proof.
#include "renegade/bridge/RiverFlowFieldService.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
namespace river_v2_classification {
using DirectX::XMFLOAT2;using DirectX::XMFLOAT3;using DirectX::XMFLOAT4;
struct Triangle {XMFLOAT3 a,b,c;};
struct Surface {bool terrain=false;std::vector<Triangle> triangles;};
struct Segment {XMFLOAT2 a,b;};
struct Mask {
    unsigned width=0,height=0;XMFLOAT4 bounds{};
    std::vector<XMFLOAT4> cells; // R category, G bank distance, B rock distance, A valid.
    std::vector<Segment> rockLines,bankLines;
    std::array<size_t,7> counts{};size_t triangles=0,work=0;
};
enum Category {Invalid=0,Open=1,Bank=2,RockContact=3,Submerged=4,AboveExcluded=5,SolidExcluded=6};
inline bool Finite(XMFLOAT3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline float Distance(XMFLOAT2 p,Segment s){
    const float dx=s.b.x-s.a.x,dz=s.b.y-s.a.y,l=dx*dx+dz*dz;
    const float u=l>1e-14f?std::clamp(((p.x-s.a.x)*dx+(p.y-s.a.y)*dz)/l,0.f,1.f):0;
    return std::hypot(p.x-s.a.x-u*dx,p.y-s.a.y-u*dz);
}
inline std::vector<Segment> Slice(const Triangle& t,float y){
    std::vector<XMFLOAT2> points;
    const XMFLOAT3 v[]={t.a,t.b,t.c};
    auto add=[&](XMFLOAT2 p){for(auto q:points)if(std::hypot(q.x-p.x,q.y-p.y)<1e-6f)return;points.push_back(p);};
    for(int i=0;i<3;++i){
        const auto a=v[i],b=v[(i+1)%3];const float da=a.y-y,db=b.y-y;
        if(std::abs(da)<1e-6f)add({a.x,a.z});
        if((da<0&&db>0)||(da>0&&db<0)){float u=da/(da-db);add({a.x+(b.x-a.x)*u,a.z+(b.z-a.z)*u});}
    }
    // Coplanar faces are not a unique waterline; never invent their perimeter.
    if(points.size()!=2||std::hypot(points[0].x-points[1].x,points[0].y-points[1].y)<1e-6f)return {};
    return {{points[0],points[1]}};
}
template<class NativeFlow>
inline Mask Build(const NativeFlow& flow,const std::vector<Surface>& surfaces,
    unsigned width=1024,unsigned height=256,float band=.12f){
    if(width<8||height<8||width>2048||height>2048||size_t(width)*height>1048576||
       surfaces.size()>64||!std::isfinite(band)||band<=0||band>1)throw std::runtime_error("G4 settings over limit");
    const float waterY=flow.NativeRibbon().front().centre.y;
    float minX=INFINITY,minZ=INFINITY,maxX=-INFINITY,maxZ=-INFINITY;
    for(const auto& row:flow.NativeRibbon()){
        if(std::abs(row.centre.y-waterY)>.0001f||std::abs(row.leftBank.y-waterY)>.0001f||std::abs(row.rightBank.y-waterY)>.0001f)throw std::runtime_error("Flow and foam currently require a flat river");
        for(auto p:{row.leftBank,row.rightBank}){minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);minZ=std::min(minZ,p.z);maxZ=std::max(maxZ,p.z);}
    }
    if(!std::isfinite(waterY)||!std::isfinite(maxX-minX)||!std::isfinite(maxZ-minZ)||maxX<=minX||maxZ<=minZ)throw std::runtime_error("G4 invalid native bounds");
    minX-=.25f;minZ-=.25f;maxX+=.25f;maxZ+=.25f;
    Mask out;out.width=width;out.height=height;out.bounds={minX,minZ,maxX-minX,maxZ-minZ};
    const size_t count=size_t(width)*height;
    std::vector<float> terrain(count,-INFINITY),bankDistance(count,1000),rockDistance(count,1000);
    std::vector<unsigned char> above(count),submerged(count),solid(count);
    auto point=[&](size_t k){return XMFLOAT2{minX+(k%width+.5f)/width*out.bounds.z,minZ+(k/width+.5f)/height*out.bounds.w};};
    auto rect=[&](float x0,float z0,float x1,float z1){
        auto pixel=[](double p,double minimum,double extent,unsigned count,bool upper){
            const double value=(p-minimum)/extent*count;
            return int(std::clamp(upper?std::ceil(value):std::floor(value),0.0,double(count-1)));
        };
        return std::array<int,4>{pixel(x0,minX,out.bounds.z,width,false),pixel(z0,minZ,out.bounds.w,height,false),
            pixel(x1,minX,out.bounds.z,width,true),pixel(z1,minZ,out.bounds.w,height,true)};
    };
    auto work=[&](){if(++out.work>32000000)throw std::runtime_error("G4 raster work limit");};
    for(const auto& surface:surfaces){
        std::vector<float> low,high;std::vector<int> winding;
        if(!surface.terrain){low.assign(count,INFINITY);high.assign(count,-INFINITY);winding.assign(count,0);}
        for(const auto& t:surface.triangles){
            // Reject corrupt input regardless of location. The 250k triangle
            // RASTER budget applies only to triangles that can affect this
            // river's bounded waterline, not to every far terrain chunk in
            // the scene. Counting before this spatial clip made valid 19x19
            // native terrain scenes fail after unrelated obstacle edits.
            if(!Finite(t.a)||!Finite(t.b)||!Finite(t.c))
                throw std::runtime_error("G4 invalid/overlimit triangle snapshot");
            float x0=std::min({t.a.x,t.b.x,t.c.x}),x1=std::max({t.a.x,t.b.x,t.c.x});
            float z0=std::min({t.a.z,t.b.z,t.c.z}),z1=std::max({t.a.z,t.b.z,t.c.z});
            if(x1<minX-band||x0>maxX+band||z1<minZ-band||z0>maxZ+band)continue;
            if(++out.triangles>250000)
                throw std::runtime_error("G4 invalid/overlimit triangle snapshot");
            auto lines=Slice(t,waterY);
            auto& dest=surface.terrain?out.bankLines:out.rockLines;
            auto& distances=surface.terrain?bankDistance:rockDistance;
            for(auto line:lines){
                dest.push_back(line);
                auto box=rect(std::min(line.a.x,line.b.x)-band,std::min(line.a.y,line.b.y)-band,
                    std::max(line.a.x,line.b.x)+band,std::max(line.a.y,line.b.y)+band);
                for(int z=box[1];z<=box[3];++z)for(int x=box[0];x<=box[2];++x){work();size_t k=z*width+x;distances[k]=std::min(distances[k],Distance(point(k),line));}
            }
            const double dx1=t.b.x-t.a.x,dz1=t.b.z-t.a.z,dx2=t.c.x-t.a.x,dz2=t.c.z-t.a.z;
            const double det=dx1*dz2-dz1*dx2;
            if(std::abs(det)<1e-12)continue; // Vertical face handled by its true slice above.
            const auto box=rect(x0,z0,x1,z1);
            for(int z=box[1];z<=box[3];++z)for(int x=box[0];x<=box[2];++x){
                work();const size_t k=z*width+x;auto p=point(k);
                const double px=p.x-t.a.x,pz=p.y-t.a.z;
                const double u=(px*dz2-pz*dx2)/det,v=(dx1*pz-dz1*px)/det;
                if(u<-.0000001||v<-.0000001||u+v>1.0000001)continue;
                const float y=float(t.a.y+u*(t.b.y-t.a.y)+v*(t.c.y-t.a.y));
                if(surface.terrain)terrain[k]=std::max(terrain[k],y);
                else {low[k]=std::min(low[k],y);high[k]=std::max(high[k],y);if(y>waterY)winding[k]+=det>0?1:-1;}
            }
        }
        if(!surface.terrain)for(size_t k=0;k<count;++k)if(std::isfinite(low[k])){
            if(high[k]<waterY-1e-5f)submerged[k]=1;
            else if(low[k]>waterY+1e-5f)above[k]=1;
            else if(winding[k]!=0)solid[k]=1;
        }
    }
    out.cells.resize(count);
    for(size_t k=0;k<count;++k){
        work();const auto q=flow.Evaluate(point(k));
        int category=Invalid;
        if(q.valid&&!q.ambiguous){
            category=Open;
            if(submerged[k])category=Submerged;
            if(bankDistance[k]<=band||(std::isfinite(terrain[k])&&terrain[k]>=waterY-band&&terrain[k]<=waterY))category=Bank;
            if(rockDistance[k]<=band)category=RockContact;
            if(above[k])category=AboveExcluded;
            if(solid[k]||(std::isfinite(terrain[k])&&terrain[k]>waterY+1e-5f))category=SolidExcluded;
        }
        ++out.counts[category];
        out.cells[k]={float(category),bankDistance[k],rockDistance[k],q.valid?1.f:0.f};
    }
    return out;
}
}
