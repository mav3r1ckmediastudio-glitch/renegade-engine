#pragma once
// Grenade radial damage: world visibility and faction attribution are
// supplied by existing scene/AI systems; no independent NPC damage bypass.
#include "RuntimeProjectileWorld.h"
#include "RuntimeCombatDamage.h"
#include <cmath>

namespace renegade::runtime {
    inline float GrenadeRadialDamage(const bridge::ProjectileExplosion& e,
                                     const XMFLOAT3& target) noexcept {
        if(!bridge::FiniteProjectileVector(e.position) || !std::isfinite(e.radius) ||
           !std::isfinite(e.damage) || e.radius<=0 || e.damage<=0)return 0;
        const auto dx=target.x-e.position.x,dy=target.y-e.position.y,dz=target.z-e.position.z;
        const auto distance=std::sqrt(dx*dx+dy*dy+dz*dz);
        // Impact wave is strongest near the epicentre and goes smoothly to zero.
        const float q=std::clamp(1.f-distance/e.radius,0.f,1.f);
        return e.damage*q*q;
    }
    inline unsigned ApplyGrenadeBlastToCharacters(
        wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characters,
        RuntimeCharacterPerceptionState& perception,
        RuntimeCombatState& combat,
        const ProjectileOwnerBinding& owner,
        const bridge::ProjectileExplosion& e,
        const CombatEventEmitter& emit) {
        if(e.radius<=0||e.damage<=0 ||
           e.source.ownerSubjectId!=owner.subjectId ||
           !bridge::FiniteProjectileVector(e.position))return 0;
        unsigned hitCount=0;
        for(const auto& target:characters.characters) {
            if(target.stableEntityId==e.source.ownerSubjectId)continue;
            const auto* transform=scene.transforms.GetComponent(target.entity);
            if(!transform)continue;
            XMFLOAT3 torso=transform->GetPosition();
            torso.y+=.85f;
            const float damage=GrenadeRadialDamage(e,torso);
            if(damage<.01f)continue;
            // World occlusion blocks blasts. Target's own mesh is a valid endpoint.
            // Use a synthetic query record with legitimate source ownership.
            bridge::ProjectileRecord ray;
            ray.launch.source=e.source;
            ray.launch.position=e.position;
            auto visibility=QueryProjectileSceneSegment(scene,characters,owner,ray,
                e.position,ProjectileBridgeVector(torso));
            if(visibility.status==bridge::ProjectileQueryStatus::Blocked)continue;
            if(visibility.status==bridge::ProjectileQueryStatus::Hit &&
               visibility.contact.targetSubjectId!=target.stableEntityId)continue;
            std::string error;
            if(ApplyAttributedCombatDamage(scene,characters,perception,combat,
                  target.stableEntityId,e.source.ownerSubjectId,e.source.factionId,
                  ProjectileNativeVector(e.source.knownPosition),
                  ProjectileNativeVector(e.source.knownVelocity),damage,emit,error))
                ++hitCount;
        }
        return hitCount;
    }
}
