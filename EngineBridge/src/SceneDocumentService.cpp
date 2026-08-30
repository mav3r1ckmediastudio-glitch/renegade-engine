#include "renegade/bridge/SceneDocumentService.h"

#include "renegade/bridge/CollisionService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/LightmapBakeService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/TerrainService.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace
{
    namespace fs = std::filesystem;

    std::atomic<std::uint64_t> saveSequence{0};

    std::string SaveToken()
    {
        const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::ostringstream token;
        token << ticks << '-' << std::setfill('0') << std::setw(10)
              << saveSequence.fetch_add(1);
        return token.str();
    }

    bool ReplaceFileAtomically(
        const fs::path& temporary,
        const fs::path& destination,
        std::error_code& error)
    {
#if defined(_WIN32)
        const bool destinationExists = fs::exists(destination, error);
        if (error)
        {
            return false;
        }

        BOOL replaced = FALSE;
        if (destinationExists)
        {
            replaced = ReplaceFileW(
                destination.c_str(),
                temporary.c_str(),
                nullptr,
                REPLACEFILE_WRITE_THROUGH,
                nullptr,
                nullptr);
        }
        else
        {
            replaced = MoveFileExW(
                temporary.c_str(),
                destination.c_str(),
                MOVEFILE_WRITE_THROUGH);
        }
        if (replaced == FALSE)
        {
            error = std::error_code(
                static_cast<int>(GetLastError()),
                std::system_category());
            return false;
        }
        return true;
#else
        fs::rename(temporary, destination, error);
        return !error;
#endif
    }

    void RemoveWithoutThrow(const fs::path& path)
    {
        std::error_code ignored;
        fs::remove(path, ignored);
    }
}

namespace renegade::bridge
{
    SceneDocumentService::SceneDocumentService(
        SceneService& scenes,
        SelectionService& selection,
        CommandService& commands,
        ProjectService& projects) noexcept
        : scenes_(scenes),
          selection_(selection),
          commands_(commands),
          projects_(projects)
    {
    }

    PreparedSceneOpen PrepareWickedSceneOpen(const std::string& filePath)
    {
        PreparedSceneOpen prepared;
        prepared.path_ = filePath;

        if (filePath.empty())
        {
            prepared.error_ = "A scene path is required.";
            return prepared;
        }

        const std::string extension = wi::helper::toUpper(
            wi::helper::GetExtensionFromFileName(filePath));
        if (extension != "WISCENE")
        {
            prepared.error_ = "Open Scene only accepts .wiscene files: " +
                filePath;
            return prepared;
        }

        if (!wi::helper::FileExists(filePath))
        {
            prepared.error_ = "Scene file does not exist: " + filePath;
            return prepared;
        }

        wi::Archive archive(filePath, true, false);
        if (!archive.IsOpen())
        {
            prepared.error_ = "Could not read a compatible WISCENE archive: " +
                filePath;
            return prepared;
        }

        try
        {
            prepared.scene_ = std::make_unique<wi::scene::Scene>();
            prepared.scene_->Serialize(archive);
        }
        catch (const std::exception& error)
        {
            prepared.scene_.reset();
            prepared.error_ = "WISCENE deserialization failed: " +
                std::string(error.what());
            return prepared;
        }
        catch (...)
        {
            prepared.scene_.reset();
            prepared.error_ = "WISCENE deserialization failed: " + filePath;
            return prepared;
        }

        if (archive.GetPos() != archive.GetSize())
        {
            prepared.scene_.reset();
            prepared.error_ = "WISCENE archive is incomplete or has "
                "unexpected trailing data: " + filePath;
            return prepared;
        }

        // JP01 owner-recovery boundary. A previous Physics Lab build allowed
        // a dynamic rigid body to be serialized on a deeply nested reusable
        // asset payload node. Wicked's parented dynamic-body transform path
        // repeatedly decomposes the import hierarchy and can amplify scale or
        // shear. Move the unambiguous one-body case to Renegade's stable
        // instance wrapper before the scene ever reaches its first physics
        // update. The repair is in-memory until the creator saves again.
        (void)RepairReusableAssetCollisionTargets(*prepared.scene_);

        if (prepared.scene_->cameras.GetCount() > 0)
        {
            prepared.firstCamera_ = prepared.scene_->cameras.GetEntity(0);
        }
        return prepared;
    }

    PreparedSceneOpen SceneDocumentService::PrepareOpen(
        const std::string& filePath) const
    {
        return PrepareWickedSceneOpen(filePath);
    }

    bool SceneDocumentService::CommitPreparedOpen(PreparedSceneOpen prepared)
    {
        if (!prepared.IsReady())
        {
            scenes_.lastError_ = prepared.Error().empty()
                ? "The prepared scene was not ready to open."
                : prepared.Error();
            return false;
        }

        // Gate 8: the pinned Wicked serializer restores baked bytes but
        // does not reconstruct ObjectComponent::lightmap. Hydrate the prepared
        // candidate here, at Studio's thread-safe adoption point, so a failure
        // leaves the active document untouched.
        std::string lightmapError;
        if (!LightmapBakeService::HydratePersistedLightmaps(
                *prepared.scene_, lightmapError))
        {
            scenes_.lastError_ = "Could not restore baked lightmaps: " + lightmapError;
            return false;
        }

        // This is the one replacement point. Preparation has already
        // completed, so the active document remains intact on every failure
        // path above. Studio invokes this at EVENT_THREAD_SAFE_POINT.
        scenes_.scene_.Clear();
        scenes_.scene_.Merge(*prepared.scene_);
        for (std::size_t index = 0;
            index < scenes_.scene_.terrains.GetCount(); ++index)
        {
            auto& terrain = scenes_.scene_.terrains[index];
            terrain.terrainEntity = scenes_.scene_.terrains.GetEntity(index);
            terrain.scene = &scenes_.scene_;
        }
        RebindDefaultTerrainMaterials(scenes_.scene_);
        scenes_.currentPath_ = std::move(prepared.path_);
        scenes_.lastError_.clear();
        lastWarning_.clear();

        selection_.Clear();
        commands_.Clear();
        lastOpenedCamera_ = prepared.firstCamera_;
        return true;
    }

    bool SceneDocumentService::Open(const std::string& filePath)
    {
        return CommitPreparedOpen(PrepareOpen(filePath));
    }

    std::string SceneDocumentService::AutomaticBackupDirectory(
        const std::string& scenePath) const
    {
        const fs::path destination = fs::u8path(scenePath);
        fs::path backupRoot;
        if (projects_.HasProject())
        {
            backupRoot = fs::u8path(projects_.CurrentProject().rootPath) /
                "Saved" / "Backups" / "Scenes";
        }
        else
        {
            backupRoot = destination.parent_path() /
                "Saved" / "Backups" / "Scenes";
        }
        return (backupRoot / destination.stem()).generic_u8string();
    }

    bool SceneDocumentService::Save(const std::string& filePath)
    {
        lastWarning_.clear();
        if (filePath.empty())
        {
            scenes_.lastError_ = "A scene path is required.";
            return false;
        }

        if (wi::helper::toUpper(
                wi::helper::GetExtensionFromFileName(filePath)) != "WISCENE")
        {
            scenes_.lastError_ = "Save Scene only accepts .wiscene files: " +
                filePath;
            return false;
        }

        const fs::path destination = fs::u8path(filePath).lexically_normal();
        fs::path parent = destination.parent_path();
        if (parent.empty())
        {
            std::error_code currentPathError;
            parent = fs::current_path(currentPathError);
            if (currentPathError)
            {
                scenes_.lastError_ = "Could not resolve the scene folder: " +
                    currentPathError.message();
                return false;
            }
        }

        std::error_code fileError;
        fs::create_directories(parent, fileError);
        if (fileError)
        {
            scenes_.lastError_ = "Could not create the scene folder: " +
                fileError.message();
            return false;
        }
        if (fs::exists(destination, fileError) &&
            !fs::is_regular_file(destination, fileError))
        {
            scenes_.lastError_ = "The scene destination is not a file: " +
                destination.generic_u8string();
            return false;
        }
        if (fileError)
        {
            scenes_.lastError_ = "Could not inspect the scene destination: " +
                fileError.message();
            return false;
        }

        const bool replacingExisting = fs::exists(destination, fileError);
        if (fileError)
        {
            scenes_.lastError_ = "Could not inspect the scene destination: " +
                fileError.message();
            return false;
        }

        std::string identityError;
        if (!EnsurePersistentEntityIdentities(
                scenes_.scene_,
                identityError))
        {
            scenes_.lastError_ = "Scene identity validation failed: " +
                identityError;
            return false;
        }

        // Keep the active document canonical too. PrepareWickedSceneOpen()
        // repairs loaded copies before physics runs; doing the same immediately
        // before serialization ensures the corrected component ownership is
        // persisted when an affected creator saves the scene again.
        const auto physicsRepair =
            RepairReusableAssetCollisionTargets(scenes_.scene_);
        if (physicsRepair.conflictCount > 0)
        {
            lastWarning_ =
                "Scene saved with ambiguous reusable-asset rigid-body "
                "ownership; " + std::to_string(physicsRepair.conflictCount) +
                " nested body/bodies require explicit creator review.";
        }

        const std::string token = SaveToken();
        const fs::path temporary = parent /
            (destination.filename().generic_u8string() + ".saving-" + token +
                ".wiscene");
        const fs::path pendingPrevious = parent /
            (destination.filename().generic_u8string() + ".previous-" + token +
                ".bak");

        bool archiveWritten = false;
        try
        {
            // Construct against the final name so Wicked records the correct
            // resource directory and container name, but explicitly write to
            // the temporary file. Replacing the archive object prevents its
            // destructor from writing directly over the live destination.
            wi::Archive archive(destination.generic_u8string(), false, false);
            if (!archive.IsOpen())
            {
                scenes_.lastError_ = "Could not create a WISCENE archive for: " +
                    destination.generic_u8string();
                return false;
            }
            archive.SetCompressionEnabled(true);
            scenes_.scene_.Serialize(archive);
            archiveWritten = archive.SaveFile(temporary.generic_u8string());
            archive = wi::Archive();
        }
        catch (const std::exception& error)
        {
            RemoveWithoutThrow(temporary);
            scenes_.lastError_ = "WISCENE serialization failed: " +
                std::string(error.what());
            return false;
        }
        catch (...)
        {
            RemoveWithoutThrow(temporary);
            scenes_.lastError_ = "WISCENE serialization failed: " + filePath;
            return false;
        }

        if (!archiveWritten)
        {
            RemoveWithoutThrow(temporary);
            scenes_.lastError_ = "The new scene archive could not be written: " +
                destination.generic_u8string();
            return false;
        }

        auto validation = PrepareWickedSceneOpen(
            temporary.generic_u8string());
        if (!validation.IsReady())
        {
            RemoveWithoutThrow(temporary);
            scenes_.lastError_ = "The newly written scene failed validation: " +
                validation.Error();
            return false;
        }

        if (replacingExisting)
        {
            fs::copy_file(
                destination,
                pendingPrevious,
                fs::copy_options::overwrite_existing,
                fileError);
            if (fileError)
            {
                RemoveWithoutThrow(temporary);
                scenes_.lastError_ = "The previous scene could not be protected: " +
                    fileError.message();
                return false;
            }
        }

        fileError.clear();
        if (!ReplaceFileAtomically(temporary, destination, fileError))
        {
            RemoveWithoutThrow(temporary);
            RemoveWithoutThrow(pendingPrevious);
            scenes_.lastError_ = "The validated scene could not replace the "
                "destination: " + fileError.message();
            return false;
        }

        auto finalValidation = PrepareWickedSceneOpen(
            destination.generic_u8string());
        if (!finalValidation.IsReady())
        {
            if (replacingExisting)
            {
                std::error_code restoreError;
                fs::copy_file(
                    pendingPrevious,
                    destination,
                    fs::copy_options::overwrite_existing,
                    restoreError);
                if (restoreError)
                {
                    scenes_.lastError_ = "The saved scene failed final "
                        "validation and its previous version could not be "
                        "restored: " + restoreError.message();
                    return false;
                }
            }
            else
            {
                RemoveWithoutThrow(destination);
            }
            RemoveWithoutThrow(pendingPrevious);
            scenes_.lastError_ = "The saved scene failed final validation: " +
                finalValidation.Error();
            return false;
        }

        if (replacingExisting)
        {
            const fs::path previousBackup = destination.parent_path() /
                (destination.stem().generic_u8string() + ".bak.wiscene");
            fileError.clear();
            fs::copy_file(
                pendingPrevious,
                previousBackup,
                fs::copy_options::overwrite_existing,
                fileError);
            if (fileError)
            {
                if (!lastWarning_.empty())
                {
                    lastWarning_ += " ";
                }
                lastWarning_ += "Scene saved, but the previous backup could "
                    "not be promoted. Its recovery copy remains at " +
                    pendingPrevious.generic_u8string() + ": " +
                    fileError.message();
            }
            else
            {
                RemoveWithoutThrow(pendingPrevious);
            }
        }

        const fs::path backupDirectory = fs::u8path(
            AutomaticBackupDirectory(destination.generic_u8string()));
        fileError.clear();
        fs::create_directories(backupDirectory, fileError);
        const fs::path automaticBackup = backupDirectory /
            (destination.stem().generic_u8string() + "." + token +
                ".wiscene");
        if (!fileError)
        {
            fs::copy_file(
                destination,
                automaticBackup,
                fs::copy_options::overwrite_existing,
                fileError);
        }
        if (fileError)
        {
            if (!lastWarning_.empty())
            {
                lastWarning_ += " ";
            }
            lastWarning_ += "Scene saved, but its automatic backup failed: " +
                fileError.message();
        }
        else
        {
            std::vector<fs::directory_entry> backups;
            for (fs::directory_iterator iterator(backupDirectory, fileError);
                !fileError && iterator != fs::directory_iterator();
                iterator.increment(fileError))
            {
                if (iterator->is_regular_file() &&
                    wi::helper::toUpper(
                        iterator->path().extension().generic_u8string()) ==
                        ".WISCENE")
                {
                    backups.push_back(*iterator);
                }
            }
            std::sort(
                backups.begin(),
                backups.end(),
                [](const fs::directory_entry& left,
                    const fs::directory_entry& right)
                {
                    return left.path().filename() < right.path().filename();
                });
            while (backups.size() > MaximumAutomaticBackups)
            {
                RemoveWithoutThrow(backups.front().path());
                backups.erase(backups.begin());
            }
        }

        scenes_.currentPath_ = destination.generic_u8string();
        scenes_.lastError_.clear();
        commands_.MarkSaved();
        return true;
    }

    void SceneDocumentService::NewScene()
    {
        scenes_.NewScene();
        selection_.Clear();
        commands_.Clear();
        lastOpenedCamera_ = wi::ecs::INVALID_ENTITY;
        lastWarning_.clear();
    }

    bool SceneDocumentService::Reload()
    {
        if (scenes_.CurrentPath().empty())
        {
            scenes_.lastError_ = "There is no saved scene to reopen.";
            return false;
        }
        return Open(scenes_.CurrentPath());
    }
}
