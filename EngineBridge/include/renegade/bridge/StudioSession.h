#pragma once

#include <string>
#include <utility>
#include <vector>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptAuthoringService.h"
#include "renegade/bridge/StudioProjectService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/VideoAssetService.h"

namespace renegade::bridge
{
    class StudioSession
    {
    public:
        StudioSession() noexcept
        {
            current_ = this;

            // Entity duplication is a single editor Undo/Redo action even when
            // part of its governed authoring state lives in the Scene's
            // companion .rscripts document. Register the active Studio session
            // as that companion boundary. DuplicateEntityCommand has already
            // assigned the new hierarchy persistent IDs before this callback.
            SetDuplicateEntityCompanionFactory(
                [this](
                    wi::scene::Scene& scene,
                    const wi::ecs::Entity source,
                    const wi::ecs::Entity duplicate,
                    DuplicateEntityCompanionCallbacks& callbacks,
                    std::string& error) -> bool
                {
                    callbacks = {};
                    if (&scenes_.GetScene() != &scene || !projects_.HasProject())
                    {
                        error.clear();
                        return true;
                    }

                    // Renegade does not allow script attachment to an unsaved
                    // Level, so there is no companion state to preserve here.
                    if (scenes_.CurrentPath().empty())
                    {
                        error.clear();
                        return true;
                    }
                    if (!scripts_.EnsureCurrent(error))
                        return false;

                    ScriptDocument* document = scripts_.Document();
                    if (document == nullptr)
                    {
                        error = "The active scripting companion is unavailable during duplication.";
                        return false;
                    }

                    const StableId sourceId = PersistentEntityId(scene, source);
                    const StableId duplicateId = PersistentEntityId(scene, duplicate);
                    if (!IsValidStableId(sourceId) || !IsValidStableId(duplicateId) ||
                        sourceId == duplicateId)
                    {
                        error = "Scene duplication did not establish two distinct persistent entity IDs.";
                        return false;
                    }

                    bool hasSourceAttachments = false;
                    for (const auto& attachment : document->attachments)
                    {
                        if (attachment.scope == ScriptScope::Entity &&
                            attachment.ownerEntityId == sourceId)
                        {
                            hasSourceAttachments = true;
                            break;
                        }
                    }
                    if (!hasSourceAttachments)
                    {
                        error.clear();
                        return true;
                    }

                    const ScriptDocument before = *document;
                    std::vector<StableId> created;
                    if (!DuplicateEntityScriptAttachments(
                            *document,
                            sourceId,
                            duplicateId,
                            created,
                            error))
                    {
                        *document = before;
                        return false;
                    }

                    // A property explicitly targeting the source actor is a
                    // self-reference for duplication purposes. Repoint only
                    // that reference to the new actor; references to other
                    // same-Scene entities intentionally remain unchanged.
                    for (const StableId& scriptId : created)
                    {
                        auto* attachment = FindScriptAttachment(*document, scriptId);
                        if (attachment == nullptr)
                        {
                            *document = before;
                            error = "Duplicated script attachment disappeared before reference remap.";
                            return false;
                        }
                        for (auto& property : attachment->properties)
                        {
                            if (property.type == ScriptPropertyType::EntityReference &&
                                property.referenceId == sourceId)
                            {
                                property.referenceId = duplicateId;
                                property.pathHint.clear();
                            }
                        }
                    }
                    if (!ValidateScriptDocumentAgainstScene(*document, scene, error))
                    {
                        *document = before;
                        return false;
                    }

                    const ScriptDocument after = *document;
                    callbacks.undo = [this, before]() mutable
                    {
                        if (auto* active = scripts_.Document())
                            *active = before;
                    };
                    callbacks.redo = [this, after]() mutable -> bool
                    {
                        auto* active = scripts_.Document();
                        if (active == nullptr)
                            return false;
                        *active = after;
                        return true;
                    };
                    error.clear();
                    return true;
                });
        }

        ~StudioSession()
        {
            if (current_ == this)
            {
                ClearDuplicateEntityCompanionFactory();
                current_ = nullptr;
            }
        }

        StudioSession(const StudioSession&) = delete;
        StudioSession& operator=(const StudioSession&) = delete;
        StudioSession(StudioSession&&) = delete;
        StudioSession& operator=(StudioSession&&) = delete;

        [[nodiscard]] static StudioSession* Current() noexcept
        {
            return current_;
        }

        [[nodiscard]] SceneService& Scenes() noexcept
        {
            return scenes_;
        }

        [[nodiscard]] const SceneService& Scenes() const noexcept
        {
            return scenes_;
        }

        [[nodiscard]] SelectionService& Selection() noexcept
        {
            return selection_;
        }

        [[nodiscard]] const SelectionService& Selection() const noexcept
        {
            return selection_;
        }

        [[nodiscard]] SceneDocumentService& Documents() noexcept
        {
            return documents_;
        }

        [[nodiscard]] const SceneDocumentService& Documents() const noexcept
        {
            return documents_;
        }

        [[nodiscard]] CommandService& Commands() noexcept
        {
            return commands_;
        }

        [[nodiscard]] StudioProjectService& Projects() noexcept
        {
            return projects_;
        }

        [[nodiscard]] const StudioProjectService& Projects() const noexcept
        {
            return projects_;
        }

        [[nodiscard]] ScriptAuthoringService& Scripts() noexcept
        {
            return scripts_;
        }

        [[nodiscard]] const ScriptAuthoringService& Scripts() const noexcept
        {
            return scripts_;
        }

        void NewScene()
        {
            documents_.NewScene();
            scripts_.Invalidate();
        }

        bool CommitPendingProjectScene(PreparedSceneOpen prepared)
        {
            if (!projects_.HasPendingProject())
            {
                const bool committed =
                    documents_.CommitPreparedOpen(std::move(prepared));
                if (committed)
                {
                    scripts_.Invalidate();
                    RestoreGovernedVideoBindingsAfterOpen();
                }
                return committed;
            }

            if (!prepared.IsReady())
            {
                const bool ignored =
                    documents_.CommitPreparedOpen(std::move(prepared));
                (void)ignored;
                projects_.DiscardPendingProject();
                return false;
            }

            if (!projects_.CommitPendingProject())
            {
                scenes_.SetLastError(
                    "Project adoption failed after startup-scene validation: " +
                    projects_.LastError());
                projects_.DiscardPendingProject();
                return false;
            }

            const bool committed =
                documents_.CommitPreparedOpen(std::move(prepared));
            if (committed)
            {
                scripts_.Invalidate();
                RestoreGovernedVideoBindingsAfterOpen();
            }
            return committed;
        }

        bool CommitPendingProjectWithoutScene()
        {
            if (!projects_.HasPendingProject())
            {
                scenes_.SetLastError(
                    "No Story Flow project is pending adoption.");
                return false;
            }
            if (!projects_.PendingProject().startupScene.empty())
            {
                scenes_.SetLastError(
                    "Scene-first projects require the prepared Scene adoption boundary.");
                projects_.DiscardPendingProject();
                return false;
            }
            if (!projects_.CommitPendingProject())
            {
                scenes_.SetLastError(
                    "Project adoption failed after startup-Flow validation: " +
                    projects_.LastError());
                projects_.DiscardPendingProject();
                return false;
            }
            documents_.NewScene();
            scripts_.Invalidate();
            return true;
        }

        bool LoadScene(const std::string& filePath)
        {
            return CommitPendingProjectScene(documents_.PrepareOpen(filePath));
        }

        bool SaveScene(const std::string& filePath)
        {
            const std::string previousPath = scenes_.CurrentPath();
            if (!documents_.Save(filePath))
                return false;

            if (!projects_.HasProject())
                return true;

            std::string scriptError;
            if (!scripts_.SaveForScene(filePath, previousPath, scriptError))
            {
                commands_.MarkUnsaved();
                scenes_.SetLastError(
                    "Scene saved but scripting companion failed: " +
                    scriptError);
                return false;
            }
            return true;
        }

        bool ReloadScene()
        {
            const bool reloaded = documents_.Reload();
            if (reloaded)
            {
                scripts_.Invalidate();
                RestoreGovernedVideoBindingsAfterOpen();
            }
            return reloaded;
        }

    private:
        void RestoreGovernedVideoBindingsAfterOpen() noexcept
        {
            if (!projects_.HasProject())
                return;
            const auto& project = projects_.CurrentProject();
            const auto restored = RestoreVideoAssetBindings(
                scenes_.GetScene(), project.rootPath, project.projectId);
            if (!restored.succeeded)
            {
                scenes_.SetLastError(
                    "Scene opened but governed video restoration failed: " +
                    restored.error);
            }
        }

        inline static StudioSession* current_ = nullptr;

        StudioProjectService projects_;
        SceneService scenes_;
        SelectionService selection_;
        CommandService commands_;
        SceneDocumentService documents_{
            scenes_, selection_, commands_, projects_};
        ScriptAuthoringService scripts_{scenes_, projects_, commands_};
    };
}
