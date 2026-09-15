#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"

#include <WickedEngine.h>

#include <iostream>
#include <string>

namespace
{
    using namespace renegade::bridge;

    constexpr const char* BaseAssetId =
        "33333333-3333-4333-8333-333333333333";

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "CW-05 COMMAND COMPANION FAIL // " << message << '\n';
        return false;
    }

    wi::ecs::Entity CreateConfiguredCharacter(wi::scene::Scene& scene)
    {
        const wi::ecs::Entity entity =
            scene.Entity_CreateTransform("Configured Guard");
        std::string error;
        if (!AssignNewPersistentEntityId(scene, entity, error))
            return wi::ecs::INVALID_ENTITY;

        CharacterAuthoringSettings settings;
        settings.role = CharacterRole::PatrolGuard;
        settings.personality = PersonalityPreset::Veteran;
        settings.factionId = "Enemy";
        settings.combatStyle = CombatStyle::Ranged;
        settings.skill = SkillPreset::Elite;
        settings.awareness = AwarenessPreset::Alert;
        settings.canSurrender = true;
        MakeCharacterCommand make(scene, entity, settings);
        return make.Execute() ? entity : wi::ecs::INVALID_ENTITY;
    }

    wi::allocator::shared_ptr<wi::scene::Scene> MakePreparedCharacter()
    {
        auto prepared =
            wi::allocator::make_shared_single<wi::scene::Scene>();
        const wi::ecs::Entity root =
            prepared->Entity_CreateTransform("Prepared Character");
        const wi::ecs::Entity child =
            prepared->Entity_CreateTransform("Prepared Character Mesh");
        prepared->Component_Attach(child, root, true);
        if (!MarkCharacterAssetTemplate(*prepared))
            return {};
        return prepared;
    }
}

int main()
{
    using namespace renegade::bridge;

    // The ordinary Studio duplicate command is the CW-05 Character duplicate
    // authority. It must rebuild native Character state, assign a fresh stable
    // identity and keep external authored state in the same Undo/Redo action.
    wi::scene::Scene duplicateScene;
    const wi::ecs::Entity source =
        CreateConfiguredCharacter(duplicateScene);
    if (!Require(source != wi::ecs::INVALID_ENTITY,
            "could not create source Character"))
        return 1;
    const StableId sourceId = PersistentEntityId(duplicateScene, source);
    const CharacterAuthoringSettings sourceSettings =
        CaptureCharacterSettings(duplicateScene, source);

    int duplicateCompanionState = 0;
    SetDuplicateEntityCompanionFactory(
        [&](wi::scene::Scene& scene,
            const wi::ecs::Entity callbackSource,
            const wi::ecs::Entity callbackDuplicate,
            DuplicateEntityCompanionCallbacks& callbacks,
            std::string& error)
        {
            if (callbackSource != source ||
                !IsRenegadeCharacter(scene, callbackDuplicate) ||
                !IsValidStableId(PersistentEntityId(scene, callbackDuplicate)))
            {
                error = "duplicate companion ran before Character rebuild/identity";
                return false;
            }
            duplicateCompanionState = 1;
            callbacks.undo = [&duplicateCompanionState]()
            {
                duplicateCompanionState = 0;
            };
            callbacks.redo = [&duplicateCompanionState]()
            {
                duplicateCompanionState = 1;
                return true;
            };
            error.clear();
            return true;
        });

    DuplicateEntityCommand duplicate(duplicateScene, source);
    if (!Require(duplicate.Execute(), "central Character duplicate failed") ||
        !Require(duplicateCompanionState == 1,
            "duplicate companion did not execute"))
    {
        ClearDuplicateEntityCompanionFactory();
        return 1;
    }

    const wi::ecs::Entity copied = duplicate.DuplicatedEntity();
    const StableId copiedId = PersistentEntityId(duplicateScene, copied);
    if (!Require(IsRenegadeCharacter(duplicateScene, copied),
            "central duplicate lost Character state") ||
        !Require(IsValidStableId(copiedId) && copiedId != sourceId,
            "central duplicate reused source identity") ||
        !Require(CaptureCharacterSettings(duplicateScene, copied) == sourceSettings,
            "central duplicate changed authored Character settings") ||
        !Require(duplicateScene.characters.Contains(copied) &&
            !duplicateScene.characters.GetComponent(copied)->IsActive(),
            "central duplicate leaked active native controller state"))
    {
        ClearDuplicateEntityCompanionFactory();
        return 1;
    }

    duplicate.Undo();
    if (!Require(duplicateCompanionState == 0,
            "duplicate Undo did not roll back companion state") ||
        !Require(!duplicateScene.transforms.Contains(copied),
            "duplicate Undo did not remove copied Character"))
    {
        ClearDuplicateEntityCompanionFactory();
        return 1;
    }
    if (!Require(duplicate.Execute(), "central duplicate Redo failed") ||
        !Require(duplicateCompanionState == 1,
            "duplicate Redo did not restore companion state") ||
        !Require(PersistentEntityId(duplicateScene, copied) == copiedId,
            "duplicate Redo changed copied Character identity"))
    {
        ClearDuplicateEntityCompanionFactory();
        return 1;
    }
    ClearDuplicateEntityCompanionFactory();

    // The existing reusable placement command remains the Asset Browser/drag
    // authority. Its companion must run only after fresh identity + automatic
    // Character promotion and participate in the same Undo/Redo operation.
    wi::scene::Scene placementScene;
    int placementCompanionState = 0;
    StableId placedId;
    SetReusablePlacementCompanionFactory(
        [&](wi::scene::Scene& scene,
            const wi::ecs::Entity instanceRoot,
            const wi::ecs::Entity payloadRoot,
            ReusablePlacementCompanionCallbacks& callbacks,
            std::string& error)
        {
            if (!IsRenegadeCharacter(scene, instanceRoot) ||
                payloadRoot == wi::ecs::INVALID_ENTITY ||
                !scene.Entity_IsDescendant(payloadRoot, instanceRoot))
            {
                error = "placement companion ran before Character promotion";
                return false;
            }
            placedId = PersistentEntityId(scene, instanceRoot);
            if (!IsValidStableId(placedId))
            {
                error = "placement companion ran before fresh identity";
                return false;
            }
            placementCompanionState = 1;
            callbacks.undo = [&placementCompanionState]()
            {
                placementCompanionState = 0;
            };
            callbacks.redo = [&placementCompanionState]()
            {
                placementCompanionState = 1;
                return true;
            };
            error.clear();
            return true;
        });

    PlaceReusableModelCommand placement(
        placementScene,
        MakePreparedCharacter(),
        BaseAssetId,
        XMFLOAT3(2.0f, 0.0f, 3.0f),
        1.0f,
        "Enemy Guard Prefab");
    if (!Require(placement.Execute(), "central Character placement failed") ||
        !Require(placementCompanionState == 1,
            "placement companion did not execute") ||
        !Require(IsRenegadeCharacter(
            placementScene, placement.PlacedEntity()),
            "placement did not create Character before companion") ||
        !Require(PersistentEntityId(
            placementScene, placement.PlacedEntity()) == placedId,
            "placement companion observed a different stable identity"))
    {
        ClearReusablePlacementCompanionFactory();
        return 1;
    }

    const wi::ecs::Entity placed = placement.PlacedEntity();
    placement.Undo();
    if (!Require(placementCompanionState == 0,
            "placement Undo did not roll back companion state") ||
        !Require(!placementScene.transforms.Contains(placed),
            "placement Undo did not remove Character"))
    {
        ClearReusablePlacementCompanionFactory();
        return 1;
    }
    if (!Require(placement.Execute(), "central Character placement Redo failed") ||
        !Require(placementCompanionState == 1,
            "placement Redo did not restore companion state") ||
        !Require(PersistentEntityId(placementScene, placed) == placedId,
            "placement Redo changed Character identity"))
    {
        ClearReusablePlacementCompanionFactory();
        return 1;
    }
    ClearReusablePlacementCompanionFactory();

    std::cout << "CW-05 COMMAND COMPANION PASS\n";
    return 0;
}
