#include "CharacterPrefabStudioIntegration.h"

#include "RenegadeStudioChrome.h"

#include "renegade/bridge/CharacterPrefabService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/ScriptDocumentService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace renegade::studio
{
    namespace
    {
        bool CurrentStudioScene(
            wi::scene::Scene& scene,
            bridge::StudioSession*& session,
            std::string& error)
        {
            session = bridge::StudioSession::Current();
            if (session == nullptr ||
                &session->Scenes().GetScene() != &scene ||
                !session->Projects().HasProject())
            {
                error = "CW-05 Studio companion requires the active project Scene.";
                return false;
            }
            error.clear();
            return true;
        }

        bool HasEntityScripts(
            const bridge::ScriptDocument& document,
            const bridge::StableId& ownerId) noexcept
        {
            return std::any_of(
                document.attachments.begin(),
                document.attachments.end(),
                [&ownerId](const bridge::ScriptAttachment& attachment)
                {
                    return attachment.scope == bridge::ScriptScope::Entity &&
                        attachment.ownerEntityId == ownerId;
                });
        }

        bool PrepareScriptDocument(
            bridge::StudioSession& session,
            bridge::ScriptDocument*& document,
            std::string& error)
        {
            document = nullptr;
            if (session.Scenes().CurrentPath().empty())
            {
                error = "Save the Level before authoring reusable Character scripts.";
                return false;
            }
            if (!session.Scripts().EnsureCurrent(error))
                return false;
            document = session.Scripts().Document();
            if (document == nullptr)
            {
                error = "The current Level scripting document is unavailable.";
                return false;
            }
            error.clear();
            return true;
        }

        void RemoveTransientPrefabMarker(wi::scene::Scene& scene) noexcept
        {
            // The marker exists only to carry the portable prefab layer through
            // the existing prepared-asset/drag-preview pipeline. Never persist
            // it into a WISCENE snapshot after the layer has been consumed.
            for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
            {
                scene.metadatas[index].string_values.erase(
                    bridge::CharacterPrefabPlacementTemplateMetadataKey);
            }
        }

        bool ApplyPrefabScripts(
            bridge::ScriptDocument& document,
            const bridge::CharacterPrefabDocument& prefab,
            const bridge::StableId& ownerId,
            std::string& error)
        {
            for (const auto& scriptTemplate : prefab.scripts)
            {
                bridge::ScriptAttachment attachment =
                    bridge::CreateScriptAttachment(
                        bridge::ScriptScope::Entity,
                        ownerId,
                        scriptTemplate.source);
                attachment.enabled = scriptTemplate.enabled;
                attachment.order = scriptTemplate.order;
                for (const auto& propertyTemplate : scriptTemplate.properties)
                {
                    bridge::ScriptPropertyValue property = propertyTemplate.value;
                    if (propertyTemplate.selfEntityReference)
                    {
                        property.referenceId = ownerId;
                        property.pathHint.clear();
                    }
                    attachment.properties.push_back(std::move(property));
                }
                if (!bridge::AddScriptAttachment(
                        document, std::move(attachment), error))
                {
                    return false;
                }
            }
            error.clear();
            return true;
        }

        bool ApplyPrefabSceneLayer(
            wi::scene::Scene& scene,
            const wi::ecs::Entity instanceRoot,
            const bridge::CharacterPrefabDocument& prefab,
            std::string& error)
        {
            if (!bridge::IsRenegadeCharacter(scene, instanceRoot))
            {
                error =
                    "Character Prefab placement did not produce a governed Character instance.";
                return false;
            }

            const auto current = bridge::CaptureCharacterSettings(scene, instanceRoot);
            if (current != prefab.settings)
            {
                bridge::SetCharacterSettingsCommand settings(
                    scene, instanceRoot, prefab.settings);
                if (!settings.Execute())
                {
                    error = "Character Prefab gameplay settings could not be applied.";
                    return false;
                }
            }

            bridge::CharacterAdvancedOverrides overrides;
            if (!bridge::DeserializeCharacterAdvancedOverrides(
                    prefab.advancedOverrides, overrides, error) ||
                !bridge::ApplyCharacterAdvancedOverrides(
                    scene, instanceRoot, overrides, error))
            {
                return false;
            }

            auto* metadata = scene.metadatas.GetComponent(instanceRoot);
            if (metadata == nullptr)
                metadata = &scene.metadatas.Create(instanceRoot);

            // The live reusable instance remains bound to the physical base
            // Character Asset. Prefab provenance is a separate authoring layer,
            // so reimport/runtime refresh never mistakes .rcharprefab for .rasset.
            metadata->string_values.set(
                bridge::ReusableAssetInstanceIdMetadataKey,
                prefab.baseCharacterAssetId);
            metadata->string_values.set(
                bridge::CharacterPrefabOriginAssetMetadataKey,
                prefab.assetId);
            metadata->string_values.set(
                bridge::CharacterPrefabBaseAssetMetadataKey,
                prefab.baseCharacterAssetId);
            metadata->int_values.set(
                bridge::CharacterPrefabVersionMetadataKey,
                static_cast<int>(bridge::CharacterPrefabSchemaVersion));

            RemoveTransientPrefabMarker(scene);
            error.clear();
            return true;
        }
    }

    void InstallCharacterPrefabStudioIntegration()
    {
        bridge::SetDuplicateEntityCompanionFactory(
            [](wi::scene::Scene& scene,
               const wi::ecs::Entity source,
               const wi::ecs::Entity duplicate,
               bridge::DuplicateEntityCompanionCallbacks& callbacks,
               std::string& error)
            {
                callbacks = {};
                bridge::StudioSession* session = nullptr;
                if (!CurrentStudioScene(scene, session, error))
                    return false;

                // Unsaved Levels cannot own a governed .rscripts companion, so
                // there is no external authoring state to duplicate yet.
                if (session->Scenes().CurrentPath().empty())
                {
                    error.clear();
                    return true;
                }

                bridge::ScriptDocument* document = nullptr;
                if (!PrepareScriptDocument(*session, document, error))
                    return false;

                const bridge::StableId sourceId =
                    bridge::PersistentEntityId(scene, source);
                const bridge::StableId duplicateId =
                    bridge::PersistentEntityId(scene, duplicate);
                if (!bridge::IsValidStableId(sourceId) ||
                    !bridge::IsValidStableId(duplicateId))
                {
                    error = "Duplicated Character lacks stable Scene identity.";
                    return false;
                }
                if (!HasEntityScripts(*document, sourceId))
                {
                    error.clear();
                    return true;
                }

                const bridge::ScriptDocument before = *document;
                std::vector<bridge::StableId> created;
                if (!bridge::DuplicateEntityScriptAttachments(
                        *document,
                        sourceId,
                        duplicateId,
                        created,
                        error))
                {
                    *document = before;
                    return false;
                }
                const bridge::ScriptDocument after = *document;
                callbacks.undo = [document, before]() mutable
                {
                    *document = before;
                };
                callbacks.redo = [document, after]() mutable
                {
                    *document = after;
                    return true;
                };
                error.clear();
                return true;
            });

        bridge::SetReusablePlacementCompanionFactory(
            [](wi::scene::Scene& scene,
               const wi::ecs::Entity instanceRoot,
               const wi::ecs::Entity payloadRoot,
               bridge::ReusablePlacementCompanionCallbacks& callbacks,
               std::string& error)
            {
                callbacks = {};
                bridge::CharacterPrefabDocument prefab;
                std::string markerError;
                const bool hasPrefab =
                    bridge::ReadCharacterPrefabPlacementTemplate(
                        scene, payloadRoot, prefab, markerError);
                if (!hasPrefab)
                {
                    if (!markerError.empty())
                    {
                        error = "Character Prefab placement marker is invalid: " +
                            markerError;
                        return false;
                    }
                    error.clear();
                    return true;
                }

                bridge::StudioSession* session = nullptr;
                if (!CurrentStudioScene(scene, session, error))
                    return false;
                const auto& project = session->Projects().CurrentProject();
                if (prefab.projectId != project.projectId)
                {
                    error = "Character Prefab belongs to a different project.";
                    return false;
                }

                if (!ApplyPrefabSceneLayer(
                        scene, instanceRoot, prefab, error))
                {
                    return false;
                }

                if (prefab.scripts.empty())
                {
                    error.clear();
                    return true;
                }

                bridge::ScriptDocument* document = nullptr;
                if (!PrepareScriptDocument(*session, document, error))
                    return false;
                const bridge::StableId ownerId =
                    bridge::PersistentEntityId(scene, instanceRoot);
                if (!bridge::IsValidStableId(ownerId))
                {
                    error = "Placed Character Prefab lacks stable Scene identity.";
                    return false;
                }

                const bridge::ScriptDocument before = *document;
                if (!ApplyPrefabScripts(*document, prefab, ownerId, error))
                {
                    *document = before;
                    return false;
                }
                const bridge::ScriptDocument after = *document;
                callbacks.undo = [document, before]() mutable
                {
                    *document = before;
                };
                callbacks.redo = [document, after]() mutable
                {
                    *document = after;
                    return true;
                };
                error.clear();
                return true;
            });
    }

    bool SaveSelectedCharacterPrefab(
        const wi::ecs::Entity character,
        std::string& status)
    {
        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Projects().HasProject())
        {
            status = "CW-05 // open a project before saving a Character Prefab";
            return false;
        }
        auto& scene = session->Scenes().GetScene();
        if (!bridge::IsRenegadeCharacter(scene, character))
        {
            status = "CW-05 // select a configured Character first";
            return false;
        }

        const bridge::ScriptDocument* scriptDocument = nullptr;
        if (!session->Scenes().CurrentPath().empty())
        {
            std::string scriptError;
            if (!session->Scripts().EnsureCurrent(scriptError))
            {
                status = "CW-05 // could not read Character scripts: " + scriptError;
                return false;
            }
            scriptDocument = session->Scripts().Document();
        }

        std::string prefabName = "Character Prefab";
        if (const auto* name = scene.names.GetComponent(character);
            name != nullptr && !name->name.empty())
        {
            prefabName = name->name;
        }

        const auto& project = session->Projects().CurrentProject();
        bridge::CharacterPrefabSaveRequest request;
        request.projectRoot = project.rootPath;
        request.projectId = project.projectId;
        request.scene = &scene;
        request.character = character;
        request.prefabName = std::move(prefabName);
        request.scriptDocument = scriptDocument;

        const bridge::CharacterPrefabSaveResult result =
            bridge::SaveCharacterPrefab(request);
        if (!result.succeeded)
        {
            status = "CW-05 // Character Prefab save failed: " + result.error;
            return false;
        }

        // RevealCreatorAsset() refreshes and selects the newly registered card.
        // Its LP07 return contract still describes imported model products, so
        // a generated prefab can refresh successfully while returning a benign
        // model-specific warning. The CW-05 save result remains authoritative.
        std::string revealWarning;
        if (auto* chrome = CreatorAssetStudioChrome::Current(); chrome != nullptr)
        {
            (void)chrome->RevealCreatorAsset(
                result.assetId,
                result.projectRelativePath,
                revealWarning);
        }

        status = "CW-05 // CHARACTER PREFAB SAVED";
        if (!result.warnings.empty())
            status += " // " + result.warnings.front();
        return true;
    }
}
