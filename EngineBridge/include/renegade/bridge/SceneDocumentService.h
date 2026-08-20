#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include <WickedEngine.h>

namespace renegade::bridge
{
    class CommandService;
    class ProjectService;
    class SceneService;
    class SelectionService;

    // Move-only result of the read/deserialize phase of Open Scene.
    //
    // Preparing an open never mutates the active document, so Studio can run
    // it on Wicked's job system. CommitPreparedOpen() is the only operation
    // that replaces the active scene and must be called at a Wicked thread-safe
    // point.
    class PreparedSceneOpen
    {
    public:
        PreparedSceneOpen() = default;
        PreparedSceneOpen(PreparedSceneOpen&&) noexcept = default;
        PreparedSceneOpen& operator=(PreparedSceneOpen&&) noexcept = default;
        PreparedSceneOpen(const PreparedSceneOpen&) = delete;
        PreparedSceneOpen& operator=(const PreparedSceneOpen&) = delete;

        [[nodiscard]] bool IsReady() const noexcept
        {
            return scene_ != nullptr && error_.empty();
        }

        [[nodiscard]] const std::string& Path() const noexcept
        {
            return path_;
        }

        [[nodiscard]] const std::string& Error() const noexcept
        {
            return error_;
        }

        // Read-only inspection seam for bridge services such as dependency
        // extraction. The prepared scene remains owned by this result and can
        // never replace or mutate Studio's active document through this view.
        [[nodiscard]] const wi::scene::Scene* ReadOnlyScene() const noexcept
        {
            return scene_.get();
        }

        // Mutable preparation seam for background restoration work. This scene
        // is still detached from Studio's active document; callers must finish
        // all mutation before CommitPreparedOpen() adopts it.
        [[nodiscard]] wi::scene::Scene* MutablePreparedScene() noexcept
        {
            return scene_.get();
        }

    private:
        friend class SceneService;
        friend class SceneDocumentService;
        friend PreparedSceneOpen PrepareWickedSceneOpen(
            const std::string& filePath);

        std::unique_ptr<wi::scene::Scene> scene_;
        std::string path_;
        std::string error_;
        wi::ecs::Entity firstCamera_ = wi::ecs::INVALID_ENTITY;
    };

    // Shared Wicked archive preparation used by both the Runtime's direct
    // startup load and the editor document transaction.
    [[nodiscard]] PreparedSceneOpen PrepareWickedSceneOpen(
        const std::string& filePath);

    // UI-free document operation boundary over Wicked scene, archive and
    // editor-state primitives. Renegade presentation chooses files and shows
    // prompts; this service owns the transactional document replacement.
    class SceneDocumentService
    {
    public:
        static constexpr std::size_t MaximumAutomaticBackups = 10;

        SceneDocumentService(
            SceneService& scenes,
            SelectionService& selection,
            CommandService& commands,
            ProjectService& projects) noexcept;

        [[nodiscard]] PreparedSceneOpen PrepareOpen(
            const std::string& filePath) const;
        bool CommitPreparedOpen(PreparedSceneOpen prepared);
        bool Open(const std::string& filePath);
        bool Save(const std::string& filePath);

        void NewScene();
        bool Reload();

        [[nodiscard]] wi::ecs::Entity LastOpenedCamera() const noexcept
        {
            return lastOpenedCamera_;
        }

        [[nodiscard]] const std::string& LastWarning() const noexcept
        {
            return lastWarning_;
        }

        [[nodiscard]] std::string AutomaticBackupDirectory(
            const std::string& scenePath) const;

    private:
        SceneService& scenes_;
        SelectionService& selection_;
        CommandService& commands_;
        ProjectService& projects_;
        wi::ecs::Entity lastOpenedCamera_ = wi::ecs::INVALID_ENTITY;
        std::string lastWarning_;
    };
}
