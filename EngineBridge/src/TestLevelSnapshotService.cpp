#include "renegade/bridge/TestLevelSnapshotService.h"

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    std::atomic<std::uint64_t> snapshotSequence{0};

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

    bool TestLevelSnapshotService::Cleanup(
        const TestLevelSnapshot& snapshot,
        std::string& error) const
    {
        if (snapshot.sessionDirectory.empty())
        {
            error.clear();
            return true;
        }
        if (snapshot.projectRoot.empty())
        {
            error = "Test Level snapshot cleanup is missing its project root.";
            return false;
        }

        try
        {
            std::error_code pathError;
            const fs::path root =
                fs::absolute(fs::u8path(snapshot.projectRoot), pathError)
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
                fs::absolute(fs::u8path(snapshot.sessionDirectory), pathError)
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
}
