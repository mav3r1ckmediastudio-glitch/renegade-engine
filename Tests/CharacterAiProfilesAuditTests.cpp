#include "renegade/bridge/CharacterProfileService.h"
#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include "RuntimeCharacterSystem.h"

#include <WickedEngine.h>

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
}

int main()
{
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    wi::scene::Scene scene;
    const wi::ecs::Entity character = wi::ecs::CreateEntity();
    scene.names.Create(character) = "AI02 Audit Character";
    scene.transforms.Create(character).UpdateTransform();

    CommandService commands;
    if (!commands.Execute(std::make_unique<MakeCharacterCommand>(scene, character)))
        return Fail("could not create audit Character fixture");

    auto* metadata = scene.metadatas.GetComponent(character);
    if (metadata == nullptr)
        return Fail("Character fixture has no metadata");

    std::string error;

    // AI-01 recognizes only the current Character schema. AI-02 Runtime must
    // still detect an owned marker whose schema is unsupported instead of
    // silently treating the level as though the Character did not exist.
    metadata->int_values.set(CharacterSchemaVersionMetadataKey, CharacterSchemaVersion + 1);
    CharacterRuntimeState unsupportedFoundation;
    if (!InitializeRuntimeCharacters(scene, unsupportedFoundation, error) ||
        !unsupportedFoundation.characters.empty())
    {
        return Fail("AI-01 unsupported-schema fixture did not exercise the skip boundary");
    }
    RuntimeCharacterSystemState unsupportedRuntime;
    if (InitializeRuntimeCharacterSystem(
            scene, unsupportedFoundation, unsupportedRuntime, error) ||
        !unsupportedRuntime.characters.empty() ||
        unsupportedRuntime.factions.Size() != BuiltInFactions().size())
    {
        return Fail("unsupported Character schema marker was silently ignored");
    }
    metadata->int_values.set(CharacterSchemaVersionMetadataKey, CharacterSchemaVersion);

    // Persisted profile metadata is validated from the raw scene values. The
    // ordinary capture API remains backwards-compatible, but Runtime AI-02 may
    // not silently reinterpret corrupt enums as a default Guard profile.
    const int originalRole = metadata->int_values.get(CharacterRoleMetadataKey);
    metadata->int_values.set(CharacterRoleMetadataKey, 999);
    if (ValidateCharacterProfileAuthoring(scene, character, error))
        return Fail("malformed raw profile enum did not fail closed");

    CharacterRuntimeState foundation;
    if (!InitializeRuntimeCharacters(scene, foundation, error))
        return Fail("AI-01 foundation unexpectedly rejected raw profile audit fixture");
    RuntimeCharacterSystemState runtime;
    if (InitializeRuntimeCharacterSystem(scene, foundation, runtime, error) ||
        !runtime.characters.empty() || runtime.factions.Size() != BuiltInFactions().size())
    {
        ResetRuntimeCharacters(scene, foundation);
        return Fail("AI-02 Runtime accepted malformed persisted profile metadata");
    }
    ResetRuntimeCharacters(scene, foundation);
    metadata->int_values.set(CharacterRoleMetadataKey, originalRole);

    // Faction validation is also performed against raw persisted metadata, not
    // only against the creator-facing text field.
    const std::string originalFaction = metadata->string_values.get(CharacterFactionMetadataKey);
    metadata->string_values.set(CharacterFactionMetadataKey, std::string("bad\nname"));
    if (ValidateCharacterProfileAuthoring(scene, character, error))
        return Fail("malformed persisted faction did not fail closed");
    metadata->string_values.set(CharacterFactionMetadataKey, originalFaction);

    // Duplicate/contradictory advanced fields are corruption, not an implicit
    // last-wins policy that can change AI behaviour silently.
    CharacterAdvancedOverrides parsed;
    if (DeserializeCharacterAdvancedOverrides(
            "v1;vision=10;vision=20", parsed, error))
    {
        return Fail("duplicate advanced field did not fail closed");
    }
    CharacterAdvancedOverrides contradictory;
    contradictory.alertThreshold = 80.0f;
    contradictory.combatThreshold = 20.0f;
    if (ValidateCharacterAdvancedOverrides(contradictory, error))
        return Fail("contradictory advanced thresholds were accepted");

    CharacterAdvancedOverrides decimal;
    decimal.visionDistance = 12.5f;
    const std::string localeNeutral = SerializeCharacterAdvancedOverrides(decimal);
    if (localeNeutral.find("vision=12.5") == std::string::npos ||
        !DeserializeCharacterAdvancedOverrides(localeNeutral, parsed, error) ||
        parsed.visionDistance != decimal.visionDistance)
    {
        return Fail("advanced payload was not locale-neutral and deterministic");
    }

    // The recovery command snapshots raw bytes before parsing. RESET ADVANCED
    // must therefore repair malformed authoring, while Undo restores the exact
    // malformed bytes and Redo repairs it again.
    const std::string malformed = "v1;vision=25;vision=30";
    metadata->string_values.set(CharacterAdvancedMetadataKey, malformed);
    CharacterAdvancedOverrides capture;
    if (CaptureCharacterAdvancedOverrides(scene, character, capture, error))
        return Fail("malformed advanced payload unexpectedly parsed");

    if (!commands.Execute(std::make_unique<SetCharacterAdvancedOverridesCommand>(
            scene, character, CharacterAdvancedOverrides{})))
    {
        return Fail("RESET ADVANCED could not repair malformed payload");
    }
    if (metadata->string_values.has(CharacterAdvancedMetadataKey))
        return Fail("RESET ADVANCED did not erase malformed payload");

    if (!commands.Undo() ||
        !metadata->string_values.has(CharacterAdvancedMetadataKey) ||
        metadata->string_values.get(CharacterAdvancedMetadataKey) != malformed)
    {
        return Fail("advanced repair Undo did not restore exact raw payload");
    }
    if (!commands.Redo() || metadata->string_values.has(CharacterAdvancedMetadataKey))
        return Fail("advanced repair Redo did not reapply the repair");

    // A normal well-formed edit retains the same command semantics.
    CharacterAdvancedOverrides valid;
    valid.aggression = 0.75f;
    if (!commands.Execute(std::make_unique<SetCharacterAdvancedOverridesCommand>(
            scene, character, valid)))
    {
        return Fail("well-formed advanced edit failed after recovery path");
    }
    if (!CaptureCharacterAdvancedOverrides(scene, character, capture, error) ||
        capture != valid)
    {
        return Fail("well-formed advanced edit did not persist after recovery path");
    }

    std::cout << "AI-02 audit regressions passed\n";
    return 0;
}
