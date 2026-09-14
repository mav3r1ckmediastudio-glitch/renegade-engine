#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"

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

    wi::scene::Scene scene;
    const wi::ecs::Entity actor = wi::ecs::CreateEntity();
    scene.names.Create(actor) = "AI01 Test Actor";
    scene.transforms.Create(actor).UpdateTransform();
    auto& unrelatedMetadata = scene.metadatas.Create(actor);
    unrelatedMetadata.string_values.set("test.unrelated", "keep-me");

    CommandService commands;
    if (!commands.Execute(std::make_unique<MakeCharacterCommand>(scene, actor)))
        return Fail("MAKE CHARACTER command did not execute");
    if (!commands.IsDirty() || !IsRenegadeCharacter(scene, actor))
        return Fail("MAKE CHARACTER did not establish governed authoring state");

    const StableId characterId = PersistentEntityId(scene, actor);
    if (!IsValidStableId(characterId))
        return Fail("MAKE CHARACTER did not assign stable identity");

    auto* nativeCharacter = scene.characters.GetComponent(actor);
    if (nativeCharacter == nullptr || nativeCharacter->IsActive())
        return Fail("MAKE CHARACTER did not create an inactive native CharacterComponent");

    auto* metadata = scene.metadatas.GetComponent(actor);
    if (metadata == nullptr ||
        !metadata->bool_values.has(CharacterControllerOwnedMetadataKey) ||
        !metadata->bool_values.get(CharacterControllerOwnedMetadataKey))
    {
        return Fail("MAKE CHARACTER did not record Renegade controller ownership");
    }

    auto authored = CaptureCharacterSettings(scene, actor);
    authored.role = CharacterRole::Soldier;
    authored.personality = PersonalityPreset::Cautious;
    authored.factionId = "Enemy";
    authored.animationSetId = GenerateStableId();
    authored.combatStyle = CombatStyle::Ranged;
    authored.skill = SkillPreset::Veteran;
    authored.awareness = AwarenessPreset::Alert;

    if (!commands.Execute(std::make_unique<SetCharacterSettingsCommand>(
            scene, actor, authored)))
    {
        return Fail("Character settings command did not execute");
    }
    if (CaptureCharacterSettings(scene, actor) != authored)
        return Fail("Character settings did not persist in MetadataComponent");

    if (!commands.Undo())
        return Fail("Character settings Undo failed");
    const auto defaults = CaptureCharacterSettings(scene, actor);
    if (defaults.role != CharacterRole::Guard ||
        defaults.personality != PersonalityPreset::Balanced ||
        defaults.factionId != "Neutral" || !defaults.animationSetId.empty())
    {
        return Fail("Character settings Undo did not restore defaults");
    }
    if (!commands.Redo() || CaptureCharacterSettings(scene, actor) != authored)
        return Fail("Character settings Redo did not restore authored values");

    // Prove the exact Wicked Metadata + CharacterComponent serialization seam
    // used by WISCENE preserves governed AI-01 authoring and stable identity.
    wi::Archive snapshot;
    snapshot.SetReadModeAndResetPos(false);
    wi::ecs::EntitySerializer writeSerializer;
    scene.Entity_Serialize(snapshot, writeSerializer, actor);

    wi::scene::Scene restoredScene;
    snapshot.SetReadModeAndResetPos(true);
    wi::ecs::EntitySerializer readSerializer;
    const wi::ecs::Entity restoredActor =
        restoredScene.Entity_Serialize(snapshot, readSerializer);
    if (restoredActor == wi::ecs::INVALID_ENTITY ||
        !IsRenegadeCharacter(restoredScene, restoredActor) ||
        PersistentEntityId(restoredScene, restoredActor) != characterId ||
        CaptureCharacterSettings(restoredScene, restoredActor) != authored ||
        !restoredScene.characters.Contains(restoredActor))
    {
        return Fail("Character did not survive native WISCENE component serialization");
    }

    CharacterRuntimeState runtime;
    std::string runtimeError;
    if (!InitializeRuntimeCharacters(restoredScene, runtime, runtimeError) ||
        runtime.characters.size() != 1 ||
        runtime.characters.front().characterId != characterId)
    {
        return Fail("Runtime did not discover serialized Character authoring");
    }
    if (!restoredScene.characters.GetComponent(restoredActor)->IsActive())
        return Fail("Runtime did not activate native CharacterComponent");
    ResetRuntimeCharacters(restoredScene, runtime);
    if (!runtime.characters.empty() ||
        restoredScene.characters.GetComponent(restoredActor)->IsActive())
    {
        return Fail("Runtime Character reset was not deterministic");
    }

    // Undo both the settings edit and MAKE CHARACTER. The generated identity
    // and Renegade-owned controller must disappear, while unrelated metadata
    // on the imported hierarchy is preserved.
    if (!commands.Undo() || !commands.Undo())
        return Fail("AI-01 command stack could not Undo to the imported hierarchy");
    if (IsRenegadeCharacter(scene, actor) || scene.characters.Contains(actor) ||
        !PersistentEntityId(scene, actor).empty())
    {
        return Fail("MAKE CHARACTER Undo leaked governed identity/controller state");
    }
    metadata = scene.metadatas.GetComponent(actor);
    if (metadata == nullptr || !metadata->string_values.has("test.unrelated") ||
        metadata->string_values.get("test.unrelated") != "keep-me")
    {
        return Fail("MAKE CHARACTER Undo damaged unrelated imported metadata");
    }

    if (!commands.Redo() || !commands.Redo() ||
        !IsRenegadeCharacter(scene, actor) ||
        PersistentEntityId(scene, actor) != characterId ||
        CaptureCharacterSettings(scene, actor) != authored)
    {
        return Fail("AI-01 Redo did not restore deterministic Character authoring");
    }

    // Existing Wicked controllers are adopted non-destructively. REMOVE
    // CHARACTER restores their previous active state instead of deleting them.
    wi::scene::Scene adoptedScene;
    const wi::ecs::Entity adopted = wi::ecs::CreateEntity();
    adoptedScene.transforms.Create(adopted).UpdateTransform();
    auto& preexisting = adoptedScene.characters.Create(adopted);
    preexisting.SetActive(true);
    std::string identityError;
    if (!AssignNewPersistentEntityId(adoptedScene, adopted, identityError))
        return Fail("could not seed stable identity for adopted controller test");

    CommandService adoptedCommands;
    if (!adoptedCommands.Execute(
            std::make_unique<MakeCharacterCommand>(adoptedScene, adopted)))
    {
        return Fail("MAKE CHARACTER could not adopt existing CharacterComponent");
    }
    const auto* adoptedMetadata = adoptedScene.metadatas.GetComponent(adopted);
    if (adoptedMetadata == nullptr ||
        adoptedMetadata->bool_values.get(CharacterControllerOwnedMetadataKey) ||
        adoptedScene.characters.GetComponent(adopted)->IsActive())
    {
        return Fail("existing CharacterComponent was not adopted non-destructively");
    }

    if (!adoptedCommands.Execute(
            std::make_unique<RemoveCharacterCommand>(adoptedScene, adopted)) ||
        IsRenegadeCharacter(adoptedScene, adopted) ||
        !adoptedScene.characters.Contains(adopted) ||
        !adoptedScene.characters.GetComponent(adopted)->IsActive())
    {
        return Fail("REMOVE CHARACTER damaged an adopted native controller");
    }
    if (!adoptedCommands.Undo() || !IsRenegadeCharacter(adoptedScene, adopted) ||
        adoptedScene.characters.GetComponent(adopted)->IsActive())
    {
        return Fail("REMOVE CHARACTER Undo did not restore governed adopted state");
    }

    std::cout << "AI-01 Character foundation tests passed\n";
    return 0;
}
