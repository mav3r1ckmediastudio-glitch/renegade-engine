#pragma once

#include <cstddef>
#include <string>

namespace renegade::bridge
{
    class CommandService;
    struct ProjectMetadata;
    class SceneService;

    struct TestLevelSnapshot
    {
        std::string projectRoot;
        std::string sessionDirectory;
        std::string scenePath;
        // Populated by the project-aware Gate 2 overload. The scene-only Gate
        // 1 primitive deliberately leaves this empty.
        std::string descriptorPath;

        [[nodiscard]] bool IsValid() const noexcept
        {
            return !projectRoot.empty() &&
                !sessionDirectory.empty() &&
                !scenePath.empty();
        }

        [[nodiscard]] bool IsRuntimeReady() const noexcept
        {
            return IsValid() && !descriptorPath.empty();
        }
    };

    // Deterministic fault seam for automated cleanup evidence. Production
    // callers leave this at None.
    enum class TestLevelSnapshotFailureInjection
    {
        None,
        AfterArchiveWrite,
        AfterDescriptorWrite,
    };

    // UI-free LP04 primitive. Serializes the currently live Wicked scene into
    // a disposable project-contained snapshot without using the ordinary
    // SceneDocumentService::Save() path. Therefore it must not change the
    // authoritative scene path, saved depth, Undo/Redo history or dirty state.
    class TestLevelSnapshotService
    {
    public:
        TestLevelSnapshotService(
            SceneService& scenes,
            const CommandService& commands) noexcept;

        // Builds a Runtime-ready shadow project around the unsaved scene
        // snapshot. The shadow descriptor reuses only the authoritative
        // project's stable ID and name; Flow/Screen startup roots are omitted
        // so Test Level starts directly in the captured scene.
        [[nodiscard]] bool Create(
            const ProjectMetadata& project,
            TestLevelSnapshot& snapshot,
            std::string& error,
            TestLevelSnapshotFailureInjection failureInjection =
                TestLevelSnapshotFailureInjection::None);

        [[nodiscard]] bool Create(
            const std::string& projectRoot,
            TestLevelSnapshot& snapshot,
            std::string& error,
            TestLevelSnapshotFailureInjection failureInjection =
                TestLevelSnapshotFailureInjection::None);

        // Idempotent for an already-removed snapshot. Refuses paths that are
        // not direct children of <project>/Intermediate/TestLevelSnapshots.
        [[nodiscard]] bool Cleanup(
            const TestLevelSnapshot& snapshot,
            std::string& error) const;

    private:
        SceneService& scenes_;
        const CommandService& commands_;
    };
}
