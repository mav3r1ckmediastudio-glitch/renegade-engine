#pragma once

#include "RuntimeCharacterDecision.h"
#include "RuntimeCombatService.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace renegade::runtime
{
    [[nodiscard]] inline std::vector<CharacterIntentScore> ScoreCharacterCombatIntents(
        const RuntimeCharacterRecord& character,
        const CharacterCognitionRecord& cognition,
        const CharacterDecisionRecord& decision,
        const CharacterCombatRecord& combat)
    {
        std::vector<CharacterIntentScore> scores;
        scores.reserve(6);
        if (combat.dead)
        {
            scores.push_back({CharacterIntent::Dead, 1000.0f});
            return scores;
        }
        if (!character.authoring.autonomous)
            return scores;

        const CharacterMemoryRecord* memory = BestActionableMemory(cognition);
        const bool hostileKnowledge = memory != nullptr && memory->hostile;
        const bool directHostile = hostileKnowledge && memory->directSight &&
            combat.hasDirectHostileTarget;
        const float health = HealthFraction(combat);
        const float courage = std::clamp(character.tuning.courage, 0.0f, 1.0f);
        const float aggression = std::clamp(character.tuning.aggression, 0.0f, 1.0f);
        const bool civilianLike =
            character.authoring.role == bridge::CharacterRole::Civilian ||
            character.authoring.role == bridge::CharacterRole::Passive;
        const bool usableWeapon = HasUsableWeapon(combat);

        // Unarmed/non-viable Characters must not inherit AI-04's hostile Chase
        // as their best response. Civilians/passive creatures flee by default;
        // other Characters retreat or surrender according to authored traits.
        if (hostileKnowledge && !usableWeapon)
        {
            if (character.authoring.canFlee &&
                (civilianLike || courage < 0.40f))
            {
                scores.push_back({
                    CharacterIntent::Flee,
                    148.0f + (1.0f - courage) * 24.0f});
            }
            else if (character.authoring.canFlee)
            {
                scores.push_back({
                    CharacterIntent::Retreat,
                    124.0f + (1.0f - courage) * 18.0f});
            }
            if (character.authoring.canSurrender && courage < 0.55f)
            {
                scores.push_back({
                    CharacterIntent::Surrender,
                    128.0f + (1.0f - courage) * 20.0f});
            }
        }

        if (hostileKnowledge && health <= character.tuning.retreatHealthThreshold)
        {
            if (character.authoring.canSurrender && courage < 0.55f &&
                (!HasUsableWeapon(combat) || health <= 0.12f))
            {
                scores.push_back({
                    CharacterIntent::Surrender,
                    145.0f + (1.0f - courage) * 30.0f + (1.0f - health) * 20.0f});
            }
            if (character.authoring.canFlee && (civilianLike || courage < 0.40f))
            {
                scores.push_back({
                    CharacterIntent::Flee,
                    132.0f + (1.0f - courage) * 26.0f + (1.0f - health) * 18.0f});
            }
            else if (character.authoring.canFlee)
            {
                scores.push_back({
                    CharacterIntent::Retreat,
                    114.0f + (1.0f - health) * 18.0f +
                    character.tuning.coverPreference * 8.0f});
            }
        }

        if (!hostileKnowledge || !usableWeapon)
        {
            if (hostileKnowledge && character.authoring.canSurrender &&
                UsesAmmunition(combat) && combat.magazineAmmo <= 0 &&
                combat.reserveAmmo <= 0 && courage < 0.65f)
            {
                scores.push_back({CharacterIntent::Surrender, 118.0f + (1.0f - courage) * 20.0f});
            }
            return scores;
        }

        if (UsesAmmunition(combat))
        {
            if (combat.reloadRemainingSeconds > 0.0f)
            {
                scores.push_back({CharacterIntent::Reload, 138.0f});
            }
            else if (combat.magazineAmmo <= 0 && combat.reserveAmmo > 0)
            {
                scores.push_back({CharacterIntent::Reload, 136.0f});
            }
            else if (CanReload(combat))
            {
                const float magazineFraction = combat.weapon.magazineSize <= 0
                    ? 1.0f
                    : static_cast<float>(combat.magazineAmmo) /
                        static_cast<float>(combat.weapon.magazineSize);
                const bool saferWindow = !directHostile ||
                    combat.targetDistance > combat.effectiveRange.preferredRange * 1.20f;
                if (magazineFraction <= 0.20f && saferWindow)
                    scores.push_back({CharacterIntent::Reload, 78.0f});
            }
        }

        if (!directHostile || !std::isfinite(combat.targetDistance))
            return scores;

        if (combat.targetDistance > combat.effectiveRange.maxRange)
        {
            scores.push_back({
                CharacterIntent::Chase,
                96.0f + aggression * 14.0f + memory->confidence * 6.0f});
            return scores;
        }

        if (combat.effectiveRange.minRange > 0.0f &&
            combat.targetDistance < combat.effectiveRange.minRange &&
            combat.weapon.style == bridge::WeaponAiStyle::Ranged)
        {
            scores.push_back({
                CharacterIntent::Retreat,
                112.0f + (1.0f - aggression) * 16.0f +
                character.tuning.coverPreference * 8.0f});
            scores.push_back({CharacterIntent::Attack, 82.0f + aggression * 16.0f});
            return scores;
        }

        const float preferredDelta = std::abs(
            combat.targetDistance - combat.effectiveRange.preferredRange);
        const float rangeSpan = std::max(
            1.0f, combat.effectiveRange.maxRange - combat.effectiveRange.minRange);
        const float rangeFit = std::clamp(1.0f - preferredDelta / rangeSpan, 0.0f, 1.0f);
        scores.push_back({
            CharacterIntent::Attack,
            102.0f + aggression * 16.0f + rangeFit * 12.0f +
            memory->confidence * 8.0f});
        scores.push_back({
            CharacterIntent::HoldPosition,
            70.0f + rangeFit * 10.0f + (1.0f - aggression) * 8.0f});
        return scores;
    }

    inline void SelectCombatIntent(
        const RuntimeCharacterRecord& character,
        const CharacterCognitionRecord& cognition,
        CharacterDecisionRecord& decision,
        const CharacterCombatRecord& combat,
        RuntimeCombatState& state,
        const CombatEventEmitter& emitter)
    {
        RefreshSearchExhaustion(cognition, decision);
        auto scores = ScoreCharacterIntents(character, cognition, decision);
        const auto combatScores = ScoreCharacterCombatIntents(
            character, cognition, decision, combat);

        // Merge duplicate intent candidates by their strongest score. AI-04 and
        // AI-05 can both propose Chase/Hold; there must still be one winner.
        for (const auto& candidate : combatScores)
        {
            const auto existing = std::find_if(
                scores.begin(), scores.end(),
                [&candidate](const CharacterIntentScore& score)
                {
                    return score.intent == candidate.intent;
                });
            if (existing == scores.end())
                scores.push_back(candidate);
            else
                existing->score = std::max(existing->score, candidate.score);
        }

        CaptureTopScores(decision, scores);
        if (scores.empty())
            return;
        std::sort(
            scores.begin(), scores.end(),
            [](const CharacterIntentScore& lhs, const CharacterIntentScore& rhs)
            {
                if (std::abs(lhs.score - rhs.score) > 0.0001f)
                    return lhs.score > rhs.score;
                return static_cast<std::int32_t>(lhs.intent) <
                    static_cast<std::int32_t>(rhs.intent);
            });

        CharacterIntentScore winner = scores.front();
        const auto current = std::find_if(
            scores.begin(), scores.end(),
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

        const bool combatDriven = std::any_of(
            combatScores.begin(), combatScores.end(),
            [&winner](const CharacterIntentScore& score)
            {
                return score.intent == winner.intent &&
                    score.score + 0.0001f >= winner.score;
            });
        const CharacterIntent before = decision.intent;
        decision.previousIntent = before;
        decision.intent = winner.intent;
        decision.intentAgeSeconds = 0.0f;
        decision.commitmentRemainingSeconds = CharacterDecisionCommitmentSeconds;
        decision.waitRemainingSeconds = 0.0f;
        decision.repathRemainingSeconds = 0.0f;
        decision.hasGoal = false;
        decision.arrived = false;
        ++decision.transitionCount;
        decision.lastTransitionReason =
            std::string(ToString(before)) + " -> " +
            ToString(decision.intent) +
            (combatDriven ? " by combat utility" : " by utility");
        if (decision.intent == CharacterIntent::Search)
            decision.searchRemainingSeconds = character.tuning.searchSeconds;

        if (decision.intent == CharacterIntent::Surrender ||
            decision.intent == CharacterIntent::Flee ||
            decision.intent == CharacterIntent::Retreat ||
            decision.intent == CharacterIntent::Dead)
        {
            (void)EmitCombatGameplayEvent(
                state,
                emitter,
                {0, "ai.combat_intent",
                 std::string("target=") + RuntimePlayerKnowledgeId +
                    ";intent=" + ToString(decision.intent),
                 character.stableEntityId,
                 {}});
        }
    }

    [[nodiscard]] inline bool ResolveCombatEscapeGoal(
        const wi::scene::CharacterComponent& nativeCharacter,
        const CharacterCognitionRecord& cognition,
        const CharacterIntent intent,
        XMFLOAT3& goal) noexcept
    {
        if (intent != CharacterIntent::Retreat && intent != CharacterIntent::Flee)
            return false;
        const CharacterMemoryRecord* memory = BestActionableMemory(cognition);
        if (memory == nullptr || !memory->hasPosition)
            return false;
        const XMFLOAT3 position = nativeCharacter.GetPositionInterpolated();
        float dx = position.x - memory->lastKnownPosition.x;
        float dz = position.z - memory->lastKnownPosition.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        if (!(length > 0.001f) || !std::isfinite(length))
        {
            dx = 1.0f;
            dz = 0.0f;
        }
        else
        {
            dx /= length;
            dz /= length;
        }
        const float distance = intent == CharacterIntent::Flee ? 18.0f : 9.0f;
        goal = XMFLOAT3(
            position.x + dx * distance,
            position.y,
            position.z + dz * distance);
        return true;
    }

    inline void MoveCombatEscape(
        wi::scene::Scene& scene,
        const RuntimeCharacterRecord& character,
        const CharacterCognitionRecord& cognition,
        CharacterDecisionRecord& decision,
        RuntimeCharacterDecisionState& decisionState) noexcept
    {
        auto* nativeCharacter = scene.characters.GetComponent(character.entity);
        if (nativeCharacter == nullptr || !nativeCharacter->IsActive())
            return;
        XMFLOAT3 goal;
        if (!ResolveCombatEscapeGoal(*nativeCharacter, cognition, decision.intent, goal) ||
            decisionState.navigationGrid == wi::ecs::INVALID_ENTITY)
        {
            StopNativeCharacter(*nativeCharacter);
            return;
        }
        const bool goalChanged = !decision.hasGoal ||
            std::abs(decision.goal.x - goal.x) > 0.5f ||
            std::abs(decision.goal.z - goal.z) > 0.5f;
        if (goalChanged || decision.repathRemainingSeconds <= 0.0f)
        {
            bridge::NavigationQuerySettings query;
            std::string ignored;
            if (bridge::SetCharacterNavigationGoal(
                    scene,
                    character.entity,
                    decisionState.navigationGrid,
                    goal,
                    query,
                    ignored))
            {
                decision.goal = goal;
                decision.hasGoal = true;
                decision.repathRemainingSeconds = CharacterDecisionRepathSeconds;
                ++decision.pathRequests;
                ++decisionState.pathRequests;
            }
            else
            {
                ++decisionState.rejectedGoals;
            }
        }
        if (!decision.hasGoal || !nativeCharacter->pathquery.is_succesful())
        {
            StopNativeCharacter(*nativeCharacter);
            return;
        }
        const XMFLOAT3 position = nativeCharacter->GetPositionInterpolated();
        const XMFLOAT3 waypoint = nativeCharacter->pathquery.get_next_waypoint();
        XMFLOAT3 direction(waypoint.x - position.x, 0.0f, waypoint.z - position.z);
        const float length = std::sqrt(direction.x * direction.x + direction.z * direction.z);
        if (!(length > 0.0001f))
        {
            StopNativeCharacter(*nativeCharacter);
            return;
        }
        direction.x /= length;
        direction.z /= length;
        nativeCharacter->Turn(direction);
        nativeCharacter->Move(XMFLOAT3(
            direction.x * CharacterDecisionMoveAmount,
            0.0f,
            direction.z * CharacterDecisionMoveAmount));
    }

    inline void UpdateRuntimeCombatDecision(
        wi::scene::Scene& scene,
        const RuntimeCharacterSystemState& characters,
        RuntimeCharacterPerceptionState& perception,
        RuntimeCharacterDecisionState& decisions,
        RuntimeCombatState& combatState,
        const CombatEventEmitter& emitter,
        const float dt) noexcept
    {
        if (!(dt >= 0.0f) || !std::isfinite(dt))
            return;

        RefreshRuntimeCombat(scene, characters, perception, combatState, dt);
        for (const auto& character : characters.characters)
        {
            auto* cognition = FindCharacterCognition(perception, character.stableEntityId);
            auto* decision = FindCharacterDecision(decisions, character.stableEntityId);
            auto* combat = FindCharacterCombat(combatState, character.stableEntityId);
            if (cognition == nullptr || decision == nullptr || combat == nullptr)
                continue;
            auto* nativeCharacter = scene.characters.GetComponent(character.entity);
            if (nativeCharacter == nullptr || !nativeCharacter->IsActive())
                continue;

            if (decision->lastCognitionTick != cognition->cognitionTicks)
            {
                decision->lastCognitionTick = cognition->cognitionTicks;
                ++decisions.decisionTicks;
            }
            SelectCombatIntent(
                character, *cognition, *decision, *combat, combatState, emitter);

            switch (decision->intent)
            {
            case CharacterIntent::Dead:
            case CharacterIntent::Surrender:
                StopNativeCharacter(*nativeCharacter);
                break;
            case CharacterIntent::Reload:
                StopNativeCharacter(*nativeCharacter);
                if (combat->reloadRemainingSeconds <= 0.0f && BeginCombatReload(*combat))
                {
                    ++combatState.reloads;
                    (void)EmitCombatGameplayEvent(
                        combatState,
                        emitter,
                        {0, "ai.reload_started",
                         "ammo=" + std::to_string(combat->magazineAmmo) +
                            ";reserve=" + std::to_string(combat->reserveAmmo),
                         character.stableEntityId,
                         {}});
                }
                break;
            case CharacterIntent::Attack:
            {
                StopNativeCharacter(*nativeCharacter);
                CombatFireResult result;
                (void)TryFireAtRuntimePlayer(
                    scene,
                    character,
                    *cognition,
                    *combat,
                    perception,
                    combatState,
                    emitter,
                    result);
                break;
            }
            case CharacterIntent::Retreat:
            case CharacterIntent::Flee:
                MoveCombatEscape(
                    scene, character, *cognition, *decision, decisions);
                break;
            default:
                break;
            }
        }
    }
}
