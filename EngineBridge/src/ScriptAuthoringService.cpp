#include "renegade/bridge/ScriptAuthoringService.h"

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/SceneService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    std::string NormalizedPathIdentity(const std::string& path)
    {
        std::string value = fs::u8path(path).lexically_normal().generic_u8string();
        std::transform(
            value.begin(), value.end(), value.begin(),
            [](const unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
        return value;
    }

    bool FileExistsAsRegular(
        const fs::path& path,
        bool& exists,
        std::string& error)
    {
        std::error_code ec;
        exists = fs::exists(path, ec);
        if (ec)
        {
            error = "Could not inspect scripting companion: " + ec.message();
            return false;
        }
        if (!exists)
        {
            error.clear();
            return true;
        }
        const bool regular = fs::is_regular_file(path, ec);
        if (ec)
        {
            error = "Could not inspect scripting companion type: " + ec.message();
            return false;
        }
        if (!regular)
        {
            error = "Scripting companion exists but is not a regular file.";
            return false;
        }
        error.clear();
        return true;
    }

    bool ProjectRelativeSceneHint(
        const std::string& projectRoot,
        const std::string& scenePath,
        std::string& hint,
        std::string& error)
    {
        std::error_code ec;
        const fs::path root = fs::weakly_canonical(fs::u8path(projectRoot), ec);
        if (ec || root.empty())
        {
            error = "Could not resolve project root for scripting: " + ec.message();
            return false;
        }
        const fs::path scene = fs::weakly_canonical(fs::u8path(scenePath), ec);
        if (ec || scene.empty())
        {
            error = "Could not resolve Scene path for scripting: " + ec.message();
            return false;
        }
        const fs::path relative = fs::relative(scene, root, ec);
        if (ec || relative.empty() || relative.is_absolute())
        {
            error = "Scene is not inside the active project root.";
            return false;
        }
        for (const auto& part : relative)
        {
            if (part == "..")
            {
                error = "Scene is not inside the active project root.";
                return false;
            }
        }
        hint = relative.lexically_normal().generic_u8string();
        error.clear();
        return true;
    }

    StableId ResolveEntityScriptAuthoringOwner(
        const wi::scene::Scene& scene,
        const StableId& requestedOwner)
    {
        if (!IsValidStableId(requestedOwner))
            return {};

        wi::ecs::Entity selected = wi::ecs::INVALID_ENTITY;
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const wi::ecs::Entity candidate = scene.metadatas.GetEntity(index);
            if (PersistentEntityId(scene, candidate) == requestedOwner)
            {
                selected = candidate;
                break;
            }
        }
        if (selected == wi::ecs::INVALID_ENTITY)
            return {};

        // Imported nodes inside a reusable asset are replaceable payload.
        // Creator-owned scripts belong to the nearest stable reusable
        // instance root so payload refresh/reimport cannot orphan them.
        wi::ecs::Entity current = selected;
        const std::size_t maximumDepth = scene.hierarchy.GetCount() + 1;
        for (std::size_t depth = 0;
            current != wi::ecs::INVALID_ENTITY && depth <= maximumDepth;
            ++depth)
        {
            const auto* metadata = scene.metadatas.GetComponent(current);
            if (metadata != nullptr &&
                metadata->string_values.has(
                    ReusableAssetInstanceIdMetadataKey))
            {
                const StableId wrapperId = PersistentEntityId(scene, current);
                return IsValidStableId(wrapperId)
                    ? wrapperId
                    : StableId{};
            }

            const auto* hierarchy = scene.hierarchy.GetComponent(current);
            if (hierarchy == nullptr ||
                hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                hierarchy->parentID == current)
            {
                break;
            }
            current = hierarchy->parentID;
        }

        return requestedOwner;
    }

    bool SceneContainsPersistentOwner(
        const wi::scene::Scene& scene,
        const StableId& ownerId)
    {
        if (!IsValidStableId(ownerId))
            return false;
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.metadatas.GetEntity(index);
            if (PersistentEntityId(scene, entity) == ownerId)
                return true;
        }
        return false;
    }

    bool SourceSortLess(
        const ScriptAuthoringSource& left,
        const ScriptAuthoringSource& right)
    {
        if (left.metadata.category != right.metadata.category)
            return left.metadata.category < right.metadata.category;
        if (left.metadata.name != right.metadata.name)
            return left.metadata.name < right.metadata.name;
        return left.sourcePath < right.sourcePath;
    }
}

namespace renegade::bridge
{
    ScriptAuthoringService::ScriptAuthoringService(
        SceneService& scenes,
        ProjectService& projects,
        CommandService& commands) noexcept
        : scenes_(&scenes)
        , projects_(&projects)
        , commands_(&commands)
    {
    }

    void ScriptAuthoringService::Invalidate() noexcept
    {
        document_ = {};
        loaded_ = false;
        companionExistedWhenLoaded_ = false;
        loadedSceneRevision_ = 0;
        loadedScenePath_.clear();
        loadedProjectId_.clear();
    }

    bool ScriptAuthoringService::LoadSceneDocument(
        const std::string& scenePath,
        ScriptDocument& document,
        bool& companionExists,
        std::string& scenePathHint,
        DocumentEnvelope& sceneEnvelope,
        std::string& error) const
    {
        if (projects_ == nullptr || !projects_->HasProject())
        {
            error = "Scripting requires an active Renegade project.";
            return false;
        }
        if (scenePath.empty())
        {
            error = "Save the Level before attaching scripts.";
            return false;
        }

        const auto& project = projects_->CurrentProject();
        if (!IsValidStableId(project.projectId))
        {
            error = "Active project has no valid stable project ID.";
            return false;
        }
        if (!ProjectRelativeSceneHint(
                project.rootPath,
                scenePath,
                scenePathHint,
                error))
        {
            return false;
        }

        if (!ReadDocumentEnvelope(scenePath + ".rmeta", sceneEnvelope, error))
        {
            error = "Could not read the Level identity for scripting: " + error;
            return false;
        }
        if (sceneEnvelope.projectId != project.projectId ||
            sceneEnvelope.documentType != SceneDocumentType ||
            !IsValidStableId(sceneEnvelope.documentId))
        {
            error = "Level identity does not belong to the active project.";
            return false;
        }

        const std::string companion = ScriptDocumentPathForScene(scenePath);
        if (!FileExistsAsRegular(
                fs::u8path(companion),
                companionExists,
                error))
        {
            return false;
        }
        if (companionExists)
        {
            if (!ReadScriptDocument(
                    companion,
                    project.projectId,
                    sceneEnvelope.documentId,
                    document,
                    error))
            {
                error = "Could not load the Level scripting companion: " + error;
                return false;
            }
        }
        else
        {
            document = CreateScriptDocument(
                project.projectId,
                sceneEnvelope.documentId,
                scenePathHint,
                "Renegade Studio S4B");
        }
        error.clear();
        return true;
    }

    bool ScriptAuthoringService::EnsureCurrent(std::string& error)
    {
        if (scenes_ == nullptr || projects_ == nullptr || commands_ == nullptr)
        {
            error = "Scripting authoring service is not bound to Studio.";
            return false;
        }
        if (!projects_->HasProject())
        {
            error = "Scripting requires an active Renegade project.";
            return false;
        }

        const std::string scenePath = scenes_->CurrentPath();
        const auto& project = projects_->CurrentProject();
        const std::string normalizedScene =
            fs::u8path(scenePath).lexically_normal().generic_u8string();
        if (loaded_ &&
            loadedSceneRevision_ == scenes_->Revision() &&
            loadedScenePath_ == normalizedScene &&
            loadedProjectId_ == project.projectId)
        {
            error.clear();
            return true;
        }

        const auto pruneOrphanedEntityAttachments =
            [&](ScriptDocument& candidate)
            {
                // Scene revision is a coarse lifecycle counter. A loaded
                // document can still outlive an imported/reusable entity
                // replacement without changing that counter, so this check
                // must also run on the fast path used by every inspector and
                // Test Level snapshot call.
                candidate.attachments.erase(
                    std::remove_if(
                        candidate.attachments.begin(),
                        candidate.attachments.end(),
                        [&](const ScriptAttachment& attachment)
                        {
                            return attachment.scope == ScriptScope::Entity &&
                                !SceneContainsPersistentOwner(
                                    scenes_->GetScene(),
                                    attachment.ownerEntityId);
                        }),
                    candidate.attachments.end());
                NormalizeScriptAttachmentOrder(candidate);
            };

        if (loaded_ &&
            loadedSceneRevision_ == scenes_->Revision() &&
            loadedScenePath_ == normalizedScene &&
            loadedProjectId_ == project.projectId)
        {
            pruneOrphanedEntityAttachments(document_);
            error.clear();
            return true;
        }

        ScriptDocument candidate;
        bool companionExists = false;
        std::string sceneHint;
        DocumentEnvelope sceneEnvelope;
        if (!LoadSceneDocument(
                scenePath,
                candidate,
                companionExists,
                sceneHint,
                sceneEnvelope,
                error))
        {
            return false;
        }

        // Drop only entity attachments whose owners are no longer present;
        // level-scoped scripts and all live owners remain authoritative.
        pruneOrphanedEntityAttachments(candidate);

        document_ = std::move(candidate);
        loaded_ = true;
        companionExistedWhenLoaded_ = companionExists;
        loadedSceneRevision_ = scenes_->Revision();
        loadedScenePath_ = normalizedScene;
        loadedProjectId_ = project.projectId;
        error.clear();
        return true;
    }

    bool ScriptAuthoringService::SaveForScene(
        const std::string& scenePath,
        const std::string& previousScenePath,
        std::string& error)
    {
        if (scenes_ == nullptr || projects_ == nullptr || commands_ == nullptr ||
            !projects_->HasProject())
        {
            error = "Scripting save requires an active Studio project.";
            return false;
        }

        const auto& project = projects_->CurrentProject();
        const std::string normalizedNew =
            fs::u8path(scenePath).lexically_normal().generic_u8string();
        const std::string normalizedPrevious =
            fs::u8path(previousScenePath).lexically_normal().generic_u8string();
        const bool saveAs = !normalizedPrevious.empty() &&
            normalizedPrevious != normalizedNew;

        ScriptDocument sourceDocument;
        bool haveSourceDocument = false;

        if (loaded_ && loadedProjectId_ == project.projectId &&
            (loadedScenePath_ == normalizedPrevious ||
             (!saveAs && loadedScenePath_ == normalizedNew)))
        {
            sourceDocument = document_;
            haveSourceDocument = true;
        }
        else if (saveAs)
        {
            bool oldExists = false;
            std::string oldHint;
            DocumentEnvelope oldSceneEnvelope;
            ScriptDocument oldDocument;
            if (!LoadSceneDocument(
                    previousScenePath,
                    oldDocument,
                    oldExists,
                    oldHint,
                    oldSceneEnvelope,
                    error))
            {
                return false;
            }
            if (oldExists)
            {
                sourceDocument = std::move(oldDocument);
                haveSourceDocument = true;
            }
        }

        // A Scene that has never loaded/created scripting state and has no old
        // companion does not need an empty .rscripts file merely because it was
        // saved.
        if (!haveSourceDocument && !loaded_)
        {
            error.clear();
            return true;
        }

        ScriptDocument target;
        bool newCompanionExists = false;
        std::string newHint;
        DocumentEnvelope newSceneEnvelope;
        if (!LoadSceneDocument(
                scenePath,
                target,
                newCompanionExists,
                newHint,
                newSceneEnvelope,
                error))
        {
            return false;
        }

        if (saveAs && haveSourceDocument)
        {
            ScriptDocument copied = CreateScriptDocument(
                project.projectId,
                newSceneEnvelope.documentId,
                newHint,
                "Renegade Studio S4B");
            copied.schemaVersion = sourceDocument.schemaVersion;
            copied.attachments = sourceDocument.attachments;
            copied.unknownRootFields = sourceDocument.unknownRootFields;
            copied.unknownDocumentFields = sourceDocument.unknownDocumentFields;
            copied.unknownScriptDocumentFields =
                sourceDocument.unknownScriptDocumentFields;
            copied.unknownSections = sourceDocument.unknownSections;
            target = std::move(copied);
        }
        else if (haveSourceDocument)
        {
            target = sourceDocument;
            target.sceneDocumentId = newSceneEnvelope.documentId;
            target.scenePathHint = newHint;
            target.envelope.projectId = project.projectId;
            target.envelope.documentType = ScriptDocumentType;
            if (!RetargetDocumentEnvelope(
                    target.envelope,
                    ScriptDocumentPathHintForScene(newHint),
                    error))
            {
                return false;
            }
        }

        if (!ValidateScriptDocument(target, error))
            return false;
        if (!ValidateScriptDocumentAgainstScene(
                target,
                scenes_->GetScene(),
                error))
        {
            return false;
        }
        if (!WriteScriptDocument(
                ScriptDocumentPathForScene(scenePath),
                target,
                error))
        {
            return false;
        }

        document_ = std::move(target);
        loaded_ = true;
        companionExistedWhenLoaded_ = true;
        loadedSceneRevision_ = scenes_->Revision();
        loadedScenePath_ = normalizedNew;
        loadedProjectId_ = project.projectId;
        error.clear();
        return true;
    }

    bool ScriptAuthoringService::EnumerateProjectSources(
        const ScriptPresentation presentation,
        std::vector<ScriptAuthoringSource>& sources,
        std::vector<ScriptMetadataDiagnostic>& diagnostics,
        std::string& error)
    {
        sources.clear();
        diagnostics.clear();
        if (!EnsureCurrent(error))
            return false;
        const auto& project = projects_->CurrentProject();
        const fs::path scriptsRoot =
            fs::u8path(project.rootPath) / "Content" / "Scripts";
        std::error_code ec;
        if (!fs::exists(scriptsRoot, ec))
        {
            if (ec)
            {
                error = "Could not inspect Content/Scripts: " + ec.message();
                return false;
            }
            error.clear();
            return true;
        }
        if (!fs::is_directory(scriptsRoot, ec) || ec)
        {
            error = "Content/Scripts is not a readable directory.";
            return false;
        }

        fs::recursive_directory_iterator iterator(
            scriptsRoot,
            fs::directory_options::skip_permission_denied,
            ec);
        const fs::recursive_directory_iterator end;
        if (ec)
        {
            error = "Could not enumerate Content/Scripts: " + ec.message();
            return false;
        }
        for (; iterator != end; iterator.increment(ec))
        {
            if (ec)
            {
                error = "Could not enumerate Content/Scripts: " + ec.message();
                return false;
            }
            std::error_code typeError;
            if (!iterator->is_regular_file(typeError) || typeError)
                continue;

            std::string extension = iterator->path().extension().generic_u8string();
            std::transform(
                extension.begin(), extension.end(), extension.begin(),
                [](const unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });
            if (extension != ".lua")
                continue;

            const fs::path relative = fs::relative(
                iterator->path(),
                fs::u8path(project.rootPath),
                typeError);
            if (typeError || relative.empty() || relative.is_absolute())
                continue;
            const std::string relativePath =
                relative.lexically_normal().generic_u8string();
            auto evaluated = EvaluateScriptMetadata(
                project.rootPath,
                relativePath);
            if (!evaluated.succeeded)
            {
                diagnostics.insert(
                    diagnostics.end(),
                    evaluated.diagnostics.begin(),
                    evaluated.diagnostics.end());
                continue;
            }
            if (evaluated.descriptor.presentation != presentation)
                continue;

            ScriptAuthoringSource source;
            source.sourcePath = relativePath;
            source.metadata = std::move(evaluated.descriptor);
            source.binding.sourcePath = relativePath;
            source.binding.presentation = presentation;
            source.binding.apiVersion = 1;
            source.binding.unsafe = false;
            source.binding.provenance.kind = ScriptProvenanceKind::Project;
            source.binding = ResolveSourceBinding(source);
            sources.push_back(std::move(source));
        }

        std::sort(sources.begin(), sources.end(), SourceSortLess);
        error.clear();
        return true;
    }

    ScriptSourceBinding ScriptAuthoringService::ResolveSourceBinding(
        const ScriptAuthoringSource& source) const
    {
        const std::string identity = NormalizedPathIdentity(source.sourcePath);
        if (loaded_)
        {
            for (const auto& attachment : document_.attachments)
            {
                if (NormalizedPathIdentity(attachment.sourcePath) == identity)
                    return CaptureScriptSourceBinding(attachment);
            }
        }

        ScriptSourceBinding binding = source.binding;
        if (!IsValidStableId(binding.sourceId))
            binding.sourceId = GenerateStableId();
        binding.sourcePath = source.sourcePath;
        binding.presentation = source.metadata.presentation;
        binding.apiVersion = 1;
        binding.unsafe = false;
        binding.provenance.kind = ScriptProvenanceKind::Project;
        return binding;
    }

    std::vector<const ScriptAttachment*> ScriptAuthoringService::EntityAttachments(
        const StableId& ownerEntityId,
        const ScriptPresentation presentation) const
    {
        std::vector<const ScriptAttachment*> result;
        if (!loaded_ || !IsValidStableId(ownerEntityId) || scenes_ == nullptr)
            return result;

        const StableId resolvedOwner = ResolveEntityScriptAuthoringOwner(
            scenes_->GetScene(), ownerEntityId);
        if (!IsValidStableId(resolvedOwner))
            return result;

        for (const auto& attachment : document_.attachments)
        {
            if (attachment.scope == ScriptScope::Entity &&
                attachment.ownerEntityId == resolvedOwner &&
                attachment.presentation == presentation)
            {
                result.push_back(&attachment);
            }
        }
        std::sort(
            result.begin(), result.end(),
            [](const ScriptAttachment* left, const ScriptAttachment* right)
            {
                return left->order < right->order;
            });
        return result;
    }

    std::vector<const ScriptAttachment*> ScriptAuthoringService::LevelAttachments(
        const ScriptPresentation presentation) const
    {
        std::vector<const ScriptAttachment*> result;
        if (!loaded_)
            return result;
        for (const auto& attachment : document_.attachments)
        {
            if (attachment.scope == ScriptScope::Level &&
                attachment.ownerEntityId.empty() &&
                attachment.presentation == presentation)
            {
                result.push_back(&attachment);
            }
        }
        std::sort(
            result.begin(), result.end(),
            [](const ScriptAttachment* left, const ScriptAttachment* right)
            {
                return left->order < right->order;
            });
        return result;
    }

    bool ScriptAuthoringService::AttachEntitySource(
        const StableId& ownerEntityId,
        const ScriptAuthoringSource& source,
        StableId& scriptInstanceId,
        std::string& error)
    {
        if (!EnsureCurrent(error))
            return false;
        if (!IsValidStableId(ownerEntityId))
        {
            error = "Selected entity has no valid persistent Renegade ID.";
            return false;
        }

        const StableId resolvedOwner = ResolveEntityScriptAuthoringOwner(
            scenes_->GetScene(), ownerEntityId);
        if (!IsValidStableId(resolvedOwner))
        {
            error = "Selected entity no longer resolves in the active Scene.";
            return false;
        }

        EntityIdentityIndex identities;
        if (!identities.Build(scenes_->GetScene(), error))
        {
            error = "Cannot attach script because Scene identity is invalid: " + error;
            return false;
        }
        if (identities.Resolve(resolvedOwner) == wi::ecs::INVALID_ENTITY)
        {
            error = "Resolved script owner does not exist in the active Scene.";
            return false;
        }

        if (source.metadata.presentation == ScriptPresentation::GlobalScript)
        {
            error = "GLOBAL SCRIPT sources must be attached at Level scope.";
            return false;
        }

        ScriptAttachment attachment = CreateScriptAttachment(
            ScriptScope::Entity,
            resolvedOwner,
            ResolveSourceBinding(source));
        if (!ApplyScriptMetadataDefaults(source.metadata, attachment, error))
            return false;
        scriptInstanceId = attachment.scriptInstanceId;
        auto command = MakeAddScriptAttachmentCommand(
            document_,
            std::move(attachment),
            error);
        if (!command)
            return false;
        if (!commands_->Execute(std::move(command)))
        {
            error = "Could not attach script through the Studio Undo/Redo stack.";
            return false;
        }
        error.clear();
        return true;
    }

    bool ScriptAuthoringService::AttachLevelSource(
        const ScriptAuthoringSource& source,
        StableId& scriptInstanceId,
        std::string& error)
    {
        if (!EnsureCurrent(error))
            return false;
        if (source.metadata.presentation != ScriptPresentation::GlobalScript)
        {
            error = "Only GLOBAL SCRIPT sources can be attached at Level scope.";
            return false;
        }

        ScriptAttachment attachment = CreateScriptAttachment(
            ScriptScope::Level,
            {},
            ResolveSourceBinding(source));
        if (!ApplyScriptMetadataDefaults(source.metadata, attachment, error))
            return false;
        scriptInstanceId = attachment.scriptInstanceId;
        auto command = MakeAddScriptAttachmentCommand(
            document_,
            std::move(attachment),
            error);
        if (!command)
            return false;
        if (!commands_->Execute(std::move(command)))
        {
            error = "Could not attach GLOBAL SCRIPT through the Studio Undo/Redo stack.";
            return false;
        }
        error.clear();
        return true;
    }

    bool ScriptAuthoringService::RemoveAttachment(
        const StableId& scriptInstanceId,
        std::string& error)
    {
        if (!EnsureCurrent(error))
            return false;
        auto command = MakeRemoveScriptAttachmentCommand(
            document_, scriptInstanceId, error);
        if (!command)
            return false;
        if (!commands_->Execute(std::move(command)))
        {
            error = "Could not remove script through the Studio Undo/Redo stack.";
            return false;
        }
        error.clear();
        return true;
    }

    bool ScriptAuthoringService::SetAttachmentEnabled(
        const StableId& scriptInstanceId,
        const bool enabled,
        std::string& error)
    {
        if (!EnsureCurrent(error))
            return false;
        auto command = MakeSetScriptEnabledCommand(
            document_, scriptInstanceId, enabled, error);
        if (!command)
            return false;
        if (!commands_->Execute(std::move(command)))
        {
            error = "Could not change script state through the Studio Undo/Redo stack.";
            return false;
        }
        error.clear();
        return true;
    }

    bool ScriptAuthoringService::MoveAttachment(
        const StableId& scriptInstanceId,
        const std::uint32_t newOrder,
        std::string& error)
    {
        if (!EnsureCurrent(error))
            return false;
        auto command = MakeMoveScriptAttachmentCommand(
            document_, scriptInstanceId, newOrder, error);
        if (!command)
            return false;
        if (!commands_->Execute(std::move(command)))
        {
            error = "Could not reorder script through the Studio Undo/Redo stack.";
            return false;
        }
        error.clear();
        return true;
    }
}
