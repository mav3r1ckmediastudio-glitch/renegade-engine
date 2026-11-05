#include "renegade/bridge/PlaceableWaterfallRenderer.h"
#include "renegade/bridge/SceneDocumentService.h"

#include "renegade/bridge/CollisionService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/LightmapBakeService.h"
#include "renegade/bridge/NightSkyService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/RiverSplineService.h"
#include "renegade/bridge/RiverbedMaterialService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/TerrainService.h"
#include "renegade/bridge/VegetationService.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
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

    // A new project Scene and its stable level identity are one disk change.
    // Reuse the existing journaled multi-document transaction so a failed
    // second replacement cannot leave a playable-looking identity-less Scene.
    bool CommitSceneWithNewIdentity(
        const fs::path& archivePath,
        const fs::path& destination,
        const fs::path& projectRoot,
        const renegade::bridge::DocumentEnvelope& envelope,
        std::string& error)
    {
        using namespace renegade::bridge;
        struct RenderDirectory
        {
            fs::path path;
            ~RenderDirectory()
            {
                std::error_code ignored;
                if (!path.empty()) fs::remove_all(path, ignored);
            }
        } render;
        std::error_code ec;
        const auto tempRoot = fs::temp_directory_path(ec);
        if (ec)
        {
            error = "Could not resolve Scene identity staging directory: " + ec.message();
            return false;
        }
        render.path = tempRoot / ("renegade-scene-identity-" + GenerateStableId());
        fs::create_directories(render.path, ec);
        if (ec)
        {
            error = "Could not create Scene identity staging directory: " + ec.message();
            return false;
        }
        const auto renderedIdentity = render.path / "Scene.rmeta";
        if (!WriteDocumentEnvelope(renderedIdentity.generic_u8string(), envelope, error))
            return false;

        const auto read = [](const fs::path& path, std::vector<std::uint8_t>& bytes)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream) return false;
            bytes.assign(std::istreambuf_iterator<char>(stream), {});
            return !stream.bad();
        };
        ProjectDocumentWrite sceneWrite;
        sceneWrite.destinationPath = destination.generic_u8string();
        if (!read(archivePath, sceneWrite.content))
        {
            error = "Could not read validated Scene archive for transaction.";
            return false;
        }
        // The archive was already deserialized above. Transaction staging
        // appends a suffix, so compare its exact validated bytes rather than
        // invoking the extension-restricted Open Scene API on the staged name.
        sceneWrite.validator = [read, expected = sceneWrite.content](
            const std::string& path, std::string& validationError)
        {
            std::vector<std::uint8_t> actual;
            if (!read(fs::u8path(path), actual) || actual != expected)
            {
                validationError = "Staged Scene differs from the validated archive.";
                return false;
            }
            return true;
        };
        ProjectDocumentWrite identityWrite;
        identityWrite.destinationPath = destination.generic_u8string() + ".rmeta";
        if (!read(renderedIdentity, identityWrite.content))
        {
            error = "Could not read rendered Scene identity for transaction.";
            return false;
        }
        identityWrite.validator = [envelope](
            const std::string& path, std::string& validationError)
        {
            DocumentEnvelope actual;
            if (!ReadDocumentEnvelope(path, actual, validationError)) return false;
            if (actual.documentId != envelope.documentId ||
                actual.projectId != envelope.projectId ||
                actual.documentType != envelope.documentType ||
                actual.pathHint != envelope.pathHint)
            {
                validationError = "Staged Scene identity does not match its document.";
                return false;
            }
            return true;
        };
        ProjectDocumentTransactionOptions options;
        options.allowedRoot = projectRoot.generic_u8string();
        options.journalDirectory =
            (projectRoot / "Intermediate/Transactions").generic_u8string();
        std::vector<ProjectDocumentWrite> writes;
        writes.push_back(std::move(sceneWrite));
        writes.push_back(std::move(identityWrite));
        const auto result = ProjectDocumentTransaction().Execute(
            std::move(writes), std::move(options));
        if (!result.success)
            error = "Scene/identity transaction failed [" + result.code + "]: " + result.message;
        return result.success;
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
        std::string riverNormalError;
        if (!RestoreBundledRiverWaterNormals(*prepared.scene_, riverNormalError))
        {
            scenes_.lastError_ = "Could not restore river water: " + riverNormalError;
            return false;
        }

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
        std::string rockyError;
        if(!RebindBundledRockyRiverbedMaterials(scenes_.scene_,rockyError))
            wi::backlog::post("ROCKY RIVERBED RESOURCE WARNING: "+rockyError);
        RebindWickedVegetationResources(scenes_.scene_);
        RestoreNightSky(scenes_.scene_);
        scenes_.currentPath_ = std::move(prepared.path_);
        scenes_.lastError_.clear();
        ++scenes_.revision_;
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

        // Raw WISCENE documents remain supported without a project. Inside
        // a project every saved Level needs its own stable .rmeta identity,
        // including a first Save and Save As without any scripting attachments.
        DocumentEnvelope sceneEnvelope;
        bool createSceneIdentity = false;
        fs::path projectRoot;
        if (projects_.HasProject())
        {
            const auto& project = projects_.CurrentProject();
            projectRoot = fs::weakly_canonical(fs::u8path(project.rootPath), fileError);
            const auto hint = fs::relative(destination, projectRoot, fileError);
            const auto resolved = ResolveDependencyPath(
                project.rootPath, hint.generic_u8string());
            if (fileError || !resolved.accepted)
            {
                scenes_.lastError_ = "Save the Level inside the active project: " +
                    (fileError ? fileError.message() : resolved.error);
                return false;
            }
            const fs::path sidecar = destination.generic_u8string() + ".rmeta";
            const bool hasIdentity = fs::exists(sidecar, fileError);
            if (fileError)
            {
                scenes_.lastError_ = "Could not inspect the Level identity: " + fileError.message();
                return false;
            }
            if (hasIdentity)
            {
                std::string error;
                if (!ReadDocumentEnvelope(sidecar.generic_u8string(), sceneEnvelope, error) ||
                    sceneEnvelope.projectId != project.projectId ||
                    sceneEnvelope.documentType != SceneDocumentType)
                {
                    scenes_.lastError_ = "The destination Level identity is invalid or belongs "
                        "to another project. " + error;
                    return false;
                }
            }
            else
            {
                sceneEnvelope = CreateDocumentEnvelope(
                    project.projectId, SceneDocumentType,
                    resolved.canonicalRelativePath, "Renegade Studio");
                std::string error;
                if (!ValidateDocumentEnvelope(sceneEnvelope, error))
                {
                    scenes_.lastError_ = "Could not create the Level identity: " + error;
                    return false;
                }
                createSceneIdentity = true;
            }
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
            {
                // An opt-in diagnostic surface shader is a process-specific
                // renderer ID, never part of a portable WISCENE document.
                RemoveGeneratedWaterfallVisuals(scenes_.scene_);
                ScopedRiverLocalDepthArchiveMask nativeWater(scenes_.scene_);
                scenes_.scene_.Serialize(archive);
            }
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
        std::string commitError;
        const bool committed = createSceneIdentity
            ? CommitSceneWithNewIdentity(
                temporary, destination, projectRoot, sceneEnvelope, commitError)
            : ReplaceFileAtomically(temporary, destination, fileError);
        if (!committed)
        {
            RemoveWithoutThrow(temporary);
            RemoveWithoutThrow(pendingPrevious);
            scenes_.lastError_ = "The validated scene could not replace the "
                "destination: " + (createSceneIdentity ? commitError : fileError.message());
            return false;
        }
        if (createSceneIdentity)
            RemoveWithoutThrow(temporary);

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
            if (createSceneIdentity)
                RemoveWithoutThrow(destination.generic_u8string() + ".rmeta");
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
