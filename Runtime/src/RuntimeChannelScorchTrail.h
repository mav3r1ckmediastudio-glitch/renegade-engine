#pragma once
#include "RuntimeProjectileWorld.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace renegade::runtime {
// A continuous contact traces one painted surface. Decals are emitted by the
// shared impact-mark owner, never by the held visual root (so they survive release).
// Sampling is distance-based rather than frame-based: holding still burns once,
// moving contact stamps overlapping soot at bounded intervals.
struct RuntimeChannelScorchTrail {
    static constexpr float StampSpacing=.105f;
    static constexpr unsigned MaxMarksPerFrame=7;
    bool touching=false;
    wi::ecs::Entity receiver=wi::ecs::INVALID_ENTITY;
    bridge::StableId surface;
    XMFLOAT3 lastStamp{},lastNormal{};
    void Reset(){*this={};}
    static bool Markable(const bridge::ProjectileQueryResult& result,wi::ecs::Entity entity) {
        if(result.status!=bridge::ProjectileQueryStatus::Hit || entity==wi::ecs::INVALID_ENTITY)return false;
        const auto type=result.contact.surfaceType;
        if(type==bridge::ImpactSurfaceType::Water||type==bridge::ImpactSurfaceType::Character)return false;
        const auto& normal=result.contact.normal;
        const auto& point=result.contact.position;
        const auto length=normal.x*normal.x+normal.y*normal.y+normal.z*normal.z;
        return bridge::FiniteProjectileVector(normal)&&bridge::FiniteProjectileVector(point)&&
            std::isfinite(length)&&length>.01f;
    }
    template<class Stamp> unsigned Update(const bridge::ProjectileQueryResult& result,
                                          wi::ecs::Entity hitEntity,const Stamp& stamp) {
        if(!Markable(result,hitEntity)){Reset();return 0;}
        const auto point=ProjectileNativeVector(result.contact.position);
        const auto normal=ProjectileNativeVector(result.contact.normal);
        const bool newSurface=!touching||receiver!=hitEntity||surface!=result.contact.surfaceId;
        const auto diff=XMLoadFloat3(&point)-XMLoadFloat3(&lastStamp);
        const float distance=XMVectorGetX(XMVector3Length(diff));
        const auto n=XMVector3Normalize(XMLoadFloat3(&normal));
        const auto prior=XMVector3Normalize(XMLoadFloat3(&lastNormal));
        // Don't draw across air or round a sharp edge as if it were one wall.
        const bool continuous=!newSurface&&std::isfinite(distance)&&distance<.96f&&
            XMVectorGetX(XMVector3Dot(n,prior))>.95f;
        unsigned count=0;
        if(!continuous){
            stamp(point,normal,hitEntity);count=1;
            lastStamp=point;
        } else if(distance>=StampSpacing){
            const auto origin=XMLoadFloat3(&lastStamp);
            const unsigned steps=std::min(MaxMarksPerFrame,
                static_cast<unsigned>(std::floor(distance/StampSpacing)));
            for(unsigned i=1;i<=steps;++i){
                XMFLOAT3 p;XMStoreFloat3(&p,origin+diff*(StampSpacing*float(i)/distance));
                stamp(p,normal,hitEntity);lastStamp=p;++count;
            }
            // Fast camera sweeps shouldn't leave long straight trails across
            // surfaces we have not individually ray-tested.
        }
        touching=true;receiver=hitEntity;surface=result.contact.surfaceId;lastNormal=normal;
        return count;
    }
};
}
