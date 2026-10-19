#pragma once

#include "renegade/bridge/ImpactSurface.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct ProjectileVector
    {
        float x = 0, y = 0, z = 0;
    };

    inline bool FiniteProjectileVector(const ProjectileVector& v) noexcept
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    struct ProjectileSource
    {
        std::string ownerSubjectId;
        std::string sourceAssetId;
        std::string factionId;
        // Legitimate source knowledge captured at launch, never a hidden lookup.
        ProjectileVector knownPosition;
        ProjectileVector knownVelocity;
    };

    struct BurningPayload {
        float damagePerSecond=0, durationSeconds=0;
    };
    inline bool ValidBurningPayload(const BurningPayload& b) noexcept {
        return std::isfinite(b.damagePerSecond) && std::isfinite(b.durationSeconds) &&
            b.damagePerSecond>=0 && b.damagePerSecond<=1000 && b.durationSeconds>=0 && b.durationSeconds<=60 &&
            ((b.damagePerSecond==0)==(b.durationSeconds==0));
    }
    struct GrenadePhysicsPolicy
    {
        bool enabled = false;
        float radius = 0.11f;
        float fuseSeconds = 3.0f;
        float restitution = 0.42f;
        float surfaceFriction = 0.55f;
        float settleSpeed = 0.45f;
        float blastRadius = 3.5f;
        float blastDamage = 65.0f;
    };
    inline bool ValidGrenadePhysics(const GrenadePhysicsPolicy& p) noexcept {
        return std::isfinite(p.radius) && p.radius >= .02f && p.radius <= .5f &&
            std::isfinite(p.fuseSeconds) && p.fuseSeconds >= .1f && p.fuseSeconds <= 30.f &&
            std::isfinite(p.restitution) && p.restitution >= 0 && p.restitution <= .95f &&
            std::isfinite(p.surfaceFriction) && p.surfaceFriction >= 0 && p.surfaceFriction <= 1 &&
            std::isfinite(p.settleSpeed) && p.settleSpeed >= .05f && p.settleSpeed <= 5.f &&
            std::isfinite(p.blastRadius) && p.blastRadius >= .1f && p.blastRadius <= 30.f &&
            std::isfinite(p.blastDamage) && p.blastDamage >= 0 && p.blastDamage <= 100000.f;
    }
    struct ProjectileDefinition
    {
        ProjectileVector acceleration{0, -9.81f, 0};
        float lifetimeSeconds = 5;
        float damage = 10;
        std::string impactProfileId;
        BurningPayload burning;
        GrenadePhysicsPolicy grenade;
        // First slice is a point projectile swept along each travel segment.
        // Radius, penetration, ricochet and embedding require later policies.
    };

    struct ProjectileLaunch
    {
        ProjectileDefinition definition;
        ProjectileSource source;
        ProjectileVector position;
        ProjectileVector velocity;
    };

    enum class ProjectileQueryStatus { Miss, Hit, Blocked };

    struct ProjectileContact
    {
        float fraction = 0;
        ProjectileVector position;
        ProjectileVector normal;
        std::string targetSubjectId; // Empty for ungoverned static world geometry.
        std::string surfaceId;       // Stable material identity when available.
        ImpactSurfaceType surfaceType = ImpactSurfaceType::Default;
    };

    struct ProjectileQueryResult
    {
        ProjectileQueryStatus status = ProjectileQueryStatus::Miss;
        ProjectileContact contact;
    };

    struct ProjectileImpact
    {
        std::uint64_t projectileId = 0;
        ProjectileSource source;
        ProjectileContact contact;
        ProjectileVector incomingVelocity;
        float damage = 0;
        std::string impactProfileId;
        BurningPayload burning;
    };

    struct ProjectileBounce {
        std::uint64_t projectileId=0;
        ProjectileSource source;
        ProjectileContact contact;
        ProjectileVector incomingVelocity;
    };
    struct ProjectileExplosion {
        std::uint64_t projectileId=0;
        ProjectileSource source;
        ProjectileVector position;
        float radius=0, damage=0;
    };
    struct ProjectileRecord
    {
        std::uint64_t id = 0;
        ProjectileLaunch launch;
        float ageSeconds = 0;
        bool grenadeSettled = false;
        bool grenadeRolling = false;
        float grenadeAngle = 0.f;
        float grenadeSpinRate = 8.f;
        unsigned grenadeBounces = 0;
    };

    // Query adapter owns world filters and exclusion of the source hierarchy.
    // Blocked is fail-closed (query failure/ambiguous owner exclusion).
    using ProjectileSegmentQuery = std::function<ProjectileQueryResult(
        const ProjectileRecord&, const ProjectileVector&, const ProjectileVector&)>;

    class ProjectileSimulation final
    {
    public:
        static constexpr std::size_t MaxProjectiles = 1024;
        static constexpr float MaxStepSeconds = 1.0f / 120.0f;
        static constexpr float MaxUpdateSeconds = 1.0f;

        bool Launch(const ProjectileLaunch& launch, std::uint64_t& id, std::string& error)
        {
            id = 0;
            const auto& d = launch.definition;
            const auto& s = launch.source;
            if (s.ownerSubjectId.empty() || s.ownerSubjectId.size() > 128 ||
                s.sourceAssetId.empty() || s.sourceAssetId.size() > 128 ||
                s.factionId.empty() || s.factionId.size() > 128 ||
                d.impactProfileId.size() > 128 ||
                !FiniteProjectileVector(s.knownPosition) ||
                !FiniteProjectileVector(s.knownVelocity) ||
                !FiniteProjectileVector(launch.position) ||
                !FiniteProjectileVector(launch.velocity) ||
                !FiniteProjectileVector(d.acceleration) ||
                !std::isfinite(d.lifetimeSeconds) || d.lifetimeSeconds <= 0 ||
                d.lifetimeSeconds > 120 || !std::isfinite(d.damage) ||
                d.damage < 0 || d.damage > 100000 || !ValidBurningPayload(d.burning) ||
                (d.grenade.enabled && (!ValidGrenadePhysics(d.grenade) ||
                    d.grenade.fuseSeconds > d.lifetimeSeconds)))
            {
                error = "Projectile launch contains invalid identity or simulation values.";
                return false;
            }
            if (records_.size() >= MaxProjectiles || nextId_ == UINT64_MAX)
            {
                error = "Projectile simulation capacity or identity range exhausted.";
                return false;
            }
            id = nextId_++;
            records_.push_back({id, launch, 0});
            error.clear();
            return true;
        }

        // Caller uses gameplay time: zero pauses, Reset clears scene/session state.
        // >1s is rejected atomically; caller must subdivide long elapsed intervals.
        bool Update(float dt, const ProjectileSegmentQuery& query,
                    std::vector<ProjectileImpact>& impacts, std::string& error)
        {
            std::vector<ProjectileBounce> ignoredBounces;
            std::vector<ProjectileExplosion> ignoredExplosions;
            return Update(dt,query,impacts,ignoredBounces,ignoredExplosions,error);
        }
        bool Update(float dt, const ProjectileSegmentQuery& query,
                    std::vector<ProjectileImpact>& impacts,
                    std::vector<ProjectileBounce>& bounces,
                    std::vector<ProjectileExplosion>& explosions,
                    std::string& error)
        {
            impacts.clear();bounces.clear();explosions.clear();
            if (!std::isfinite(dt) || dt < 0 || dt > MaxUpdateSeconds ||
                (dt > 0 && !query))
            {
                error = "Projectile update requires bounded gameplay time and a world query.";
                return false;
            }
            if (dt == 0)
            {
                error.clear();
                return true;
            }

            for (auto& record : records_)
            {
                float remaining = std::min(dt,
                    record.launch.definition.lifetimeSeconds - record.ageSeconds);
                bool retired = false;
                while (remaining > 0 && !retired)
                {
                    const float step = std::min(remaining, MaxStepSeconds);
                    if(record.launch.definition.grenade.enabled) {
                        const auto& policy=record.launch.definition.grenade;
                        const float untilFuse=policy.fuseSeconds-record.ageSeconds;
                        if(untilFuse<=.000001f) {
                            explosions.push_back({record.id,record.launch.source,
                                record.launch.position,policy.blastRadius,policy.blastDamage});
                            retired=true;break;
                        }
                        const float duration=std::min(step,untilFuse);
                        if(record.grenadeSettled) {
                            // The shell may have stopped translating, but its
                            // remaining angular momentum should dissipate
                            // smoothly rather than snapping to a frozen pose.
                            record.grenadeSpinRate*=std::exp(-3.7f*duration);
                            record.grenadeAngle+=record.grenadeSpinRate*duration;
                            if(record.grenadeSpinRate<.06f)record.grenadeSpinRate=0;
                            record.ageSeconds+=duration;remaining-=duration;
                        } else if(record.grenadeRolling) {
                            // A low-energy landing becomes rolling, not an endless
                            // chain of tiny vertical bounces. Probe support at the
                            // NEW horizontal position so ledges still work.
                            auto& v=record.launch.velocity;
                            const float drag=std::exp(-3.2f*policy.surfaceFriction*duration);
                            v.x*=drag;v.z*=drag;v.y=0;
                            const auto from=record.launch.position;
                            const ProjectileVector next{
                                from.x+v.x*duration,from.y,from.z+v.z*duration};
                            const auto obstacle=query(record,from,next);
                            if(obstacle.status==ProjectileQueryStatus::Blocked) {
                                retired=true;break;
                            }
                            if(obstacle.status==ProjectileQueryStatus::Hit) {
                                const auto& c=obstacle.contact;
                                const float n2=c.normal.x*c.normal.x+c.normal.y*c.normal.y+
                                    c.normal.z*c.normal.z;
                                if(!std::isfinite(c.fraction)||c.fraction<0||
                                   c.fraction>1||!FiniteProjectileVector(c.position)||
                                   !FiniteProjectileVector(c.normal)||n2<.001f) {
                                    retired=true;break;
                                }
                                const float inv=1.f/std::sqrt(n2);
                                const ProjectileVector n{
                                    c.normal.x*inv,c.normal.y*inv,c.normal.z*inv};
                                record.launch.position={
                                    c.position.x+n.x*(policy.radius+.002f),
                                    c.position.y+n.y*(policy.radius+.002f),
                                    c.position.z+n.z*(policy.radius+.002f)};
                                const float along=v.x*n.x+v.y*n.y+v.z*n.z;
                                // Walls rebound sideways with strong energy loss;
                                // a floor or gentle incline does not kick the
                                // grenade upward again.
                                v={v.x-(1.25f*std::min(0.f,along))*n.x,
                                   v.y-(1.25f*std::min(0.f,along))*n.y,
                                   v.z-(1.25f*std::min(0.f,along))*n.z};
                                v.x*=.52f;v.z*=.52f;
                                record.grenadeRolling=n.y>.65f;
                            } else {
                                const auto support=query(record,
                                    {next.x,next.y+.025f,next.z},
                                    {next.x,next.y-policy.radius-.08f,next.z});
                                if(support.status==ProjectileQueryStatus::Blocked) {
                                    retired=true;break;
                                }
                                if(support.status==ProjectileQueryStatus::Hit &&
                                    FiniteProjectileVector(support.contact.normal) &&
                                    support.contact.normal.y>.6f) {
                                    const auto& c=support.contact;
                                    record.launch.position={c.position.x,
                                        c.position.y+policy.radius+.002f,c.position.z};
                                } else {
                                    record.launch.position=next;
                                    record.grenadeRolling=false;
                                }
                            }
                            const float speed=std::sqrt(v.x*v.x+v.z*v.z);
                            record.grenadeSpinRate=std::min(17.f,speed/
                                std::max(policy.radius,.03f));
                            record.grenadeAngle+=record.grenadeSpinRate*duration;
                            if(record.grenadeRolling && speed<policy.settleSpeed) {
                                record.grenadeSettled=true;
                                record.launch.velocity={};
                                // Keep angular motion alive for resting spin-down.
                            }
                            record.ageSeconds+=duration;remaining-=duration;
                        } else {
                            const auto from=record.launch.position;
                            const auto velocity=record.launch.velocity;
                            const auto gravity=record.launch.definition.acceleration;
                            const ProjectileVector to{
                                from.x+velocity.x*duration+.5f*gravity.x*duration*duration,
                                from.y+velocity.y*duration+.5f*gravity.y*duration*duration,
                                from.z+velocity.z*duration+.5f*gravity.z*duration*duration};
                            const ProjectileVector endVelocity{
                                velocity.x+gravity.x*duration,velocity.y+gravity.y*duration,
                                velocity.z+gravity.z*duration};
                            if(!FiniteProjectileVector(to)||!FiniteProjectileVector(endVelocity)){
                                retired=true;break;
                            }
                            const auto hit=query(record,from,to);
                            if(hit.status==ProjectileQueryStatus::Blocked){retired=true;break;}
                            if(hit.status==ProjectileQueryStatus::Hit){
                                const auto& c=hit.contact;
                                const float normSq=c.normal.x*c.normal.x+c.normal.y*c.normal.y+
                                    c.normal.z*c.normal.z;
                                if(!std::isfinite(c.fraction)||c.fraction<0||c.fraction>1||
                                   !FiniteProjectileVector(c.position)||
                                   !FiniteProjectileVector(c.normal)||normSq<.001f||
                                   c.targetSubjectId==record.launch.source.ownerSubjectId){
                                    retired=true;break;
                                }
                                const float invNorm=1.f/std::sqrt(normSq);
                                const ProjectileVector n{
                                    c.normal.x*invNorm,c.normal.y*invNorm,c.normal.z*invNorm};
                                // Contact is for the projectile centre until world query
                                // supports full sphere sweeps; offset by the radius.
                                record.launch.position={
                                    c.position.x+n.x*(policy.radius+.002f),
                                    c.position.y+n.y*(policy.radius+.002f),
                                    c.position.z+n.z*(policy.radius+.002f)};
                                const float t=duration*c.fraction;
                                const ProjectileVector atHit{velocity.x+gravity.x*t,
                                    velocity.y+gravity.y*t,velocity.z+gravity.z*t};
                                bounces.push_back({record.id,record.launch.source,c,atHit});
                                const float dot=atHit.x*n.x+atHit.y*n.y+atHit.z*n.z;
                                const float normalSpeed=std::max(0.f,-dot);
                                const float tangentSpeed=std::sqrt(
                                    (atHit.x-dot*n.x)*(atHit.x-dot*n.x)+
                                    (atHit.y-dot*n.y)*(atHit.y-dot*n.y)+
                                    (atHit.z-dot*n.z)*(atHit.z-dot*n.z));
                                // A rigid grenade doesn't behave like a rubber
                                // ball: damp restitution on slower impacts and
                                // apply impulse friction proportional to contact
                                // severity, not a flat 55% loss on EVERY touch.
                                const float severity=std::clamp(normalSpeed/6.f,0.f,1.f);
                                const float restitution=policy.restitution*(.50f+.50f*severity);
                                const float friction=policy.surfaceFriction*(.18f+.42f*severity);
                                const float tangential=std::max(0.f,1.f-friction);
                                ++record.grenadeBounces;
                                record.grenadeAngle+=record.grenadeSpinRate*duration;
                                record.grenadeSpinRate*=.72f;
                                // Raycasts track the projectile centre, so the
                                // correction by radius introduces at least
                                // sqrt(2*g*radius) apparent impact speed even
                                // when the shell should already be rolling.
                                // Account for this numerical artefact; genuine
                                // fast contacts must still rebound.
                                const float correctionSpeed=std::sqrt(
                                    2.f*std::abs(gravity.y)*policy.radius)+.35f;
                                if(n.y>.65f && normalSpeed<std::max(.95f,correctionSpeed)) {
                                    record.grenadeRolling=true;
                                    record.launch.velocity={
                                        (atHit.x-dot*n.x)*tangential,0.f,
                                        (atHit.z-dot*n.z)*tangential};
                                    const float rollSpeed=std::sqrt(
                                        record.launch.velocity.x*record.launch.velocity.x+
                                        record.launch.velocity.z*record.launch.velocity.z);
                                    record.grenadeSpinRate=std::min(17.f,rollSpeed/
                                        std::max(policy.radius,.03f));
                                    if(rollSpeed<policy.settleSpeed) {
                                        record.grenadeSettled=true;
                                        record.launch.velocity={};
                                    }
                                } else {
                                    record.launch.velocity={
                                        (atHit.x-dot*n.x)*tangential+normalSpeed*restitution*n.x,
                                        (atHit.y-dot*n.y)*tangential+normalSpeed*restitution*n.y,
                                        (atHit.z-dot*n.z)*tangential+normalSpeed*restitution*n.z};
                                    if(record.grenadeBounces>=24 ||
                                       (normalSpeed<.15f && tangentSpeed<policy.settleSpeed)) {
                                        record.grenadeSettled=true;
                                        record.launch.velocity={};
                                    }
                                }
                            } else {
                                record.launch.position=to;record.launch.velocity=endVelocity;
                                record.grenadeAngle+=record.grenadeSpinRate*duration;
                            }
                            record.ageSeconds+=duration;remaining-=duration;
                        }
                        if(record.ageSeconds>=policy.fuseSeconds-.000001f){
                            explosions.push_back({record.id,record.launch.source,
                                record.launch.position,policy.blastRadius,policy.blastDamage});
                            retired=true;
                        }
                        continue;
                    }
                    const auto from = record.launch.position;
                    const auto velocity = record.launch.velocity;
                    const auto gravity = record.launch.definition.acceleration;
                    const ProjectileVector to{
                        from.x + velocity.x * step + 0.5f * gravity.x * step * step,
                        from.y + velocity.y * step + 0.5f * gravity.y * step * step,
                        from.z + velocity.z * step + 0.5f * gravity.z * step * step};
                    const ProjectileVector endVelocity{
                        velocity.x + gravity.x * step,
                        velocity.y + gravity.y * step,
                        velocity.z + gravity.z * step};
                    if (!FiniteProjectileVector(to) || !FiniteProjectileVector(endVelocity))
                    {
                        retired = true;
                        break;
                    }

                    const auto result = query(record, from, to);
                    if (result.status == ProjectileQueryStatus::Hit)
                    {
                        const auto& hit = result.contact;
                        const bool valid = std::isfinite(hit.fraction) &&
                            hit.fraction >= 0 && hit.fraction <= 1 &&
                            FiniteProjectileVector(hit.position) &&
                            FiniteProjectileVector(hit.normal) &&
                            hit.targetSubjectId != record.launch.source.ownerSubjectId;
                        if (valid)
                        {
                            const float contactTime = step * hit.fraction;
                            impacts.push_back({record.id, record.launch.source, hit,
                                {velocity.x + gravity.x * contactTime,
                                 velocity.y + gravity.y * contactTime,
                                 velocity.z + gravity.z * contactTime},
                                record.launch.definition.damage,
                                record.launch.definition.impactProfileId, record.launch.definition.burning});
                        }
                        // Malformed/self contacts fail closed without damage or effects.
                        retired = true;
                    }
                    else if (result.status != ProjectileQueryStatus::Miss)
                    {
                        retired = true;
                    }
                    else
                    {
                        record.launch.position = to;
                        record.launch.velocity = endVelocity;
                        record.ageSeconds += step;
                        remaining -= step;
                    }
                }
                if (retired)
                    record.ageSeconds = record.launch.definition.lifetimeSeconds;
            }
            records_.erase(std::remove_if(records_.begin(), records_.end(),
                [](const ProjectileRecord& record) {
                    return record.ageSeconds >= record.launch.definition.lifetimeSeconds;
                }), records_.end());
            error.clear();
            return true;
        }

        void Reset() noexcept { records_.clear(); }
        const std::vector<ProjectileRecord>& Records() const noexcept { return records_; }

    private:
        std::vector<ProjectileRecord> records_;
        std::uint64_t nextId_ = 1; // Reset must not reuse live diagnostic identities.
    };
}
