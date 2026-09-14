from pathlib import Path
import runpy

# Apply the first audit repair set (effective range, one event queue, fixture API fixes).
runpy.run_path(".github/ai05_repair.py", run_name="__main__")


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


# AI-04 remains the movement executor, but AI-05 is now the single utility
# selector once combat exists. This prevents combat intents being overwritten
# and re-selected on each 5 Hz cognition tick.
path = "Runtime/src/RuntimeCharacterDecision.h"
text = read(path)
text = replace_once(
    text,
    '''    inline void StopNativeCharacter(
        wi::scene::CharacterComponent& character) noexcept
    {
        character.Move(XMFLOAT3(0.0f, 0.0f, 0.0f));
    }

    inline void UpdateRuntimeCharacterDecision(
''',
    '''    inline void StopNativeCharacter(
        wi::scene::CharacterComponent& character) noexcept
    {
        character.Move(XMFLOAT3(0.0f, 0.0f, 0.0f));
    }

    [[nodiscard]] inline bool IsCombatExecutionIntent(
        const CharacterIntent intent) noexcept
    {
        switch (intent)
        {
        case CharacterIntent::Attack:
        case CharacterIntent::Reload:
        case CharacterIntent::Retreat:
        case CharacterIntent::Flee:
        case CharacterIntent::Surrender:
        case CharacterIntent::Dead:
            return true;
        default:
            return false;
        }
    }

    inline void UpdateRuntimeCharacterDecision(
''',
    "combat-execution intent helper")
text = replace_once(
    text,
    '''        RuntimeCharacterDecisionState& state,
        const float dt) noexcept
''',
    '''        RuntimeCharacterDecisionState& state,
        const float dt,
        const bool selectIntent = true) noexcept
''',
    "decision selection ownership parameter")
text = replace_once(
    text,
    '''            if (decision->lastCognitionTick != cognition->cognitionTicks)
            {
                decision->lastCognitionTick = cognition->cognitionTicks;
                SelectIntent(authored, *cognition, *decision);
                ++state.decisionTicks;
            }
''',
    '''            if (selectIntent &&
                decision->lastCognitionTick != cognition->cognitionTicks)
            {
                decision->lastCognitionTick = cognition->cognitionTicks;
                SelectIntent(authored, *cognition, *decision);
                ++state.decisionTicks;
            }
''',
    "AI-04 selector ownership gate")
text = replace_once(
    text,
    '''            XMFLOAT3 desiredGoal;
            if (!ResolveIntentGoal(authored, *cognition, *decision, desiredGoal))
''',
    '''            // Once AI-05 owns unified utility selection, combat execution
            // intents are acted by RuntimeCombatDecision. AI-04 must not stop or
            // replace their native movement in this movement-only pass.
            if (!selectIntent && IsCombatExecutionIntent(decision->intent))
                continue;

            XMFLOAT3 desiredGoal;
            if (!ResolveIntentGoal(authored, *cognition, *decision, desiredGoal))
''',
    "AI-04 combat execution preservation")
write(path, text)


# Replace combat-only override selection with one combined AI-04 + AI-05
# utility winner using the existing commitment/hysteresis constants.
path = "Runtime/src/RuntimeCombatDecision.h"
text = read(path)
start = text.index("    inline void SelectCombatIntent(\n")
end = text.index("    [[nodiscard]] inline bool ResolveCombatEscapeGoal(\n", start)
new_selector = '''    inline void SelectCombatIntent(
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

'''
text = text[:start] + new_selector + text[end:]
text = replace_once(
    text,
    '''            SelectCombatIntent(
                character, *cognition, *decision, *combat, combatState, emitter);

            switch (decision->intent)
''',
    '''            if (decision->lastCognitionTick != cognition->cognitionTicks)
            {
                decision->lastCognitionTick = cognition->cognitionTicks;
                ++decisions.decisionTicks;
            }
            SelectCombatIntent(
                character, *cognition, *decision, *combat, combatState, emitter);

            switch (decision->intent)
''',
    "integrated decision tick ownership")
text = text.replace(
    '''                         character.stableEntityId,
                         RuntimePlayerKnowledgeId});''',
    '''                         character.stableEntityId,
                         {}});''')
write(path, text)


# Runtime order is perception -> unified utility/combat execution -> AI-04
# movement-only execution. This prevents a stale pre-combat movement command.
path = "Runtime/src/RuntimeLiveDiagnostics.cpp"
text = read(path)
decision_call = '''            UpdateRuntimeCharacterDecision(
                scenes_.GetScene(),
                characterAiState_,
                characterPerceptionState_,
                characterDecisionState_,
                simulationDt);

'''
text = replace_once(text, decision_call, "", "remove pre-combat AI-04 selection/movement")
combat_call = '''            UpdateRuntimeCombatDecision(
                scenes_.GetScene(),
                characterAiState_,
                characterPerceptionState_,
                characterDecisionState_,
                combatState_,
                combatEmitter,
                simulationDt);
'''
text = replace_once(
    text,
    combat_call,
    combat_call + '''            UpdateRuntimeCharacterDecision(
                scenes_.GetScene(),
                characterAiState_,
                characterPerceptionState_,
                characterDecisionState_,
                simulationDt,
                false);
''',
    "post-combat AI-04 movement-only pass")
write(path, text)


# Runtime-player events are broadcasts because $runtime.player is a cognition
# subject, not a persistent Scene entity/script owner. Preserve target semantics
# in the payload while leaving targetEntityId empty so governed on_event handlers
# can actually observe public AI combat events.
path = "Runtime/src/RuntimeCombatService.h"
text = read(path)
text = replace_once(
    text,
    '''        const std::string firedPayload =
            "distance=" + std::to_string(distance) +
''',
    '''        const std::string firedPayload =
            std::string("target=") + RuntimePlayerKnowledgeId +
            ";distance=" + std::to_string(distance) +
''',
    "weapon-fired player target payload")
text = replace_once(
    text,
    '''            {0, "ai.weapon_fired", firedPayload,
             combat.characterId, RuntimePlayerKnowledgeId});
''',
    '''            {0, "ai.weapon_fired", firedPayload,
             combat.characterId, {}});
''',
    "weapon-fired broadcast")
text = replace_once(
    text,
    '''            const std::string damagePayload =
                "damage=" + std::to_string(result.damage) +
''',
    '''            const std::string damagePayload =
                std::string("target=") + RuntimePlayerKnowledgeId +
                ";damage=" + std::to_string(result.damage) +
''',
    "player damage target payload")
text = replace_once(
    text,
    '''                {0, "ai.damage", damagePayload,
                 combat.characterId, RuntimePlayerKnowledgeId});
''',
    '''                {0, "ai.damage", damagePayload,
                 combat.characterId, {}});
''',
    "player damage broadcast")
write(path, text)


# Add regressions for unified ownership and public event deliverability.
path = "Tests/CharacterAiCombatTests.cpp"
text = read(path)
insert_after = '''    const CombatEventEmitter emitter = [&events](
        bridge::GameplayEvent event, std::string& eventError)
    {
        return events.Enqueue(std::move(event), eventError);
    };

'''
addition = '''    CharacterDecisionRecord integratedDecision = decision;
    integratedDecision.commitmentRemainingSeconds = 0.0f;
    SelectCombatIntent(
        character,
        perception.characters.front(),
        integratedDecision,
        *combat,
        combatState,
        emitter);
    if (integratedDecision.intent != CharacterIntent::Attack)
        return Fail("unified AI-04 + AI-05 utility should select Attack in range");
    const std::uint64_t integratedTransitions = integratedDecision.transitionCount;
    RuntimeCharacterDecisionState movementOnly;
    movementOnly.characters.push_back(integratedDecision);
    UpdateRuntimeCharacterDecision(
        scene,
        characterSystem,
        perception,
        movementOnly,
        0.1f,
        false);
    if (movementOnly.characters.front().intent != CharacterIntent::Attack ||
        movementOnly.characters.front().transitionCount != integratedTransitions)
        return Fail("AI-04 movement pass must not reselect combat-owned intent");

'''
text = replace_once(text, insert_after, insert_after + addition, "unified intent regression insertion")
text = replace_once(
    text,
    '''    if (combat->magazineAmmo != ammoBefore - 1 || events.Size() == 0)
        return Fail("fire must consume ammo and publish GameplayEventService event");
    if (perception.sounds.empty())
''',
    '''    if (combat->magazineAmmo != ammoBefore - 1 || events.Size() == 0)
        return Fail("fire must consume ammo and publish GameplayEventService event");
    bridge::GameplayEvent publicCombatEvent;
    if (!events.TryDequeue(publicCombatEvent) ||
        publicCombatEvent.name != "ai.weapon_fired" ||
        !publicCombatEvent.targetEntityId.empty())
        return Fail("runtime-player combat event must be publicly deliverable broadcast");
    if (perception.sounds.empty())
''',
    "public combat event regression")
write(path, text)


# Harden source contract around unified selection and broadcast routing.
path = "Tests/CharacterAiCombatSourceContract.cmake"
text = read(path)
text = replace_once(
    text,
    'read_required("Runtime/src/RuntimeCombatDecision.h" decision)\n',
    'read_required("Runtime/src/RuntimeCombatDecision.h" decision)\nread_required("Runtime/src/RuntimeCharacterDecision.h" base_decision)\n',
    "source contract base decision read")
text = replace_once(
    text,
    'require_text("${decision}" "CharacterDecisionEmergencyMargin" "combat hysteresis/commitment")\n',
    'require_text("${decision}" "CharacterDecisionEmergencyMargin" "combat hysteresis/commitment")\nrequire_text("${decision}" "ScoreCharacterIntents" "single combined AI-04 + AI-05 utility winner")\nrequire_text("${base_decision}" "const bool selectIntent = true" "AI-04 movement-only execution mode")\nrequire_text("${base_decision}" "IsCombatExecutionIntent" "combat intent movement ownership")\n',
    "source contract unified intent")
text = replace_once(
    text,
    'require_text("${runtime}" "UpdateRuntimeCombatDecision" "Runtime combat execution")\n',
    'require_text("${runtime}" "UpdateRuntimeCombatDecision" "Runtime combat execution")\nrequire_text("${runtime}" "simulationDt,\\n                false" "AI-04 post-combat movement-only pass")\n',
    "source contract runtime order")
text = replace_once(
    text,
    'require_text("${tests}" "publish GameplayEventService event" "combat event regression")\n',
    'require_text("${tests}" "publish GameplayEventService event" "combat event regression")\nrequire_text("${tests}" "AI-04 movement pass must not reselect combat-owned intent" "single selector regression")\nrequire_text("${tests}" "runtime-player combat event must be publicly deliverable broadcast" "public combat event routing regression")\n',
    "source contract integration regressions")
write(path, text)

print("AI-05 final unified-intent audit repairs applied")
