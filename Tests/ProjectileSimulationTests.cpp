#include "renegade/bridge/ProjectileSimulation.h"
#include <iostream>
#include <limits>

using namespace renegade::bridge;

int main()
{
    ProjectileSimulation simulation;
    ProjectileLaunch launch;
    launch.source = {"player-owner", "equipment-source", "Player", {1,2,3}, {0,0,0}};
    launch.position = {0,0,0};
    launch.velocity = {100,0,0};
    launch.definition.acceleration = {0,-10,0};
    launch.definition.impactProfileId = "default-impact";
    std::string error;
    std::uint64_t id = 0;
    std::vector<ProjectileImpact> impacts;
    auto fail = [](const char* text) { std::cerr << text << '\n'; return 1; };
    const auto miss = [](const ProjectileRecord&, const ProjectileVector&,
                         const ProjectileVector&) { return ProjectileQueryResult{}; };
    if (!simulation.Launch(launch, id, error) || id == 0)
        return fail("valid launch");
    if (!simulation.Update(0, {}, impacts, error) ||
        simulation.Records()[0].ageSeconds != 0)
        return fail("pause");
    if (!simulation.Update(0.5f, miss, impacts, error))
        return fail("ballistic update");
    const auto& record = simulation.Records()[0];
    if (std::abs(record.launch.position.x - 50) > 0.001f ||
        std::abs(record.launch.position.y + 1.25f) > 0.001f ||
        std::abs(record.launch.velocity.y + 5) > 0.001f)
        return fail("analytic gravity trajectory");
    const float age = record.ageSeconds;
    if (simulation.Update(std::numeric_limits<float>::quiet_NaN(), miss, impacts, error) ||
        simulation.Update(-1, miss, impacts, error) ||
        simulation.Update(2, miss, impacts, error) ||
        simulation.Records()[0].ageSeconds != age)
        return fail("invalid time mutates simulation");
    simulation.Reset();
    if (!simulation.Records().empty())
        return fail("reset");
    std::uint64_t nextId;
    if (!simulation.Launch(launch, nextId, error) || nextId <= id)
        return fail("reset reused identity");

    // A thin wall at x=10 must stop a 100m/s projectile even across a 0.5s frame.
    int queries = 0;
    const auto wall = [&queries](const ProjectileRecord&, const ProjectileVector& from,
                                const ProjectileVector& to) {
        ++queries;
        ProjectileQueryResult result;
        if (from.x <= 10 && to.x >= 10)
        {
            result.status = ProjectileQueryStatus::Hit;
            result.contact.fraction = (10 - from.x) / (to.x - from.x);
            result.contact.position = {10,0,0};
            result.contact.normal = {-1,0,0};
            result.contact.targetSubjectId = "character-target";
            result.contact.surfaceId = "stone";
        }
        return result;
    };
    if (!simulation.Update(0.5f, wall, impacts, error) ||
        impacts.size() != 1 || !simulation.Records().empty() ||
        impacts[0].projectileId != nextId ||
        impacts[0].source.ownerSubjectId != "player-owner" ||
        impacts[0].source.sourceAssetId != "equipment-source" ||
        impacts[0].source.knownPosition.x != 1 ||
        impacts[0].contact.surfaceId != "stone" ||
        impacts[0].damage != 10 || impacts[0].impactProfileId != "default-impact" ||
        queries < 2)
        return fail("swept hit and attribution");
    if (!simulation.Update(0.1f, wall, impacts, error) || !impacts.empty())
        return fail("duplicate impact");

    // Lifetime clips travel; there must be no query beyond the expiry point.
    launch.definition.lifetimeSeconds = 0.05f;
    launch.definition.acceleration = {};
    if (!simulation.Launch(launch, id, error))
        return fail("short launch");
    float farthest = 0;
    const auto expiryQuery = [&farthest](const ProjectileRecord&, const ProjectileVector&,
                                      const ProjectileVector& to) {
        farthest = std::max(farthest, to.x);
        return ProjectileQueryResult{};
    };
    if (!simulation.Update(0.5f, expiryQuery, impacts, error) ||
        !simulation.Records().empty() || farthest > 5.001f)
        return fail("lifetime travel clipping");

    const auto invalidContact = [](const ProjectileRecord&, const ProjectileVector&,
                                   const ProjectileVector&) {
        ProjectileQueryResult result;
        result.status = ProjectileQueryStatus::Hit;
        result.contact.fraction = 2;
        return result;
    };
    if (!simulation.Launch(launch, id, error) ||
        !simulation.Update(0.1f, invalidContact, impacts, error) ||
        !impacts.empty() || !simulation.Records().empty())
        return fail("malformed contact fail closed");
    const auto selfContact = [](const ProjectileRecord& record, const ProjectileVector&,
                                const ProjectileVector&) {
        ProjectileQueryResult result;
        result.status = ProjectileQueryStatus::Hit;
        result.contact.targetSubjectId = record.launch.source.ownerSubjectId;
        return result;
    };
    if (!simulation.Launch(launch, id, error) ||
        !simulation.Update(0.1f, selfContact, impacts, error) || !impacts.empty())
        return fail("self contact");
    const auto blocked = [](const ProjectileRecord&, const ProjectileVector&,
                            const ProjectileVector&) {
        ProjectileQueryResult result;
        result.status = ProjectileQueryStatus::Blocked;
        return result;
    };
    if (!simulation.Launch(launch, id, error) ||
        !simulation.Update(0.1f, blocked, impacts, error) ||
        !simulation.Records().empty() || !impacts.empty())
        return fail("query failure");

    launch.source.ownerSubjectId.clear();
    if (simulation.Launch(launch, id, error) || id != 0)
        return fail("missing owner accepted");
    launch.source.ownerSubjectId = "player-owner";
    launch.definition.damage = std::numeric_limits<float>::infinity();
    if (simulation.Launch(launch, id, error))
        return fail("nonfinite damage accepted");
    launch.definition.damage = 10;
    for (std::size_t i = 0; i < ProjectileSimulation::MaxProjectiles; ++i)
        if (!simulation.Launch(launch, id, error))
            return fail("capacity fill");
    if (simulation.Launch(launch, id, error) ||
        simulation.Records().size() != ProjectileSimulation::MaxProjectiles)
        return fail("capacity overflow");
    std::cout << "ProjectileSimulationTests passed\n";
    return 0;
}
