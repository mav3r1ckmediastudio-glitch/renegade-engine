#include "renegade/bridge/TestLevelSnapshotService.h"

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/NavigationService.h"
#include "renegade/bridge/PatrolRouteService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/TestLevelNavigationCacheService.h"

#include <array>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr std::uint64_t FnvSeed = 1469598103934665603ull;
    constexpr std::uint64_t FnvPrime = 1099511628211ull;

    struct AuthoredNavigationGridIdentity
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        renegade::bridge::StableId stableId;
    };

    void HashText(std::uint64_t& hash, const std::string& text) noexcept
    {
        for (const unsigned char byte : text)
        {
            hash ^= byte;
            hash *= FnvPrime;
        }
    }

    std::string StableAutomaticNavigationGridId(
        const std::string& sourceIdentity)
    {
        std::uint64_t first = FnvSeed;
        std::uint64_t second = FnvSeed;
        HashText(
            first,
            "renegade.testgame.navigation.default.a|" + sourceIdentity);
        HashText(
            second,
            "renegade.testgame.navigation.default.b|" + sourceIdentity);

        std::array<std::uint8_t, 16> bytes = {};
        for (std::size_t index = 0; index < 8; ++index)
        {
            bytes[index] = static_cast<std::uint8_t>(
                (first >> ((7 - index) * 8)) & 0xffu);
            bytes[index + 8] = static_cast<std::uint8_t>(
                (second >> ((7 - index) * 8)) & 0xffu);
        }
        // Renegade stable IDs require UUID version 4 and the RFC-4122 variant.
        // The payload is deterministic so repeated TestGame snapshots of a
        // scene without an authored grid keep one stable Runtime grid identity.
        bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0fu) | 0x40u);
        bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3fu) | 0x80u);

        std::ostringstream out;
        out << std::hex << std::setfill('0');
        for (std::size_t index = 0; index < bytes.size(); ++index)
        {
            if (index == 4 || index == 6 || index == 8 || index == 10)
                out << '-';
            out << std::setw(2) << static_cast<unsigned int>(bytes[index]);
        }
        return out.str();
    }

    std::string StableSourceSceneIdentity(
        const renegade::bridge::ProjectMetadata& project,
        const std::string& currentScenePath)
    {
        using namespace renegade::bridge;

        if (!currentScenePath.empty())
        {
            DocumentEnvelope envelope;
            std::string ignored;
            if (ReadDocumentEnvelope(
                    currentScenePath + ".rmeta",
                    envelope,
                    ignored) &&
                envelope.projectId == project.projectId &&
                envelope.documentType == "scene" &&
                IsValidStableId(envelope.documentId))
            {
                return project.projectId + "|" + envelope.documentId;
            }
        }

        return project.projectId + "|" +
            (currentScenePath.empty()
                ? std::string("unsaved-active-scene")
                : currentScenePath);
    }

    std::size_t CountRenegadeNavigationGrids(
        const wi::scene::Scene& scene) noexcept
    {
        using namespace renegade::bridge;

        std::size_t count = 0;
        for (std::size_t index = 0;
            index < scene.voxel_grids.GetCount(); ++index)
        {
            if (IsRenegadeNavigationGrid(
                    scene,
                    scene.voxel_grids.GetEntity(index)))
            {
                ++count;
            }
        }
        return count;
    }

    bool CaptureAuthoredNavigationIdentities(
        const wi::scene::Scene& scene,
        std::vector<AuthoredNavigationGridIdentity>& identities,
        std::string& error)
    {
        using namespace renegade::bridge;

        identities.clear();
        for (std::size_t index = 0;
            index < scene.voxel_grids.GetCount(); ++index)
        {
            const wi::ecs::Entity entity =
                scene.voxel_grids.GetEntity(index);
            if (!IsRenegadeNavigationGrid(scene, entity))
                continue;

            const StableId id = PersistentEntityId(scene, entity);
            if (!IsValidStableId(id))
            {
                error =
                    "Authored TestGame navigation grid has no valid persistent identity.";
                identities.clear();
                return false;
            }
            identities.push_back({entity, id});
        }

        error.clear();
        return true;
    }

    bool VerifyAuthoredNavigationIdentities(
        const wi::scene::Scene& scene,
        const std::vector<AuthoredNavigationGridIdentity>& identities,
        std::string& error)
    {
        using namespace renegade::bridge;

        for (const auto& identity : identities)
        {
            if (!IsRenegadeNavigationGrid(scene, identity.entity) ||
                PersistentEntityId(scene, identity.entity) != identity.stableId)
            {
                error =
                    "Detached TestGame navigation preparation changed an authored grid identity.";
                return false;
            }
        }

        error.clear();
        return true;
    }

    bool PrepareOwnedNavigation(
        const renegade::bridge::ProjectMetadata& project,
        const std::string& currentScenePath,
        wi::scene::Scene& scene,
        renegade::bridge::TestLevelNavigationPreparation& result,
        std::string& error)
    {
        using namespace renegade::bridge;

        const std::string sourceIdentity =
            StableSourceSceneIdentity(project, currentScenePath);

        // Preserve authored ownership instead of manufacturing a second grid.
        // The cache service already has the correct split:
        //   * authored grid(s) present -> bake/cache those grids in this detached
        //     TestGame copy while retaining their entity and persistent identity;
        //   * no authored grid -> create one automatic Runtime grid.
        // The live authoring Scene is never passed to this function.
        std::vector<AuthoredNavigationGridIdentity> authored;
        if (!CaptureAuthoredNavigationIdentities(scene, authored, error))
            return false;

        if (!PrepareTestLevelNavigationCache(
                project.rootPath,
                sourceIdentity,
                scene,
                result,
                error))
        {
            return false;
        }

        if (!VerifyAuthoredNavigationIdentities(scene, authored, error))
            return false;

        const std::size_t preparedGridCount =
            CountRenegadeNavigationGrids(scene);
        if (preparedGridCount == 0)
        {
            error =
                "TestGame navigation preparation produced no Runtime navigation grid.";
            return false;
        }
        if (!authored.empty() && preparedGridCount != authored.size())
        {
            error =
                "TestGame navigation preparation unexpectedly added or removed an authored grid.";
            return false;
        }
        if (authored.empty() && preparedGridCount != 1)
        {
            error =
                "Automatic TestGame navigation did not produce exactly one Runtime grid.";
            return false;
        }

        wi::ecs::Entity runtimeGrid = FindDefaultNavigationGrid(scene);
        if (runtimeGrid == wi::ecs::INVALID_ENTITY)
        {
            error =
                "TestGame navigation could not resolve a Runtime default grid.";
            return false;
        }

        // Only the genuinely automatic grid receives the deterministic TestGame
        // identity. Authored grids retain their own persistent IDs unchanged.
        if (authored.empty())
        {
            const StableId stableAutomaticId =
                StableAutomaticNavigationGridId(sourceIdentity);
            if (!AssignPersistentEntityId(
                    scene,
                    runtimeGrid,
                    stableAutomaticId,
                    error))
            {
                return false;
            }
        }

        auto* metadata = scene.metadatas.GetComponent(runtimeGrid);
        if (metadata == nullptr)
            metadata = &scene.metadatas.Create(runtimeGrid);
        metadata->string_values.set(
            NavigationDefaultGridMetadataKey,
            NavigationDefaultGridMetadataVersion);

        if (FindDefaultNavigationGrid(scene) != runtimeGrid)
        {
            error =
                "Prepared TestGame navigation grid did not become the Runtime default grid.";
            return false;
        }

        result.gridCount = preparedGridCount;
        error.clear();
        return true;
    }

    bool WritePreparedTestLevelScene(
        wi::scene::Scene& scene,
        const std::string& scenePath,
        std::string& error)
    {
        try
        {
            wi::Archive archive(scenePath, false, false);
            if (!archive.IsOpen())
            {
                error =
                    "Could not reopen the disposable TestGame scene for navigation output: " +
                    scenePath;
                return false;
            }

            scene.Serialize(archive);
            const bool written = archive.SaveFile(scenePath);
            // A filename-backed Wicked archive saves again from Close(). Disarm
            // after the explicit write, matching Renegade's existing archive
            // ownership pattern.
            archive = wi::Archive();
            if (!written)
            {
                error =
                    "Could not write prepared TestGame navigation to: " +
                    scenePath;
                return false;
            }

            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error =
                std::string(
                    "Could not serialize prepared TestGame navigation: ") +
                exception.what();
            return false;
        }
        catch (...)
        {
            error =
                "Could not serialize prepared TestGame navigation.";
            return false;
        }
    }
}

namespace renegade::bridge
{
    TestLevelSnapshotService::TestLevelSnapshotService(
        SceneService& scenes,
        const CommandService& commands,
        ScriptAuthoringService* scripts) noexcept
        : scenes_(scenes),
          commands_(commands),
          scripts_(scripts)
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

        const std::string currentPathBefore = scenes_.CurrentPath();
        const bool dirtyBefore = commands_.IsDirty();
        const std::size_t undoBefore = commands_.UndoCount();
        const std::size_t redoBefore = commands_.RedoCount();

        LegacyTestLevelSnapshotService legacy(
            scenes_, commands_, scripts_);
        TestLevelSnapshot created;
        if (!legacy.Create(
                project,
                created,
                error,
                failureInjection))
        {
            return false;
        }

        const auto failAndCleanup = [&](std::string message)
        {
            std::string cleanupError;
            if (!LegacyTestLevelSnapshotService::CleanupDirectory(
                    created.projectRoot,
                    created.sessionDirectory,
                    cleanupError))
            {
                message += " Cleanup also failed: " + cleanupError;
            }
            snapshot = {};
            error = std::move(message);
            return false;
        };

        TestLevelNavigationPreparation navigation;
        {
            // Wicked Scene instances own native component storage. Do not
            // deserialize the just-written scene for validation while this
            // mutable preparation instance is still alive: a TestGame build
            // performs this path repeatedly and must not overlap ownership of
            // the same VoxelGrid archive payload.
            auto prepared =
                PrepareWickedSceneOpen(created.scenePath);
            if (!prepared.IsReady() ||
                prepared.MutablePreparedScene() == nullptr)
            {
                return failAndCleanup(
                    "Could not reopen the disposable TestGame scene for automatic "
                    "navigation: " + prepared.Error());
            }

            if (!PrepareOwnedNavigation(
                    project,
                    currentPathBefore,
                    *prepared.MutablePreparedScene(),
                    navigation,
                    error))
            {
                return failAndCleanup(
                    "Could not prepare TestGame navigation: " + error);
            }

            if (!WritePreparedTestLevelScene(
                    *prepared.MutablePreparedScene(),
                    created.scenePath,
                    error))
            {
                return failAndCleanup(error);
            }
        }

        // Validate only after the mutable prepared Scene has been destroyed.
        // This keeps every TestGame read/write/reopen transaction sequential
        // at the native Wicked ownership boundary.
        auto validation = PrepareWickedSceneOpen(created.scenePath);
        if (!validation.IsReady())
        {
            return failAndCleanup(
                "Prepared TestGame navigation produced an unreadable scene: " +
                validation.Error());
        }

        if (scenes_.CurrentPath() != currentPathBefore ||
            commands_.IsDirty() != dirtyBefore ||
            commands_.UndoCount() != undoBefore ||
            commands_.RedoCount() != redoBefore)
        {
            return failAndCleanup(
                "TestGame navigation changed authoritative editor "
                "path, dirty state or Undo/Redo history.");
        }

        created.navigationCacheReused =
            navigation.reusedCache;
        created.navigationCacheRebuilt =
            navigation.rebuiltCache;
        created.navigationGridCount =
            navigation.gridCount;
        created.navigationSignature =
            std::move(navigation.signature);
        created.navigationCacheDirectory =
            std::move(navigation.cacheDirectory);

        const char* navigationAction =
            navigation.rebuiltCache && navigation.reusedCache
                ? "mixed reuse/rebuild"
                : navigation.rebuiltCache
                    ? "rebuilt"
                    : navigation.reusedCache
                        ? "reused"
                        : "prepared";
        wi::backlog::post(
            std::string("Renegade TestGame navigation: ") +
            navigationAction +
            " // grids " +
            std::to_string(created.navigationGridCount) +
            " // signature " +
            created.navigationSignature);

        snapshot = std::move(created);
        error.clear();
        return true;
    }

    bool TestLevelSnapshotService::Create(
        const std::string& projectRoot,
        TestLevelSnapshot& snapshot,
        std::string& error,
        const TestLevelSnapshotFailureInjection failureInjection)
    {
        LegacyTestLevelSnapshotService legacy(
            scenes_, commands_, scripts_);
        return legacy.Create(
            projectRoot,
            snapshot,
            error,
            failureInjection);
    }

    bool TestLevelSnapshotService::CleanupDirectory(
        const std::string& projectRoot,
        const std::string& sessionDirectory,
        std::string& error)
    {
        return LegacyTestLevelSnapshotService::CleanupDirectory(
            projectRoot,
            sessionDirectory,
            error);
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
