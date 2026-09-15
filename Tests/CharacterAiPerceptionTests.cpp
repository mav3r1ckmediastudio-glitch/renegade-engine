#include "RuntimeCharacterPerception.h"

#include <WickedEngine.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool Near(const float lhs, const float rhs)
    {
        return std::abs(lhs - rhs) < 0.0001f;
    }
}

int main()
{
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    RuntimeCharacterSystemState characters;
    RuntimeCharacterRecord guard;
    guard.stableEntityId = GenerateStableId();
    guard.authoring.factionId = "Enemy";
    guard.tuning = CharacterTuning{};
    guard.tuning.visualReactionSeconds = 0.40f;
    guard.tuning.audioReactionSeconds = 0.30f;
    guard.tuning.memorySeconds = 1.0f;
    guard.tuning.suspicionGain = 100.0f;
    guard.tuning.suspicionDecay = 0.0f;
    guard.tuning.alertThreshold = 20.0f;
    guard.tuning.combatThreshold = 40.0f;
    guard.tuning.alertness = 0.5f;
    characters.characters.push_back(guard);

    RuntimeCharacterPerceptionState perception;
    std::string error;
    if (!InitializeRuntimeCharacterPerception(characters, perception, error) ||
        perception.characters.size() != 1)
    {
        return Fail("AI-03 perception state did not initialize");
    }
    auto* cognition = FindCharacterCognition(perception, guard.stableEntityId);
    if (cognition == nullptr)
        return Fail("AI-03 stable-ID cognition lookup failed");

    const auto hostile = DefaultFactionRelationship("Enemy", "Player");
    if (hostile != FactionRelationship::Hostile ||
        FindCharacterMemory(*cognition, RuntimePlayerKnowledgeId) != nullptr ||
        cognition->suspicion != 0.0f ||
        cognition->awareness != AwarenessState::Unaware)
    {
        return Fail("faction relationship leaked knowledge before perception");
    }

    VisualObservation seen;
    seen.visible = true;
    seen.position = XMFLOAT3(10.0f, 0.0f, 2.0f);
    seen.velocity = XMFLOAT3(1.0f, 0.0f, 0.0f);

    ApplyVisualObservation(
        *cognition, guard.tuning, RuntimePlayerKnowledgeId, "Player", hostile, seen, 0.20f);
    if (FindCharacterMemory(*cognition, RuntimePlayerKnowledgeId) != nullptr)
        return Fail("visual reaction latency was bypassed");

    ApplyVisualObservation(
        *cognition, guard.tuning, RuntimePlayerKnowledgeId, "Player", hostile, seen, 0.20f);
    const CharacterMemoryRecord* memory =
        FindCharacterMemory(*cognition, RuntimePlayerKnowledgeId);
    if (memory == nullptr || memory->source != KnowledgeSource::Seen ||
        !memory->directSight || !memory->hasPosition ||
        !Near(memory->lastKnownPosition.x, 10.0f))
    {
        return Fail("legitimate visual observation did not create Seen memory");
    }

    ApplyVisualObservation(
        *cognition, guard.tuning, RuntimePlayerKnowledgeId, "Player", hostile, seen, 0.20f);
    ApplyVisualObservation(
        *cognition, guard.tuning, RuntimePlayerKnowledgeId, "Player", hostile, seen, 0.20f);
    if (cognition->awareness != AwarenessState::Combat)
        return Fail("hostile direct sight did not progress awareness to Combat");

    perception_detail::DecayCharacterState(*cognition, guard.tuning, 0.01f);
    memory = FindCharacterMemory(*cognition, RuntimePlayerKnowledgeId);
    if (memory == nullptr || !memory->directSight)
        return Fail("render-frame decay fabricated sight loss between cognition samples");

    VisualObservation hidden;
    hidden.visible = false;
    hidden.position = XMFLOAT3(999.0f, 999.0f, 999.0f);
    hidden.velocity = XMFLOAT3(50.0f, 0.0f, 0.0f);
    ApplyVisualObservation(
        *cognition, guard.tuning, RuntimePlayerKnowledgeId, "Player", hostile, hidden, 0.20f);
    memory = FindCharacterMemory(*cognition, RuntimePlayerKnowledgeId);
    if (memory == nullptr || memory->directSight ||
        !Near(memory->lastKnownPosition.x, 10.0f) ||
        !Near(memory->lastKnownPosition.y, 0.0f) ||
        !Near(memory->lastKnownPosition.z, 2.0f) ||
        !Near(memory->lastKnownVelocity.x, 1.0f))
    {
        return Fail("hidden target coordinates refreshed last-known memory");
    }
    if (cognition->awareness != AwarenessState::Searching)
        return Fail("lost hostile Combat sight did not transition to Searching memory state");

    const XMFLOAT3 remembered = memory->lastKnownPosition;
    perception_detail::DecayCharacterState(*cognition, guard.tuning, 0.25f);
    memory = FindCharacterMemory(*cognition, RuntimePlayerKnowledgeId);
    if (memory == nullptr || !(memory->confidence < 1.0f) ||
        !Near(memory->lastKnownPosition.x, remembered.x))
    {
        return Fail("memory decay corrupted last-known information");
    }

    RuntimeCharacterPerceptionState hearingState;
    if (!InitializeRuntimeCharacterPerception(characters, hearingState, error))
        return Fail("could not initialize hearing fixture");
    auto* hearing = FindCharacterCognition(hearingState, guard.stableEntityId);
    if (hearing == nullptr)
        return Fail("hearing cognition fixture missing");

    const float nan = std::numeric_limits<float>::quiet_NaN();
    if (EmitSoundStimulus(
            hearingState, "bad\nsubject", "Player",
            XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0), 5.0f, 1.0f) != 0 ||
        EmitSoundStimulus(
            hearingState, RuntimePlayerKnowledgeId, "Player",
            XMFLOAT3(nan, 0, 0), XMFLOAT3(0, 0, 0), 5.0f, 1.0f) != 0)
    {
        return Fail("malformed external sound stimulus was accepted");
    }

    const std::uint64_t soundSequence = EmitSoundStimulus(
        hearingState,
        RuntimePlayerKnowledgeId,
        "Player",
        XMFLOAT3(4.0f, 0.0f, 0.0f),
        XMFLOAT3(0.0f, 0.0f, 1.0f),
        20.0f,
        1.0f,
        2.0f);
    if (soundSequence == 0)
        return Fail("valid explicit sound stimulus was rejected");
    ProcessAudibleSound(guard, *hearing, XMFLOAT3(0, 0, 0), hearingState);
    if (FindCharacterMemory(*hearing, RuntimePlayerKnowledgeId) != nullptr)
        return Fail("hearing reaction latency was bypassed");
    hearingState.elapsedSeconds = guard.tuning.audioReactionSeconds - 0.01f;
    ProcessAudibleSound(guard, *hearing, XMFLOAT3(0, 0, 0), hearingState);
    if (FindCharacterMemory(*hearing, RuntimePlayerKnowledgeId) != nullptr)
        return Fail("sound was remembered before reaction delay elapsed");
    hearingState.elapsedSeconds = guard.tuning.audioReactionSeconds + 0.01f;
    ProcessAudibleSound(guard, *hearing, XMFLOAT3(0, 0, 0), hearingState);
    memory = FindCharacterMemory(*hearing, RuntimePlayerKnowledgeId);
    if (memory == nullptr || memory->source != KnowledgeSource::Heard ||
        memory->directSight || !Near(memory->lastKnownPosition.x, 4.0f))
    {
        return Fail("explicit sound stimulus did not create legitimate Heard memory");
    }

    // Repeated louder sounds must not continually replace the sound already in
    // the reaction window. This is essential for sprint footsteps and automatic
    // weapons: the first audible event must still mature into knowledge.
    RuntimeCharacterPerceptionState repeatedState;
    if (!InitializeRuntimeCharacterPerception(characters, repeatedState, error))
        return Fail("could not initialize repeated-sound fixture");
    auto* repeated = FindCharacterCognition(repeatedState, guard.stableEntityId);
    if (repeated == nullptr)
        return Fail("repeated-sound cognition fixture missing");
    const std::uint64_t firstSound = EmitSoundStimulus(
        repeatedState, "first-step", "Player",
        XMFLOAT3(8, 0, 0), XMFLOAT3(0, 0, 0), 20.0f, 0.7f, 2.0f);
    ProcessAudibleSound(guard, *repeated, XMFLOAT3(0, 0, 0), repeatedState);
    if (firstSound == 0 || repeated->pendingSoundSequence != firstSound)
        return Fail("first audible sound did not become the pending reaction");
    repeatedState.elapsedSeconds = 0.20f;
    const std::uint64_t newerSound = EmitSoundStimulus(
        repeatedState, "newer-step", "Player",
        XMFLOAT3(2, 0, 0), XMFLOAT3(0, 0, 0), 20.0f, 1.0f, 2.0f);
    ProcessAudibleSound(guard, *repeated, XMFLOAT3(0, 0, 0), repeatedState);
    if (newerSound == 0 || repeated->pendingSoundSequence != firstSound)
        return Fail("newer sound restarted an in-flight hearing reaction");
    repeatedState.elapsedSeconds = guard.tuning.audioReactionSeconds + 0.01f;
    ProcessAudibleSound(guard, *repeated, XMFLOAT3(0, 0, 0), repeatedState);
    if (FindCharacterMemory(*repeated, "first-step") == nullptr ||
        repeated->lastHeardSoundSequence != firstSound)
    {
        return Fail("repeated sounds starved the first legitimate hearing reaction");
    }

    RuntimeCharacterPerceptionState damageState;
    if (!InitializeRuntimeCharacterPerception(characters, damageState, error) ||
        !ReportDamageStimulus(
            characters,
            damageState,
            guard.stableEntityId,
            RuntimePlayerKnowledgeId,
            "Player",
            XMFLOAT3(7.0f, 0.0f, 3.0f),
            XMFLOAT3(0.0f, 0.0f, 0.0f),
            15.0f,
            error))
    {
        return Fail("damage attribution stimulus failed");
    }
    const auto* damaged = FindCharacterCognition(damageState, guard.stableEntityId);
    memory = damaged == nullptr
        ? nullptr
        : FindCharacterMemory(*damaged, RuntimePlayerKnowledgeId);
    if (memory == nullptr || memory->source != KnowledgeSource::DamagedBy ||
        !Near(memory->lastKnownPosition.x, 7.0f) || damageState.damageStimuli != 1)
    {
        return Fail("damage attribution did not create legitimate memory");
    }

    RuntimeCharacterPerceptionState bounded;
    for (std::size_t index = 0; index < MaxSoundStimuli + 12; ++index)
    {
        if (EmitSoundStimulus(
                bounded,
                "noise-" + std::to_string(index),
                "Neutral",
                XMFLOAT3(static_cast<float>(index), 0, 0),
                XMFLOAT3(0, 0, 0),
                5.0f,
                0.5f,
                2.0f) == 0)
        {
            return Fail("bounded sound fixture rejected valid stimulus");
        }
    }
    if (bounded.sounds.size() != MaxSoundStimuli || bounded.droppedSounds != 12)
        return Fail("sound stimulus queue was not bounded deterministically");

    RuntimeCharacterSystemState duplicates = characters;
    duplicates.characters.push_back(guard);
    RuntimeCharacterPerceptionState duplicateState;
    if (InitializeRuntimeCharacterPerception(duplicates, duplicateState, error) ||
        !duplicateState.characters.empty())
    {
        return Fail("duplicate Character stable identities did not fail closed");
    }

    const float staggerBeforeReset = perception.characters.front().nextCognitionTime;
    ResetRuntimeCharacterPerception(perception);
    if (!perception.characters.empty() || !perception.sounds.empty() ||
        perception.elapsedSeconds != 0.0f || perception.damageStimuli != 0 ||
        !InitializeRuntimeCharacterPerception(characters, perception, error) ||
        !Near(perception.characters.front().nextCognitionTime, staggerBeforeReset))
    {
        return Fail("AI-03 transient reset/reinitialization was not deterministic");
    }

    std::cout << "AI-03 perception/memory tests passed\n";
    return 0;
}
