#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace renegade::bridge
{
    class CommandService;
    struct ProjectMetadata;
    class SceneService;
    class ScriptAuthoringService;

    struct TestLevelSnapshot
    {
        // Cleanup/storage root. A prepared test copy may use local temporary storage.
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
            const CommandService& commands,
            ScriptAuthoringService* scripts = nullptr) noexcept;

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

        // Studio captures live state on its owning thread. The process launcher
        // must validate the pending archive before starting a child.
        [[nodiscard]] bool CreateDeferred(
            const ProjectMetadata& project, TestLevelSnapshot& snapshot,
            std::string& error);

        [[nodiscard]] static bool NeedsDeferredScenePreparation(
            const TestLevelSnapshot& snapshot);
        // Only owned archive bytes are written here; no live authoring scene is used.
        [[nodiscard]] static bool FinalizeDeferredScene(
            TestLevelSnapshot& snapshot, std::string& error,
            bool preferLocalStorage = false,
            const std::function<bool(bool validating)>& progress = {});

        // Process-lifecycle code needs the same path-safety guarantee without
        // constructing a scene-bound service instance. This is the single
        // cleanup primitive used by both normal snapshot cleanup and Gate 3.
        [[nodiscard]] static bool CleanupDirectory(
            const std::string& projectRoot,
            const std::string& sessionDirectory,
            std::string& error);

        // Idempotent for an already-removed snapshot. Refuses paths that are
        // not direct children of <project>/Intermediate/TestLevelSnapshots.
        [[nodiscard]] bool Cleanup(
            const TestLevelSnapshot& snapshot,
            std::string& error) const;

    private:
        bool CreateProjectSnapshot(const ProjectMetadata&, TestLevelSnapshot&,
            std::string&, TestLevelSnapshotFailureInjection, bool deferValidation);
        bool CreateSceneSnapshot(const std::string&, TestLevelSnapshot&,
            std::string&, TestLevelSnapshotFailureInjection, bool deferValidation);
        SceneService& scenes_;
        const CommandService& commands_;
        ScriptAuthoringService* scripts_ = nullptr;
    };
}
