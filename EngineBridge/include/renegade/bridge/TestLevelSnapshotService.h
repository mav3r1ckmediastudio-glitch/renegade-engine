#pragma once

#include <cstddef>
#include <string>

namespace renegade::bridge
{
    class CommandService;
    struct ProjectMetadata;
    class SceneService;
    class ScriptAuthoringService;

    struct TestLevelSnapshot
    {
        std::string projectRoot;
        std::string sessionDirectory;
        std::string scenePath;
        // Populated by the project-aware Gate 2 overload. The scene-only Gate
        // 1 primitive deliberately leaves this empty.
        std::string descriptorPath;

        // CW-05 owner-validation evidence. These describe navigation prepared
        // only for the disposable TestGame scene; the live authoring document
        // is never rewritten by the automatic bake/cache path.
        bool navigationCacheReused = false;
        bool navigationCacheRebuilt = false;
        std::size_t navigationGridCount = 0;
        std::string navigationSignature;
        std::string navigationCacheDirectory;

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

#define RENEGADE_DECLARE_TEST_LEVEL_SNAPSHOT_SERVICE(ClassName) \
    class ClassName \
    { \
    public: \
        ClassName( \
            SceneService& scenes, \
            const CommandService& commands, \
            ScriptAuthoringService* scripts = nullptr) noexcept; \
        [[nodiscard]] bool Create( \
            const ProjectMetadata& project, \
            TestLevelSnapshot& snapshot, \
            std::string& error, \
            TestLevelSnapshotFailureInjection failureInjection = \
                TestLevelSnapshotFailureInjection::None); \
        [[nodiscard]] bool Create( \
            const std::string& projectRoot, \
            TestLevelSnapshot& snapshot, \
            std::string& error, \
            TestLevelSnapshotFailureInjection failureInjection = \
                TestLevelSnapshotFailureInjection::None); \
        [[nodiscard]] static bool CleanupDirectory( \
            const std::string& projectRoot, \
            const std::string& sessionDirectory, \
            std::string& error); \
        [[nodiscard]] bool Cleanup( \
            const TestLevelSnapshot& snapshot, \
            std::string& error) const; \
    private: \
        SceneService& scenes_; \
        const CommandService& commands_; \
        ScriptAuthoringService* scripts_ = nullptr; \
    };

#if defined(RENEGADE_TEST_LEVEL_SNAPSHOT_LEGACY_IMPLEMENTATION)
    // The established LP04 implementation is compiled under this internal
    // class name. The creator-facing class below wraps it only on TestGame's
    // project-aware path so the proven scene-only primitive remains intact.
    RENEGADE_DECLARE_TEST_LEVEL_SNAPSHOT_SERVICE(
        LegacyTestLevelSnapshotService)
#else
    RENEGADE_DECLARE_TEST_LEVEL_SNAPSHOT_SERVICE(
        LegacyTestLevelSnapshotService)

    // UI-free LP04/CW-05 boundary. The project-aware overload delegates to the
    // established LP04 implementation, then prepares navigation on the
    // detached TestGame WISCENE before Runtime sees it.
    RENEGADE_DECLARE_TEST_LEVEL_SNAPSHOT_SERVICE(
        TestLevelSnapshotService)
#endif

#undef RENEGADE_DECLARE_TEST_LEVEL_SNAPSHOT_SERVICE
}
