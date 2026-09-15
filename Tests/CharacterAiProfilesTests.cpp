#include "renegade/bridge/CharacterProfileService.h"
#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/FactionService.h"
#include "renegade/bridge/IdentityService.h"
#include "RuntimeCharacterSystem.h"

#include <WickedEngine.h>

#include <cmath>
#include <iostream>
#include <memory>
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

    wi::ecs::Entity CreateTransformEntity(
        wi::scene::Scene& scene,
        const char* name)
    {
        const wi::ecs::Entity entity = wi::ecs::CreateEntity();
        scene.names.Create(entity) = name;
        scene.transforms.Create(entity).UpdateTransform();
        return entity;
    }
}

int main()
{
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    CharacterAuthoringSettings cautiousGuard;
    cautiousGuard.role = CharacterRole::Guard;
    cautiousGuard.personality = PersonalityPreset::Cautious;
    cautiousGuard.skill = SkillPreset::Trained;
    cautiousGuard.awareness = AwarenessPreset::Normal;

    CharacterAuthoringSettings aggressiveSoldier;
    aggressiveSoldier.role = CharacterRole::Soldier;
    aggressiveSoldier.personality = PersonalityPreset::Aggressive;
    aggressiveSoldier.skill = SkillPreset::Veteran;
    aggressiveSoldier.awareness = AwarenessPreset::Alert;

    const CharacterTuning guardTuning = ResolveCharacterTuning(cautiousGuard);
    const CharacterTuning soldierTuning = ResolveCharacterTuning(aggressiveSoldier);
    if (!(soldierTuning.aggression > guardTuning.aggression) ||
        !(soldierTuning.accuracy > guardTuning.accuracy) ||
        !(soldierTuning.communicationRange > guardTuning.communicationRange))
    {
        return Fail("profile composition did not distinguish soldier from cautious guard");
    }

    CharacterAdvancedOverrides authoredOverrides;
    authoredOverrides.visionDistance = 123.0f;
    authoredOverrides.aggression = 0.91f;
    authoredOverrides.searchSeconds = 44.0f;
    authoredOverrides.communicationRange = 77.0f;
    const CharacterTuning overridden = ResolveCharacterTuning(
        cautiousGuard, authoredOverrides);
    if (!Near(overridden.visionDistance, 123.0f) ||
        !Near(overridden.aggression, 0.91f) ||
        !Near(overridden.searchSeconds, 44.0f) ||
        !Near(overridden.communicationRange, 77.0f))
    {
        return Fail("explicit advanced overrides did not win after profile composition");
    }

    const std::string serialized = SerializeCharacterAdvancedOverrides(authoredOverrides);
    CharacterAdvancedOverrides roundTripped;
    std::string error;
    if (!DeserializeCharacterAdvancedOverrides(serialized, roundTripped, error) ||
        roundTripped != authoredOverrides)
    {
        return Fail("advanced override serialization was not deterministic");
    }
    CharacterAdvancedOverrides malformed;
    if (DeserializeCharacterAdvancedOverrides("v99;vision=25", malformed, error) ||
        DeserializeCharacterAdvancedOverrides("v1;unknown=2", malformed, error))
    {
        return Fail("advanced override deserialization did not fail closed");
    }

    if (DefaultFactionRelationship("Player", "Enemy") != FactionRelationship::Hostile ||
        DefaultFactionRelationship("Friendly", "Player") != FactionRelationship::Friendly ||
        DefaultFactionRelationship("Civilian", "Enemy") != FactionRelationship::Suspicious ||
        DefaultFactionRelationship("Enemy", "Enemy") != FactionRelationship::Ally ||
        DefaultFactionRelationship("Merchants", "Bandits") != FactionRelationship::Neutral ||
        DefaultFactionRelationship("Merchants", "Merchants") != FactionRelationship::Ally)
    {
        return Fail("built-in/default faction relationship matrix is incorrect");
    }
    if (!ValidateFactionId("CreatorFaction", error) ||
        ValidateFactionId("", error) || ValidateFactionId(std::string(65, 'x'), error) ||
        ValidateFactionId(std::string("bad\nname"), error))
    {
        return Fail("faction ID validation is incorrect");
    }

    FactionRegistry factionRegistry;
    const std::size_t builtInFactionCount = BuiltInFactions().size();
    if (factionRegistry.Size() != builtInFactionCount ||
        !factionRegistry.Contains("Player") || !factionRegistry.Contains("Neutral"))
    {
        return Fail("faction registry did not seed the built-in factions");
    }
    if (!factionRegistry.Register("CreatorFaction", error) ||
        !factionRegistry.Register("CreatorFaction", error) ||
        !factionRegistry.Contains("CreatorFaction") ||
        factionRegistry.Size() != builtInFactionCount + 1)
    {
        return Fail("creator-defined faction registration was not deterministic");
    }
    factionRegistry.ResetToBuiltIns();
    if (factionRegistry.Size() != builtInFactionCount ||
        factionRegistry.Contains("CreatorFaction"))
    {
        return Fail("faction registry reset did not return to built-in state");
    }

    wi::scene::Scene scene;
    const wi::ecs::Entity patrol = CreateTransformEntity(scene, "AI02 Patrol Route Fixture");
    const wi::ecs::Entity weapon = CreateTransformEntity(scene, "AI02 Weapon Fixture");
    std::string identityError;
    if (!AssignNewPersistentEntityId(scene, patrol, identityError) ||
        !AssignNewPersistentEntityId(scene, weapon, identityError))
    {
        return Fail("could not seed AI-02 stable reference fixtures");
    }
    const StableId patrolId = PersistentEntityId(scene, patrol);
    const StableId weaponId = PersistentEntityId(scene, weapon);

    const wi::ecs::Entity guard = CreateTransformEntity(scene, "AI02 Guard");
    const wi::ecs::Entity soldier = CreateTransformEntity(scene, "AI02 Soldier");
    CommandService commands;
    if (!commands.Execute(std::make_unique<MakeCharacterCommand>(scene, guard)) ||
        !commands.Execute(std::make_unique<MakeCharacterCommand>(scene, soldier)))
    {
        return Fail("could not create AI-02 Character fixtures");
    }

    auto guardSettings = CaptureCharacterSettings(scene, guard);
    guardSettings.role = CharacterRole::Guard;
    guardSettings.personality = PersonalityPreset::Cautious;
    guardSettings.skill = SkillPreset::Trained;
    guardSettings.awareness = AwarenessPreset::Normal;
    guardSettings.factionId = "Enemy";
    guardSettings.patrolRouteEntityId = patrolId;
    guardSettings.weaponEntityId = weaponId;
    if (!commands.Execute(std::make_unique<SetCharacterSettingsCommand>(
            scene, guard, guardSettings)))
    {
        return Fail("could not author Guard profile inputs");
    }

    auto soldierSettings = CaptureCharacterSettings(scene, soldier);
    soldierSettings.role = CharacterRole::Soldier;
    soldierSettings.personality = PersonalityPreset::Aggressive;
    soldierSettings.skill = SkillPreset::Veteran;
    soldierSettings.awareness = AwarenessPreset::Alert;
    soldierSettings.factionId = "Player";
    if (!commands.Execute(std::make_unique<SetCharacterSettingsCommand>(
            scene, soldier, soldierSettings)))
    {
        return Fail("could not author Soldier profile inputs");
    }

    CharacterAdvancedOverrides guardOverrides;
    guardOverrides.visionDistance = 88.0f;
    guardOverrides.coverPreference = 0.93f;
    if (!commands.Execute(std::make_unique<SetCharacterAdvancedOverridesCommand>(
            scene, guard, guardOverrides)))
    {
        return Fail("advanced override command did not execute");
    }
    CharacterAdvancedOverrides capturedOverrides;
    if (!CaptureCharacterAdvancedOverrides(scene, guard, capturedOverrides, error) ||
        capturedOverrides != guardOverrides)
    {
        return Fail("advanced overrides did not persist in native Character metadata");
    }
    if (!commands.Undo() ||
        !CaptureCharacterAdvancedOverrides(scene, guard, capturedOverrides, error) ||
        HasAnyCharacterAdvancedOverride(capturedOverrides))
    {
        return Fail("advanced override Undo did not restore profile-driven state");
    }
    if (!commands.Redo() ||
        !CaptureCharacterAdvancedOverrides(scene, guard, capturedOverrides, error) ||
        capturedOverrides != guardOverrides)
    {
        return Fail("advanced override Redo did not restore authored values");
    }

    // Prove that the AI-02 payload survives the exact native entity archive seam
    // used by WISCENE. Runtime cognition is not serialized; only authoring is.
    wi::Archive characterSnapshot;
    characterSnapshot.SetReadModeAndResetPos(false);
    wi::ecs::EntitySerializer writeSerializer;
    scene.Entity_Serialize(characterSnapshot, writeSerializer, guard);

    wi::scene::Scene restoredCharacterScene;
    characterSnapshot.SetReadModeAndResetPos(true);
    wi::ecs::EntitySerializer readSerializer;
    const wi::ecs::Entity restoredGuard =
        restoredCharacterScene.Entity_Serialize(characterSnapshot, readSerializer);
    CharacterAdvancedOverrides restoredOverrides;
    if (restoredGuard == wi::ecs::INVALID_ENTITY ||
        !IsRenegadeCharacter(restoredCharacterScene, restoredGuard) ||
        CaptureCharacterSettings(restoredCharacterScene, restoredGuard) != guardSettings ||
        !CaptureCharacterAdvancedOverrides(
            restoredCharacterScene, restoredGuard, restoredOverrides, error) ||
        restoredOverrides != guardOverrides)
    {
        return Fail("AI-02 Character authoring did not survive native WISCENE serialization");
    }

    CharacterRuntimeState foundation;
    std::string runtimeError;
    if (!InitializeRuntimeCharacters(scene, foundation, runtimeError))
        return Fail("AI-01 foundation could not initialize AI-02 Runtime fixtures");

    RuntimeCharacterSystemState runtime;
    if (!InitializeRuntimeCharacterSystem(scene, foundation, runtime, runtimeError) ||
        runtime.characters.size() != 2)
    {
        ResetRuntimeCharacters(scene, foundation);
        return Fail("AI-02 Runtime character records did not initialize");
    }

    const StableId guardId = PersistentEntityId(scene, guard);
    const StableId soldierId = PersistentEntityId(scene, soldier);
    const RuntimeCharacterRecord* runtimeGuard = FindRuntimeCharacter(runtime, guardId);
    const RuntimeCharacterRecord* runtimeSoldier = FindRuntimeCharacter(runtime, soldierId);
    if (runtimeGuard == nullptr || runtimeSoldier == nullptr)
    {
        ResetRuntimeCharacters(scene, foundation);
        return Fail("stable-ID Runtime Character lookup failed");
    }
    if (runtimeGuard->references.patrolRouteEntity != patrol ||
        runtimeGuard->references.weaponEntity != weapon)
    {
        ResetRuntimeCharacters(scene, foundation);
        return Fail("AI-02 did not resolve persisted stable Character references");
    }
    if (!Near(runtimeGuard->tuning.visionDistance, 88.0f) ||
        !Near(runtimeGuard->tuning.coverPreference, 0.93f) ||
        !(runtimeSoldier->tuning.aggression > runtimeGuard->tuning.aggression))
    {
        ResetRuntimeCharacters(scene, foundation);
        return Fail("Runtime effective tuning does not match authoring/profile inputs");
    }
    if (!runtime.factions.Contains("Enemy") || !runtime.factions.Contains("Player") ||
        runtime.factions.Size() != builtInFactionCount)
    {
        ResetRuntimeCharacters(scene, foundation);
        return Fail("Runtime did not publish deterministic known faction state");
    }
    if (RelationshipBetween(*runtimeGuard, *runtimeSoldier) != FactionRelationship::Hostile)
    {
        ResetRuntimeCharacters(scene, foundation);
        return Fail("Runtime faction relationship did not resolve independently of perception");
    }

    const float firstVision = runtimeGuard->tuning.visionDistance;
    ResetRuntimeCharacterSystem(runtime);
    if (!runtime.characters.empty() ||
        runtime.factions.Size() != builtInFactionCount ||
        !InitializeRuntimeCharacterSystem(scene, foundation, runtime, runtimeError))
    {
        ResetRuntimeCharacters(scene, foundation);
        return Fail("AI-02 Runtime reset/reinitialization was not deterministic");
    }
    runtimeGuard = FindRuntimeCharacter(runtime, guardId);
    if (runtimeGuard == nullptr || !Near(runtimeGuard->tuning.visionDistance, firstVision))
    {
        ResetRuntimeCharacters(scene, foundation);
        return Fail("AI-02 effective tuning changed across deterministic reset");
    }

    // Missing entity references fail closed; Runtime never mutates authoring to
    // hide an invalid setup and published AI-02 state returns to its clean base.
    ResetRuntimeCharacterSystem(runtime);
    ResetRuntimeCharacters(scene, foundation);
    auto broken = CaptureCharacterSettings(scene, guard);
    broken.patrolRouteEntityId = GenerateStableId();
    if (!commands.Execute(std::make_unique<SetCharacterSettingsCommand>(scene, guard, broken)))
        return Fail("could not seed missing-reference failure fixture");
    if (!InitializeRuntimeCharacters(scene, foundation, runtimeError))
        return Fail("AI-01 foundation unexpectedly rejected syntactically valid missing reference");
    if (InitializeRuntimeCharacterSystem(scene, foundation, runtime, runtimeError) ||
        !runtime.characters.empty() || runtime.factions.Size() != builtInFactionCount ||
        CaptureCharacterSettings(scene, guard) != broken)
    {
        ResetRuntimeCharacters(scene, foundation);
        return Fail("missing Character reference did not fail closed without mutating authoring");
    }
    ResetRuntimeCharacters(scene, foundation);

    // Removing Character authoring must also remove AI-02 advanced metadata;
    // Undo must restore the advanced payload as part of the governed Character.
    if (!commands.Undo())
        return Fail("could not restore valid Guard settings before removal test");
    if (!CaptureCharacterAdvancedOverrides(scene, guard, capturedOverrides, error) ||
        capturedOverrides != guardOverrides)
    {
        return Fail("advanced override state was lost before REMOVE CHARACTER test");
    }
    if (!commands.Execute(std::make_unique<RemoveCharacterCommand>(scene, guard)))
        return Fail("REMOVE CHARACTER failed for AI-02 metadata fixture");
    const auto* removedMetadata = scene.metadatas.GetComponent(guard);
    if (removedMetadata != nullptr &&
        removedMetadata->string_values.has(CharacterAdvancedMetadataKey))
    {
        return Fail("REMOVE CHARACTER leaked AI-02 advanced metadata");
    }
    if (!commands.Undo() ||
        !CaptureCharacterAdvancedOverrides(scene, guard, capturedOverrides, error) ||
        capturedOverrides != guardOverrides)
    {
        return Fail("REMOVE CHARACTER Undo did not restore AI-02 advanced metadata");
    }

    std::cout << "AI-02 profile/faction/runtime tests passed\n";
    return 0;
}
