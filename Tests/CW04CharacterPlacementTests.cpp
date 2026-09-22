#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"

#include <WickedEngine.h>

#include <iostream>
#include <string>

namespace
{
    constexpr const char* CharacterAssetId =
        "44444444-4444-4444-8444-444444444444";
    constexpr const char* ModelAssetId =
        "55555555-5555-4555-8555-555555555555";

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "CW-04 CHARACTER PLACEMENT FAIL // " << message << '\n';
        return false;
    }

    wi::allocator::shared_ptr<wi::scene::Scene> MakeTemplate(
        const std::string& rootName,
        const bool character)
    {
        auto scene = wi::allocator::make_shared<wi::scene::Scene>();
        const wi::ecs::Entity root = scene->Entity_CreateTransform(rootName);
        const wi::ecs::Entity child = scene->Entity_CreateTransform(rootName + " Child");
        scene->Component_Attach(child, root, true);
        if (character && !renegade::bridge::MarkCharacterAssetTemplate(*scene))
            return {};
        if (character)
        {
            for (const char* clipName : {"Idle", "Run"})
            {
                const auto clip = scene->Entity_CreateTransform(clipName);
                scene->animations.Create(clip);
                scene->Component_Attach(clip, root, true);
            }
        }
        return scene;
    }

    bool DefaultsAreFresh(const renegade::bridge::CharacterAuthoringSettings& settings)
    {
        using namespace renegade::bridge;
        return settings == CharacterAuthoringSettings{} &&
            settings.factionId == "Neutral" &&
            settings.role == CharacterRole::Guard &&
            settings.personality == PersonalityPreset::Balanced &&
            settings.combatStyle == CombatStyle::None &&
            settings.patrolRouteEntityId.empty() &&
            settings.weaponEntityId.empty() &&
            settings.squadId.empty();
    }
}

int main()
{
    using namespace renegade::bridge;

    // A prepared Character Asset becomes a concrete Character scene instance
    // as part of the same placement command. Its wrapper gets fresh scene
    // identity before MakeCharacterCommand adopts that identity.
    wi::scene::Scene scene;
    auto firstTemplate = MakeTemplate("Prepared Soldier", true);
    if (!Require(firstTemplate.IsValid(), "could not build Character template"))
        return 1;

    PlaceReusableModelCommand first(
        scene,
        std::move(firstTemplate),
        CharacterAssetId,
        XMFLOAT3(1.0f, 2.0f, 3.0f),
        1.0f,
        "Soldier.rasset");
    if (!Require(first.Execute(), "prepared Character placement failed"))
        return 1;

    const wi::ecs::Entity firstCharacter = first.PlacedEntity();
    const auto* initialPosition = scene.transforms.GetComponent(firstCharacter);
    if (!Require(initialPosition != nullptr &&
            initialPosition->GetPosition().x == 1.0f &&
            initialPosition->GetPosition().y == 2.0f &&
            initialPosition->GetPosition().z == 3.0f,
            "native Character wrapper world position was not published on placement"))
        return 1;
    const wi::ecs::Entity firstPayload = first.PayloadRootEntity();
    // The imported action library is not a request to play Idle and Run
    // simultaneously; Runtime AI must select the one active action.
    int playingClips = 0;
    for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
        playingClips += scene.animations[index].IsPlaying() ? 1 : 0;
    if (!Require(scene.animations.GetCount() == 2 && playingClips == 0,
            "Character placement started all mutually exclusive animations"))
        return 1;
    const StableId firstCharacterId = PersistentEntityId(scene, firstCharacter);
    const StableId firstPayloadId = PersistentEntityId(scene, firstPayload);
    if (!Require(IsRenegadeCharacter(scene, firstCharacter),
            "prepared Character did not auto-promote") ||
        !Require(scene.characters.Contains(firstCharacter),
            "prepared Character did not receive native Wicked CharacterComponent") ||
        !Require(IsValidStableId(firstCharacterId),
            "prepared Character wrapper is missing fresh persistent identity") ||
        !Require(IsValidStableId(firstPayloadId) && firstPayloadId != firstCharacterId,
            "Character payload and wrapper do not have independent persistent identities") ||
        !Require(CharacterAssetTemplateInHierarchy(scene, firstPayload),
            "placed payload lost its Character Asset template classification") ||
        !Require(DefaultsAreFresh(CaptureCharacterSettings(scene, firstCharacter)),
            "base Character Asset placement did not start with default gameplay settings"))
    {
        return 1;
    }

    // Placement + Character promotion are one Undo/Redo unit. Redo restores the
    // same authored scene identity from the placement snapshot rather than
    // allocating a new Character or requiring MAKE CHARACTER again.
    first.Undo();
    if (!Require(!scene.transforms.Contains(firstCharacter),
            "Character placement Undo did not remove the instance"))
        return 1;
    if (!Require(first.Execute(), "Character placement Redo failed") ||
        !Require(IsRenegadeCharacter(scene, firstCharacter),
            "Character placement Redo lost Character metadata") ||
        !Require(scene.characters.Contains(firstCharacter),
            "Character placement Redo lost native CharacterComponent") ||
        !Require(PersistentEntityId(scene, firstCharacter) == firstCharacterId,
            "Character placement Redo changed the authored scene identity"))
    {
        return 1;
    }

    // Dragging the same base Character Asset again creates a fresh scene actor
    // with fresh identity and fresh default gameplay settings. It does not
    // inherit a previous scene instance's configuration.
    auto secondTemplate = MakeTemplate("Prepared Soldier", true);
    PlaceReusableModelCommand second(
        scene,
        std::move(secondTemplate),
        CharacterAssetId,
        XMFLOAT3(4.0f, 2.0f, 3.0f),
        1.0f,
        "Soldier.rasset");
    if (!Require(second.Execute(), "second Character Asset placement failed"))
        return 1;
    const wi::ecs::Entity secondCharacter = second.PlacedEntity();
    const StableId secondCharacterId = PersistentEntityId(scene, secondCharacter);
    if (!Require(IsRenegadeCharacter(scene, secondCharacter),
            "second Character Asset placement did not auto-promote") ||
        !Require(IsValidStableId(secondCharacterId) &&
                 secondCharacterId != firstCharacterId,
            "separate Character Asset placements shared scene identity") ||
        !Require(DefaultsAreFresh(CaptureCharacterSettings(scene, secondCharacter)),
            "second base Character Asset placement was not a fresh gameplay instance"))
    {
        return 1;
    }

    // Generic reusable Models retain the existing placement contract and must
    // never become Characters just because CW-04 is enabled.
    wi::scene::Scene modelScene;
    auto modelTemplate = MakeTemplate("Crate", false);
    PlaceReusableModelCommand model(
        modelScene,
        std::move(modelTemplate),
        ModelAssetId,
        XMFLOAT3(0.0f, 0.0f, 0.0f),
        1.0f,
        "Crate.rasset");
    if (!Require(model.Execute(), "generic reusable Model placement failed") ||
        !Require(!IsRenegadeCharacter(modelScene, model.PlacedEntity()),
            "generic Model was incorrectly promoted to Character") ||
        !Require(!modelScene.characters.Contains(model.PlacedEntity()),
            "generic Model incorrectly received a native CharacterComponent"))
    {
        return 1;
    }

    // Studio drag/drop adopts a live cursor hierarchy rather than merging a
    // second copy. The payload marker must therefore drive Character promotion
    // through the adopt-existing constructor too.
    wi::scene::Scene dragScene;
    const wi::ecs::Entity dragWrapper =
        dragScene.Entity_CreateTransform("Reusable Asset Drag Preview");
    const wi::ecs::Entity dragPayload =
        dragScene.Entity_CreateTransform("Prepared Drag Character");
    dragScene.Component_Attach(dragPayload, dragWrapper, true);
    auto& dragMarker = dragScene.metadatas.Create(dragPayload);
    dragMarker.bool_values.set(CharacterAssetTemplateMetadataKey, true);
    dragMarker.int_values.set(
        CharacterAssetTemplateVersionMetadataKey,
        CharacterAssetTemplateVersion);

    PlaceReusableModelCommand adopted(
        dragScene,
        CharacterAssetId,
        dragWrapper,
        dragPayload,
        0,
        "Soldier.rasset");
    if (!Require(adopted.Execute(), "live drag Character adoption failed"))
        return 1;
    const StableId adoptedId = PersistentEntityId(dragScene, dragWrapper);
    if (!Require(adopted.PlacedEntity() == dragWrapper,
            "drag adoption replaced the visible cursor wrapper") ||
        !Require(IsRenegadeCharacter(dragScene, dragWrapper),
            "drag-adopted prepared Character did not auto-promote") ||
        !Require(dragScene.characters.Contains(dragWrapper),
            "drag-adopted Character lacks native CharacterComponent") ||
        !Require(IsValidStableId(adoptedId),
            "drag-adopted Character lacks fresh persistent identity") ||
        !Require(DefaultsAreFresh(CaptureCharacterSettings(dragScene, dragWrapper)),
            "drag-adopted Character did not receive default gameplay settings"))
    {
        return 1;
    }

    adopted.Undo();
    if (!Require(!dragScene.transforms.Contains(dragWrapper),
            "drag-adopted Character Undo did not remove the instance") ||
        !Require(adopted.Execute(), "drag-adopted Character Redo failed") ||
        !Require(IsRenegadeCharacter(dragScene, dragWrapper),
            "drag-adopted Character Redo lost Character state") ||
        !Require(PersistentEntityId(dragScene, dragWrapper) == adoptedId,
            "drag-adopted Character Redo changed persistent identity"))
    {
        return 1;
    }

    std::cout << "CW-04 CHARACTER PLACEMENT PASS\n";
    return 0;
}
