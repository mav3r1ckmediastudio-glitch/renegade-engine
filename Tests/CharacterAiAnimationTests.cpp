#include "RuntimeCharacterAnimation.h"

#include <iostream>
#include <string>

namespace
{
    int Fail(const std::string& message)
    {
        std::cerr << "AI06_FAIL: " << message << '\n';
        return 1;
    }

    wi::ecs::Entity AddClip(
        wi::scene::Scene& scene,
        const wi::ecs::Entity target,
        const std::string& name)
    {
        const wi::ecs::Entity entity = scene.Entity_CreateTransform(name);
        auto& animation = scene.animations.Create(entity);
        animation.start = 2.0f;
        animation.end = 4.0f;
        wi::scene::AnimationComponent::AnimationChannel channel;
        channel.target = target;
        animation.channels.push_back(channel);
        return entity;
    }
}

int main()
{
    using namespace renegade::runtime;

    // A6 one-shots use independent, stable-ID-seeded pseudo-random choices;
    // no immediate repeat, including across interruptions by other actions.
    RuntimeCharacterAnimationRecord seeded;
    seeded.characterId = "00000000-0000-4000-8000-000000000006";
    RuntimeCharacterAnimationRecord replay = seeded;
    bool varied = false;
    std::size_t first = 0;
    for (std::size_t i = 0; i < 20; ++i)
    {
        const auto pick = PickCharacterOneShotVariant(
            seeded, CharacterAnimationSemantic::Attack, 5);
        if (pick >= 5 || (i > 0 && pick == seeded.lastOneShotVariant[
                CharacterAnimationIndex(CharacterAnimationSemantic::Attack)]))
            return Fail("one-shot variant selection repeated or out of range");
        if (i == 0) first = pick;
        varied = varied || (pick != first);
        if (pick != PickCharacterOneShotVariant(
                replay, CharacterAnimationSemantic::Attack, 5))
            return Fail("stable-ID variant sequence is not reproducible");
        const auto attackIndex = CharacterAnimationIndex(CharacterAnimationSemantic::Attack);
        seeded.lastOneShotVariant[attackIndex] = pick;
        replay.lastOneShotVariant[attackIndex] = pick;
        ++seeded.oneShotVariantSequence[attackIndex];
        ++replay.oneShotVariantSequence[attackIndex];
        // Activity in a different semantic must not move the attack sequence.
        ++seeded.oneShotVariantSequence[CharacterAnimationIndex(CharacterAnimationSemantic::Hit)];
        ++seeded.variantSequence;
    }
    if (!varied || seeded.oneShotVariantSequence[
            CharacterAnimationIndex(CharacterAnimationSemantic::Attack)] != 20)
        return Fail("multiple authored attack variants were not exercised");

    wi::scene::Scene scene;
    const wi::ecs::Entity character = scene.Entity_CreateTransform("Mutant");

    if (InferCharacterAnimationSemantic("mutant swiping") !=
        CharacterAnimationSemantic::Attack ||
        InferCharacterAnimationSemantic("Mutant_Attack_02") !=
        CharacterAnimationSemantic::Attack ||
        InferCharacterAnimationSemantic("Death_Back") !=
        CharacterAnimationSemantic::Death ||
        InferCharacterAnimationSemantic("Walk_Forward") !=
        CharacterAnimationSemantic::Locomotion ||
        InferCharacterAnimationSemantic("Mutant_Run_Forward") !=
        CharacterAnimationSemantic::Run)
    {
        return Fail("native clip semantic inference");
    }

    // The owner's 14-clip Mutant has a 0.033s bind-pose clip named "Mutant".
    // Prior inference sorted it before two real idle clips and looped T-pose.
    wi::scene::Scene ownerScene;
    const auto owner = ownerScene.Entity_CreateTransform("mutty123");
    for (const char* name : {"Mutant", "left turn 45", "mutant breathing idle",
            "mutant dying", "mutant flexing muscles", "mutant idle",
            "mutant jumping", "mutant left turn 45", "mutant right turn 45",
            "mutant swiping", "Standing Melee Punch", "Mutant Punch",
            "Mutant Run", "Mutant Walking"})
    {
        const auto clip = AddClip(ownerScene, owner, name);
        if (std::string(name) == "Mutant")
            ownerScene.animations.GetComponent(clip)->end = 0.0333333f;
    }
    RuntimeCharacterSystemState ownerCharacters;
    RuntimeCharacterRecord ownerRecord;
    ownerRecord.stableEntityId = "00000000-0000-4000-8000-000000000014";
    ownerRecord.entity = owner;
    ownerCharacters.characters.push_back(ownerRecord);
    RuntimeCombatState ownerCombat;
    CharacterCombatRecord ownerCombatRecord;
    ownerCombatRecord.characterId = ownerRecord.stableEntityId;
    ownerCombatRecord.entity = owner;
    ownerCombat.characters.push_back(ownerCombatRecord);
    RuntimeCharacterAnimationState ownerState;
    std::string ownerError;
    if (!InitializeRuntimeCharacterAnimations(ownerScene, ownerCharacters,
            ownerCombat, ownerState, ownerError))
        return Fail("actual 14-clip Mutant semantic setup: " + ownerError);
    auto* ownerAnimations = FindCharacterAnimation(ownerState, ownerRecord.stableEntityId);
    if (ownerAnimations == nullptr ||
        ownerAnimations->clips[CharacterAnimationIndex(CharacterAnimationSemantic::Idle)].size() != 2 ||
        !RequestCharacterAnimation(ownerScene, ownerState, *ownerAnimations,
            CharacterAnimationSemantic::Idle) ||
        ownerAnimations->resolvedClipName != "mutant breathing idle")
        return Fail("Mutant must choose real breathing/idle, never bind-pose/turn/jump");

    const wi::ecs::Entity idle = AddClip(scene, character, "Idle_Breathe");
    const wi::ecs::Entity attack = AddClip(scene, character, "Claw_Attack_01");
    const wi::ecs::Entity attackTwo = AddClip(scene, character, "Claw_Attack_02");
    const wi::ecs::Entity walk = AddClip(scene, character, "Walk_Forward");
    const wi::ecs::Entity run = AddClip(scene, character, "Run_Forward");

    RuntimeCharacterSystemState characters;
    RuntimeCharacterRecord authored;
    authored.stableEntityId = "00000000-0000-4000-8000-000000000006";
    authored.entity = character;
    characters.characters.push_back(authored);

    RuntimeCombatState combat;
    CharacterCombatRecord combatRecord;
    combatRecord.characterId = authored.stableEntityId;
    combatRecord.entity = character;
    combat.characters.push_back(combatRecord);

    RuntimeCharacterAnimationState state;
    std::string error;
    if (!InitializeRuntimeCharacterAnimations(
            scene, characters, combat, state, error))
    {
        return Fail("animation initialization: " + error);
    }
    auto* record = FindCharacterAnimation(state, authored.stableEntityId);
    if (record == nullptr ||
        record->clips[CharacterAnimationIndex(CharacterAnimationSemantic::Attack)].size() != 2)
    {
        return Fail("multiple attack variants were not resolved");
    }

    // Runtime starts with semantic Idle but NO active clip. The first real
    // frame must start a native clip, not mistake the enum default for playback.
    RuntimeCharacterDecisionState initialDecisions;
    CharacterDecisionRecord initialDecision;
    initialDecision.characterId = authored.stableEntityId;
    initialDecision.intent = CharacterIntent::Idle;
    initialDecisions.characters.push_back(initialDecision);
    UpdateRuntimeCharacterAnimations(
        scene, characters, initialDecisions, combat, state);
    if (record->activeClip != idle ||
        !scene.animations.GetComponent(idle)->IsPlaying())
        return Fail("initial Idle default never starts a native animation");
    (void)renegade::bridge::StopAnimation(scene, idle);
    UpdateRuntimeCharacterAnimations(
        scene, characters, initialDecisions, combat, state);
    if (!scene.animations.GetComponent(idle)->IsPlaying())
        return Fail("stopped active loop never restarts during active AI intent");

    if (!RequestCharacterAnimation(
            scene, state, *record, CharacterAnimationSemantic::Idle))
    {
        return Fail("idle playback request");
    }
    auto* idleAnimation = scene.animations.GetComponent(idle);
    if (idleAnimation == nullptr || !idleAnimation->IsPlaying() ||
        !idleAnimation->IsLooped() || idleAnimation->timer != idleAnimation->start)
    {
        return Fail("idle must use looped native playback from authored start");
    }

    if (!RequestCharacterAnimation(
            scene, state, *record, CharacterAnimationSemantic::Attack))
    {
        return Fail("attack playback request");
    }
    auto* attackAnimation = scene.animations.GetComponent(record->activeClip);
    if (attackAnimation == nullptr || !attackAnimation->IsPlaying() ||
        !attackAnimation->IsPlayingOnce() ||
        record->activeSemantic != CharacterAnimationSemantic::Attack)
    {
        return Fail("attack must use native play-once playback");
    }
    const wi::ecs::Entity firstAttack = record->activeClip;
    const auto attackSequenceAfterFirst = record->oneShotVariantSequence[
        CharacterAnimationIndex(CharacterAnimationSemantic::Attack)];

    // A second request while the first one-shot plays must not switch clips.
    if (!RequestCharacterAnimation(
            scene, state, *record, CharacterAnimationSemantic::Attack) ||
        record->activeClip != firstAttack ||
        record->oneShotVariantSequence[
            CharacterAnimationIndex(CharacterAnimationSemantic::Attack)] != attackSequenceAfterFirst)
        return Fail("second attack request interrupted the active one-shot or consumed a variant");
    (void)renegade::bridge::StopAnimation(scene, firstAttack);
    if (!RequestCharacterAnimation(
            scene, state, *record, CharacterAnimationSemantic::Attack))
        return Fail("second attack variant request after completion");
    auto* attackTwoAnimation = scene.animations.GetComponent(record->activeClip);
    if (attackTwoAnimation == nullptr || !attackTwoAnimation->IsPlaying() ||
        record->activeClip == firstAttack)
    {
        return Fail("attack variants must avoid immediate repeats");
    }

    RuntimeCharacterDecisionState decisions;
    CharacterDecisionRecord chasing;
    chasing.characterId = authored.stableEntityId;
    chasing.intent = CharacterIntent::Chase;
    decisions.characters.push_back(chasing);
    combat.characters.front().shotsFired = 1;
    UpdateRuntimeCharacterAnimations(scene, characters, decisions, combat, state);
    if (record->activeSemantic != CharacterAnimationSemantic::Attack)
        return Fail("a combat shot must request a native attack clip");
    const wi::ecs::Entity shotClip = record->activeClip;
    UpdateRuntimeCharacterAnimations(scene, characters, decisions, combat, state);
    if (record->activeClip != shotClip ||
        record->activeSemantic != CharacterAnimationSemantic::Attack)
        return Fail("attack play-once interrupted by chase on next frame");
    (void)renegade::bridge::StopAnimation(scene, shotClip);
    UpdateRuntimeCharacterAnimations(scene, characters, decisions, combat, state);
    const auto* runAnimation = scene.animations.GetComponent(run);
    if (record->activeSemantic != CharacterAnimationSemantic::Run ||
        record->activeClip != run || runAnimation == nullptr || !runAnimation->IsLooped())
        return Fail("chase must run after authored attack finishes");
    decisions.characters.front().intent = CharacterIntent::Patrol;
    UpdateRuntimeCharacterAnimations(scene, characters, decisions, combat, state);
    if (record->activeSemantic != CharacterAnimationSemantic::Locomotion ||
        record->activeClip != walk)
        return Fail("patrol must use authored walk clip");
    record->clips[CharacterAnimationIndex(CharacterAnimationSemantic::Run)].clear();
    decisions.characters.front().intent = CharacterIntent::Chase;
    UpdateRuntimeCharacterAnimations(scene, characters, decisions, combat, state);
    if (record->activeSemantic != CharacterAnimationSemantic::Run ||
        record->activeClip != walk || !scene.animations.GetComponent(walk)->IsLooped())
        return Fail("run must safely fall back to walk when no run clip exists");

    const std::uint64_t missingBefore = state.missingRequests;

    if (RequestCharacterAnimation(
            scene, state, *record, CharacterAnimationSemantic::Reload) ||
        state.missingRequests != missingBefore + 1)
    {
        return Fail("missing optional animation must fail safely");
    }

    // Author-defined actions override filename guesses and never play a bind pose.
    wi::scene::Scene explicitScene;
    const auto explicitActor = explicitScene.Entity_CreateTransform("AssignedCharacter");
    const auto pose = AddClip(explicitScene, explicitActor, "Mutant");
    explicitScene.animations.GetComponent(pose)->end = 0.0333333f;
    const auto explicitAttack = AddClip(explicitScene, explicitActor, "LooksLikeIdle");
    const auto explicitRun = AddClip(explicitScene, explicitActor, "LooksLikePunch");
    for (const auto& pair : {std::pair{pose, "Unassigned"},
            std::pair{explicitAttack, "Attack"}, std::pair{explicitRun, "Run"}})
        explicitScene.metadatas.Create(pair.first).string_values.set(
            renegade::bridge::CreatorCharacterAnimationActionMetadataKey, pair.second);
    RuntimeCharacterSystemState explicitCharacters;
    RuntimeCharacterRecord explicitRecord;
    explicitRecord.stableEntityId = "00000000-0000-4000-8000-000000000016";
    explicitRecord.entity = explicitActor;
    explicitCharacters.characters.push_back(explicitRecord);
    RuntimeCombatState explicitCombat;
    CharacterCombatRecord explicitCombatRecord;
    explicitCombatRecord.characterId = explicitRecord.stableEntityId;
    explicitCombatRecord.entity = explicitActor;
    explicitCombat.characters.push_back(explicitCombatRecord);
    RuntimeCharacterAnimationState explicitState;
    if (!InitializeRuntimeCharacterAnimations(explicitScene, explicitCharacters,
            explicitCombat, explicitState, error))
        return Fail("explicit action setup: " + error);
    auto* assigned = FindCharacterAnimation(explicitState, explicitRecord.stableEntityId);
    if (assigned == nullptr ||
        !assigned->clips[CharacterAnimationIndex(CharacterAnimationSemantic::Idle)].empty() ||
        assigned->clips[CharacterAnimationIndex(CharacterAnimationSemantic::Attack)].size() != 1 ||
        assigned->clips[CharacterAnimationIndex(CharacterAnimationSemantic::Run)].size() != 1 ||
        !RequestCharacterAnimation(explicitScene, explicitState, *assigned,
            CharacterAnimationSemantic::Attack) || assigned->activeClip != explicitAttack)
        return Fail("authored actions must override names; reference pose is not Idle");

    std::cout << "AI06_PASS semantic-native-playback-variants-action-run-fallback\n";
    return 0;
}
