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

    wi::scene::Scene scene;
    const wi::ecs::Entity character = scene.Entity_CreateTransform("Mutant");

    if (InferCharacterAnimationSemantic("Mutant_Attack_02") !=
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

    if (!RequestCharacterAnimation(
            scene, state, *record, CharacterAnimationSemantic::Attack))
    {
        return Fail("second attack variant request");
    }
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

    if (RequestCharacterAnimation(
            scene, state, *record, CharacterAnimationSemantic::Reload) ||
        state.missingRequests != missingBefore + 1)
    {
        return Fail("missing optional animation must fail safely");
    }

    std::cout << "AI06_PASS semantic-native-playback-variants-action-run-fallback\n";
    return 0;
}
