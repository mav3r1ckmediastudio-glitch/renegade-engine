#pragma once
#include "RuntimeProjectileWorld.h"
#include <unordered_set>
#include "renegade/bridge/ProjectileAssetService.h"

namespace renegade::runtime
{
    // Transient world state only. No projectile/impact geometry is serialized.
    struct RuntimeProjectileSession
    {
        bridge::ProjectileSimulation simulation;
        struct Trace { bridge::ProjectileVector from, to; float seconds; bool meshless = true; };
        struct Marker { bridge::ProjectileContact contact; float seconds; };
        std::vector<Trace> traces;
        std::unordered_set<std::uint64_t> meshProjectiles;
        std::vector<Marker> markers;
        std::uint64_t launched = 0, impacted = 0, generation = 0;
        bridge::ProjectileContact lastContact;
        XMFLOAT3 lastLaunchPosition = {};
        std::string lastLaunchSocket;
        std::string lastError;

        void Reset()
        {
            simulation.Reset(); traces.clear(); markers.clear(); meshProjectiles.clear();
            launched = impacted = 0; lastContact = {}; lastError.clear();
            lastLaunchPosition={};lastLaunchSocket.clear();
            ++generation;
        }

        // Callers resolve legacy camera launch or authored post-animation muzzle
        // pose and cover-aware aim before entering the shared simulation.
        bool Launch(const bridge::ProjectileAssetDocument& asset,
                    const bridge::ProjectileSource& source,
                    const XMFLOAT3& eye, const XMFLOAT3& forward,
                    std::uint64_t& id)
        {
            if (!bridge::ValidateProjectileAsset(asset, lastError)) return false;
            const float length = std::sqrt(forward.x*forward.x + forward.y*forward.y + forward.z*forward.z);
            if (!std::isfinite(length) || length < 0.000001f) {
                lastError = "Projectile launch has no valid aim direction."; return false;
            }
            bridge::ProjectileLaunch launch;
            launch.definition = bridge::ProjectileSimulationDefinition(asset);
            launch.source = source;
            launch.position = ProjectileBridgeVector(eye);
            const float speed = asset.speedMetresPerSecond / length;
            launch.velocity = {forward.x*speed, forward.y*speed, forward.z*speed};
            if (!simulation.Launch(launch, id, lastError)) return false;
            if (!asset.meshAssetId.empty()) meshProjectiles.insert(id);
            ++launched;lastLaunchPosition=eye;
            return true;
        }

        bool Update(float dt, const bridge::ProjectileSegmentQuery& worldQuery,
                    std::vector<bridge::ProjectileImpact>& allImpacts)
        {
            allImpacts.clear();
            if (!std::isfinite(dt) || dt < 0 || dt > 10 || (dt > 0 && !worldQuery)) {
                lastError = "Projectile frame time/query is invalid."; return false;
            }
            // Pause freezes flight, traces and confirmed contact feedback.
            if (dt == 0) return true;
            for (auto& trace : traces) trace.seconds -= dt;
            for (auto& marker : markers) marker.seconds -= dt;
            traces.erase(std::remove_if(traces.begin(), traces.end(),
                [](const auto& t){return t.seconds <= 0;}), traces.end());
            markers.erase(std::remove_if(markers.begin(), markers.end(),
                [](const auto& m){return m.seconds <= 0;}), markers.end());
            const auto query = [&](const bridge::ProjectileRecord& record,
                                   const bridge::ProjectileVector& from,
                                   const bridge::ProjectileVector& to) {
                auto result = worldQuery(record, from, to);
                if (result.status != bridge::ProjectileQueryStatus::Blocked) {
                    // Bounded basic feedback follows actual swept segments.
                    if (traces.size() >= 2048) traces.erase(traces.begin());
                    traces.push_back({from,
                        result.status == bridge::ProjectileQueryStatus::Hit ? result.contact.position : to,
                        0.10f, meshProjectiles.count(record.id) == 0});
                }
                return result;
            };
            float remaining = dt;
            while (remaining > 0) {
                const float step = std::min(remaining, bridge::ProjectileSimulation::MaxUpdateSeconds);
                std::vector<bridge::ProjectileImpact> impacts;
                if (!simulation.Update(step, query, impacts, lastError)) return false;
                for (const auto& impact : impacts) {
                    ++impacted; lastContact = impact.contact;
                    if (markers.size() >= 64) markers.erase(markers.begin());
                    markers.push_back({impact.contact, 2.0f});
                    allImpacts.push_back(impact);
                }
                remaining -= step;
            }
            for (auto it = meshProjectiles.begin(); it != meshProjectiles.end();) {
                const auto& records = simulation.Records();
                if (std::none_of(records.begin(), records.end(), [&](const auto& r){ return r.id == *it; }))
                    it = meshProjectiles.erase(it);
                else ++it;
            }
            return true;
        }

        void Draw() const
        {
            for (const auto& trace : traces) {
                if (!trace.meshless) continue;
                wi::renderer::RenderableLine line;
                line.start = ProjectileNativeVector(trace.from);
                line.end = ProjectileNativeVector(trace.to);
                line.color_start = {1.0f, 0.75f, 0.2f, 1};
                line.color_end = {1.0f, 0.4f, 0.1f, 1};
                wi::renderer::DrawLine(line, true);
            }
            for (const auto& record : simulation.Records())
                if (meshProjectiles.count(record.id) == 0)
                wi::renderer::DrawSphere(wi::primitive::Sphere(
                    ProjectileNativeVector(record.launch.position), 0.025f),
                    {1, 0.75f, 0.2f, 1}, true);
            // Contact markers remain in diagnostics; native surface VFX/decals present hits.

        }
    };
}
