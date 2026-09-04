#include "renegade/bridge/TestLevelSnapshotService.h"

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/ScriptAuthoringService.h"
#include "renegade/bridge/ScriptDocumentService.h"

#include <algorithm>
#include <atomic>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

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

    bool CopyCreatorScriptTree(
        const fs::path& sourceRoot,
        const fs::path& destinationRoot,
        std::string& error)
    {
        std::error_code ec;
        const fs::file_status rootStatus = fs::status(sourceRoot, ec);
        if (rootStatus.type() == fs::file_type::not_found)
        {
            error =
                "The active script document references Content/Scripts, but "
                "that project directory does not exist.";
            return false;
        }
        if (ec || !fs::is_directory(rootStatus))
        {
            error = "Could not inspect the project Content/Scripts directory";
            if (ec)
                error += ": " + ec.message();
            return false;
        }

        fs::create_directories(destinationRoot, ec);
        if (ec)
        {
            error = "Could not create the Test Level Content/Scripts directory: " +
                ec.message();
            return false;
        }

        fs::recursive_directory_iterator iterator(
            sourceRoot,
            fs::directory_options::none,
            ec);
        const fs::recursive_directory_iterator end;
        if (ec)
        {
            error = "Could not enumerate project creator scripts: " + ec.message();
            return false;
        }

        for (; iterator != end; iterator.increment(ec))
        {
            if (ec)
            {
                error = "Could not enumerate project creator scripts: " +
                    ec.message();
                return false;
            }

            const fs::path source = iterator->path();
            const fs::file_status status = iterator->symlink_status(ec);
            if (ec)
            {
                error = "Could not inspect creator script entry: " +
                    source.generic_u8string() + ": " + ec.message();
                return false;
            }
            if (fs::is_symlink(status))
            {
                error = "Refusing a symlink inside governed Content/Scripts: " +
                    source.generic_u8string();
                return false;
            }

            const fs::path relative = source.lexically_relative(sourceRoot);
            const std::string relativeText = relative.generic_u8string();
            if (relative.empty() ||
                relativeText == ".." ||
                relativeText.rfind("../", 0) == 0)
            {
                error = "Creator script escaped the governed Content/Scripts root.";
                return false;
            }
            const fs::path destination = destinationRoot / relative;

            if (fs::is_directory(status))
            {
                fs::create_directories(destination, ec);
            }
            else if (fs::is_regular_file(status))
            {
                fs::create_directories(destination.parent_path(), ec);
                if (!ec)
                {
                    fs::copy_file(
                        source,
                        destination,
                        fs::copy_options::overwrite_existing,
                        ec);
                }
            }
            else
            {
                error = "Unsupported file type inside governed Content/Scripts: " +
                    source.generic_u8string();
                return false;
            }

            if (ec)
            {
                error = "Could not snapshot creator script entry: " +
                    source.generic_u8string() + ": " + ec.message();
                return false;
            }
        }

        error.clear();
        return true;
    }

    bool IsSafeSnapshotContentPath(const fs::path& value)
    {
        const fs::path normalized = value.lexically_normal();
        if (normalized.empty() || normalized.is_absolute() ||
            normalized.has_root_name() || normalized.has_root_directory())
        {
            return false;
        }
        auto part = normalized.begin();
        if (part == normalized.end() || (*part).generic_u8string() != "Content")
            return false;
        for (; part != normalized.end(); ++part)
        {
            if (*part == "." || *part == "..")
                return false;
        }
        return true;
    }

    bool SnapshotGovernedMaterialInputs(
        const renegade::bridge::ProjectMetadata& project,
        const renegade::bridge::TestLevelSnapshot& snapshot,
        const wi::scene::Scene& scene,
        std::string& error)
    {
        using namespace renegade::bridge;

        std::vector<MaterialTextureBindingRecord> bindings;
        if (!InspectMaterialTextureBindings(scene, bindings, error))
        {
            error = "Could not inspect governed material bindings for Test Level: " + error;
            return false;
        }
        if (bindings.empty())
        {
            error.clear();
            return true;
        }

        AssetRegistry registry;
        if (!ReadAssetRegistry(project.rootPath, project.projectId, registry, error))
        {
            error = "Could not read the governed asset registry for Test Level: " + error;
            return false;
        }

        std::string registrySourceText;
        if (!ResolveAssetRegistryDocumentPath(project.rootPath, registrySourceText, error))
        {
            error = "Could not resolve the governed asset registry for Test Level: " + error;
            return false;
        }

        const fs::path sourceRoot = fs::u8path(project.rootPath);
        const fs::path snapshotRoot = fs::u8path(snapshot.sessionDirectory);
        std::error_code ec;
        fs::copy_file(
            fs::u8path(registrySourceText),
            snapshotRoot / AssetRegistryDocumentName,
            fs::copy_options::overwrite_existing,
            ec);
        if (ec)
        {
            error = "Could not snapshot the governed asset registry: " + ec.message();
            return false;
        }

        std::unordered_set<StableId> copiedAssets;
        for (const auto& binding : bindings)
        {
            if (!copiedAssets.insert(binding.textureAssetId).second)
                continue;

            const auto record = std::find_if(
                registry.records.begin(), registry.records.end(),
                [&](const AssetRecord& candidate)
                {
                    return candidate.assetId == binding.textureAssetId;
                });
            if (record == registry.records.end() ||
                record->dependencyClass != DependencyClass::Texture ||
                !record->sourceAvailable)
            {
                error = "Test Level material binding references an unavailable governed texture: " +
                    binding.textureAssetId;
                return false;
            }

            const fs::path relative =
                fs::u8path(record->projectRelativePath).lexically_normal();
            if (!IsSafeSnapshotContentPath(relative))
            {
                error = "Governed Test Level texture product escaped Content: " +
                    record->projectRelativePath;
                return false;
            }

            ec.clear();
            const fs::path source = sourceRoot / relative;
            const fs::file_status status = fs::symlink_status(source, ec);
            if (ec || fs::is_symlink(status) || !fs::is_regular_file(status))
            {
                error = "Governed Test Level texture product is unavailable: " +
                    source.generic_u8string();
                if (ec)
                    error += ": " + ec.message();
                return false;
            }

            const fs::path destination = snapshotRoot / relative;
            fs::create_directories(destination.parent_path(), ec);
            if (!ec)
            {
                fs::copy_file(
                    source,
                    destination,
                    fs::copy_options::overwrite_existing,
                    ec);
            }
            if (ec)
            {
                error = "Could not snapshot governed Test Level texture product: " +
                    source.generic_u8string() + ": " + ec.message();
                return false;
            }
        }

        error.clear();
        return true;
    }

    bool SnapshotCreatorScripts(
        const renegade::bridge::ProjectMetadata& project,
        const renegade::bridge::TestLevelSnapshot& snapshot,
        renegade::bridge::SceneService& scenes,
        renegade::bridge::ScriptAuthoringService* scripts,
        std::string& error)
    {
        using namespace renegade::bridge;

        const std::string sourceScenePath = scenes.CurrentPath();
        if (sourceScenePath.empty())
        {
            error.clear();
            return true;
        }

        ScriptDocument document;
        bool haveDocument = false;
        bool liveAuthority = false;

        if (scripts != nullptr)
        {
            if (!scripts->EnsureCurrent(error))
            {
                error = "Could not bind live creator scripting for Test Level: " + error;
                return false;
            }
            const ScriptDocument* live = scripts->Document();
            if (live == nullptr)
            {
                error =
                    "Studio scripting authority did not provide a live document.";
                return false;
            }

            std::error_code pathError;
            const bool sameScene = fs::equivalent(
                fs::u8path(scripts->LoadedScenePath()),
                fs::u8path(sourceScenePath),
                pathError);
            if (pathError || !sameScene)
            {
                error =
                    "Live creator scripting is bound to a different Scene than Test Level.";
                return false;
            }

            document = *live;
            haveDocument = true;
            liveAuthority = true;
        }

        const std::string sourceCompanionPath =
            ScriptDocumentPathForScene(sourceScenePath);
        DocumentEnvelope sceneEnvelope;

        if (!haveDocument)
        {
            std::error_code ec;
            const fs::file_status companionStatus =
                fs::status(fs::u8path(sourceCompanionPath), ec);
            if (companionStatus.type() == fs::file_type::not_found)
            {
                ec.clear();
                error.clear();
                return true;
            }
            if (ec)
            {
                error = "Could not inspect the active Scene script companion: " +
                    ec.message();
                return false;
            }
            if (!fs::is_regular_file(companionStatus))
            {
                error.clear();
                return true;
            }

            if (!ReadDocumentEnvelope(
                    sourceScenePath + ".rmeta",
                    sceneEnvelope,
                    error))
            {
                error = "Could not read the active Scene identity for Test Level: " +
                    error;
                return false;
            }
            if (!ReadScriptDocument(
                    sourceCompanionPath,
                    project.projectId,
                    sceneEnvelope.documentId,
                    document,
                    error))
            {
                error = "Could not read the active Scene script companion: " + error;
                return false;
            }
            haveDocument = true;
        }

        if (!haveDocument || document.attachments.empty())
        {
            // A loaded empty document is authoritative too: an unsaved removal
            // must not resurrect scripts from an older on-disk companion.
            error.clear();
            return true;
        }

        if (!ValidateScriptDocumentAgainstScene(document, scenes.GetScene(), error))
        {
            error = "Live creator scripting state is not valid for the active Scene: " +
                error;
            return false;
        }

        if (liveAuthority)
        {
            if (!ReadDocumentEnvelope(
                    sourceScenePath + ".rmeta",
                    sceneEnvelope,
                    error))
            {
                error = "Could not read the active Scene identity for Test Level: " +
                    error;
                return false;
            }
        }

        if (sceneEnvelope.projectId != project.projectId ||
            document.envelope.projectId != project.projectId ||
            sceneEnvelope.documentId != document.sceneDocumentId ||
            sceneEnvelope.documentType != "scene")
        {
            error =
                "Creator scripting identity does not match the active project/Scene.";
            return false;
        }

        DocumentEnvelope snapshotSceneEnvelope = sceneEnvelope;
        if (!RetargetDocumentEnvelope(
                snapshotSceneEnvelope,
                TestLevelStartupScene,
                error))
        {
            error = "Could not retarget the Test Level Scene identity: " + error;
            return false;
        }
        // Transactional Renegade document writes create render and journal
        // suffixes. The Test Level snapshot already lives under a deliberately
        // deep project/Intermediate path, so doing those transactions in-place
        // can cross the Windows legacy path limit even when the final shadow
        // files themselves fit. Render and validate under the OS temp root,
        // then install the committed bytes into the disposable snapshot.
        std::error_code stagingError;
        fs::path stagingRoot = fs::temp_directory_path(stagingError);
        if (stagingError)
        {
            error = "Could not resolve short Test Level staging storage: " +
                stagingError.message();
            return false;
        }
        stagingRoot /= fs::u8path(
            "renegade-tls-" + GenerateStableId());
        fs::create_directories(stagingRoot, stagingError);
        if (stagingError)
        {
            error = "Could not create short Test Level staging storage: " +
                stagingError.message();
            return false;
        }
        const auto cleanupStaging = [&]()
        {
            std::error_code ignored;
            fs::remove_all(stagingRoot, ignored);
        };

        const fs::path stagedSceneEnvelopePath =
            stagingRoot / "TestLevelScene.rmeta";
        const fs::path finalSceneEnvelopePath =
            fs::u8path(snapshot.scenePath + ".rmeta");
        if (!WriteDocumentEnvelope(
                stagedSceneEnvelopePath.generic_u8string(),
                snapshotSceneEnvelope,
                error))
        {
            cleanupStaging();
            error = "Could not stage the Test Level Scene identity: " + error;
            return false;
        }

        fs::copy_file(
            stagedSceneEnvelopePath,
            finalSceneEnvelopePath,
            fs::copy_options::overwrite_existing,
            stagingError);
        if (stagingError)
        {
            cleanupStaging();
            error = "Could not install the Test Level Scene identity: " +
                stagingError.message();
            return false;
        }

        ScriptDocument snapshotDocument = document;
        snapshotDocument.scenePathHint = TestLevelStartupScene;
        if (!RetargetDocumentEnvelope(
                snapshotDocument.envelope,
                ScriptDocumentPathHintForScene(TestLevelStartupScene),
                error))
        {
            error = "Could not retarget the Test Level script companion: " + error;
            return false;
        }
        const fs::path stagedScriptPath =
            stagingRoot / "TestLevel.rscripts";
        const fs::path finalScriptPath =
            fs::u8path(ScriptDocumentPathForScene(snapshot.scenePath));
        if (!WriteScriptDocument(
                stagedScriptPath.generic_u8string(),
                snapshotDocument,
                error))
        {
            cleanupStaging();
            error = "Could not stage the Test Level script companion: " + error;
            return false;
        }

        stagingError.clear();
        fs::copy_file(
            stagedScriptPath,
            finalScriptPath,
            fs::copy_options::overwrite_existing,
            stagingError);
        if (stagingError)
        {
            cleanupStaging();
            error = "Could not install the Test Level script companion: " +
                stagingError.message();
            return false;
        }
        cleanupStaging();

        const fs::path sourceScripts =
            fs::u8path(project.rootPath) / "Content" / "Scripts";
        const fs::path snapshotScripts =
            fs::u8path(snapshot.sessionDirectory) / "Content" / "Scripts";
        if (!CopyCreatorScriptTree(sourceScripts, snapshotScripts, error))
            return false;

        error.clear();
        return true;
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
            if (!SnapshotCreatorScripts(
                    project,
                    created,
                    scenes_,
                    scripts_,
                    error))
            {
                return failAndCleanup(
                    "Could not snapshot creator scripting state: " + error);
            }

            if (!SnapshotGovernedMaterialInputs(
                    project,
                    created,
                    scenes_.GetScene(),
                    error))
            {
                return failAndCleanup(
                    "Could not snapshot governed material state: " + error);
            }

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
