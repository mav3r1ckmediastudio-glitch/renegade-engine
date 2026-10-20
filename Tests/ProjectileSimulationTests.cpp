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

    // Opt-in thrown grenades must bounce without damage, then explode once
    // on their fuse even if resting against a solid receiver.
    simulation.Reset();
    launch.definition = {};
    launch.definition.lifetimeSeconds = 6.f;
    launch.definition.grenade.enabled = true;
    launch.definition.grenade.fuseSeconds = .55f;
    launch.definition.grenade.blastRadius = 3.5f;
    launch.definition.grenade.blastDamage = 60.f;
    launch.position={0,.6f,0};
    launch.velocity={0,-8.f,0};
    std::vector<ProjectileBounce> bounces;
    std::vector<ProjectileExplosion> explosions;
    auto floor = [](const ProjectileRecord&,const ProjectileVector& from,
                    const ProjectileVector& to) {
        ProjectileQueryResult result;
        if(from.y>=0.f && to.y<=0.f) {
            result.status=ProjectileQueryStatus::Hit;
            result.contact.fraction=from.y/(from.y-to.y);
            result.contact.position={to.x,0,to.z};
            result.contact.normal={0,1,0};
            result.contact.surfaceType=ImpactSurfaceType::Concrete;
            result.contact.surfaceId="concrete";
        }
        return result;
    };
    if(!simulation.Launch(launch,id,error))return fail("grenade launch");
    if(!simulation.Update(.20f,floor,impacts,bounces,explosions,error) ||
       !impacts.empty() || bounces.empty() || !explosions.empty() ||
       simulation.Records().empty() ||
       simulation.Records()[0].launch.velocity.y<=0)
        return fail("grenade elastic bounce without damage");
    if(!simulation.Update(0,floor,impacts,bounces,explosions,error) ||
       !explosions.empty())return fail("grenade paused fuse");
    if(!simulation.Update(.35f,floor,impacts,bounces,explosions,error) ||
       !impacts.empty() || explosions.size()!=1 || !simulation.Records().empty() ||
       explosions[0].projectileId!=id || explosions[0].radius!=3.5f ||
       explosions[0].damage!=60.f)
        return fail("grenade exactly-once fuse explosion");
    if(!simulation.Update(.25f,floor,impacts,bounces,explosions,error) ||
       !explosions.empty())return fail("grenade duplicate explosion");
    // A moving grenade should rebound, make a low-energy transition to rolling,
    // lose horizontal momentum continuously, and remain supported by the floor.
    // The old code applied 55% friction in one impulse on every micro-bounce.
    simulation.Reset();
    launch.definition={};
    launch.definition.lifetimeSeconds=6.f;
    launch.definition.grenade.enabled=true;
    launch.definition.grenade.fuseSeconds=4.f;
    launch.definition.grenade.radius=.078f;
    launch.definition.grenade.restitution=.25f;
    launch.definition.grenade.surfaceFriction=.55f;
    launch.position={0,.36f,0};launch.velocity={4.f,-3.f,0};
    if(!simulation.Launch(launch,id,error))return fail("rolling grenade launch");
    float previousX=0,previousAngle=0;
    bool rolled=false,continuedAfterFirstImpact=false;
    unsigned numberOfContacts=0;
    for(unsigned frame=0;frame<210;++frame) {
        const auto before=simulation.Records().front();
        if(!simulation.Update(1.f/60.f,floor,impacts,bounces,explosions,error)||
           simulation.Records().empty()||!explosions.empty())
            return fail("rolling grenade update/fuse");
        const auto& item=simulation.Records().front();
        numberOfContacts+=static_cast<unsigned>(bounces.size());
        if(frame>20 && item.launch.position.x<previousX-.001f)
            return fail("grenade reverses horizontal travel without a wall");
        if(item.grenadeRolling) {
            rolled=true;
            if(item.launch.velocity.x>1.f)continuedAfterFirstImpact=true;
            if(std::abs(item.launch.position.y-(item.launch.definition.grenade.radius+.002f))>.02f)
                return fail("grenade rolls below/above floor");
        }
        if(item.grenadeAngle+1e-6f<previousAngle)
            return fail("grenade spin angle reverses discontinuously");
        previousX=item.launch.position.x;
        previousAngle=item.grenadeAngle;
    }
    if(!rolled || !continuedAfterFirstImpact || numberOfContacts>8 ||
       simulation.Records().front().launch.velocity.x>.6f ||
       simulation.Records().front().launch.position.x<.5f ||
       simulation.Records().front().grenadeAngle<1.f) {
        const auto& last=simulation.Records().front();
        std::cerr<<"GRENADE_ROLL_DEBUG rolled="<<rolled<<" movingAfterFirst="<<continuedAfterFirstImpact
            <<" contacts="<<numberOfContacts<<" x="<<last.launch.position.x
            <<" y="<<last.launch.position.y<<" vx="<<last.launch.velocity.x
            <<" spin="<<last.grenadeAngle<<" grounded="<<last.grenadeRolling
            <<" settled="<<last.grenadeSettled<<"\n";
        return fail("grenade unnatural bounce/rolling regression");
    }
    simulation.Reset();
    // A grenade can stop translating before its spin has finished. Prove
    // angular velocity decays over several frames rather than zeroing at
    // the first settled frame (the visible P27 snap-to-stop).
    launch.position={0,.09f,0};launch.velocity={.20f,-.38f,0};
    if(!simulation.Launch(launch,id,error))return fail("grenade spin-down launch");
    bool sawSettled=false;
    float angularAtSettle=0,angleAtSettle=0;
    for(unsigned frame=0;frame<85;++frame) {
        if(!simulation.Update(1.f/60.f,floor,impacts,bounces,explosions,error)||
           simulation.Records().empty())return fail("grenade spin-down update");
        const auto& item=simulation.Records().front();
        if(item.grenadeSettled && !sawSettled){
            sawSettled=true;
            angularAtSettle=item.grenadeSpinRate;
            angleAtSettle=item.grenadeAngle;
        }
    }
    if(!sawSettled || angularAtSettle<.1f ||
       simulation.Records().front().grenadeAngle<=angleAtSettle+.1f ||
       simulation.Records().front().grenadeSpinRate>=angularAtSettle*.15f)
        return fail("grenade should keep rotating then gradually settle");
    simulation.Reset();
    // Restore the earlier grenade fuse fail-closed test.
    launch.definition.grenade.fuseSeconds=7.f;
    if(simulation.Launch(launch,id,error))return fail("invalid grenade fuse accepted");
    launch.definition.grenade.fuseSeconds=.55f;
    if(!simulation.Launch(launch,id,error) ||
       !simulation.Update(.2f,blocked,impacts,bounces,explosions,error) ||
       !simulation.Records().empty() || !explosions.empty())
       return fail("blocked grenade query didn't fail closed");
    simulation.Reset();
    launch.definition.grenade.enabled=false;
    launch.definition.damage=10.f;

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
