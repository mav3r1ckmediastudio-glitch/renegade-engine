#pragma once

#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/HumanoidRetargetService.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct CreatorExternalAnimationClip
    {
        std::string localSourcePath;
        std::string sourceDisplayName;
        HumanoidAnimationSourceFormat sourceFormat = HumanoidAnimationSourceFormat::Unknown;
        std::uint32_t sourceAnimationIndex = 0;
        std::string sourceActionName;
        std::string name;
        float start = 0.0f;
        float end = 0.0f;
        bool enabled = true;
    };

    struct CreatorExternalAnimationQueueSnapshot
    {
        std::string projectRoot;
        std::vector<CreatorExternalAnimationClip> clips;
        std::string error;
    };

    // Starts a non-mutating Character-import animation session. Selecting files
    // only inspects them in temporary Wicked scenes; SourceAssets is not touched
    // until the final governed import transaction stages the queue.
    void BeginCreatorExternalAnimationImportSession(const std::string& projectRoot);
    void ClearCreatorExternalAnimationImportSession() noexcept;

    // Inspects one external animation source using the same native Wicked
    // importers accepted by HumanoidRetargetService. Every native action/take in
    // the source becomes one queue row. Re-adding an already queued source is a
    // no-op rather than duplicating all of its actions.
    [[nodiscard]] bool QueueCreatorExternalAnimationSource(
        const std::string& sourcePath,
        std::string& error);

    [[nodiscard]] CreatorExternalAnimationQueueSnapshot
    CaptureCreatorExternalAnimationQueue();

    [[nodiscard]] bool RenameCreatorExternalAnimationClip(
        std::size_t index,
        const std::string& name,
        std::string& error);

    [[nodiscard]] bool SetCreatorExternalAnimationClipEnabled(
        std::size_t index,
        bool enabled,
        std::string& error);

    [[nodiscard]] bool RemoveCreatorExternalAnimationClip(
        std::size_t index,
        std::string& error);

    // Value-based final-commit primitive. This is deliberately separate from
    // source inspection so tests and future import transactions can freeze a
    // queue snapshot before any filesystem mutation begins.
    [[nodiscard]] bool StageCreatorExternalAnimationClipsForRecipe(
        const std::string& projectRoot,
        const std::vector<CreatorExternalAnimationClip>& clips,
        std::vector<CreatorExternalAnimationImportRecipe>& recipe,
        std::string& error);

    // Stages the currently active importer queue. Selected local files are
    // retained beneath project-owned SourceAssets/Animations/Snapshots and only
    // project-relative retained paths are emitted into the durable recipe.
    [[nodiscard]] bool StageCreatorExternalAnimationsForRecipe(
        const std::string& projectRoot,
        std::vector<CreatorExternalAnimationImportRecipe>& recipe,
        std::string& error);

    [[nodiscard]] inline bool StagePendingCreatorExternalAnimationsForRecipe(
        std::vector<CreatorExternalAnimationImportRecipe>& recipe,
        std::string& error)
    {
        const auto snapshot = CaptureCreatorExternalAnimationQueue();
        if (snapshot.clips.empty())
        {
            recipe.clear();
            error.clear();
            return true;
        }
        return StageCreatorExternalAnimationsForRecipe(
            snapshot.projectRoot, recipe, error);
    }
}
