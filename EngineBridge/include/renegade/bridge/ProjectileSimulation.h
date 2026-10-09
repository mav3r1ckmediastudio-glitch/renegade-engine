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

    struct ProjectileDefinition
    {
        ProjectileVector acceleration{0, -9.81f, 0};
        float lifetimeSeconds = 5;
        float damage = 10;
        std::string impactProfileId;
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
    };

    struct ProjectileRecord
    {
        std::uint64_t id = 0;
        ProjectileLaunch launch;
        float ageSeconds = 0;
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
                d.damage < 0 || d.damage > 100000)
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
            impacts.clear();
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
                                record.launch.definition.impactProfileId});
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
