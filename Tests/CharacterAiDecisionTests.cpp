#include "RuntimeCharacterDecision.h"

#include "renegade/bridge/CharacterProfileService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/PatrolRouteService.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
    int Fail(const std::string& message)
    {
        std::cerr << "AI04_FAIL: " << message << '\n';
        return 1;
    }

    bool Near(const float lhs, const float rhs, const float epsilon = 0.001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool Near3(const XMFLOAT3& lhs, const XMFLOAT3& rhs)
    {
        return Near(lhs.x, rhs.x) && Near(lhs.y, rhs.y) && Near(lhs.z, rhs.z);
    }
}

int main()
{
    using namespace renegade;
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    // Governed Patrol Route authoring: persistent root + persistent ordered
    // child points survive command Undo/Redo semantics without raw ECS refs.
    wi::scene::Scene scene;
    CreatePatrolRouteCommand createRoute(
        scene,
        XMFLOAT3(1.0f, 0.0f, 2.0f),
        PatrolRouteSettings{PatrolRouteMode::PingPong, 1.5f});
    if (!createRoute.Execute())
        return Fail("CreatePatrolRouteCommand execute");
    const StableId routeId = createRoute.CreatedStableId();
    if (!IsValidStableId(routeId) || createRoute.CreatedEntity() == wi::ecs::INVALID_ENTITY)
        return Fail("route stable identity");

    AddPatrolRoutePointCommand addPoint(
        scene, createRoute.CreatedEntity(), XMFLOAT3(8.0f, 0.0f, 2.0f));
    if (!addPoint.Execute())
        return Fail("AddPatrolRoutePointCommand execute");

    PatrolRoute route;
    std::string error;
    if (!CapturePatrolRoute(scene, createRoute.CreatedEntity(), route, error))
        return Fail("capture patrol route: " + error);
    if (route.stableId != routeId || route.points.size() != 2)
        return Fail("route identity/point count");
    if (route.settings.mode != PatrolRouteMode::PingPong ||
        !Near(route.settings.waitSeconds, 1.5f))
        return Fail("route settings");
    if (!Near3(route.points[0].position, XMFLOAT3(1.0f, 0.0f, 2.0f)) ||
        !Near3(route.points[1].position, XMFLOAT3(8.0f, 0.0f, 2.0f)))
        return Fail("route point world positions");
    if (!IsValidStableId(route.points[0].stableId) ||
        !IsValidStableId(route.points[1].stableId) ||
        route.points[0].stableId == route.points[1].stableId)
        return Fail("route point stable identities");

    addPoint.Undo();
    if (!CapturePatrolRoute(scene, createRoute.CreatedEntity(), route, error) ||
        route.points.size() != 1)
        return Fail("route point Undo");
    if (!addPoint.Execute())
        return Fail("route point Redo execute");
    if (!CapturePatrolRoute(scene, createRoute.CreatedEntity(), route, error) ||
        route.points.size() != 2)
        return Fail("route point Redo capture");

    // Deterministic traversal: PingPong must never step outside its ordered
    // route and Random must not immediately choose the same point.
    CharacterDecisionRecord traversal;
    traversal.characterId = "00000000-0000-4000-8000-000000000004";
    traversal.patrol.route = route;
    traversal.patrolPointIndex = 0;
    AdvancePatrolPoint(traversal);
    if (traversal.patrolPointIndex != 1)
        return Fail("ping-pong first advance");
    AdvancePatrolPoint(traversal);
    if (traversal.patrolPointIndex != 0)
        return Fail("ping-pong reverse advance");
    traversal.patrol.route.settings.mode = PatrolRouteMode::Random;
    traversal.patrolPointIndex = 0;
    AdvancePatrolPoint(traversal);
    if (traversal.patrolPointIndex == 0 || traversal.patrolPointIndex >= 2)
        return Fail("deterministic random advance");

    // Decision layer consumes AI-03 memory only. A remembered hidden position
    // is the only investigate goal exposed to AI-04; no live player transform
    // is present in this decision API.
    RuntimeCharacterRecord character;
    character.stableEntityId = traversal.characterId;
    character.authoring.role = CharacterRole::PatrolGuard;
    character.authoring.autonomous = true;
    character.tuning = ResolveCharacterTuning(character.authoring);

    CharacterCognitionRecord cognition;
    cognition.characterId = character.stableEntityId;
    cognition.awareness = AwarenessState::Suspicious;
    CharacterMemoryRecord memory;
    memory.subjectId = RuntimePlayerKnowledgeId;
    memory.source = KnowledgeSource::Heard;
    memory.lastKnownPosition = XMFLOAT3(12.0f, 0.0f, -4.0f);
    memory.lastKnownVelocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
    memory.confidence = 0.8f;
    memory.threat = 0.4f;
    memory.hasPosition = true;
    memory.hostile = true;
    memory.directSight = false;
    memory.secondsSinceSeen = character.tuning.pursuitSeconds + 1.0f;
    cognition.memories.push_back(memory);

    CharacterDecisionRecord decision;
    decision.characterId = character.stableEntityId;
    decision.patrol.route = route;
    decision.intent = CharacterIntent::Patrol;
    decision.previousIntent = CharacterIntent::Patrol;
    decision.commitmentRemainingSeconds = 0.0f;
    SelectIntent(character, cognition, decision);
    if (decision.intent != CharacterIntent::Search &&
        decision.intent != CharacterIntent::Investigate)
        return Fail("memory-driven investigate/search utility");

    // Force Investigate and prove the goal is exactly last-known memory.
    decision.intent = CharacterIntent::Investigate;
    XMFLOAT3 goal;
    if (!ResolveIntentGoal(character, cognition, decision, goal))
        return Fail("investigate goal resolution");
    if (!Near3(goal, memory.lastKnownPosition))
        return Fail("investigate must use AI-03 last-known position");

    // Direct hostile sight raises Chase above normal role work, while a manual
    // non-autonomous Character deterministically stays Idle.
    cognition.memories.front().directSight = true;
    cognition.memories.front().secondsSinceSeen = 0.0f;
    cognition.awareness = AwarenessState::Combat;
    decision.intent = CharacterIntent::Patrol;
    decision.commitmentRemainingSeconds = 0.0f;
    SelectIntent(character, cognition, decision);
    if (decision.intent != CharacterIntent::Chase)
        return Fail("direct hostile sight should select Chase");

    character.authoring.autonomous = false;
    decision.intent = CharacterIntent::Patrol;
    decision.commitmentRemainingSeconds = 0.0f;
    SelectIntent(character, cognition, decision);
    if (decision.intent != CharacterIntent::Idle)
        return Fail("non-autonomous Character must remain Idle");

    // Search samples deterministic offsets around remembered knowledge and
    // never replace the center with a hidden target transform.
    character.authoring.autonomous = true;
    decision.intent = CharacterIntent::Search;
    decision.searchPointIndex = 0;
    if (!ResolveIntentGoal(character, cognition, decision, goal))
        return Fail("search goal resolution");
    if (!Near(goal.x, memory.lastKnownPosition.x + 3.0f) ||
        !Near(goal.z, memory.lastKnownPosition.z))
        return Fail("search offset around last-known position");

    std::cout << "AI04_PASS patrol-authoring utility-memory hysteresis traversal\n";
    return 0;
}
