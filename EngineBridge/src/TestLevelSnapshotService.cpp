#include "renegade/bridge/ParticleEffectRuntimeDependencies.h"
#include "renegade/bridge/TestLevelLaunchTiming.h"
#include "renegade/bridge/PlaceableWaterfallRenderer.h"
#include "renegade/bridge/TestLevelSnapshotService.h"
#include "renegade/bridge/ImpactAudioService.h"
#include "renegade/bridge/InventoryTestLevelSnapshot.h"
#include "renegade/bridge/InventoryDependencyProvider.h"

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/PlayerPrefabService.h"
#include "renegade/bridge/GameplayInputService.h"
#include "renegade/bridge/EquipmentAssetService.h"
#include "renegade/bridge/ProjectileAssetService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/HudSelection.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/ScriptAuthoringService.h"
#include "renegade/bridge/ScriptDocumentService.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <cstdlib>
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

    struct PendingSceneCaptures
    {
        std::mutex mutex;
        std::unordered_map<std::string, wi::vector<std::uint8_t>> bytes;
    };
    PendingSceneCaptures& PendingCaptures()
    {
        static PendingSceneCaptures captures;
        return captures;
    }

    std::atomic<std::uint64_t> snapshotSequence{0};
    constexpr const char* TestLevelStartupScene =
        "Content/Scenes/TestLevel.wiscene";

    bool SnapshotGameplayInput(const renegade::bridge::ProjectMetadata& project,
        const renegade::bridge::TestLevelSnapshot& snapshot, std::string& error)
    {
        using namespace renegade::bridge;
        const auto resolved = ResolveDependencyPath(project.rootPath, GameplayInputDocumentRelativePath);
        if (!resolved.accepted) { error = "Gameplay input map escaped the project."; return false; }
        GameplayInputMap map;
        fs::path staging;
        fs::path source = fs::u8path(resolved.absolutePath);
        if (resolved.exists)
        {
            if (!ReadGameplayInputMapFile(source.generic_u8string(), map, error)) return false;
        }
        else
        {
            // Generate defaults under a short temporary root; snapshots must not
            // ask Runtime to journal a new document inside a deeply nested path.
            staging = fs::temp_directory_path() / ("renegade-input-" + GenerateStableId());
            map = MakeDefaultGameplayInputMap();
            if (!WriteGameplayInputMap(staging.generic_u8string(), map, error))
            { std::error_code ignored; fs::remove_all(staging, ignored); return false; }
            source = fs::u8path(GameplayInputDocumentPath(staging.generic_u8string()));
        }
        const auto destination = fs::u8path(snapshot.sessionDirectory) / GameplayInputDocumentRelativePath;
        std::error_code ec;
        fs::create_directories(destination.parent_path(), ec);
        if (!ec) fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
        if (!staging.empty()) { std::error_code ignored; fs::remove_all(staging, ignored); }
        if (ec) { error = "Could not snapshot gameplay input map: " + ec.message(); return false; }
        return ReadGameplayInputMapFile(destination.generic_u8string(), map, error);
    }

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
        stream << "default_hud_id = " << sourceProject.defaultHudId << '\n';
        stream << "default_hud = " << sourceProject.defaultHud << '\n';
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
        {
            auto& captures = PendingCaptures();
            std::lock_guard<std::mutex> lock(captures.mutex);
            for (auto i=captures.bytes.begin(); i!=captures.bytes.end();)
            {
                if (fs::u8path(i->first).parent_path().parent_path().parent_path() == sessionDirectory)
                    i=captures.bytes.erase(i);
                else ++i;
            }
        }
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

    bool SnapshotProjectDefaultHud(
        const renegade::bridge::ProjectMetadata& project,
        const renegade::bridge::TestLevelSnapshot& snapshot,
        std::string& repairedHint,
        std::string& error)
    {
        using namespace renegade::bridge;
        repairedHint.clear();
        if (project.defaultHudId.empty() && project.defaultHud.empty())
        {
            error.clear();
            return true;
        }
        if (project.defaultHudId.empty() || project.defaultHud.empty() ||
            !ValidateHudReference({project.defaultHudId, project.defaultHud}, error))
        {
            if (error.empty()) error = "Test Level project default HUD reference is incomplete.";
            return false;
        }

        std::string sourcePath;
        if (!ResolveRuntimeScreenDocumentPath(project.rootPath, project.projectId,
                project.defaultHudId, project.defaultHud, sourcePath, error))
            return false;
        ScreenDocument hud;
        if (!ReadScreenDocument(sourcePath, project.projectId, hud, error))
            return false;
        if (hud.usage != ScreenUsage::Hud || !hud.enabledInGame)
        {
            error = "Test Level project default must be an enabled HUD Screen.";
            return false;
        }

        std::error_code ec;
        const fs::path relative = fs::relative(fs::u8path(sourcePath),
            fs::u8path(project.rootPath), ec);
        if (ec || !IsSafeSnapshotContentPath(relative))
        {
            error = "Test Level HUD path resolves outside governed project Content.";
            return false;
        }
        repairedHint = relative.generic_u8string();
        if (!ValidateHudReference({project.defaultHudId, repairedHint}, error))
            return false;

        const auto copyDependency = [&](const std::string& path) -> bool
        {
            if (!IsSafeSnapshotContentPath(fs::u8path(path)))
            {
                error = "HUD resource has an unsafe project Content path: " + path;
                return false;
            }
            const auto source = ResolveDependencyPath(project.rootPath, path);
            if (!source.accepted || !source.exists)
            {
                error = "HUD resource is missing or outside the project: " + path;
                return false;
            }
            const fs::path destination = fs::u8path(snapshot.sessionDirectory) /
                fs::u8path(path);
            std::error_code copyError;
            fs::create_directories(destination.parent_path(), copyError);
            if (!copyError)
                fs::copy_file(fs::u8path(source.absolutePath), destination,
                    fs::copy_options::overwrite_existing, copyError);
            if (copyError)
            {
                error = "Could not snapshot HUD resource '" + path + "': " +
                    copyError.message();
                return false;
            }
            return true;
        };

        RuntimeScreenDependencyDocument resources;
        if (!MakeRuntimeScreenDependencyReader(project.projectId)(
                sourcePath, resources, error))
            return false;
        for (const auto& image : resources.imagePaths)
            if (!copyDependency(image)) return false;
        for (const auto& font : resources.fontPaths)
            if (!copyDependency(font)) return false;
        if (!copyDependency(repairedHint)) return false;

        ScreenDocument verified;
        if (!ReadScreenDocument((fs::u8path(snapshot.sessionDirectory) / relative)
                .generic_u8string(), project.projectId, verified, error) ||
            verified.envelope.documentId != project.defaultHudId ||
            verified.usage != ScreenUsage::Hud)
        {
            if (error.empty()) error = "Test Level HUD identity verification failed.";
            return false;
        }
        error.clear();
        return true;
    }

    // Reuse the production Build Game inventory and Screen dependency closure.
    // Stack companions alone cannot run G5 pickup effects or an assigned overlay.
    bool SnapshotInventoryProjectInputs(
        const renegade::bridge::ProjectMetadata& project,
        const renegade::bridge::TestLevelSnapshot& snapshot,
        const std::string& sourceScenePath,
        std::string& error)
    {
        using namespace renegade::bridge;
        std::error_code ec;
        if (!fs::exists(fs::u8path(sourceScenePath + ".rinventory"), ec))
        {
            if (ec) { error = "Could not inspect inventory source: " + ec.message(); return false; }
            return true;
        }
        const auto root = fs::u8path(project.rootPath);
        const auto sceneRelative = fs::relative(fs::u8path(sourceScenePath), root, ec);
        if (ec || !IsSafeSnapshotContentPath(sceneRelative))
        {
            error = "Inventory Scene is outside governed project Content.";
            return false;
        }
        const auto catalogue = root / "Content/Gameplay/Inventory.renegade-items";
        const bool hasCatalogue = fs::exists(catalogue, ec);
        if (ec) { error = "Could not inspect inventory catalogue: " + ec.message(); return false; }
        if (hasCatalogue)
        {
            DocumentEnvelope envelope;
            if (!ReadDocumentEnvelope(catalogue.generic_u8string(), envelope, error)) return false;
            if (envelope.projectId != project.projectId)
            {
                error = "Inventory catalogue belongs to another project.";
                return false;
            }
        }
        InventoryDependencyProvider inventory;
        RuntimeScreenDependencyProvider screens(MakeRuntimeScreenDependencyReader(project.projectId));
        DependencyCollector collector(project.rootPath);
        if (!collector.RegisterProvider(inventory, error) ||
            !collector.RegisterProvider(screens, error) ||
            !collector.AddRoot({sceneRelative.generic_u8string(), DependencyClass::Scene,
                DependencyRequirement::Required, "testlevel.inventory"}, error) ||
            !collector.DiscoverTransitiveDependencies(error)) return false;
        const auto& graph = collector.Graph();
        if (!graph.diagnostics.empty())
        {
            error = "Inventory snapshot dependency rejected: " + graph.diagnostics.front().message;
            return false;
        }
        const auto sceneHint = sceneRelative.generic_u8string();
        for (const auto& node : graph.nodes)
        {
            // The live scene and renamed TestLevel companions were already
            // projected by the existing native snapshot/companion service.
            if (node.projectRelativePath == sceneHint ||
                node.projectRelativePath == sceneHint + ".rinventory" ||
                node.projectRelativePath == sceneHint + ".rinventory-access") continue;
            const auto relative = fs::u8path(node.projectRelativePath);
            const auto resolved = ResolveDependencyPath(project.rootPath, node.projectRelativePath);
            if (!IsSafeSnapshotContentPath(relative) || !resolved.accepted || !resolved.exists)
            {
                error = "Inventory input is missing or outside project Content: " + node.projectRelativePath;
                return false;
            }
            const auto destination = fs::u8path(snapshot.sessionDirectory) / relative;
            fs::create_directories(destination.parent_path(), ec);
            if (!ec) fs::copy_file(fs::u8path(resolved.absolutePath), destination,
                fs::copy_options::overwrite_existing, ec);
            if (ec)
            {
                error = "Could not snapshot inventory input '" + node.projectRelativePath + "': " + ec.message();
                return false;
            }
        }
        error.clear();
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

    bool SnapshotGovernedPlayerViewInputs(
        const renegade::bridge::ProjectMetadata&,
        const renegade::bridge::TestLevelSnapshot&,
        const wi::scene::Scene&, std::string&);

    bool SnapshotEquipmentInputs(
        const renegade::bridge::ProjectMetadata& project,
        const renegade::bridge::TestLevelSnapshot& snapshot,
        const renegade::bridge::PlayerControllerSettings& settings,
        std::string& error)
    {
        using namespace renegade::bridge;
        if (settings.primaryEquipmentAssetId.empty() &&
            settings.offHandEquipmentAssetId.empty()) return true;
        if (!ValidateStartingEquipment(project.rootPath, project.projectId,
                settings.primaryEquipmentAssetId, settings.offHandEquipmentAssetId, error))
            return false;
        AssetRegistry registry;
        if (!ReadAssetRegistry(project.rootPath, project.projectId, registry, error))
            return false;
        std::string registryPath;
        if (!ResolveAssetRegistryDocumentPath(project.rootPath, registryPath, error))
            return false;
        std::error_code ec;
        fs::copy_file(fs::u8path(registryPath),
            fs::u8path(snapshot.sessionDirectory) / AssetRegistryDocumentName,
            fs::copy_options::overwrite_existing, ec);
        if (ec) { error = "Could not snapshot equipment registry: " + ec.message(); return false; }
        std::unordered_set<StableId> copied;
        for (const auto& id : {settings.primaryEquipmentAssetId, settings.offHandEquipmentAssetId})
        {
            if (id.empty() || !copied.insert(id).second) continue;
            EquipmentAssetDocument document;
            if (!LoadEquipmentAsset(project.rootPath, project.projectId, id, document, error))
                return false;
            const auto record = std::find_if(registry.records.begin(), registry.records.end(),
                [&](const AssetRecord& value) { return value.assetId == id; });
            if (record == registry.records.end())
            { error = "Equipment is absent from snapshot registry: " + id; return false; }
            const auto source = ResolveDependencyPath(project.rootPath, record->projectRelativePath);
            if (!source.accepted || !source.exists ||
                !IsSafeSnapshotContentPath(fs::u8path(record->projectRelativePath)))
            { error = "Invalid equipment snapshot path: " + id; return false; }
            const auto destination = fs::u8path(snapshot.sessionDirectory) /
                fs::u8path(record->projectRelativePath);
            fs::create_directories(destination.parent_path(), ec);
            if (!ec) fs::copy_file(fs::u8path(source.absolutePath), destination,
                fs::copy_options::overwrite_existing, ec);
            if (ec) { error = "Could not snapshot equipment: " + ec.message(); return false; }
            // Reuse the governed presentation/texture closure without modifying
            // the authored player or assigning another runtime controller.
            if (!document.equipment.presentationAssetId.empty())
            {
                wi::scene::Scene presentation;
                CreatePlayerStartCommand create(presentation, {});
                if (!create.Execute())
                { error = "Could not prepare equipment presentation closure."; return false; }
                PlayerControllerSettings view;
                view.firstPersonArmsAssetId = document.equipment.presentationAssetId;
                SetPlayerControllerSettingsCommand apply(presentation, create.CreatedEntity(), view);
                (void)apply.Execute();
                if (!SnapshotGovernedPlayerViewInputs(project, snapshot, presentation, error))
                    return false;
            }
            for (const auto& binding : document.equipment.projectiles)
            {
                if (!copied.insert(binding.projectileAssetId).second) continue;
                ProjectileAssetDocument projectile;
                if (!LoadProjectileAsset(project.rootPath, project.projectId,
                        binding.projectileAssetId, projectile, error)) return false;
                const auto dependency = std::find_if(registry.records.begin(), registry.records.end(),
                    [&](const AssetRecord& value) { return value.assetId == binding.projectileAssetId; });
                if (dependency == registry.records.end())
                { error = "Projectile is absent from snapshot registry."; return false; }
                const auto projectileSource = ResolveDependencyPath(project.rootPath, dependency->projectRelativePath);
                if (!projectileSource.accepted || !projectileSource.exists ||
                    !IsSafeSnapshotContentPath(fs::u8path(dependency->projectRelativePath)))
                { error = "Invalid projectile snapshot path."; return false; }
                const auto projectileDestination = fs::u8path(snapshot.sessionDirectory) /
                    fs::u8path(dependency->projectRelativePath);
                fs::create_directories(projectileDestination.parent_path(), ec);
                if (!ec) fs::copy_file(fs::u8path(projectileSource.absolutePath), projectileDestination,
                    fs::copy_options::overwrite_existing, ec);
                if (ec) { error = "Could not snapshot projectile: " + ec.message(); return false; }
                if (!projectile.meshAssetId.empty()) {
                    wi::scene::Scene appearance;
                    CreatePlayerStartCommand create(appearance, {});
                    if (!create.Execute()) { error="Could not prepare projectile mesh closure."; return false; }
                    PlayerControllerSettings settings;
                    settings.firstPersonArmsAssetId=projectile.meshAssetId;
                    SetPlayerControllerSettingsCommand apply(appearance, create.CreatedEntity(), settings);
                    if (!apply.Execute() ||
                        !SnapshotGovernedPlayerViewInputs(project, snapshot, appearance, error)) return false;
                }
                ProjectileAssetDocument verifiedProjectile;
                if (!LoadProjectileAsset(snapshot.sessionDirectory, project.projectId,
                        binding.projectileAssetId, verifiedProjectile, error)) return false;
            }
            EquipmentAssetDocument verified;
            if (!LoadEquipmentAsset(snapshot.sessionDirectory, project.projectId, id, verified, error))
                return false;
        }
        error.clear();
        return true;
    }

    bool SnapshotPlayerPrefab(
        const renegade::bridge::ProjectMetadata& project,
        const renegade::bridge::TestLevelSnapshot& snapshot,
        const wi::scene::Scene& scene, std::string& error)
    {
        using namespace renegade::bridge;
        const auto start = ResolvePlayerStart(scene);
        if (start.resolution != PlayerStartResolution::Success) return true;
        const auto id = CapturePlayerPrefabOrigin(scene, start.start.entity);
        if (id.empty()) return true;
        PlayerPrefabDocument document;
        if (!LoadPlayerPrefab(project.rootPath, project.projectId, id, document, error))
            return false;
        AssetRegistry registry;
        if (!ReadAssetRegistry(project.rootPath, project.projectId, registry, error)) return false;
        const auto record = std::find_if(registry.records.begin(), registry.records.end(),
            [&](const AssetRecord& r) { return r.assetId == id; });
        if (record == registry.records.end()) return false;
        const auto source = ResolveDependencyPath(project.rootPath, record->projectRelativePath);
        if (!source.accepted || !source.exists ||
            !IsSafeSnapshotContentPath(fs::u8path(record->projectRelativePath)))
        { error = "Invalid player prefab snapshot path."; return false; }
        std::error_code ec;
        const auto destination = fs::u8path(snapshot.sessionDirectory) /
            fs::u8path(record->projectRelativePath);
        fs::create_directories(destination.parent_path(), ec);
        if (!ec) fs::copy_file(fs::u8path(source.absolutePath), destination,
            fs::copy_options::overwrite_existing, ec);
        if (ec) { error = "Could not snapshot player prefab: " + ec.message(); return false; }
        std::string registryPath;
        if (!ResolveAssetRegistryDocumentPath(project.rootPath, registryPath, error)) return false;
        fs::copy_file(fs::u8path(registryPath),
            fs::u8path(snapshot.sessionDirectory) / AssetRegistryDocumentName,
            fs::copy_options::overwrite_existing, ec);
        if (ec) { error = "Could not snapshot player prefab registry: " + ec.message(); return false; }
        if (!SnapshotEquipmentInputs(project, snapshot, document.settings, error)) return false;
        // Preserve the default arms closure as well as the level-local
        // assignment. It may differ after an explicit local override.
        if (!document.settings.firstPersonArmsAssetId.empty())
        {
            wi::scene::Scene defaults;
            CreatePlayerStartCommand create(defaults, {});
            if (!create.Execute()) { error = "Could not prepare prefab defaults."; return false; }
            SetPlayerControllerSettingsCommand settings(defaults, create.CreatedEntity(), document.settings);
            (void)settings.Execute();
            if (!SnapshotGovernedPlayerViewInputs(project, snapshot, defaults, error)) return false;
        }
        return true;
    }

    bool SnapshotGovernedPlayerViewInputs(
        const renegade::bridge::ProjectMetadata& project,
        const renegade::bridge::TestLevelSnapshot& snapshot,
        const wi::scene::Scene& scene,
        std::string& error)
    {
        using namespace renegade::bridge;

        const PlayerStartResult playerStart = ResolvePlayerStart(scene);
        if (playerStart.resolution != PlayerStartResolution::Success ||
            playerStart.start.settings.firstPersonArmsAssetId.empty())
        {
            error.clear();
            return true;
        }

        const StableId armsAssetId =
            playerStart.start.settings.firstPersonArmsAssetId;

        AssetRegistry registry;
        if (!ReadAssetRegistry(
                project.rootPath, project.projectId, registry, error))
        {
            error =
                "Could not read the governed asset registry for first-person arms Test Level snapshot: " +
                error;
            return false;
        }

        const auto armsRecord = std::find_if(
            registry.records.begin(),
            registry.records.end(),
            [&](const AssetRecord& candidate)
            {
                return candidate.assetId == armsAssetId;
            });
        const auto armsProvenance = std::find_if(
            registry.importedProducts.begin(),
            registry.importedProducts.end(),
            [&](const ImportedProductRecord& candidate)
            {
                return candidate.productAssetId == armsAssetId;
            });
        if (armsRecord == registry.records.end() ||
            armsProvenance == registry.importedProducts.end() ||
            armsRecord->dependencyClass != DependencyClass::ImportedContent ||
            !armsRecord->sourceAvailable ||
            fs::u8path(armsRecord->projectRelativePath).extension() !=
                ReusableAssetExtension)
        {
            error =
                "Player first-person arms assignment does not resolve to an available governed .rasset product: " +
                armsAssetId;
            return false;
        }

        std::string registrySourceText;
        if (!ResolveAssetRegistryDocumentPath(
                project.rootPath, registrySourceText, error))
        {
            error =
                "Could not resolve the governed asset registry for first-person arms Test Level snapshot: " +
                error;
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
            error =
                "Could not snapshot the governed asset registry for first-person arms: " +
                ec.message();
            return false;
        }

        const auto copyRecord =
            [&](const AssetRecord& record,
                const char* label) -> bool
            {
                const fs::path relative =
                    fs::u8path(record.projectRelativePath).lexically_normal();
                if (!IsSafeSnapshotContentPath(relative))
                {
                    error = std::string(label) +
                        " escaped governed Content: " +
                        record.projectRelativePath;
                    return false;
                }

                ec.clear();
                const fs::path source = sourceRoot / relative;
                const fs::file_status status =
                    fs::symlink_status(source, ec);
                if (ec || fs::is_symlink(status) ||
                    !fs::is_regular_file(status))
                {
                    error = std::string(label) +
                        " is unavailable: " + source.generic_u8string();
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
                    error = "Could not snapshot " + std::string(label) +
                        ": " + source.generic_u8string() +
                        ": " + ec.message();
                    return false;
                }
                return true;
            };

        if (!copyRecord(*armsRecord, "first-person arms product"))
            return false;

        ReusableModelPlacementRequest request;
        request.projectRoot = project.rootPath;
        request.projectId = project.projectId;
        request.assetId = armsAssetId;
        ReusableAssetService reusableAssets;
        auto prepared = reusableAssets.PrepareModelAssetPlacement(request);
        if (!prepared.IsReady() || prepared.PeekScene() == nullptr)
        {
            error =
                "Could not inspect first-person arms dependencies for Test Level: " +
                (prepared.Result().error.empty()
                    ? std::string("reusable product was not ready")
                    : prepared.Result().error);
            return false;
        }

        std::vector<MaterialTextureBindingRecord> bindings;
        if (!InspectMaterialTextureBindings(
                *prepared.PeekScene(), bindings, error))
        {
            error =
                "Could not inspect first-person arms texture bindings for Test Level: " +
                error;
            return false;
        }

        std::unordered_set<StableId> copiedTextures;
        for (const auto& binding : bindings)
        {
            if (!copiedTextures.insert(binding.textureAssetId).second)
                continue;

            const auto textureRecord = std::find_if(
                registry.records.begin(),
                registry.records.end(),
                [&](const AssetRecord& candidate)
                {
                    return candidate.assetId == binding.textureAssetId;
                });
            if (textureRecord == registry.records.end() ||
                textureRecord->dependencyClass != DependencyClass::Texture ||
                !textureRecord->sourceAvailable)
            {
                error =
                    "First-person arms references an unavailable governed texture: " +
                    binding.textureAssetId;
                return false;
            }

            if (!copyRecord(
                    *textureRecord,
                    "first-person arms texture product"))
            {
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
        const ProjectMetadata& project, TestLevelSnapshot& snapshot,
        std::string& error, TestLevelSnapshotFailureInjection failure)
    {
        return CreateProjectSnapshot(project, snapshot, error, failure, false);
    }

    bool TestLevelSnapshotService::CreateDeferred(
        const ProjectMetadata& project, TestLevelSnapshot& snapshot,
        std::string& error)
    {
        return CreateProjectSnapshot(project, snapshot, error,
            TestLevelSnapshotFailureInjection::None, true);
    }

    bool TestLevelSnapshotService::CreateProjectSnapshot(
        const ProjectMetadata& project,
        TestLevelSnapshot& snapshot,
        std::string& error,
        const TestLevelSnapshotFailureInjection failureInjection,
        const bool deferValidation)
    {
        snapshot = {};
        error.clear();

        TestLevelLaunchTiming timings(project.rootPath, "project_snapshot");
        TestLevelSnapshot created;
        const auto sceneFailure =
            failureInjection ==
                TestLevelSnapshotFailureInjection::AfterArchiveWrite
            ? TestLevelSnapshotFailureInjection::AfterArchiveWrite
            : TestLevelSnapshotFailureInjection::None;
        if (!CreateSceneSnapshot(project.rootPath, created, error, sceneFailure, deferValidation))
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

            // A new unsaved Level has no authored scene-adjacent inventory.
            // Preserve Test Level for such inventory-free scenes.
            if (!scenes_.CurrentPath().empty() && !SnapshotInventoryCompanions(
                    project.rootPath, scenes_.CurrentPath(), created.scenePath, error))
                return failAndCleanup(
                    "Could not snapshot optional inventory: " + error);

            if (!scenes_.CurrentPath().empty() &&
                !SnapshotInventoryProjectInputs(project, created, scenes_.CurrentPath(), error))
                return failAndCleanup("Could not snapshot inventory gameplay inputs: " + error);

            if (!SnapshotParticleEffectRuntimeInputs(project.rootPath, project.projectId, created.sessionDirectory, error))
                return failAndCleanup("Could not snapshot particle runtime catalog: " + error);

            if (!SnapshotGovernedMaterialInputs(
                    project,
                    created,
                    scenes_.GetScene(),
                    error))
            {
                return failAndCleanup(
                    "Could not snapshot governed material state: " + error);
            }

            if (!SnapshotImpactAudio(project.rootPath, created.sessionDirectory, project.projectId, error))
                return failAndCleanup("Could not snapshot impact audio: " + error);
            if (!SnapshotGameplayInput(project, created, error))
                return failAndCleanup("Could not snapshot gameplay input: " + error);

            const auto equipmentStart = ResolvePlayerStart(scenes_.GetScene());
            if (equipmentStart.resolution == PlayerStartResolution::Success &&
                !SnapshotEquipmentInputs(project, created, equipmentStart.start.settings, error))
                return failAndCleanup("Could not snapshot starting equipment: " + error);

            if (!SnapshotPlayerPrefab(project, created, scenes_.GetScene(), error))
                return failAndCleanup("Could not snapshot player prefab: " + error);

            if (!SnapshotGovernedPlayerViewInputs(
                    project,
                    created,
                    scenes_.GetScene(),
                    error))
            {
                return failAndCleanup(
                    "Could not snapshot first-person arms state: " + error);
            }

            std::string repairedHudHint;
            if (!SnapshotProjectDefaultHud(project, created, repairedHudHint, error))
                return failAndCleanup("Could not snapshot project default HUD: " + error);
            ProjectMetadata snapshotProject = project;
            snapshotProject.defaultHud = std::move(repairedHudHint);
            const fs::path descriptorPath =
                fs::u8path(created.sessionDirectory) / "TestLevel.renegade";
            if (!WriteSnapshotDescriptor(snapshotProject, descriptorPath, error))
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
                inspected.defaultHudId != snapshotProject.defaultHudId ||
                inspected.defaultHud != snapshotProject.defaultHud ||
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
        const std::string& projectRoot, TestLevelSnapshot& snapshot,
        std::string& error, TestLevelSnapshotFailureInjection failure)
    {
        return CreateSceneSnapshot(projectRoot, snapshot, error, failure, false);
    }

    bool TestLevelSnapshotService::CreateSceneSnapshot(
        const std::string& projectRoot,
        TestLevelSnapshot& snapshot,
        std::string& error,
        const TestLevelSnapshotFailureInjection failureInjection,
        const bool deferValidation)
    {
        snapshot = {};
        error.clear();

        if (projectRoot.empty())
        {
            error = "A project root is required for a Test Level snapshot.";
            return false;
        }

        TestLevelLaunchTiming timings(projectRoot, "scene_snapshot");
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
                // Disposable previews favour launch latency over compressed disk size.
                // Embedded asset bytes and ordinary project saves are unchanged.
                // Native resource serialization must retain embedded source bytes: it
                // updates streaming containers to this disposable archive. Preserve
                // the compressed-write retention policy, then emit uncompressed bytes.
                archive.SetCompressionEnabled(true);
                // Imported model maps can be native retained resources rather
                // than governed texture bindings. The shadow scene must carry
                // their bytes; long relative paths back to the authoring project
                // are not a portable Runtime dependency contract.
                struct RestoreResourceMode
                {
                    wi::resourcemanager::Mode previous = wi::resourcemanager::GetMode();
                    ~RestoreResourceMode() { wi::resourcemanager::SetMode(previous); }
                } resourceMode;
                wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
                RemoveGeneratedWaterfallVisuals(scenes_.GetScene());
                scenes_.GetScene().Serialize(archive);
                archive.SetCompressionEnabled(false);
                timings.Mark("serialize");
                if (deferValidation)
                {
                    wi::vector<std::uint8_t> bytes;
                    archive.WriteData(bytes);
                    archive = wi::Archive();
                    std::ofstream placeholder(scenePath, std::ios::binary | std::ios::trunc);
                    placeholder.close();
                    archiveWritten = bool(placeholder);
                    if (archiveWritten)
                    {
                        auto& captures = PendingCaptures();
                        std::lock_guard<std::mutex> lock(captures.mutex);
                        captures.bytes.emplace(scenePath.generic_u8string(), std::move(bytes));
                    }
                    timings.Mark("capture_owned_bytes");
                }
                else
                {
                    archiveWritten = archive.SaveFile(scenePath.generic_u8string());
                    timings.Mark("archive_write");
                    archive = wi::Archive();
                }
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

            if (deferValidation)
            {
                std::ofstream pending(scenePath.generic_u8string() + ".pending-validation",
                    std::ios::binary | std::ios::trunc);
                pending << "renegade-pending-scene-validation-v1\n";
                pending.close();
                if (!pending) return failAndCleanup("Could not mark pending scene validation.");
            }
            else
            {
                auto validation = PrepareWickedSceneOpen(scenePath.generic_u8string());
                timings.Mark("deserialize_validate");
                if (!validation.IsReady())
                    return failAndCleanup("The Test Level snapshot failed WISCENE validation: " +
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

    bool TestLevelSnapshotService::NeedsDeferredScenePreparation(
        const TestLevelSnapshot& snapshot)
    {
        auto& captures = PendingCaptures();
        std::lock_guard<std::mutex> lock(captures.mutex);
        std::error_code ec;
        return captures.bytes.find(snapshot.scenePath) != captures.bytes.end() ||
            fs::is_regular_file(fs::u8path(snapshot.scenePath + ".pending-validation"), ec);
    }

    bool TestLevelSnapshotService::FinalizeDeferredScene(
        TestLevelSnapshot& snapshot, std::string& error, const bool preferLocalStorage,
        const std::function<bool(bool)>& progress)
    {
        error.clear();
        TestLevelLaunchTiming timings(snapshot.projectRoot, "snapshot_finalize");
        try
        {
            const auto originalRoot=fs::absolute(fs::u8path(snapshot.projectRoot)).lexically_normal();
            const auto originalSession=fs::absolute(fs::u8path(snapshot.sessionDirectory)).lexically_normal();
            if (originalSession.parent_path() != originalRoot/"Intermediate"/"TestLevelSnapshots" ||
                fs::absolute(fs::u8path(snapshot.scenePath)).lexically_normal() !=
                    originalSession/"Content"/"Scenes"/"TestLevel.wiscene")
            {
                error="Refusing deferred preparation outside the owned snapshot directory.";
                return false;
            }
            wi::vector<std::uint8_t> bytes;
            {
                auto& captures=PendingCaptures();
                std::lock_guard<std::mutex> lock(captures.mutex);
                auto i=captures.bytes.find(snapshot.scenePath);
                if(i!=captures.bytes.end()){bytes=std::move(i->second);captures.bytes.erase(i);}
            }
            if (!bytes.empty() && preferLocalStorage)
            {
#ifdef _WIN32
                const auto* local=_wgetenv(L"LOCALAPPDATA");
                if(local && *local)
                {
                    fs::path localRoot=fs::path(local)/"Renegade"/"TestLevelStorage"/
                        std::to_string(std::hash<std::string>{}(originalRoot.generic_u8string()));
                    std::error_code ec;
                    const auto space=fs::space(fs::path(local),ec);
                    std::uintmax_t companionBytes=0;
                    for(const auto& f:fs::recursive_directory_iterator(originalSession))
                        if(f.is_regular_file())companionBytes+=f.file_size();
                    constexpr std::uintmax_t reserve=4ull*1024*1024*1024;
                    if(!ec && localRoot.root_name()!=originalRoot.root_name() &&
                        space.available>reserve+bytes.size()+companionBytes)
                    {
                        const auto owner=localRoot/".renegade-test-storage-owner";
                        const std::string ownerText="renegade-local-test-storage-v1\n"+
                            originalRoot.generic_u8string();
                        bool owned=false;
                        if(fs::is_regular_file(owner))
                        {
                            std::ifstream f(owner,std::ios::binary);
                            std::string actual((std::istreambuf_iterator<char>(f)),{});
                            owned=actual==ownerText;
                        }
                        else if(!fs::exists(localRoot))
                        {
                            fs::create_directories(localRoot,ec);
                            if(!ec){std::ofstream f(owner,std::ios::binary);f<<ownerText;f.close();owned=bool(f);}
                        }
                        if(owned)
                        {
                            const auto destination=localRoot/"Intermediate"/"TestLevelSnapshots"/
                                originalSession.filename();
                            fs::create_directories(destination.parent_path(),ec);
                            if(!ec && fs::create_directory(destination,ec))
                            {
                                fs::copy(originalSession,destination,fs::copy_options::recursive,ec);
                                if(!ec)
                                {
                                    snapshot.projectRoot=localRoot.generic_u8string();
                                    snapshot.sessionDirectory=destination.generic_u8string();
                                    snapshot.scenePath=(destination/"Content"/"Scenes"/"TestLevel.wiscene").generic_u8string();
                                    if(!snapshot.descriptorPath.empty())
                                        snapshot.descriptorPath=(destination/"TestLevel.renegade").generic_u8string();
                                    std::string cleanupWarning;
                                    RemoveSnapshotDirectory(originalSession,cleanupWarning);
                                }
                                else {std::error_code ignored;fs::remove_all(destination,ignored);}
                            }
                        }
                    }
                }
#endif
            }
            if(!bytes.empty())
            {
                if(progress && !progress(false)){error="Test Level preparation was cancelled.";return false;}
                std::ofstream file(fs::u8path(snapshot.scenePath),std::ios::binary|std::ios::trunc);
                constexpr std::size_t chunk=8*1024*1024;
                for(std::size_t offset=0;file && offset<bytes.size();offset+=chunk)
                {
                    if(progress && !progress(false)){error="Test Level preparation was cancelled.";return false;}
                    const auto size=std::min(chunk,bytes.size()-offset);
                    file.write(reinterpret_cast<const char*>(bytes.data()+offset),size);
                }
                file.close();
                if(!file){error="Could not write the owned Test Level snapshot.";return false;}
                bytes.clear();
            }
            timings.Mark("archive_write");
            if(progress && !progress(true)){error="Test Level preparation was cancelled.";return false;}
            {
                auto validation=PrepareWickedSceneOpen(snapshot.scenePath);
                if(!validation.IsReady()){error="Test Level snapshot validation failed: "+validation.Error();return false;}
            }
            timings.Mark("deserialize_validate");
            std::error_code ec;
            fs::remove(fs::u8path(snapshot.scenePath+".pending-validation"),ec);
            if(ec){error="Could not finish snapshot validation: "+ec.message();return false;}
            return true;
        }
        catch(const std::exception& e){error=std::string("Deferred snapshot preparation failed: ")+e.what();return false;}
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
