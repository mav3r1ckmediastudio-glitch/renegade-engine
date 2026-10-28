#pragma once
// G13: a waterfall effect is a PLACEABLE scene component, not a river/bed solver.
// Stable geometry + user-facing placement state. Graphics/VFX generated separately.
#include <WickedEngine.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>
namespace renegade::bridge::placeable_waterfall {
using namespace DirectX;
struct Definition {
    XMFLOAT3 crest{0,12,0};       // first user handle
    XMFLOAT3 landing{1.6f,0,0}; // second user handle
    float width=3.5f;
    float intensity=1.f;
    float spray=1.f; // airborne spray on the waterfall body
    float mist=1.f;  // ambient/falling mist
    float impactSpray=1.f; // independent pool-landing burst, [0,3]
    float turbulence=.7f;
};
struct Support {
    XMFLOAT3 center{};
    float progression=0,travelTime=0;
};
struct Geometry {
    std::vector<Support> supports;
    XMFLOAT3 lateral{},forward{};
    float drop=0,span=0;
};
inline void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
inline Geometry BuildGeometry(const Definition& d){
    for(float v:{d.crest.x,d.crest.y,d.crest.z,d.landing.x,d.landing.y,d.landing.z,d.width,d.intensity,d.spray,d.mist,d.impactSpray,d.turbulence})
        Require(std::isfinite(v),"Non-finite waterfall placement");
    float drop=d.crest.y-d.landing.y;
    Require(drop>=.5f&&drop<=60.f,"Waterfall vertical drop must be 0.5..60m");
    Require(d.width>=.3f&&d.width<=25.f,"Width must be 0.3..25m");
    Require(d.intensity>=0&&d.intensity<=3&&d.spray>=0&&d.spray<=3&&d.mist>=0&&d.mist<=3&&d.impactSpray>=0&&d.impactSpray<=3,"Effect intensity out of bounds");
    Require(d.turbulence>=0&&d.turbulence<=2,"Turbulence out of bounds");
    XMFLOAT3 drift{d.landing.x-d.crest.x,0,d.landing.z-d.crest.z};
    float horiz=std::hypot(drift.x,drift.z);
    XMFLOAT3 dir=horiz>.001f?XMFLOAT3{drift.x/horiz,0,drift.z/horiz}:XMFLOAT3{1,0,0};
    Geometry g;g.forward=dir;g.lateral={-dir.z,0,dir.x};
    g.drop=drop;g.span=horiz;
    constexpr int segments=100;
    auto bezier=[&](float a,float b,float c,float d,float t) {
        float u=1-t;return u*u*u*a+3*u*u*t*b+3*u*t*t*c+t*t*t*d;
    };
    // Soft natural overhang, gravity-led near-vertical falling flow, no long rope gaps.
    // Intermediate positions are DERIVED, and both endpoints match their handles.
    float vstart=std::min(.75f,drop*.17f);
    for(int i=0;i<=segments;++i){
        float t=float(i)/segments;
        float lateralTravel=bezier(0,.15f,std::max(.3f,horiz*.67f),horiz,t);
        float y=bezier(drop,drop+.02f,drop*.52f,0,t);
        if(i==0){lateralTravel=0;y=drop;}
        if(i==segments){lateralTravel=horiz;y=0;}
        // Non-flat ballistic-inspired travel coordinate drives native animation.
        float elapsed=std::sqrt(2*drop*std::max(0.f,t)/9.80665f);
        g.supports.push_back({{
            d.crest.x+dir.x*lateralTravel,
            d.landing.y+y,
            d.crest.z+dir.z*lateralTravel
        },t,elapsed});
    }
    return g;
}
// Bounds visual layers and particle placement in the same author-forward frame.
inline float VisualScale(const Definition& d){return std::clamp(std::sqrt((d.crest.y-d.landing.y)/12.f),.22f,1.5f);}
inline float FrontSprayOffset(const Definition& d){return .03f+.43f*VisualScale(d)+.037f*std::min(d.width,4.f)*d.turbulence;}
struct SprayVolume {XMFLOAT3 center{},halfSize{},velocity{};float density=1;};
// A continuous near-core source and a sparse overlapping breakup zone. No detached curtain.
inline std::vector<SprayVolume> FrontSprayVolumes(const Definition& d){
    const auto g=BuildGeometry(d);const float scale=VisualScale(d);
    std::vector<SprayVolume> volumes;
    for(int band=0;band<6;++band){
        const float top=d.crest.y-g.drop*band/6.f,bottom=d.crest.y-g.drop*(band+1)/6.f;
        float low=INFINITY,high=-INFINITY;size_t middle=0;float closest=INFINITY;
        for(size_t index=0;index<g.supports.size();++index){const auto& support=g.supports[index];
            float delta=std::abs(support.center.y-(top+bottom)*.5f);if(delta<closest){closest=delta;middle=index;}
            if(support.center.y>=bottom-.01f&&support.center.y<=top+.01f){float along=(support.center.x-d.crest.x)*g.forward.x+(support.center.z-d.crest.z)*g.forward.z;low=std::min(low,along);high=std::max(high,along);}
        }
        Require(std::isfinite(low)&&std::isfinite(high),"Missing water source band");
        auto prev=g.supports[middle?middle-1:middle].center,next=g.supports[std::min(middle+1,g.supports.size()-1)].center;
        XMFLOAT3 tangent;XMStoreFloat3(&tangent,XMVector3Normalize(XMLoadFloat3(&next)-XMLoadFloat3(&prev)));
        float speed=std::clamp(std::sqrt(1.f+2*9.80665f*(d.crest.y-(top+bottom)*.5f)),2.f,18.f);
        const float centreAlong=(low+high)*.5f,spread=(high-low)*.5f;
        for(int zone=0;zone<2;++zone){
            const float rear=zone==0?-spread-.08f:.05f;
            const float front=zone==0?spread+.26f:spread+.25f+.65f*scale;
            const float offset=(rear+front)*.5f,drift=zone==0?.12f:.7f;
            volumes.push_back({{d.crest.x+g.forward.x*(centreAlong+offset),(top+bottom)*.5f,d.crest.z+g.forward.z*(centreAlong+offset)},
                {(front-rear)*.5f,g.drop/12.f,d.width*.46f},
                {tangent.x*speed+g.forward.x*drift,tangent.y*speed,tangent.z*speed+g.forward.z*drift},zone==0?1.f:.18f});
        }
    }
    return volumes;
}
inline void FillSheetMesh(const Definition& d,const Geometry& g,wi::scene::MeshComponent& m,wi::ecs::Entity mat,float offset=0){
    m=wi::scene::MeshComponent();m.SetDoubleSided(true);
    constexpr unsigned across=32;
    for(const auto& q:g.supports) for(unsigned j=0;j<=across;++j){
        float u=float(j)/across;
        // Width varies gently down the fall, but both side boundaries remain stable.
        float openness=.93f+.07f*std::sin((q.progression*.8f+u*.24f)*6.2831853f);
        float side=(u-.5f)*d.width*openness + std::sin(q.progression*13.f + u*8.f)*d.width*.024f*std::sin(q.progression*3.14159265f);
        const float envelope=std::sin(q.progression*3.14159265f);
        const float warp=d.turbulence*envelope*(std::sin(u*8.3f+q.progression*12.7f+offset*7)*.025f+std::sin(u*17.f-q.progression*18.1f)*.012f)*std::min(d.width,4.f);
        const float depth=offset+warp;
        m.vertex_positions.push_back({
            q.center.x+g.lateral.x*side+g.forward.x*depth,
            q.center.y,
            q.center.z+g.lateral.z*side+g.forward.z*depth
        });
        m.vertex_uvset_0.push_back({u,q.progression});
        m.vertex_uvset_1.push_back({u,q.travelTime});
    }
    for(size_t i=0;i+1<g.supports.size();++i) for(unsigned j=0;j<across;++j){
        uint32_t a=uint32_t(i*(across+1)+j),b=a+across+1;
        for(uint32_t k:{a,a+1,b,b,a+1,b+1})m.indices.push_back(k);
    }
    auto& s=m.subsets.emplace_back();s.materialID=mat;s.indexCount=uint32_t(m.indices.size());
}
inline float EffectRate(const Definition& d,float base) {
    return d.intensity<=0?0:base*d.width/3.5f*d.intensity;
}
// Independent landing effect. A zero setting must generate zero landing particles,
// without affecting the sheet, crest, collision paths or airborne spray.
inline float ImpactSprayRate(const Definition& d,float base) {
    return EffectRate(d,base)*d.impactSpray;
}
inline bool EndpointsMatch(const Definition& d,const Geometry& g){
    const auto& a=g.supports.front().center;const auto& b=g.supports.back().center;
    auto approx=[](float x,float y){return std::abs(x-y)<.0001f;};
    return approx(a.x,d.crest.x)&&approx(a.y,d.crest.y)&&approx(a.z,d.crest.z)&&
           approx(b.x,d.landing.x)&&approx(b.y,d.landing.y)&&approx(b.z,d.landing.z);
}
} // namespace renegade::bridge::placeable_waterfall
