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
        channel.path = wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION;
        animation.channels.push_back(channel);
        return entity;
    }
}

int main()
{
    using namespace renegade::runtime;

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

    // A second request while the first one-shot plays must not switch clips.
    if (!RequestCharacterAnimation(
            scene, state, *record, CharacterAnimationSemantic::Attack) ||
        record->activeClip != firstAttack)
        return Fail("second attack request interrupted the active one-shot");
    (void)renegade::bridge::StopAnimation(scene, firstAttack);
    if (!RequestCharacterAnimation(
            scene, state, *record, CharacterAnimationSemantic::Attack))
        return Fail("second attack variant request after completion");
    auto* attackTwoAnimation = scene.animations.GetComponent(record->activeClip);
    if (attackTwoAnimation == nullptr || !attackTwoAnimation->IsPlaying() ||
        record->activeClip == firstAttack)
    {
        return Fail("attack variants must cycle deterministically");
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

    const auto hit = AddClip(scene, character, "Hit");
    record->clips[CharacterAnimationIndex(CharacterAnimationSemantic::Hit)].push_back({hit, "Hit"});
    combat.characters.front().health = 100.0f;
    record->observedHealth = 100.0f;
    ++combat.characters.front().damageTaken;
    UpdateRuntimeCharacterAnimations(scene, characters, decisions, combat, state, 0.04f);
    if (record->activeClip != hit || record->blendDuration <= 0.0f)
        return Fail("damage must interrupt locomotion with a short native fade");
    ++combat.characters.front().shotsFired;
    UpdateRuntimeCharacterAnimations(scene, characters, decisions, combat, state, 0.04f);
    if (record->activeClip != hit)
        return Fail("attack event must not interrupt an active hit reaction");
    combat.characters.front().dead = true;
    UpdateRuntimeCharacterAnimations(scene, characters, decisions, combat, state, 0.05f);
    if (record->activeClip != wi::ecs::INVALID_ENTITY ||
        record->activeSemantic != CharacterAnimationSemantic::Death ||
        !record->blendClips.empty() || scene.animations.GetComponent(hit)->IsPlaying())
        return Fail("unassigned Death must stop owned playback without a looping attack");
    const auto missingAtDeath = state.missingRequests;
    UpdateRuntimeCharacterAnimations(scene, characters, decisions, combat, state, 0.05f);
    if (state.missingRequests != missingAtDeath)
        return Fail("unassigned terminal Death must not retry every frame");
    // A finished action must blend before native playback pauses.
    combat.characters.front().dead = false;
    record->activeSemantic = CharacterAnimationSemantic::Idle;
    record->baseIdle = idle;
    record->activeClip = wi::ecs::INVALID_ENTITY;
    if (!RequestCharacterAnimation(scene, state, *record, CharacterAnimationSemantic::Attack))
        return Fail("tail overlap setup");
    const auto outgoingAttack = record->activeClip;
    auto* tail = scene.animations.GetComponent(outgoingAttack);
    tail->timer = tail->end - 0.10f;
    decisions.characters.front().intent = CharacterIntent::Idle;
    UpdateRuntimeCharacterAnimations(scene, characters, decisions, combat, state, 0.016f);
    if (record->activeClip != idle || record->blendDuration <= 0 ||
        !scene.animations.GetComponent(outgoingAttack)->IsPlaying())
        return Fail("action exit must overlap a still-playing outgoing clip");

    // Idle gestures are occasional one-shots, then return to the base loop.
    const auto gesture = AddClip(scene, character, "Gesture");
    record->clips[CharacterAnimationIndex(CharacterAnimationSemantic::Idle)].push_back({gesture, "Gesture"});
    record->idleElapsed = 12.0f;
    UpdateRuntimeCharacterAnimations(scene, characters, decisions, combat, state, 0.016f);
    if (record->activeClip != gesture || !record->idleVariation ||
        !scene.animations.GetComponent(gesture)->IsPlayingOnce())
        return Fail("idle variation must be occasional and play once");
    scene.animations.GetComponent(gesture)->timer = 3.90f;
    UpdateRuntimeCharacterAnimations(scene, characters, decisions, combat, state, 0.016f);
    if (record->activeClip != idle || record->idleVariation ||
        !scene.animations.GetComponent(idle)->IsLooped() ||
        !scene.animations.GetComponent(gesture)->IsPlaying())
        return Fail("idle variation must fade back before it stops");

    // Replaying a sole attack still needs two independent native timers.
    record->clips[CharacterAnimationIndex(CharacterAnimationSemantic::Attack)] = {{attack, "Attack"}};
    if (!RequestCharacterAnimation(scene, state, *record, CharacterAnimationSemantic::Attack)) return Fail("replay setup");
    AdvanceCharacterAnimationBlend(scene, *record, 0.20f);
    scene.animations.GetComponent(attack)->timer = 3.90f;
    if (!RequestCharacterAnimation(scene, state, *record, CharacterAnimationSemantic::Attack, true) ||
        record->blendClips.size() != 2 || record->replayOutgoing == wi::ecs::INVALID_ENTITY ||
        scene.animations.GetComponent(record->replayOutgoing)->timer != 3.90f ||
        scene.animations.GetComponent(attack)->timer != 2.0f)
        return Fail("single attack repeat needs an independent outgoing timer");

    ResetRuntimeCharacterAnimations(state);
    if (!state.characters.empty()) return Fail("reset retained transient blend state");
    std::cout << "AI06_PASS semantic-native-playback-variants-action-run-fallback-hit-death\n";
    return 0;
}
