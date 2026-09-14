#pragma once

#include "RuntimeCharacterPerception.h"
#include "RuntimeCharacterSystem.h"
#include "renegade/bridge/NavigationService.h"
#include "renegade/bridge/PatrolRouteService.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace renegade::runtime
{
    enum class CharacterIntent : std::int32_t
    {
        Idle = 0,
        Patrol,
        Guard,
        Investigate,
        Search,
        Observe,
        Warn,
        AlertAllies,
        Chase,
        HoldPosition,
        TakeCover,
        Attack,
        Reload,
        Retreat,
        Flee,
        Surrender,
        Interact,
        Scripted,
        Dead,
    };

    [[nodiscard]] inline const char* ToString(const CharacterIntent intent) noexcept
    {
        switch (intent)
        {
        case CharacterIntent::Patrol: return "Patrol";
        case CharacterIntent::Guard: return "Guard";
        case CharacterIntent::Investigate: return "Investigate";
        case CharacterIntent::Search: return "Search";
        case CharacterIntent::Observe: return "Observe";
        case CharacterIntent::Warn: return "Warn";
        case CharacterIntent::AlertAllies: return "AlertAllies";
        case CharacterIntent::Chase: return "Chase";
        case CharacterIntent::HoldPosition: return "HoldPosition";
        case CharacterIntent::TakeCover: return "TakeCover";
        case CharacterIntent::Attack: return "Attack";
        case CharacterIntent::Reload: return "Reload";
        case CharacterIntent::Retreat: return "Retreat";
        case CharacterIntent::Flee: return "Flee";
        case CharacterIntent::Surrender: return "Surrender";
        case CharacterIntent::Interact: return "Interact";
        case CharacterIntent::Scripted: return "Scripted";
        case CharacterIntent::Dead: return "Dead";
        case CharacterIntent::Idle:
        default: return "Idle";
        }
    }

    struct CharacterIntentScore
    {
        CharacterIntent intent = CharacterIntent::Idle;
        float score = 0.0f;
    };

    struct RuntimePatrolRouteBinding
    {
        bridge::StableId routeId;
        bridge::PatrolRoute route;
    };

    struct CharacterDecisionRecord
    {
        bridge::StableId characterId;
        CharacterIntent intent = CharacterIntent::Idle;
        CharacterIntent previousIntent = CharacterIntent::Idle;
        float intentAgeSeconds = 0.0f;
        float commitmentRemainingSeconds = 0.0f;
        float waitRemainingSeconds = 0.0f;
        float searchRemainingSeconds = 0.0f;
        float repathRemainingSeconds = 0.0f;
        float stuckSeconds = 0.0f;
        float exhaustedSearchMemoryAgeSeconds = -1.0f;
        std::uint64_t lastCognitionTick = 0;
        std::uint64_t transitionCount = 0;
        std::uint64_t pathRequests = 0;
        std::uint64_t stuckRecoveries = 0;
        std::size_t patrolPointIndex = 0;
        int patrolDirection = 1;
        std::uint64_t patrolVisitCount = 0;
        std::size_t searchPointIndex = 0;
        bool hasGoal = false;
        bool arrived = false;
        bool hasLastPosition = false;
        XMFLOAT3 goal = XMFLOAT3(0.0f, 0.0f, 0.0f);
        XMFLOAT3 lastPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
        std::array<CharacterIntentScore, 3> topScores{};
        std::string lastTransitionReason;
        std::string exhaustedSearchSubjectId;
        RuntimePatrolRouteBinding patrol;
    };

    struct RuntimeCharacterDecisionState
    {
        wi::ecs::Entity navigationGrid = wi::ecs::INVALID_ENTITY;
        bridge::StableId navigationGridId;
        std::vector<CharacterDecisionRecord> characters;
        std::uint64_t decisionTicks = 0;
        std::uint64_t pathRequests = 0;
        std::uint64_t stuckRecoveries = 0;
        std::uint64_t rejectedGoals = 0;
    };

    inline constexpr float CharacterDecisionCommitmentSeconds = 0.65f;
    inline constexpr float CharacterDecisionHysteresisBonus = 12.0f;
    inline constexpr float CharacterDecisionEmergencyMargin = 22.0f;
    inline constexpr float CharacterDecisionRepathSeconds = 0.75f;
    inline constexpr float CharacterDecisionArrivalDistance = 0.70f;
    inline constexpr float CharacterDecisionStuckRepathSeconds = 1.25f;
    inline constexpr float CharacterDecisionStuckAbortSeconds = 5.0f;
    inline constexpr float CharacterDecisionMoveAmount = 0.12f;
    inline constexpr float CharacterDecisionFreshMemoryAgeEpsilon = 0.05f;

    [[nodiscard]] inline CharacterDecisionRecord* FindCharacterDecision(
        RuntimeCharacterDecisionState& state,
        const bridge::StableId& id) noexcept
    {
        const auto found = std::find_if(
            state.characters.begin(), state.characters.end(),
            [&id](const CharacterDecisionRecord& record)
            {
                return record.characterId == id;
            });
        return found == state.characters.end() ? nullptr : &*found;
    }

    [[nodiscard]] inline const CharacterDecisionRecord* FindCharacterDecision(
        const RuntimeCharacterDecisionState& state,
        const bridge::StableId& id) noexcept
    {
        const auto found = std::find_if(
            state.characters.begin(), state.characters.end(),
            [&id](const CharacterDecisionRecord& record)
            {
                return record.characterId == id;
            });
        return found == state.characters.end() ? nullptr : &*found;
    }

    [[nodiscard]] inline const CharacterMemoryRecord* BestActionableMemory(
        const CharacterCognitionRecord& cognition) noexcept
    {
        const CharacterMemoryRecord* best = nullptr;
        float bestScore = -1.0f;
        for (const auto& memory : cognition.memories)
        {
            if (!memory.hasPosition || memory.confidence <= 0.0f)
                continue;
            const float score =
                memory.threat * 2.0f + memory.confidence +
                (memory.hostile ? 1.0f : 0.0f) +
                (memory.directSight ? 2.0f : 0.0f);
            if (score > bestScore)
            {
                best = &memory;
                bestScore = score;
            }
        }
        return best;
    }

    [[nodiscard]] inline bool IsSearchMemoryExhausted(
        const CharacterDecisionRecord& decision,
        const CharacterMemoryRecord& memory) noexcept
    {
        if (decision.exhaustedSearchSubjectId.empty() ||
            decision.exhaustedSearchMemoryAgeSeconds < 0.0f)
        {
            return false;
        }
        if (memory.subjectId != decision.exhaustedSearchSubjectId ||
            memory.directSight)
        {
            return false;
        }
        return memory.ageSeconds + CharacterDecisionFreshMemoryAgeEpsilon >=
            decision.exhaustedSearchMemoryAgeSeconds;
    }

    inline void RefreshSearchExhaustion(
        const CharacterCognitionRecord& cognition,
        CharacterDecisionRecord& decision) noexcept
    {
        if (decision.exhaustedSearchSubjectId.empty())
            return;
        const CharacterMemoryRecord* memory = BestActionableMemory(cognition);
        if (memory == nullptr ||
            memory->subjectId != decision.exhaustedSearchSubjectId ||
            memory->directSight ||
            memory->ageSeconds + CharacterDecisionFreshMemoryAgeEpsilon <
                decision.exhaustedSearchMemoryAgeSeconds)
        {
            decision.exhaustedSearchSubjectId.clear();
            decision.exhaustedSearchMemoryAgeSeconds = -1.0f;
        }
    }

    [[nodiscard]] inline bool RoleNormallyPatrols(
        const bridge::CharacterRole role) noexcept
    {
        return role == bridge::CharacterRole::PatrolGuard ||
            role == bridge::CharacterRole::Soldier ||
            role == bridge::CharacterRole::Predator;
    }

    [[nodiscard]] inline CharacterIntent NormalRoleIntent(
        const RuntimeCharacterRecord& character,
        const bool hasUsableRoute) noexcept
    {
        if (!character.authoring.autonomous)
            return CharacterIntent::Idle;
        if (hasUsableRoute && RoleNormallyPatrols(character.authoring.role))
            return CharacterIntent::Patrol;
        switch (character.authoring.role)
        {
        case bridge::CharacterRole::Guard:
        case bridge::CharacterRole::Soldier:
        case bridge::CharacterRole::Companion:
            return CharacterIntent::Guard;
        default:
            return CharacterIntent::Idle;
        }
    }

    [[nodiscard]] inline std::vector<CharacterIntentScore> ScoreCharacterIntents(
        const RuntimeCharacterRecord& character,
        const CharacterCognitionRecord& cognition,
        const CharacterDecisionRecord& decision)
    {
        std::vector<CharacterIntentScore> scores;
        scores.reserve(8);
        const bool hasRoute = decision.patrol.route.points.size() >= 2;
        const CharacterIntent normal = NormalRoleIntent(character, hasRoute);
        scores.push_back({normal, 24.0f + character.tuning.alertness * 8.0f});
        if (normal != CharacterIntent::Idle)
            scores.push_back({CharacterIntent::Idle, 4.0f});

        if (!character.authoring.autonomous)
        {
            scores.clear();
            scores.push_back({CharacterIntent::Idle, 100.0f});
            return scores;
        }

        const CharacterMemoryRecord* memory = BestActionableMemory(cognition);
        if (memory != nullptr && !IsSearchMemoryExhausted(decision, *memory))
        {
            const float knowledge = std::clamp(memory->confidence, 0.0f, 1.0f);
            const float curiosity = character.tuning.curiosity * 12.0f;
            const float alertness = character.tuning.alertness * 12.0f;
            const float hostile = memory->hostile ? 18.0f : 0.0f;

            if (memory->directSight && memory->hostile)
            {
                scores.push_back({
                    CharacterIntent::Chase,
                    72.0f + hostile + character.tuning.aggression * 8.0f + knowledge * 8.0f});
                scores.push_back({
                    CharacterIntent::HoldPosition,
                    48.0f + (1.0f - character.tuning.aggression) * 10.0f + alertness});
            }
            else if (memory->hostile &&
                     memory->secondsSinceSeen <= character.tuning.pursuitSeconds)
            {
                scores.push_back({
                    CharacterIntent::Investigate,
                    58.0f + hostile + curiosity + knowledge * 10.0f});
            }
            else if (cognition.awareness == AwarenessState::Searching ||
                     cognition.awareness == AwarenessState::Alerted ||
                     memory->hostile)
            {
                scores.push_back({
                    CharacterIntent::Search,
                    50.0f + alertness + curiosity * 0.5f + knowledge * 8.0f});
            }
            else if (memory->source == KnowledgeSource::Heard)
            {
                scores.push_back({
                    CharacterIntent::Investigate,
                    42.0f + curiosity + alertness * 0.5f + knowledge * 8.0f});
            }
            else
            {
                scores.push_back({
                    CharacterIntent::Observe,
                    30.0f + curiosity + knowledge * 6.0f});
            }
        }
        else if (memory == nullptr &&
                 cognition.awareness == AwarenessState::Searching &&
                 decision.searchRemainingSeconds > 0.0f)
        {
            scores.push_back({CharacterIntent::Search, 52.0f});
        }

        return scores;
    }

    inline void CaptureTopScores(
        CharacterDecisionRecord& decision,
        std::vector<CharacterIntentScore> scores)
    {
        std::sort(scores.begin(), scores.end(),
            [](const CharacterIntentScore& lhs, const CharacterIntentScore& rhs)
            {
                if (std::abs(lhs.score - rhs.score) > 0.0001f)
                    return lhs.score > rhs.score;
                return static_cast<std::int32_t>(lhs.intent) <
                    static_cast<std::int32_t>(rhs.intent);
            });
        decision.topScores = {};
        for (std::size_t index = 0;
             index < decision.topScores.size() && index < scores.size(); ++index)
        {
            decision.topScores[index] = scores[index];
        }
    }

    inline void SelectIntent(
        const RuntimeCharacterRecord& character,
        const CharacterCognitionRecord& cognition,
        CharacterDecisionRecord& decision)
    {
        RefreshSearchExhaustion(cognition, decision);
        auto scores = ScoreCharacterIntents(character, cognition, decision);
        CaptureTopScores(decision, scores);
        if (scores.empty())
            return;
        std::sort(scores.begin(), scores.end(),
            [](const CharacterIntentScore& lhs, const CharacterIntentScore& rhs)
            {
                if (std::abs(lhs.score - rhs.score) > 0.0001f)
                    return lhs.score > rhs.score;
                return static_cast<std::int32_t>(lhs.intent) <
                    static_cast<std::int32_t>(rhs.intent);
            });

        CharacterIntentScore winner = scores.front();
        const auto current = std::find_if(scores.begin(), scores.end(),
            [&decision](const CharacterIntentScore& score)
            {
                return score.intent == decision.intent;
            });
        if (current != scores.end())
        {
            const float retained = current->score + CharacterDecisionHysteresisBonus;
            if (decision.commitmentRemainingSeconds > 0.0f &&
                winner.score < current->score + CharacterDecisionEmergencyMargin)
            {
                winner = *current;
            }
            else if (retained >= winner.score)
            {
                winner = *current;
            }
        }

        if (winner.intent == decision.intent)
            return;
        decision.previousIntent = decision.intent;
        decision.intent = winner.intent;
        decision.intentAgeSeconds = 0.0f;
        decision.commitmentRemainingSeconds = CharacterDecisionCommitmentSeconds;
        decision.waitRemainingSeconds = 0.0f;
        decision.repathRemainingSeconds = 0.0f;
        decision.hasGoal = false;
        decision.arrived = false;
        ++decision.transitionCount;
        decision.lastTransitionReason =
            std::string(ToString(decision.previousIntent)) + " -> " +
            ToString(decision.intent) + " by utility";
        if (decision.intent == CharacterIntent::Search)
            decision.searchRemainingSeconds = character.tuning.searchSeconds;
    }

    [[nodiscard]] inline std::size_t StableRandomPatrolIndex(
        const CharacterDecisionRecord& decision,
        const std::size_t pointCount) noexcept
    {
        if (pointCount == 0)
            return 0;
        std::uint64_t hash = 1469598103934665603ull;
        for (const unsigned char value : decision.characterId)
        {
            hash ^= value;
            hash *= 1099511628211ull;
        }
        hash ^= decision.patrolVisitCount + 0x9e3779b97f4a7c15ull;
        hash *= 1099511628211ull;
        return static_cast<std::size_t>(hash % pointCount);
    }

    inline void AdvancePatrolPoint(CharacterDecisionRecord& decision) noexcept
    {
        const std::size_t count = decision.patrol.route.points.size();
        if (count < 2)
            return;
        ++decision.patrolVisitCount;
        switch (decision.patrol.route.settings.mode)
        {
        case bridge::PatrolRouteMode::PingPong:
            if (decision.patrolDirection > 0 && decision.patrolPointIndex + 1 >= count)
                decision.patrolDirection = -1;
            else if (decision.patrolDirection < 0 && decision.patrolPointIndex == 0)
                decision.patrolDirection = 1;
            decision.patrolPointIndex = static_cast<std::size_t>(
                static_cast<std::ptrdiff_t>(decision.patrolPointIndex) +
                decision.patrolDirection);
            break;
        case bridge::PatrolRouteMode::Random:
        {
            std::size_t next = StableRandomPatrolIndex(decision, count);
            if (next == decision.patrolPointIndex)
                next = (next + 1) % count;
            decision.patrolPointIndex = next;
            break;
        }
        case bridge::PatrolRouteMode::Loop:
        default:
            decision.patrolPointIndex = (decision.patrolPointIndex + 1) % count;
            break;
        }
    }

    [[nodiscard]] inline XMFLOAT3 SearchOffset(
        const std::size_t index) noexcept
    {
        static constexpr std::array<XMFLOAT3, 8> offsets = {{
            XMFLOAT3(3.0f, 0.0f, 0.0f),
            XMFLOAT3(0.0f, 0.0f, 3.0f),
            XMFLOAT3(-3.0f, 0.0f, 0.0f),
            XMFLOAT3(0.0f, 0.0f, -3.0f),
            XMFLOAT3(5.0f, 0.0f, 5.0f),
            XMFLOAT3(-5.0f, 0.0f, 5.0f),
            XMFLOAT3(-5.0f, 0.0f, -5.0f),
            XMFLOAT3(5.0f, 0.0f, -5.0f),
        }};
        return offsets[index % offsets.size()];
    }

    [[nodiscard]] inline bool ResolveIntentGoal(
        const RuntimeCharacterRecord& character,
        const CharacterCognitionRecord& cognition,
        CharacterDecisionRecord& decision,
        XMFLOAT3& goal) noexcept
    {
        const CharacterMemoryRecord* memory = BestActionableMemory(cognition);
        switch (decision.intent)
        {
        case CharacterIntent::Patrol:
            if (decision.patrol.route.points.size() < 2)
                return false;
            if (decision.patrolPointIndex >= decision.patrol.route.points.size())
                decision.patrolPointIndex = 0;
            goal = decision.patrol.route.points[decision.patrolPointIndex].position;
            return true;
        case CharacterIntent::Investigate:
        case CharacterIntent::Chase:
            if (memory == nullptr || !memory->hasPosition)
                return false;
            goal = memory->lastKnownPosition;
            return true;
        case CharacterIntent::Search:
            if (memory == nullptr || !memory->hasPosition)
                return false;
            goal = memory->lastKnownPosition;
            {
                const XMFLOAT3 offset = SearchOffset(decision.searchPointIndex);
                goal.x += offset.x;
                goal.z += offset.z;
            }
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] inline bool InitializeRuntimeCharacterDecision(
        const wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characters,
        const RuntimeCharacterPerceptionState& perception,
        RuntimeCharacterDecisionState& state,
        std::string& error)
    {
        RuntimeCharacterDecisionState next;
        if (characters.characters.size() != perception.characters.size())
        {
            error = "Character decision setup requires matching profile and perception records.";
            return false;
        }

        next.navigationGrid = bridge::FindDefaultNavigationGrid(scene);
        if (next.navigationGrid != wi::ecs::INVALID_ENTITY)
            next.navigationGridId = bridge::PersistentEntityId(scene, next.navigationGrid);

        next.characters.reserve(characters.characters.size());
        for (const auto& character : characters.characters)
        {
            const auto* cognition = FindCharacterCognition(
                perception, character.stableEntityId);
            if (cognition == nullptr)
            {
                error = "Character decision setup could not resolve matching cognition for '" +
                    character.stableEntityId + "'.";
                return false;
            }
            CharacterDecisionRecord decision;
            decision.characterId = character.stableEntityId;
            if (character.references.patrolRouteEntity != wi::ecs::INVALID_ENTITY)
            {
                bridge::PatrolRoute route;
                if (!bridge::CapturePatrolRoute(
                        scene,
                        character.references.patrolRouteEntity,
                        route,
                        error))
                {
                    error = "Character '" + character.stableEntityId +
                        "' Patrol Route is invalid: " + error;
                    return false;
                }
                if (route.stableId != character.authoring.patrolRouteEntityId)
                {
                    error = "Character Patrol Route stable identity changed during Runtime setup.";
                    return false;
                }
                decision.patrol.routeId = route.stableId;
                decision.patrol.route = std::move(route);
            }
            decision.intent = NormalRoleIntent(
                character, decision.patrol.route.points.size() >= 2);
            decision.previousIntent = decision.intent;
            decision.lastTransitionReason = "Runtime role default";
            next.characters.push_back(std::move(decision));
        }
        state = std::move(next);
        error.clear();
        return true;
    }

    inline void ResetRuntimeCharacterDecision(
        RuntimeCharacterDecisionState& state) noexcept
    {
        state = {};
    }

    inline void StopNativeCharacter(
        wi::scene::CharacterComponent& character) noexcept
    {
        character.Move(XMFLOAT3(0.0f, 0.0f, 0.0f));
    }

    inline void UpdateRuntimeCharacterDecision(
        wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characters,
        const RuntimeCharacterPerceptionState& perception,
        RuntimeCharacterDecisionState& state,
        const float dt) noexcept
    {
        if (!(dt > 0.0f) || !std::isfinite(dt))
            return;
        if (state.characters.size() != characters.characters.size() ||
            state.characters.size() != perception.characters.size())
            return;

        for (std::size_t index = 0; index < characters.characters.size(); ++index)
        {
            const auto& authored = characters.characters[index];
            auto* decision = FindCharacterDecision(state, authored.stableEntityId);
            const auto* cognition = FindCharacterCognition(
                perception, authored.stableEntityId);
            if (decision == nullptr || cognition == nullptr)
                continue;
            auto* character = scene.characters.GetComponent(authored.entity);
            if (character == nullptr || !character->IsActive())
                continue;

            decision->intentAgeSeconds += dt;
            decision->commitmentRemainingSeconds = std::max(
                0.0f, decision->commitmentRemainingSeconds - dt);
            decision->waitRemainingSeconds = std::max(
                0.0f, decision->waitRemainingSeconds - dt);
            decision->repathRemainingSeconds = std::max(
                0.0f, decision->repathRemainingSeconds - dt);
            if (decision->searchRemainingSeconds > 0.0f)
                decision->searchRemainingSeconds = std::max(
                    0.0f, decision->searchRemainingSeconds - dt);

            if (decision->lastCognitionTick != cognition->cognitionTicks)
            {
                decision->lastCognitionTick = cognition->cognitionTicks;
                SelectIntent(authored, *cognition, *decision);
                ++state.decisionTicks;
            }

            if (decision->intent == CharacterIntent::Search &&
                decision->searchRemainingSeconds <= 0.0f)
            {
                const CharacterMemoryRecord* exhaustedMemory =
                    BestActionableMemory(*cognition);
                if (exhaustedMemory != nullptr)
                {
                    decision->exhaustedSearchSubjectId = exhaustedMemory->subjectId;
                    decision->exhaustedSearchMemoryAgeSeconds = exhaustedMemory->ageSeconds;
                }
                else
                {
                    decision->exhaustedSearchSubjectId.clear();
                    decision->exhaustedSearchMemoryAgeSeconds = -1.0f;
                }
                const CharacterIntent normal = NormalRoleIntent(
                    authored, decision->patrol.route.points.size() >= 2);
                decision->previousIntent = decision->intent;
                decision->intent = normal;
                decision->intentAgeSeconds = 0.0f;
                decision->commitmentRemainingSeconds = CharacterDecisionCommitmentSeconds;
                decision->hasGoal = false;
                decision->lastTransitionReason = "Search timeout -> return to role";
                ++decision->transitionCount;
            }

            XMFLOAT3 desiredGoal;
            if (!ResolveIntentGoal(authored, *cognition, *decision, desiredGoal))
            {
                decision->hasGoal = false;
                decision->arrived = true;
                decision->stuckSeconds = 0.0f;
                StopNativeCharacter(*character);
                continue;
            }

            if (decision->waitRemainingSeconds > 0.0f)
            {
                StopNativeCharacter(*character);
                continue;
            }

            const XMFLOAT3 position = character->GetPositionInterpolated();
            const float dx = desiredGoal.x - position.x;
            const float dz = desiredGoal.z - position.z;
            const float distance = std::sqrt(dx * dx + dz * dz);
            if (distance <= CharacterDecisionArrivalDistance)
            {
                decision->arrived = true;
                decision->hasGoal = false;
                decision->stuckSeconds = 0.0f;
                StopNativeCharacter(*character);
                if (decision->intent == CharacterIntent::Patrol)
                {
                    decision->waitRemainingSeconds =
                        decision->patrol.route.settings.waitSeconds;
                    AdvancePatrolPoint(*decision);
                }
                else if (decision->intent == CharacterIntent::Investigate)
                {
                    decision->previousIntent = decision->intent;
                    decision->intent = CharacterIntent::Search;
                    decision->intentAgeSeconds = 0.0f;
                    decision->commitmentRemainingSeconds = CharacterDecisionCommitmentSeconds;
                    decision->searchRemainingSeconds = authored.tuning.searchSeconds;
                    decision->searchPointIndex = 0;
                    decision->lastTransitionReason = "Investigate arrival -> search";
                    ++decision->transitionCount;
                }
                else if (decision->intent == CharacterIntent::Search)
                {
                    ++decision->searchPointIndex;
                    decision->waitRemainingSeconds = 0.35f;
                }
                continue;
            }

            const bool goalChanged = !decision->hasGoal ||
                std::abs(decision->goal.x - desiredGoal.x) > 0.25f ||
                std::abs(decision->goal.y - desiredGoal.y) > 0.25f ||
                std::abs(decision->goal.z - desiredGoal.z) > 0.25f;
            if ((goalChanged || decision->repathRemainingSeconds <= 0.0f) &&
                state.navigationGrid != wi::ecs::INVALID_ENTITY)
            {
                std::string ignored;
                bridge::NavigationQuerySettings query;
                if (bridge::SetCharacterNavigationGoal(
                        scene,
                        authored.entity,
                        state.navigationGrid,
                        desiredGoal,
                        query,
                        ignored))
                {
                    decision->goal = desiredGoal;
                    decision->hasGoal = true;
                    decision->arrived = false;
                    decision->repathRemainingSeconds = CharacterDecisionRepathSeconds;
                    ++decision->pathRequests;
                    ++state.pathRequests;
                }
                else
                {
                    ++state.rejectedGoals;
                }
            }

            if (!decision->hasGoal || state.navigationGrid == wi::ecs::INVALID_ENTITY)
            {
                StopNativeCharacter(*character);
                continue;
            }

            if (!character->pathquery.is_succesful())
            {
                StopNativeCharacter(*character);
                continue;
            }
            const XMFLOAT3 waypoint = character->pathquery.get_next_waypoint();
            XMFLOAT3 direction(
                waypoint.x - position.x,
                0.0f,
                waypoint.z - position.z);
            const float length = std::sqrt(
                direction.x * direction.x + direction.z * direction.z);
            if (length > 0.0001f)
            {
                direction.x /= length;
                direction.z /= length;
                character->Turn(direction);
                character->Move(XMFLOAT3(
                    direction.x * CharacterDecisionMoveAmount,
                    0.0f,
                    direction.z * CharacterDecisionMoveAmount));
            }
            else
            {
                StopNativeCharacter(*character);
            }

            if (decision->hasLastPosition)
            {
                const float movedX = position.x - decision->lastPosition.x;
                const float movedZ = position.z - decision->lastPosition.z;
                const float moved = std::sqrt(movedX * movedX + movedZ * movedZ);
                if (moved < 0.02f)
                    decision->stuckSeconds += dt;
                else
                    decision->stuckSeconds = 0.0f;
            }
            decision->lastPosition = position;
            decision->hasLastPosition = true;

            if (decision->stuckSeconds >= CharacterDecisionStuckRepathSeconds)
                decision->repathRemainingSeconds = 0.0f;
            if (decision->stuckSeconds >= CharacterDecisionStuckAbortSeconds)
            {
                StopNativeCharacter(*character);
                decision->stuckSeconds = 0.0f;
                decision->hasGoal = false;
                decision->repathRemainingSeconds = 0.0f;
                ++decision->stuckRecoveries;
                ++state.stuckRecoveries;
                decision->lastTransitionReason =
                    "Stuck recovery requested native repath; no teleport used";
            }
        }
    }
}
