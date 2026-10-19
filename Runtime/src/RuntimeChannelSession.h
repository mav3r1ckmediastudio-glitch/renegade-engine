#pragma once
#include "renegade/bridge/ProjectileSimulation.h"
#include <cmath>
namespace renegade::runtime {
// One held effect owns one current contact. It is not a travelling projectile.
// Accrued target time is settled before aim/source changes, never reassigned.
struct RuntimeChannelSession {
    bool active=false;
    float pendingSeconds=0,damagePerSecond=0;
    bridge::ProjectileImpact pending,lastImpact;
    unsigned long long ticks=0,starts=0;
    template<class Apply> void Flush(const Apply& apply) {
        if(pendingSeconds>0 && !pending.contact.targetSubjectId.empty()) {
            pending.damage=pendingSeconds*damagePerSecond;
            if(pending.damage>0){apply(pending);lastImpact=pending;++ticks;}
        }
        pendingSeconds=0;
    }
    template<class Apply> void Stop(const Apply& apply) {
        Flush(apply);active=false;pending={};
    }
    void Reset(){*this={};}
    template<class Apply> bool Update(float dt,const bridge::ProjectileSource& source,
        const bridge::ProjectileQueryResult& result,float dps,const Apply& apply) {
        if(!std::isfinite(dt)||dt<0||dt>10||!std::isfinite(dps)||dps<0)return false;
        if(dt==0)return true;
        if(!active){active=true;++starts;}
        const auto target=result.status==bridge::ProjectileQueryStatus::Hit?
            result.contact.targetSubjectId:std::string{};
        if(target!=pending.contact.targetSubjectId || source.sourceAssetId!=pending.source.sourceAssetId ||
           source.ownerSubjectId!=pending.source.ownerSubjectId || source.factionId!=pending.source.factionId || dps!=damagePerSecond)
            Flush(apply);
        pending.source=source;pending.contact=result.status==bridge::ProjectileQueryStatus::Hit?
            result.contact:bridge::ProjectileContact{};
        damagePerSecond=dps;
        if(!target.empty())pendingSeconds+=dt;
        if(pendingSeconds>=.1f)Flush(apply);
        return true;
    }
};
}
