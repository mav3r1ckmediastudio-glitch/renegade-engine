#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/PlayerService.h"

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

    // Regression: native Wicked controllers overwrite the Transform every
    // editor frame, including while inactive. Promotion must seed its world
    // position and inverse visual facing BEFORE the first scene update.
    wi::scene::Scene placed;
    const auto placedActor = placed.Entity_CreateTransform("Placed mutant");
    auto* placedTransform = placed.transforms.GetComponent(placedActor);
    placedTransform->translation_local = XMFLOAT3(17.0f, 3.0f, -9.0f);
    placedTransform->RotateRollPitchYaw(XMFLOAT3(0.0f, 0.7f, 0.0f));
    placedTransform->SetDirty();
    placedTransform->UpdateTransform();
    const auto initialForward = placedTransform->GetForward();
    MakeCharacterCommand promotePlaced(placed, placedActor);
    if (!promotePlaced.Execute()) return Fail("placed Character promotion rejected");
    const auto* placedController = placed.characters.GetComponent(placedActor);
    if (placedController == nullptr ||
        !Near(placedController->GetPosition().x, 17.0f) ||
        !Near(placedController->GetPosition().y, 3.0f) ||
        !Near(placedController->GetPosition().z, -9.0f) ||
        !Near(placedController->GetFacing().x, -initialForward.x) ||
        !Near(placedController->GetFacing().z, -initialForward.z))
        return Fail("MAKE CHARACTER reset native position or reversed visual facing");

    // The same native controller must follow gizmo/inspector edits AND undo.
    auto edited = CaptureTransform(*placedTransform);
    edited.translation = XMFLOAT3(21.0f, 3.0f, -11.0f);
    XMStoreFloat4(&edited.rotation,
        XMQuaternionRotationRollPitchYaw(0.0f, 1.4f, 0.0f));
    SetTransformCommand editPlaced(placed, placedActor, edited);
    if (!editPlaced.Execute()) return Fail("placed Character transform edit rejected");
    if (!Near(placedController->GetPosition().x, 21.0f) ||
        !Near(placedController->GetPosition().z, -11.0f) ||
        !Near(placedController->GetFacing().x, -placedTransform->GetForward().x) ||
        !Near(placedController->GetFacing().z, -placedTransform->GetForward().z))
        return Fail("edited Character controller did not follow gizmo transform");
    editPlaced.Undo();
    if (!Near(placedController->GetPosition().x, 17.0f) ||
        !Near(placedController->GetPosition().z, -9.0f) ||
        !Near(placedController->GetFacing().x, -initialForward.x) ||
        !Near(placedController->GetFacing().z, -initialForward.z))
        return Fail("Character transform Undo left native controller at wrong pose");
    if (!editPlaced.Execute() || !Near(placedController->GetPosition().x, 21.0f))
        return Fail("Character transform Redo did not re-synchronise controller");

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

    // Renegade-owned controllers must round-trip their authored native state
    // through REMOVE / Undo / Redo, not silently return to MAKE defaults.
    wi::scene::Scene exactScene;
    const wi::ecs::Entity exactActor = wi::ecs::CreateEntity();
    exactScene.transforms.Create(exactActor).UpdateTransform();
    CommandService exactCommands;
    if (!exactCommands.Execute(
            std::make_unique<MakeCharacterCommand>(exactScene, exactActor)))
    {
        return Fail("could not seed owned controller restoration test");
    }
    auto* exactController = exactScene.characters.GetComponent(exactActor);
    exactController->width = 0.77f;
    exactController->height = 2.33f;
    exactController->SetFootPlacementEnabled(true);
    if (!exactCommands.Execute(
            std::make_unique<RemoveCharacterCommand>(exactScene, exactActor)) ||
        exactScene.characters.Contains(exactActor))
    {
        return Fail("REMOVE CHARACTER did not remove its owned native controller");
    }
    if (!exactCommands.Undo())
        return Fail("REMOVE CHARACTER Undo failed for owned controller");
    exactController = exactScene.characters.GetComponent(exactActor);
    if (exactController == nullptr || !Near(exactController->width, 0.77f) ||
        !Near(exactController->height, 2.33f) ||
        !exactController->IsFootPlacementEnabled())
    {
        return Fail("REMOVE CHARACTER Undo did not restore exact native controller state");
    }
    if (!exactCommands.Redo() || exactScene.characters.Contains(exactActor) ||
        IsRenegadeCharacter(exactScene, exactActor))
    {
        return Fail("REMOVE CHARACTER Redo did not reapply removal deterministically");
    }
    if (!exactCommands.Undo())
        return Fail("REMOVE CHARACTER second Undo failed after Redo");
    exactController = exactScene.characters.GetComponent(exactActor);
    if (exactController == nullptr || !Near(exactController->width, 0.77f) ||
        !Near(exactController->height, 2.33f))
    {
        return Fail("owned native controller snapshot was not reusable after Redo");
    }

    // Player Start and other reserved gameplay markers must not be promotable
    // merely because they own a Transform.
    wi::scene::Scene reservedScene;
    CreatePlayerStartCommand playerStart(reservedScene, TransformState{});
    if (!playerStart.Execute())
        return Fail("could not create Player Start promotion guard fixture");
    const auto playerStartEntity = playerStart.CreatedEntity();
    const auto promotion = InspectCharacterPromotion(reservedScene, playerStartEntity);
    if (!promotion.incompatibleSemantic || promotion.canPromote)
        return Fail("Player Start was incorrectly considered promotable to Character");
    MakeCharacterCommand blockedPromotion(reservedScene, playerStartEntity);
    if (blockedPromotion.Execute() || IsRenegadeCharacter(reservedScene, playerStartEntity))
        return Fail("MAKE CHARACTER accepted a reserved Player Start semantic");

    // Runtime order is stable-ID order, not transient ECS allocation order.
    wi::scene::Scene orderedScene;
    const StableId laterId = "00000000-0000-4000-8000-000000000002";
    const StableId earlierId = "00000000-0000-4000-8000-000000000001";
    const wi::ecs::Entity laterEntity = wi::ecs::CreateEntity();
    orderedScene.transforms.Create(laterEntity).UpdateTransform();
    if (!AssignPersistentEntityId(orderedScene, laterEntity, laterId, identityError))
        return Fail("could not assign later stable Character ID");
    MakeCharacterCommand laterCharacter(orderedScene, laterEntity);
    if (!laterCharacter.Execute())
        return Fail("could not create later stable-order Character");

    const wi::ecs::Entity earlierEntity = wi::ecs::CreateEntity();
    orderedScene.transforms.Create(earlierEntity).UpdateTransform();
    if (!AssignPersistentEntityId(orderedScene, earlierEntity, earlierId, identityError))
        return Fail("could not assign earlier stable Character ID");
    MakeCharacterCommand earlierCharacter(orderedScene, earlierEntity);
    if (!earlierCharacter.Execute())
        return Fail("could not create earlier stable-order Character");

    CharacterRuntimeState orderedRuntime;
    if (!InitializeRuntimeCharacters(orderedScene, orderedRuntime, runtimeError) ||
        orderedRuntime.characters.size() != 2 ||
        orderedRuntime.characters[0].characterId != earlierId ||
        orderedRuntime.characters[1].characterId != laterId)
    {
        return Fail("Runtime Character order was not stable-ID deterministic");
    }
    ResetRuntimeCharacters(orderedScene, orderedRuntime);

    // Duplicate stable IDs reject the entire Runtime Character transaction and
    // must not activate a prefix of the scene before the error is discovered.
    auto* duplicateMetadata = orderedScene.metadatas.GetComponent(laterEntity);
    if (duplicateMetadata == nullptr)
        return Fail("duplicate identity fixture lost Character metadata");
    duplicateMetadata->string_values.set(PersistentEntityIdMetadataKey, earlierId);
    CharacterRuntimeState duplicateRuntime;
    if (InitializeRuntimeCharacters(orderedScene, duplicateRuntime, runtimeError) ||
        !duplicateRuntime.characters.empty() ||
        orderedScene.characters.GetComponent(earlierEntity)->IsActive() ||
        orderedScene.characters.GetComponent(laterEntity)->IsActive())
    {
        return Fail("duplicate Character identities did not fail atomically");
    }

    // A structurally invalid later Character also cannot leave an earlier
    // valid controller activated as a partial initialization side effect.
    duplicateMetadata->string_values.set(PersistentEntityIdMetadataKey, laterId);
    orderedScene.characters.Remove(laterEntity);
    CharacterRuntimeState invalidRuntime;
    if (InitializeRuntimeCharacters(orderedScene, invalidRuntime, runtimeError) ||
        !invalidRuntime.characters.empty() ||
        orderedScene.characters.GetComponent(earlierEntity)->IsActive())
    {
        return Fail("invalid Character scene produced partial Runtime activation");
    }

    std::cout << "AI-01 Character foundation tests passed\n";
    return 0;
}
