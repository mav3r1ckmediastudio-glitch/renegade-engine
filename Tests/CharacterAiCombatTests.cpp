#include "RuntimeCombatDecision.h"

#include "renegade/bridge/GameplayEventService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/WeaponCombatService.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
    int Fail(const std::string& message)
    {
        std::cerr << "AI05_FAIL: " << message << '\n';
        return 1;
    }

    bool ContainsIntent(
        const std::vector<renegade::runtime::CharacterIntentScore>& scores,
        const renegade::runtime::CharacterIntent intent)
    {
        for (const auto& score : scores)
        {
            if (score.intent == intent)
                return true;
        }
        return false;
    }
}

int main()
{
    using namespace renegade;
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    // Weapon semantics are reusable and fail closed when governed metadata is
    // malformed. Untagged assigned entities retain creator-friendly defaults
    // from the Character combat style.
    wi::scene::Scene scene;
    const wi::ecs::Entity characterEntity = scene.Entity_CreateTransform(
        "AI05 Character", XMFLOAT3(0.0f, 0.0f, 0.0f));
    auto& nativeCharacter = scene.characters.Create(characterEntity);
    nativeCharacter.health = 100;
    nativeCharacter.SetActive(true);

    const wi::ecs::Entity weaponEntity = scene.Entity_CreateTransform(
        "AI05 Rifle", XMFLOAT3(0.0f, 0.0f, 0.0f));
    (void)AssignPersistentEntityId(scene, weaponEntity);

    WeaponAiDescriptor descriptor;
    std::string error;
    if (!CaptureWeaponAiDescriptor(
            scene, weaponEntity, CombatStyle::Ranged, descriptor, error))
        return Fail("default ranged descriptor: " + error);
    if (descriptor.style != WeaponAiStyle::Ranged ||
        descriptor.magazineSize != 30 || descriptor.maxRange < 30.0f)
        return Fail("default ranged descriptor values");

    auto& weaponMetadata = scene.metadatas.Create(weaponEntity);
    weaponMetadata.string_values.set(WeaponAiMetadataKey, "1");
    weaponMetadata.int_values.set(WeaponAiSchemaMetadataKey, WeaponAiSchemaVersion);
    weaponMetadata.float_values.set(WeaponAiDamageMetadataKey, 27.0f);
    weaponMetadata.int_values.set(WeaponAiMagazineSizeMetadataKey, 12);
    weaponMetadata.int_values.set(WeaponAiReserveAmmoMetadataKey, 24);
    if (!CaptureWeaponAiDescriptor(
            scene, weaponEntity, CombatStyle::Ranged, descriptor, error))
        return Fail("governed descriptor capture: " + error);
    if (std::abs(descriptor.damage - 27.0f) > 0.001f ||
        descriptor.magazineSize != 12 || descriptor.reserveAmmo != 24)
        return Fail("governed descriptor overrides");

    weaponMetadata.int_values.set(WeaponAiSchemaMetadataKey, 999);
    if (CaptureWeaponAiDescriptor(
            scene, weaponEntity, CombatStyle::Ranged, descriptor, error))
        return Fail("unsupported weapon schema must fail closed");
    weaponMetadata.int_values.set(WeaponAiSchemaMetadataKey, WeaponAiSchemaVersion);

    RuntimeCharacterSystemState characterSystem;
    RuntimeCharacterRecord character;
    character.stableEntityId = "00000000-0000-4000-8000-000000000005";
    character.entity = characterEntity;
    character.authoring.role = CharacterRole::Soldier;
    character.authoring.factionId = "Enemy";
    character.authoring.combatStyle = CombatStyle::Ranged;
    character.authoring.canFlee = true;
    character.authoring.canSurrender = true;
    character.authoring.autonomous = true;
    character.references.weaponEntity = weaponEntity;
    character.tuning = ResolveCharacterTuning(character.authoring);
    characterSystem.characters.push_back(character);

    RuntimeCombatState combatState;
    if (!InitializeRuntimeCombat(scene, characterSystem, combatState, error))
        return Fail("combat initialization: " + error);
    auto* combat = FindCharacterCombat(combatState, character.stableEntityId);
    if (combat == nullptr || combat->magazineAmmo != 12 || combat->reserveAmmo != 24)
        return Fail("combat ammo initialization");

    RuntimeCharacterPerceptionState perception;
    CharacterCognitionRecord cognition;
    cognition.characterId = character.stableEntityId;
    cognition.awareness = AwarenessState::Combat;
    cognition.suspicion = 100.0f;
    CharacterMemoryRecord memory;
    memory.subjectId = RuntimePlayerKnowledgeId;
    memory.subjectFactionId = "Player";
    memory.source = KnowledgeSource::Seen;
    memory.lastKnownPosition = XMFLOAT3(10.0f, 0.0f, 0.0f);
    memory.confidence = 1.0f;
    memory.threat = 1.0f;
    memory.hasPosition = true;
    memory.hostile = true;
    memory.directSight = true;
    cognition.memories.push_back(memory);
    perception.characters.push_back(cognition);

    RefreshRuntimeCombat(scene, characterSystem, perception, combatState, 0.0f);
    if (!combat->hasDirectHostileTarget ||
        !std::isfinite(combat->targetDistance) || combat->targetDistance < 9.0f)
        return Fail("combat range derives from AI-03 memory");

    CharacterDecisionRecord decision;
    decision.characterId = character.stableEntityId;
    decision.intent = CharacterIntent::Chase;
    decision.previousIntent = CharacterIntent::Chase;
    decision.topScores[0] = {CharacterIntent::Chase, 98.0f};
    const auto attackScores = ScoreCharacterCombatIntents(
        character, perception.characters.front(), decision, *combat);
    if (!ContainsIntent(attackScores, CharacterIntent::Attack))
        return Fail("direct in-range hostile target should score Attack");

    // Deterministic bounded accuracy must be reproducible for the same stable
    // identity and shot sequence, with no frame RNG dependency.
    const float chance = ComputeCombatHitChance(
        character.tuning, combat->weapon, combat->targetDistance);
    if (!(chance >= 0.02f && chance <= 0.98f))
        return Fail("bounded combat hit chance");
    if (DeterministicCombatUnit(character.stableEntityId, 1) !=
        DeterministicCombatUnit(character.stableEntityId, 1))
        return Fail("deterministic combat sample");

    bridge::GameplayEventService events;
    const CombatEventEmitter emitter = [&events](
        bridge::GameplayEvent event, std::string& eventError)
    {
        return events.Enqueue(std::move(event), eventError);
    };

    CombatFireResult fire;
    const int ammoBefore = combat->magazineAmmo;
    if (!TryFireAtRuntimePlayer(
            scene,
            character,
            perception.characters.front(),
            *combat,
            perception,
            combatState,
            emitter,
            fire) || !fire.fired)
        return Fail("in-range Attack should fire");
    if (combat->magazineAmmo != ammoBefore - 1 || events.Size() == 0)
        return Fail("fire must consume ammo and publish GameplayEventService event");
    if (perception.sounds.empty())
        return Fail("weapon fire must create legitimate AI-03 sound stimulus");

    // Empty magazines choose Reload and transfer bounded reserve ammunition
    // only after the authored reload duration completes.
    combat->fireCooldownSeconds = 0.0f;
    combat->magazineAmmo = 0;
    combat->reserveAmmo = 7;
    auto reloadScores = ScoreCharacterCombatIntents(
        character, perception.characters.front(), decision, *combat);
    if (!ContainsIntent(reloadScores, CharacterIntent::Reload))
        return Fail("empty magazine should score Reload");
    if (!BeginCombatReload(*combat) || combat->reloadRemainingSeconds <= 0.0f)
        return Fail("reload start");
    RefreshRuntimeCombat(
        scene, characterSystem, perception, combatState,
        combat->weapon.reloadSeconds + 0.1f);
    if (combat->magazineAmmo != 7 || combat->reserveAmmo != 0)
        return Fail("reload ammo transfer");

    // Native Character health is the accepted NPC health seam. Damage writes
    // through the reusable combat service and mirrors the Wicked component.
    bool died = false;
    if (!ApplyCombatDamage(
            scene, combatState, character.stableEntityId, 25.0f, died) || died)
        return Fail("NPC combat damage");
    if (nativeCharacter.health != 75 || std::abs(combat->health - 75.0f) > 0.01f)
        return Fail("NPC native health synchronization");

    // Low-health timid/civilian profiles reason about escape/surrender rather
    // than blindly attacking. Escape goals are derived away from remembered
    // legitimate last-known information, not a hidden live player transform.
    character.authoring.role = CharacterRole::Civilian;
    character.authoring.personality = PersonalityPreset::Timid;
    character.authoring.canFlee = true;
    character.authoring.canSurrender = true;
    character.tuning = ResolveCharacterTuning(character.authoring);
    combat->health = 5.0f;
    combat->maxHealth = 100.0f;
    combat->magazineAmmo = 0;
    combat->reserveAmmo = 0;
    combat->weapon = DefaultWeaponAiDescriptor(CombatStyle::Ranged);
    const auto escapeScores = ScoreCharacterCombatIntents(
        character, perception.characters.front(), decision, *combat);
    if (!ContainsIntent(escapeScores, CharacterIntent::Flee) &&
        !ContainsIntent(escapeScores, CharacterIntent::Surrender))
        return Fail("low-health timid character should flee or surrender");

    XMFLOAT3 escapeGoal;
    if (!ResolveCombatEscapeGoal(
            nativeCharacter,
            perception.characters.front(),
            CharacterIntent::Flee,
            escapeGoal))
        return Fail("flee goal resolution");
    const XMFLOAT3 position = nativeCharacter.GetPositionInterpolated();
    if (!(escapeGoal.x < position.x))
        return Fail("flee goal must move away from remembered hostile position");

    std::cout << "AI05_PASS weapon-health-ammo-reload-range-accuracy-retreat-flee-surrender-events\n";
    return 0;
}
