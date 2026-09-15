#pragma once

#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/HumanoidRetargetService.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
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

    // Low-level native source inspection. This retains the CW-02 capability to
    // inspect every Wicked action/take in a source for governed recipe tooling.
    // Creator-facing file-slot ingestion should use
    // QueueCreatorExternalAnimationFileSlot() below so one selected file maps to
    // exactly one visible/importable Character animation slot.
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

    // Creator-facing GGMAX-style contract: one external animation file is one
    // slot. The native source inspector is still used underneath so the source
    // is validated by Wicked, but only its first usable action is surfaced for
    // this file-based workflow. Re-adding an existing file is a no-op and keeps
    // any creator rename already applied to that slot.
    [[nodiscard]] inline bool QueueCreatorExternalAnimationFileSlot(
        const std::string& sourcePath,
        std::string& error)
    {
        const auto sameSourcePath = [&](const std::string& queuedPath)
        {
            if (queuedPath == sourcePath)
                return true;

            std::error_code queuedError;
            std::error_code sourceError;
            const auto queuedCanonical = std::filesystem::weakly_canonical(
                std::filesystem::u8path(queuedPath), queuedError);
            const auto sourceCanonical = std::filesystem::weakly_canonical(
                std::filesystem::u8path(sourcePath), sourceError);
            return !queuedError && !sourceError &&
                queuedCanonical == sourceCanonical;
        };

        const auto before = CaptureCreatorExternalAnimationQueue();
        const auto sourceAlreadyQueued = std::any_of(
            before.clips.begin(), before.clips.end(),
            [&](const CreatorExternalAnimationClip& clip)
            {
                return sameSourcePath(clip.localSourcePath);
            });
        if (sourceAlreadyQueued)
        {
            error.clear();
            return true;
        }

        if (!QueueCreatorExternalAnimationSource(sourcePath, error))
            return false;

        const auto after = CaptureCreatorExternalAnimationQueue();
        std::vector<std::size_t> matching;
        for (std::size_t index = 0; index < after.clips.size(); ++index)
        {
            if (sameSourcePath(after.clips[index].localSourcePath))
                matching.push_back(index);
        }
        if (matching.empty())
        {
            error = "Character animation file was inspected but no queue slot was created.";
            return false;
        }

        const std::size_t keep = matching.front();
        for (auto it = matching.rbegin(); it != matching.rend(); ++it)
        {
            if (*it == keep)
                continue;
            if (!RemoveCreatorExternalAnimationClip(*it, error))
                return false;
        }

        std::string filename = sourcePath;
        const auto separator = filename.find_last_of("/\\");
        if (separator != std::string::npos)
            filename.erase(0, separator + 1);
        const auto dot = filename.find_last_of('.');
        if (dot != std::string::npos && dot > 0)
            filename.erase(dot);
        if (filename.empty())
            filename = "Animation";

        if (!RenameCreatorExternalAnimationClip(keep, filename, error))
            return false;

        error.clear();
        return true;
    }

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
