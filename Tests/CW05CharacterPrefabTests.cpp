#include "renegade/bridge/CharacterPrefabService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptDocumentService.h"

#include <WickedEngine.h>

#include <iostream>
#include <string>
#include <vector>

namespace
{
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "11111111-1111-4111-8111-111111111111";
    constexpr const char* SceneDocumentId =
        "22222222-2222-4222-8222-222222222222";
    constexpr const char* BaseAssetId =
        "33333333-3333-4333-8333-333333333333";
    constexpr const char* PrefabAssetId =
        "44444444-4444-4444-8444-444444444444";
    constexpr const char* PatrolId =
        "55555555-5555-4555-8555-555555555555";
    constexpr const char* WeaponId =
        "66666666-6666-4666-8666-666666666666";
    constexpr const char* ScriptSourceId =
        "77777777-7777-4777-8777-777777777777";

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "CW-05 CHARACTER PREFAB FAIL // " << message << '\n';
        return false;
    }

    wi::ecs::Entity MakeCharacter(
        wi::scene::Scene& scene,
        CharacterAuthoringSettings settings)
    {
        const wi::ecs::Entity entity = scene.Entity_CreateTransform("Configured Soldier");
        std::string error;
        if (!AssignNewPersistentEntityId(scene, entity, error))
            return wi::ecs::INVALID_ENTITY;
        MakeCharacterCommand command(scene, entity, std::move(settings));
        if (!command.Execute())
            return wi::ecs::INVALID_ENTITY;

        // MakeCharacterCommand has already created/populated Character metadata.
        // Do not call ComponentManager::Create() again here: doing so resets the
        // existing MetadataComponent and erases the Character marker/settings,
        // which made the fixture cease to be a Renegade Character before the
        // advanced-authoring assertions even began.
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
            return wi::ecs::INVALID_ENTITY;
        metadata->string_values.set(
            ReusableAssetInstanceIdMetadataKey, BaseAssetId);
        return entity;
    }

    ScriptSourceBinding MakeScriptSource()
    {
        ScriptSourceBinding source;
        source.sourceId = ScriptSourceId;
        source.sourcePath = "Content/Scripts/guard_action.lua";
        source.presentation = ScriptPresentation::Action;
        source.apiVersion = 1;
        source.provenance.kind = ScriptProvenanceKind::Project;
        return source;
    }

    ScriptAttachment MakeSourceAttachment(const StableId& ownerId)
    {
        ScriptAttachment attachment = CreateScriptAttachment(
            ScriptScope::Entity, ownerId, MakeScriptSource());
        attachment.enabled = true;
        ScriptPropertyValue self;
        self.name = "owner";
        self.type = ScriptPropertyType::EntityReference;
        self.referenceId = ownerId;
        attachment.properties.push_back(std::move(self));
        return attachment;
    }

    CharacterPrefabDocument MakePrefabDocument()
    {
        CharacterPrefabDocument document;
        document.projectId = ProjectId;
        document.assetId = PrefabAssetId;
        document.baseCharacterAssetId = BaseAssetId;
        document.settings.role = CharacterRole::Soldier;
        document.settings.personality = PersonalityPreset::Veteran;
        document.settings.factionId = "Enemy";
        document.settings.combatStyle = CombatStyle::Ranged;
        document.settings.skill = SkillPreset::Elite;
        document.settings.awareness = AwarenessPreset::Vigilant;
        document.settings.canSurrender = true;
        document.settings.patrolRouteEntityId.clear();
        document.settings.weaponEntityId.clear();

        CharacterAdvancedOverrides advanced;
        advanced.aggression = 0.91f;
        advanced.accuracy = 0.82f;
        document.advancedOverrides = SerializeCharacterAdvancedOverrides(advanced);

        CharacterPrefabScriptTemplate script;
        script.source = MakeScriptSource();
        script.enabled = true;
        CharacterPrefabScriptProperty self;
        self.value.name = "owner";
        self.value.type = ScriptPropertyType::EntityReference;
        self.selfEntityReference = true;
        script.properties.push_back(std::move(self));
        document.scripts.push_back(std::move(script));
        return document;
    }
}

int main()
{
    using namespace renegade::bridge;

    // Scene duplication keeps authored gameplay setup, but Character runtime
    // identity/controller state and script instance identity must be fresh.
    wi::scene::Scene scene;
    CharacterAuthoringSettings configured;
    configured.role = CharacterRole::PatrolGuard;
    configured.personality = PersonalityPreset::Aggressive;
    configured.factionId = "Enemy";
    configured.patrolRouteEntityId = PatrolId;
    configured.weaponEntityId = WeaponId;
    configured.combatStyle = CombatStyle::Ranged;
    configured.skill = SkillPreset::Veteran;
    configured.awareness = AwarenessPreset::Alert;
    configured.canSurrender = true;
    const wi::ecs::Entity source = MakeCharacter(scene, configured);
    if (!Require(source != wi::ecs::INVALID_ENTITY,
            "could not create configured source Character"))
        return 1;

    CharacterAdvancedOverrides sourceAdvanced;
    sourceAdvanced.aggression = 0.88f;
    sourceAdvanced.accuracy = 0.73f;
    std::string error;
    const bool sourceAdvancedApplied = ApplyCharacterAdvancedOverrides(
        scene, source, sourceAdvanced, error);
    if (!Require(sourceAdvancedApplied,
            "could not apply source advanced overrides: " + error))
        return 1;

    const StableId sourceId = PersistentEntityId(scene, source);
    ScriptDocument scripts = CreateScriptDocument(
        ProjectId, SceneDocumentId, "Content/Scenes/test.wiscene", "cw05-test");
    ScriptAttachment sourceAttachment = MakeSourceAttachment(sourceId);
    const StableId sourceScriptId = sourceAttachment.scriptInstanceId;
    if (!Require(AddScriptAttachment(scripts, std::move(sourceAttachment), error),
            "could not add source script attachment: " + error))
        return 1;

    DuplicateCharacterInstanceCommand duplicate(scene, source, &scripts);
    if (!Require(duplicate.Execute(), "Character duplication failed"))
        return 1;
    const wi::ecs::Entity copied = duplicate.DuplicatedEntity();
    const StableId copiedId = PersistentEntityId(scene, copied);
    if (!Require(IsRenegadeCharacter(scene, copied),
            "duplicate lost Character ownership") ||
        !Require(scene.characters.Contains(copied),
            "duplicate lost native CharacterComponent") ||
        !Require(!scene.characters.GetComponent(copied)->IsActive(),
            "duplicate native Character controller retained active runtime state") ||
        !Require(IsValidStableId(copiedId) && copiedId != sourceId,
            "duplicate did not receive fresh persistent identity") ||
        !Require(CaptureCharacterSettings(scene, copied) == configured,
            "duplicate did not preserve authored Character settings"))
        return 1;

    CharacterAdvancedOverrides copiedAdvanced;
    if (!Require(CaptureCharacterAdvancedOverrides(
            scene, copied, copiedAdvanced, error),
            "duplicate advanced overrides are invalid: " + error) ||
        !Require(copiedAdvanced == sourceAdvanced,
            "duplicate did not preserve advanced AI authoring"))
        return 1;

    const auto copiedAttachments = [&]()
    {
        std::vector<const ScriptAttachment*> result;
        for (const auto& attachment : scripts.attachments)
        {
            if (attachment.scope == ScriptScope::Entity &&
                attachment.ownerEntityId == copiedId)
                result.push_back(&attachment);
        }
        return result;
    }();
    if (!Require(copiedAttachments.size() == 1,
            "duplicate did not copy entity Action/Script attachment") ||
        !Require(copiedAttachments.front()->scriptInstanceId != sourceScriptId,
            "duplicate reused source ScriptInstanceId") ||
        !Require(copiedAttachments.front()->properties.size() == 1 &&
            copiedAttachments.front()->properties.front().referenceId == sourceId,
            "ordinary scene duplication should preserve authored same-scene references"))
        return 1;
    const StableId copiedScriptId = copiedAttachments.front()->scriptInstanceId;

    duplicate.Undo();
    if (!Require(!scene.transforms.Contains(copied),
            "duplicate Undo did not remove copied Character"))
        return 1;
    for (const auto& attachment : scripts.attachments)
    {
        if (!Require(attachment.ownerEntityId != copiedId,
                "duplicate Undo left copied script attachment behind"))
            return 1;
    }
    if (!Require(duplicate.Execute(), "duplicate Redo failed") ||
        !Require(PersistentEntityId(scene, copied) == copiedId,
            "duplicate Redo changed authored duplicate identity"))
        return 1;
    bool foundRedoneScript = false;
    for (const auto& attachment : scripts.attachments)
    {
        if (attachment.ownerEntityId == copiedId)
        {
            foundRedoneScript = attachment.scriptInstanceId == copiedScriptId;
            break;
        }
    }
    if (!Require(foundRedoneScript,
            "duplicate Redo did not restore the same copied ScriptInstanceId"))
        return 1;

    // Character Prefab documents are portable authoring layers. They reference
    // the base Character Asset, carry no scene-local patrol/weapon identity and
    // can represent an entity property that intentionally targets prefab self.
    CharacterPrefabDocument prefab = MakePrefabDocument();
    std::string encoded;
    if (!Require(SerializeCharacterPrefab(prefab, encoded, error),
            "prefab serialization failed: " + error))
        return 1;
    CharacterPrefabDocument decoded;
    if (!Require(DeserializeCharacterPrefab(encoded, decoded, error),
            "prefab round-trip failed: " + error) ||
        !Require(decoded.assetId == PrefabAssetId &&
                 decoded.baseCharacterAssetId == BaseAssetId,
            "prefab round-trip lost stable asset dependencies") ||
        !Require(decoded.settings == prefab.settings,
            "prefab round-trip changed Character authoring settings") ||
        !Require(decoded.settings.patrolRouteEntityId.empty() &&
                 decoded.settings.weaponEntityId.empty(),
            "portable prefab retained scene-local Character references") ||
        !Require(decoded.scripts.size() == 1 &&
                 decoded.scripts.front().properties.size() == 1 &&
                 decoded.scripts.front().properties.front().selfEntityReference,
            "prefab round-trip lost script self-reference semantics"))
        return 1;

    CharacterPrefabDocument invalid = prefab;
    invalid.settings.patrolRouteEntityId = PatrolId;
    std::string rejected;
    if (!Require(!SerializeCharacterPrefab(invalid, rejected, error),
            "portable prefab accepted a scene-local patrol route"))
        return 1;

    // Prefab placement instantiates the prepared base Character payload, then
    // applies the portable gameplay layer and remaps script self to the new
    // scene identity. Every separate placement receives independent identity.
    const auto makePreparedBase = []()
    {
        auto prepared = wi::allocator::make_shared<wi::scene::Scene>();
        const wi::ecs::Entity root =
            prepared->Entity_CreateTransform("Prepared Soldier");
        const wi::ecs::Entity child =
            prepared->Entity_CreateTransform("Prepared Soldier Mesh");
        prepared->Component_Attach(child, root, true);
        if (!MarkCharacterAssetTemplate(*prepared))
            return wi::allocator::shared_ptr<wi::scene::Scene>{};
        return prepared;
    };

    wi::scene::Scene prefabScene;
    ScriptDocument prefabScripts = CreateScriptDocument(
        ProjectId, SceneDocumentId, "Content/Scenes/prefab.wiscene", "cw05-test");
    PlaceCharacterPrefabCommand placeOne(
        prefabScene, makePreparedBase(), prefab,
        XMFLOAT3(1.0f, 0.0f, 1.0f), 1.0f,
        "Enemy Rifle Soldier", &prefabScripts);
    if (!Require(placeOne.Execute(), "first Character Prefab placement failed"))
        return 1;
    const wi::ecs::Entity actorOne = placeOne.PlacedEntity();
    const StableId actorOneId = PersistentEntityId(prefabScene, actorOne);
    const auto* actorOneMetadata = prefabScene.metadatas.GetComponent(actorOne);
    if (!Require(IsRenegadeCharacter(prefabScene, actorOne),
            "prefab placement did not create a Character") ||
        !Require(IsValidStableId(actorOneId),
            "prefab placement lacks fresh Character identity") ||
        !Require(CaptureCharacterSettings(prefabScene, actorOne) == prefab.settings,
            "prefab placement did not restore portable authored settings") ||
        !Require(actorOneMetadata != nullptr &&
            actorOneMetadata->string_values.has(CharacterPrefabOriginAssetMetadataKey) &&
            actorOneMetadata->string_values.get(CharacterPrefabOriginAssetMetadataKey) ==
                PrefabAssetId &&
            actorOneMetadata->string_values.get(CharacterPrefabBaseAssetMetadataKey) ==
                BaseAssetId,
            "prefab placement lost prefab/base provenance"))
        return 1;

    bool actorOneScriptOK = false;
    StableId actorOneScriptId;
    for (const auto& attachment : prefabScripts.attachments)
    {
        if (attachment.ownerEntityId == actorOneId)
        {
            actorOneScriptId = attachment.scriptInstanceId;
            actorOneScriptOK = attachment.properties.size() == 1 &&
                attachment.properties.front().referenceId == actorOneId;
        }
    }
    if (!Require(actorOneScriptOK,
            "prefab script self reference was not remapped to new Character"))
        return 1;

    PlaceCharacterPrefabCommand placeTwo(
        prefabScene, makePreparedBase(), prefab,
        XMFLOAT3(4.0f, 0.0f, 1.0f), 1.0f,
        "Enemy Rifle Soldier", &prefabScripts);
    if (!Require(placeTwo.Execute(), "second Character Prefab placement failed"))
        return 1;
    const StableId actorTwoId =
        PersistentEntityId(prefabScene, placeTwo.PlacedEntity());
    if (!Require(IsValidStableId(actorTwoId) && actorTwoId != actorOneId,
            "separate prefab placements shared Character identity"))
        return 1;
    for (const auto& attachment : prefabScripts.attachments)
    {
        if (attachment.ownerEntityId == actorTwoId)
        {
            if (!Require(attachment.scriptInstanceId != actorOneScriptId,
                    "separate prefab placements shared ScriptInstanceId") ||
                !Require(attachment.properties.size() == 1 &&
                    attachment.properties.front().referenceId == actorTwoId,
                    "second prefab self-reference did not target second Character"))
                return 1;
        }
    }

    placeOne.Undo();
    if (!Require(!prefabScene.transforms.Contains(actorOne),
            "prefab placement Undo did not remove first Character"))
        return 1;
    if (!Require(placeOne.Execute(), "prefab placement Redo failed") ||
        !Require(PersistentEntityId(prefabScene, actorOne) == actorOneId,
            "prefab Redo changed authored Character identity"))
        return 1;
    bool redoneScriptOK = false;
    for (const auto& attachment : prefabScripts.attachments)
    {
        if (attachment.ownerEntityId == actorOneId &&
            attachment.scriptInstanceId == actorOneScriptId)
            redoneScriptOK = true;
    }
    if (!Require(redoneScriptOK,
            "prefab Redo did not restore authored script identity"))
        return 1;

    std::cout << "CW-05 CHARACTER PREFAB PASS\n";
    return 0;
}
