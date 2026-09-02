#include "renegade/bridge/TestLevelSnapshotService.h"

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/GameplayScriptService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneService.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    std::atomic<std::uint64_t> snapshotSequence{0};
    constexpr const char* TestLevelStartupScene =
        "Content/Scenes/TestLevel.wiscene";

    bool WriteSnapshotDescriptor(
        const renegade::bridge::ProjectMetadata& sourceProject,
        const fs::path& descriptorPath,
        std::string& error)
    {
        std::ofstream stream(
            descriptorPath,
            std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "Could not create the Test Level project descriptor: " +
                descriptorPath.generic_u8string();
            return false;
        }

        stream << "format = renegade-project\n";
        stream << "version = "
               << renegade::bridge::ProjectService::CurrentFormatVersion
               << "\n\n";
        stream << "[project]\n";
        stream << "project_id = " << sourceProject.projectId << '\n';
        stream << "name = " << sourceProject.name << '\n';
        stream << "startup_scene = " << TestLevelStartupScene << '\n';
        stream << "startup_flow_id = \n";
        stream << "startup_flow = \n";
        stream << "startup_screen_id = \n";
        stream << "startup_screen = \n";
        stream.flush();
        if (!stream)
        {
            error = "Could not write the complete Test Level project "
                "descriptor: " + descriptorPath.generic_u8string();
            return false;
        }

        error.clear();
        return true;
    }

    std::string SnapshotToken()
    {
        const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::ostringstream stream;
        stream << ticks << '-' << std::setfill('0') << std::setw(10)
               << snapshotSequence.fetch_add(1);
        return stream.str();
    }

    void PruneIfEmpty(const fs::path& directory)
    {
        if (directory.empty())
        {
            return;
        }

        std::error_code error;
        if (!fs::is_directory(directory, error) || error)
        {
            return;
        }
        if (!fs::is_empty(directory, error) || error)
        {
            return;
        }
        fs::remove(directory, error);
    }

    bool RemoveSnapshotDirectory(
        const fs::path& sessionDirectory,
        std::string& error)
    {
        std::error_code removeError;
        fs::remove_all(sessionDirectory, removeError);
        if (removeError)
        {
            error = "Could not remove Test Level snapshot directory: " +
                sessionDirectory.generic_u8string() + ": " +
                removeError.message();
            return false;
        }

        const fs::path snapshotsRoot = sessionDirectory.parent_path();
        PruneIfEmpty(snapshotsRoot);
        PruneIfEmpty(snapshotsRoot.parent_path());
        error.clear();
        return true;
    }

    bool StageGameplayScripts(
        const wi::scene::Scene& scene,
        const fs::path& projectRoot,
        const fs::path& snapshotRoot,
        std::string& error)
    {
        for (std::size_t index = 0; index < scene.scripts.GetCount(); ++index)
        {
            const auto entity = scene.scripts.GetEntity(index);
            if (!renegade::bridge::IsGameplayScript(scene, entity))
                continue;

            const auto state =
                renegade::bridge::CaptureGameplayScript(scene, entity);
            std::string source;
            if (!renegade::bridge::ResolveGameplayScriptPath(
                    projectRoot.generic_u8string(),
                    state.projectRelativePath,
                    source,
                    error))
            {
                error = "Test Level could not stage gameplay script '" +
                    state.projectRelativePath + "': " + error;
                return false;
            }

            const fs::path destination =
                snapshotRoot / fs::u8path(state.projectRelativePath);
            std::error_code copyError;
            fs::create_directories(destination.parent_path(), copyError);
            if (copyError)
            {
                error = "Test Level could not create the gameplay script "
                    "snapshot folder: " + copyError.message();
                return false;
            }
            fs::copy_file(
                fs::u8path(source),
                destination,
                fs::copy_options::overwrite_existing,
                copyError);
            if (copyError)
            {
                error = "Test Level could not copy gameplay script '" +
                    state.projectRelativePath + "': " + copyError.message();
                return false;
            }

            std::string staged;
            if (!renegade::bridge::ResolveGameplayScriptPath(
                    snapshotRoot.generic_u8string(),
                    state.projectRelativePath,
                    staged,
                    error))
            {
                error = "Test Level gameplay script snapshot failed "
                    "validation for '" + state.projectRelativePath +
                    "': " + error;
                return false;
            }
        }
        error.clear();
        return true;
    }

    class ScopedGameplayScriptFilenames
    {
    public:
        explicit ScopedGameplayScriptFilenames(wi::scene::Scene& scene)
            : scene_(scene)
        {
            entries_.reserve(scene.scripts.GetCount());
            for (std::size_t index = 0; index < scene.scripts.GetCount(); ++index)
            {
                const auto entity = scene.scripts.GetEntity(index);
                if (renegade::bridge::IsGameplayScript(scene, entity))
                    entries_.push_back({entity, scene.scripts[index].filename});
            }
        }

        ~ScopedGameplayScriptFilenames()
        {
            Restore();
        }

        void Restore() noexcept
        {
            if (restored_)
                return;
            for (auto& entry : entries_)
            {
                auto* script = scene_.scripts.GetComponent(entry.entity);
                if (script != nullptr)
                    script->filename.swap(entry.original);
            }
            restored_ = true;
        }

        void Redirect(const fs::path& snapshotRoot)
        {
            for (auto& entry : entries_)
            {
                auto* script = scene_.scripts.GetComponent(entry.entity);
                if (script == nullptr)
                    continue;
                const auto state = renegade::bridge::CaptureGameplayScript(
                    scene_, entry.entity);
                script->filename =
                    (snapshotRoot / fs::u8path(state.projectRelativePath))
                        .lexically_normal().generic_u8string();
            }
        }

    private:
        struct Entry
        {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            std::string original;
        };

        wi::scene::Scene& scene_;
        std::vector<Entry> entries_;
        bool restored_ = false;
    };
}

namespace renegade::bridge
{
    TestLevelSnapshotService::TestLevelSnapshotService(
        SceneService& scenes,
        const CommandService& commands) noexcept
        : scenes_(scenes),
          commands_(commands)
    {
    }

    bool TestLevelSnapshotService::Create(
        const ProjectMetadata& project,
        TestLevelSnapshot& snapshot,
        std::string& error,
        const TestLevelSnapshotFailureInjection failureInjection)
    {
        snapshot = {};
        error.clear();

        TestLevelSnapshot created;
        const auto sceneFailure =
            failureInjection ==
                TestLevelSnapshotFailureInjection::AfterArchiveWrite
            ? TestLevelSnapshotFailureInjection::AfterArchiveWrite
            : TestLevelSnapshotFailureInjection::None;
        if (!Create(project.rootPath, created, error, sceneFailure))
        {
            return false;
        }

        const auto failAndCleanup = [&](std::string message)
        {
            std::string cleanupError;
            if (!Cleanup(created, cleanupError))
            {
                message += " Cleanup also failed: " + cleanupError;
            }
            snapshot = {};
            error = std::move(message);
            return false;
        };

        try
        {
            const fs::path descriptorPath =
                fs::u8path(created.sessionDirectory) / "TestLevel.renegade";
            if (!WriteSnapshotDescriptor(project, descriptorPath, error))
            {
                return failAndCleanup(error);
            }

            if (failureInjection ==
                TestLevelSnapshotFailureInjection::AfterDescriptorWrite)
            {
                return failAndCleanup(
                    "Injected Test Level snapshot failure after descriptor "
                    "write.");
            }

            ProjectService projects;
            ProjectMetadata inspected;
            std::string inspectError;
            if (!projects.InspectProject(
                    descriptorPath.generic_u8string(),
                    inspected,
                    inspectError))
            {
                return failAndCleanup(
                    "The Test Level project descriptor was rejected: " +
                    inspectError);
            }

            const fs::path expectedRoot =
                fs::u8path(created.sessionDirectory).lexically_normal();
            if (inspected.formatVersion != ProjectService::CurrentFormatVersion ||
                inspected.projectId != project.projectId ||
                inspected.name != project.name ||
                fs::u8path(inspected.rootPath).lexically_normal() !=
                    expectedRoot ||
                inspected.startupScene != TestLevelStartupScene ||
                !inspected.startupFlowId.empty() ||
                !inspected.startupFlow.empty() ||
                !inspected.startupScreenId.empty() ||
                !inspected.startupScreen.empty())
            {
                return failAndCleanup(
                    "The Test Level project descriptor did not round-trip "
                    "with the required Runtime startup metadata.");
            }

            created.descriptorPath = descriptorPath.generic_u8string();
            snapshot = std::move(created);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            return failAndCleanup(
                std::string("Could not create Test Level project snapshot: ") +
                exception.what());
        }
    }

    bool TestLevelSnapshotService::Create(
        const std::string& projectRoot,
        TestLevelSnapshot& snapshot,
        std::string& error,
        const TestLevelSnapshotFailureInjection failureInjection)
    {
        snapshot = {};
        error.clear();

        if (projectRoot.empty())
        {
            error = "A project root is required for a Test Level snapshot.";
            return false;
        }

        const std::string currentPathBefore = scenes_.CurrentPath();
        const bool dirtyBefore = commands_.IsDirty();
        const std::size_t undoBefore = commands_.UndoCount();
        const std::size_t redoBefore = commands_.RedoCount();

        fs::path root;
        fs::path sessionDirectory;
        try
        {
            std::error_code pathError;
            root = fs::absolute(fs::u8path(projectRoot), pathError)
                .lexically_normal();
            if (pathError || !fs::is_directory(root, pathError) || pathError)
            {
                error = "The Test Level project root is not an accessible "
                    "directory: " + projectRoot;
                return false;
            }

            const fs::path snapshotsRoot =
                root / "Intermediate" / "TestLevelSnapshots";
            sessionDirectory = snapshotsRoot / fs::u8path(SnapshotToken());
            const fs::path sceneDirectory =
                sessionDirectory / "Content" / "Scenes";
            fs::create_directories(sceneDirectory, pathError);
            if (pathError)
            {
                error = "Could not create the Test Level snapshot directory: " +
                    pathError.message();
                std::string ignored;
                if (!sessionDirectory.empty())
                {
                    RemoveSnapshotDirectory(sessionDirectory, ignored);
                }
                return false;
            }

            const fs::path scenePath =
                sceneDirectory / "TestLevel.wiscene";

            const auto failAndCleanup = [&](std::string message)
            {
                std::string cleanupError;
                if (!RemoveSnapshotDirectory(sessionDirectory, cleanupError))
                {
                    message += " Cleanup also failed: " + cleanupError;
                }
                snapshot = {};
                error = std::move(message);
                return false;
            };

            // Test Level launches Runtime against an isolated shadow project,
            // not the creator's source tree. Stage governed sources first,
            // then temporarily point the native serializers at those copies.
            // The scoped redirect restores every live Studio filename even if
            // archive serialization throws.
            if (!StageGameplayScripts(
                    scenes_.GetScene(), root, sessionDirectory, error))
            {
                return failAndCleanup(error);
            }
            ScopedGameplayScriptFilenames scriptFilenames(
                scenes_.GetScene());
            scriptFilenames.Redirect(sessionDirectory);

            bool archiveWritten = false;
            try
            {
                wi::Archive archive(
                    scenePath.generic_u8string(),
                    false,
                    false);
                if (!archive.IsOpen())
                {
                    return failAndCleanup(
                        "Could not create a WISCENE archive for the Test "
                        "Level snapshot.");
                }
                archive.SetCompressionEnabled(true);
                scenes_.GetScene().Serialize(archive);
                archiveWritten = archive.SaveFile(scenePath.generic_u8string());
                archive = wi::Archive();
            }
            catch (const std::exception& exception)
            {
                return failAndCleanup(
                    "Test Level snapshot serialization failed: " +
                    std::string(exception.what()));
            }
            catch (...)
            {
                return failAndCleanup(
                    "Test Level snapshot serialization failed.");
            }
            scriptFilenames.Restore();

            if (!archiveWritten || !fs::is_regular_file(scenePath, pathError) ||
                pathError)
            {
                return failAndCleanup(
                    "The Test Level snapshot archive could not be written.");
            }

            if (failureInjection ==
                TestLevelSnapshotFailureInjection::AfterArchiveWrite)
            {
                return failAndCleanup(
                    "Injected Test Level snapshot failure after archive write.");
            }

            auto validation = PrepareWickedSceneOpen(
                scenePath.generic_u8string());
            if (!validation.IsReady())
            {
                return failAndCleanup(
                    "The Test Level snapshot failed WISCENE validation: " +
                    validation.Error());
            }

            // Serialization is allowed to inspect the live Wicked scene, but
            // LP04 must never make Studio believe the authoring document was
            // saved or replaced.
            if (scenes_.CurrentPath() != currentPathBefore ||
                commands_.IsDirty() != dirtyBefore ||
                commands_.UndoCount() != undoBefore ||
                commands_.RedoCount() != redoBefore)
            {
                return failAndCleanup(
                    "Creating the Test Level snapshot changed authoritative "
                    "editor document state.");
            }

            snapshot.projectRoot = root.generic_u8string();
            snapshot.sessionDirectory = sessionDirectory.generic_u8string();
            snapshot.scenePath = scenePath.generic_u8string();
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            if (!sessionDirectory.empty())
            {
                std::string ignored;
                RemoveSnapshotDirectory(sessionDirectory, ignored);
            }
            snapshot = {};
            error = std::string("Could not create Test Level snapshot: ") +
                exception.what();
            return false;
        }
    }

    bool TestLevelSnapshotService::CleanupDirectory(
        const std::string& projectRoot,
        const std::string& sessionDirectoryText,
        std::string& error)
    {
        if (sessionDirectoryText.empty())
        {
            error.clear();
            return true;
        }
        if (projectRoot.empty())
        {
            error = "Test Level snapshot cleanup is missing its project root.";
            return false;
        }

        try
        {
            std::error_code pathError;
            const fs::path root =
                fs::absolute(fs::u8path(projectRoot), pathError)
                    .lexically_normal();
            if (pathError)
            {
                error = "Could not resolve the Test Level project root for "
                    "cleanup: " + pathError.message();
                return false;
            }
            const fs::path snapshotsRoot =
                (root / "Intermediate" / "TestLevelSnapshots")
                    .lexically_normal();
            const fs::path sessionDirectory =
                fs::absolute(fs::u8path(sessionDirectoryText), pathError)
                    .lexically_normal();
            if (pathError || sessionDirectory.filename().empty() ||
                sessionDirectory.parent_path() != snapshotsRoot)
            {
                error = "Refusing to clean a Test Level snapshot outside the "
                    "project snapshot root.";
                return false;
            }

            return RemoveSnapshotDirectory(sessionDirectory, error);
        }
        catch (const std::exception& exception)
        {
            error = std::string("Could not clean Test Level snapshot: ") +
                exception.what();
            return false;
        }
    }

    bool TestLevelSnapshotService::Cleanup(
        const TestLevelSnapshot& snapshot,
        std::string& error) const
    {
        return CleanupDirectory(
            snapshot.projectRoot,
            snapshot.sessionDirectory,
            error);
    }
}
