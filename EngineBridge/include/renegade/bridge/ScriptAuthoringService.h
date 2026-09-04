#pragma once

#include "renegade/bridge/ScriptDocumentService.h"
#include "renegade/bridge/ScriptMetadataService.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    class CommandService;
    class ProjectService;
    class SceneService;

    struct ScriptAuthoringSource
    {
        std::string sourcePath;
        ScriptMetadataDescriptor metadata;
        ScriptSourceBinding binding;
    };

    // Studio-facing document authority for creator scripting. The service owns
    // the live .rscripts model; S2 command objects mutate that model through the
    // shared CommandService so scripting participates in the normal Scene
    // Undo/Redo + dirty-state history instead of inventing a second editor
    // stack. Disk writes remain transactional and occur at the Scene save
    // boundary.
    class ScriptAuthoringService final
    {
    public:
        ScriptAuthoringService(
            SceneService& scenes,
            ProjectService& projects,
            CommandService& commands) noexcept;

        // Lazily bind to the active saved Scene. A new unsaved Scene has no
        // stable document identity yet and therefore cannot own .rscripts.
        [[nodiscard]] bool EnsureCurrent(std::string& error);
        void Invalidate() noexcept;

        [[nodiscard]] bool IsLoaded() const noexcept
        {
            return loaded_;
        }

        [[nodiscard]] const ScriptDocument* Document() const noexcept
        {
            return loaded_ ? &document_ : nullptr;
        }

        [[nodiscard]] ScriptDocument* Document() noexcept
        {
            return loaded_ ? &document_ : nullptr;
        }

        [[nodiscard]] const std::string& LoadedScenePath() const noexcept
        {
            return loadedScenePath_;
        }

        // Called after the WISCENE save succeeds. previousScenePath preserves
        // Save As semantics: an existing companion is cloned with a fresh
        // document envelope while ScriptInstanceId/sourceId identity remains
        // stable in the copied Scene.
        [[nodiscard]] bool SaveForScene(
            const std::string& scenePath,
            const std::string& previousScenePath,
            std::string& error);

        [[nodiscard]] bool EnumerateProjectSources(
            ScriptPresentation presentation,
            std::vector<ScriptAuthoringSource>& sources,
            std::vector<ScriptMetadataDiagnostic>& diagnostics,
            std::string& error);

        [[nodiscard]] std::vector<const ScriptAttachment*> EntityAttachments(
            const StableId& ownerEntityId,
            ScriptPresentation presentation) const;

        [[nodiscard]] bool AttachEntitySource(
            const StableId& ownerEntityId,
            const ScriptAuthoringSource& source,
            StableId& scriptInstanceId,
            std::string& error);
        [[nodiscard]] bool RemoveAttachment(
            const StableId& scriptInstanceId,
            std::string& error);
        [[nodiscard]] bool SetAttachmentEnabled(
            const StableId& scriptInstanceId,
            bool enabled,
            std::string& error);
        [[nodiscard]] bool MoveAttachment(
            const StableId& scriptInstanceId,
            std::uint32_t newOrder,
            std::string& error);

    private:
        [[nodiscard]] bool LoadSceneDocument(
            const std::string& scenePath,
            ScriptDocument& document,
            bool& companionExists,
            std::string& scenePathHint,
            DocumentEnvelope& sceneEnvelope,
            std::string& error) const;
        [[nodiscard]] ScriptSourceBinding ResolveSourceBinding(
            const ScriptAuthoringSource& source) const;

        SceneService* scenes_ = nullptr;
        ProjectService* projects_ = nullptr;
        CommandService* commands_ = nullptr;

        ScriptDocument document_;
        bool loaded_ = false;
        bool companionExistedWhenLoaded_ = false;
        std::uint64_t loadedSceneRevision_ = 0;
        std::string loadedScenePath_;
        StableId loadedProjectId_;
    };
}
