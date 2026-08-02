#include "renegade/bridge/ImportService.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <utility>

#include <ModelImporter.h>

namespace
{
    namespace fs = std::filesystem;

    // Meters. The default Automatic target extent: a human/prop-scale
    // reference, the same order of magnitude GameGuru MAX's Automatic mode
    // targets (60 inches / ~1.5 m against their inches-native engine), sized
    // up slightly for Wicked's metres-native convention.
    constexpr float AutomaticScaleTargetExtent = 2.0f;

    // Union of every mesh's own local vertex-position bounds -- not a
    // world-space, hierarchy-aware bounding box like GameGuru MAX's
    // Automatic mode computes (DBOAssImp.cpp walks the full node/bone tree
    // via each mesh's world/offset matrix). This is an honest approximation:
    // exactly right for the common single-node or flat-hierarchy import,
    // and not accounted for if a model's nodes carry large relative offsets
    // or their own per-node scale. Only valid against the still-isolated
    // scene PrepareGltfAsset produces.
    float ComputeAutomaticScaleFactor(
        const wi::scene::Scene& scene) noexcept
    {
        bool any = false;
        XMFLOAT3 boundsMin(0.0f, 0.0f, 0.0f);
        XMFLOAT3 boundsMax(0.0f, 0.0f, 0.0f);
        for (std::size_t meshIndex = 0;
            meshIndex < scene.meshes.GetCount();
            ++meshIndex)
        {
            const auto& mesh = scene.meshes[meshIndex];
            for (const auto& position : mesh.vertex_positions)
            {
                if (!any)
                {
                    boundsMin = position;
                    boundsMax = position;
                    any = true;
                    continue;
                }
                boundsMin.x = std::min(boundsMin.x, position.x);
                boundsMin.y = std::min(boundsMin.y, position.y);
                boundsMin.z = std::min(boundsMin.z, position.z);
                boundsMax.x = std::max(boundsMax.x, position.x);
                boundsMax.y = std::max(boundsMax.y, position.y);
                boundsMax.z = std::max(boundsMax.z, position.z);
            }
        }

        if (!any)
        {
            return 1.0f;
        }

        const float extentX = boundsMax.x - boundsMin.x;
        const float extentY = boundsMax.y - boundsMin.y;
        const float extentZ = boundsMax.z - boundsMin.z;
        const float largestExtent =
            std::max(extentX, std::max(extentY, extentZ));
        if (largestExtent <= 0.0001f)
        {
            // Degenerate (e.g. a single point or a flat plane on every
            // axis Automatic could sensibly normalize) -- leave scale
            // untouched rather than divide by a near-zero extent.
            return 1.0f;
        }

        return AutomaticScaleTargetExtent / largestExtent;
    }

    // Mirrors the file-local helper of the same name in CommandService.cpp.
    // Not shared through a header because it is a small, self-contained
    // check and both call sites already depend on the full scene API.
    bool EntityExists(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }

        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    constexpr std::uint64_t FingerprintSeed = 1469598103934665603ull;
    constexpr std::uint64_t FingerprintPrime = 1099511628211ull;

    void HashBytes(std::uint64_t& hash, const void* bytes, std::size_t count)
    {
        const auto* data = static_cast<const unsigned char*>(bytes);
        for (std::size_t index = 0; index < count; ++index)
        {
            hash ^= data[index];
            hash *= FingerprintPrime;
        }
    }

    template<typename Value>
    void HashValue(std::uint64_t& hash, const Value& value)
    {
        HashBytes(hash, &value, sizeof(value));
    }

    void HashString(std::uint64_t& hash, const std::string& value)
    {
        HashBytes(hash, value.data(), value.size());
        const unsigned char terminator = 0;
        HashBytes(hash, &terminator, 1);
    }

    template<typename Component>
    std::size_t StableIndex(
        const wi::ecs::ComponentManager<Component>& manager,
        const wi::ecs::Entity entity) noexcept
    {
        return entity == wi::ecs::INVALID_ENTITY
            ? wi::ecs::INVALID_INDEX
            : manager.GetIndex(entity);
    }

    fs::path StageLogPath(const std::string& assetPath)
    {
        return fs::u8path(assetPath + ".import.log");
    }

    void ResetStageLog(const std::string& assetPath) noexcept
    {
        std::ofstream log(
            StageLogPath(assetPath),
            std::ios::out | std::ios::trunc);
        if (log)
        {
            log << "MODEL IMPORT V1 GATE 1\n";
        }
    }

    void RecordStage(
        const std::string& assetPath,
        const char* stage) noexcept
    {
        std::ofstream log(
            StageLogPath(assetPath),
            std::ios::out | std::ios::app);
        if (log)
        {
            log << stage << '\n';
        }
    }

    bool WriteScene(
        wi::scene::Scene& scene,
        const std::string& path,
        std::string& error)
    {
        RecordStage(path, "archive_create_begin");
        wi::Archive archive(path, false, false);
        RecordStage(path, "archive_create_complete");
        if (!archive.IsOpen())
        {
            error = "Could not create the imported WISCENE asset: " + path;
            return false;
        }
        RecordStage(path, "scene_serialize_begin");
        scene.Serialize(archive);
        RecordStage(path, "scene_serialize_complete");

        RecordStage(path, "archive_save_begin");
        const bool archiveWritten = archive.SaveFile(path);
        RecordStage(path, "archive_save_complete");

        // Disarm the filename-backed archive before its destructor runs.
        // wi::Archive::Close() (invoked by ~Archive()) is not idempotent: in
        // write mode it unconditionally calls SaveFile() again, and a second
        // call after the explicit SaveFile() above writes `pos` bytes from an
        // already-cleared, null data pointer. That is an access violation,
        // not a C++ exception, so it crashes past any try/catch below Wicked's
        // Archive. Replacing the object with a fresh, filename-less Archive
        // means its eventual destructor finds an empty fileName and Close()
        // becomes a no-op. This mirrors the existing pattern in
        // SceneDocumentService.cpp.
        archive = wi::Archive();
        RecordStage(path, "archive_disarmed");

        if (!archiveWritten)
        {
            error = "Could not write the imported WISCENE asset: " + path;
            return false;
        }

        RecordStage(path, "written_archive_validation_begin");
        wi::Archive validation(path, true, false);
        RecordStage(path, "written_archive_validation_complete");
        if (!validation.IsOpen())
        {
            error = "Could not reopen the imported WISCENE asset: " + path;
            return false;
        }
        return true;
    }

    bool ReadScene(
        const std::string& path,
        wi::scene::Scene& scene,
        std::string& error)
    {
        RecordStage(path, "reload_archive_open_begin");
        wi::Archive archive(path, true, false);
        RecordStage(path, "reload_archive_open_complete");
        if (!archive.IsOpen())
        {
            error = "Could not read the imported WISCENE asset: " + path;
            return false;
        }
        RecordStage(path, "reload_deserialize_begin");
        scene.Serialize(archive);
        RecordStage(path, "reload_deserialize_complete");
        if (archive.GetPos() != archive.GetSize())
        {
            error = "Imported WISCENE validation found trailing or incomplete data: " + path;
            return false;
        }
        return true;
    }

    // Explains a round-trip mismatch field by field, instead of forcing a
    // guess from a bare true/false. If every count matches, only the content
    // fingerprint differs, which points at value-level drift (transform
    // floats, texture path strings, or component ordering) rather than a
    // missing or extra entity.
    std::string DescribeSummaryDifference(
        const renegade::bridge::ImportedSceneSummary& imported,
        const renegade::bridge::ImportedSceneSummary& reloaded)
    {
        std::ostringstream out;
        bool any = false;
        auto reportField =
            [&](const char* label, std::size_t before, std::size_t after)
        {
            if (before != after)
            {
                if (any)
                {
                    out << "; ";
                }
                out << label << ": " << before << " -> " << after;
                any = true;
            }
        };
        reportField("names", imported.names, reloaded.names);
        reportField("transforms", imported.transforms, reloaded.transforms);
        reportField("hierarchy", imported.hierarchy, reloaded.hierarchy);
        reportField("objects", imported.objects, reloaded.objects);
        reportField("meshes", imported.meshes, reloaded.meshes);
        reportField("materials", imported.materials, reloaded.materials);
        reportField("armatures", imported.armatures, reloaded.armatures);
        reportField("animations", imported.animations, reloaded.animations);
        reportField(
            "textureReferences",
            imported.textureReferences,
            reloaded.textureReferences);

        if (!any)
        {
            out << "All structural counts matched (" << imported.objects
                << " objects, " << imported.meshes << " meshes, "
                << imported.materials << " materials); only the content "
                << "fingerprint differed (0x" << std::hex
                << imported.structuralFingerprint << " -> 0x"
                << reloaded.structuralFingerprint << std::dec
                << "). Likely a value-level change: transform floats, "
                << "texture path strings, or hierarchy/subset ordering.";
        }
        return out.str();
    }
}

namespace renegade::bridge
{
    ImportedSceneSummary ImportService::Summarize(
        const wi::scene::Scene& scene) noexcept
    {
        ImportedSceneSummary result;
        result.names = scene.names.GetCount();
        result.transforms = scene.transforms.GetCount();
        result.hierarchy = scene.hierarchy.GetCount();
        result.objects = scene.objects.GetCount();
        result.meshes = scene.meshes.GetCount();
        result.materials = scene.materials.GetCount();
        result.armatures = scene.armatures.GetCount();
        result.animations = scene.animations.GetCount();

        std::uint64_t fingerprint = FingerprintSeed;
        for (std::size_t index = 0; index < scene.names.GetCount(); ++index)
        {
            HashString(fingerprint, scene.names[index].name);
        }
        for (std::size_t index = 0; index < scene.transforms.GetCount(); ++index)
        {
            const auto& transform = scene.transforms[index];
            HashValue(fingerprint, transform.scale_local);
            HashValue(fingerprint, transform.rotation_local);
            HashValue(fingerprint, transform.translation_local);
        }
        for (std::size_t index = 0; index < scene.hierarchy.GetCount(); ++index)
        {
            const auto entityIndex = StableIndex(
                scene.transforms,
                scene.hierarchy.GetEntity(index));
            const auto parentIndex = StableIndex(
                scene.transforms,
                scene.hierarchy[index].parentID);
            HashValue(fingerprint, entityIndex);
            HashValue(fingerprint, parentIndex);
        }
        for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
        {
            const auto transformIndex = StableIndex(
                scene.transforms,
                scene.objects.GetEntity(index));
            const auto meshIndex = StableIndex(
                scene.meshes,
                scene.objects[index].meshID);
            HashValue(fingerprint, transformIndex);
            HashValue(fingerprint, meshIndex);
        }
        for (std::size_t index = 0; index < scene.meshes.GetCount(); ++index)
        {
            const auto& mesh = scene.meshes[index];
            HashValue(fingerprint, mesh.vertex_positions.size());
            HashValue(fingerprint, mesh.indices.size());
            HashValue(fingerprint, mesh.subsets.size());
            for (const auto& subset : mesh.subsets)
            {
                const auto materialIndex = StableIndex(
                    scene.materials,
                    subset.materialID);
                HashValue(fingerprint, materialIndex);
                HashValue(fingerprint, subset.indexOffset);
                HashValue(fingerprint, subset.indexCount);
            }
        }

        for (std::size_t index = 0; index < scene.materials.GetCount(); ++index)
        {
            const auto& material = scene.materials[index];
            HashValue(fingerprint, material.baseColor);
            HashValue(fingerprint, material.emissiveColor);
            HashValue(fingerprint, material.roughness);
            HashValue(fingerprint, material.metalness);
            for (const auto& texture : material.textures)
            {
                // Wicked deliberately serializes texture.name relative to the
                // WISCENE's own directory and does not restore it to an
                // absolute path on reload (unlike embedded resource data,
                // which does get GetSourceDirectory()-prefixed in
                // wiResourceManager.cpp). A freshly imported scene therefore
                // holds an absolute source path, while the same scene
                // reloaded from disk holds a path relative to
                // Saved/Validation/ModelImport -- two different strings for
                // the same referenced file. Hashing the full path made this
                // proof fail on every textured model regardless of whether
                // anything was actually lost. Hash just the filename, which
                // is invariant to that legitimate relative/absolute rewrite
                // but still catches a texture slot pointing at a different
                // file.
                HashString(
                    fingerprint,
                    wi::helper::GetFileNameFromPath(texture.name));
                HashValue(fingerprint, texture.uvset);
                if (!texture.name.empty())
                {
                    ++result.textureReferences;
                }
            }
        }
        result.structuralFingerprint = fingerprint;
        return result;
    }

    PreparedModelImport ImportService::PrepareGltfAsset(
        const std::string& sourcePath,
        const std::string& assetPath) const
    {
        PreparedModelImport prepared;
        auto& result = prepared.result_;
        result.sourcePath = sourcePath;
        result.assetPath = assetPath;

        if (sourcePath.empty() || assetPath.empty())
        {
            result.error = "A source GLB/GLTF path and destination WISCENE path are required.";
            return prepared;
        }

        const auto sourceExtension = wi::helper::toUpper(
            wi::helper::GetExtensionFromFileName(sourcePath));
        if (sourceExtension != "GLB" && sourceExtension != "GLTF")
        {
            result.error = "Model Import V1 only accepts .glb and .gltf files: " + sourcePath;
            return prepared;
        }
        if (wi::helper::toUpper(
                wi::helper::GetExtensionFromFileName(assetPath)) != "WISCENE")
        {
            result.error = "The reusable imported asset must be a .wiscene file: " + assetPath;
            return prepared;
        }
        if (!wi::helper::FileExists(sourcePath))
        {
            result.error = "Source model does not exist: " + sourcePath;
            return prepared;
        }
        if (wi::graphics::GetDevice() == nullptr)
        {
            result.error = "Wicked GLTF conversion requires an initialized graphics device.";
            return prepared;
        }

        std::error_code directoryError;
        const fs::path destination = fs::u8path(assetPath).lexically_normal();
        if (!destination.parent_path().empty())
        {
            fs::create_directories(destination.parent_path(), directoryError);
        }
        if (directoryError)
        {
            result.error = "Could not create the imported asset folder: " +
                directoryError.message();
            return prepared;
        }

        ResetStageLog(assetPath);
        RecordStage(assetPath, "validated");

        // A Wicked Scene is a very large aggregate. Wicked's own editor keeps
        // temporary import scenes on the heap; placing both the imported and
        // reloaded scenes in this worker's stack can exhaust the Windows worker
        // stack before ImportModel_GLTF() is entered.
        prepared.scene_ = wi::allocator::make_shared_single<wi::scene::Scene>();
        RecordStage(assetPath, "temporary_scene_allocated");
        RecordStage(assetPath, "converter_begin");
        ImportModel_GLTF(sourcePath, *prepared.scene_);
        RecordStage(assetPath, "converter_complete");
        result.imported = Summarize(*prepared.scene_);
        RecordStage(assetPath, "import_summary_complete");
        if (result.imported.meshes == 0 || result.imported.objects == 0)
        {
            RecordStage(assetPath, "fail_no_mesh_or_object");
            result.error = "Wicked did not produce a mesh and object from the source model.";
            prepared.scene_.reset();
            return prepared;
        }

        RecordStage(assetPath, "prepared_for_thread_safe_completion");
        return prepared;
    }

    ImportResult ImportService::CompleteGltfAsset(
        PreparedModelImport prepared) const
    {
        const bool ready = prepared.IsReady();
        ImportResult result = std::move(prepared.result_);
        if (!ready)
        {
            if (result.error.empty())
            {
                result.error = "The prepared model import is not ready for WISCENE validation.";
            }
            return result;
        }

        RecordStage(result.assetPath, "thread_safe_completion_begin");

        try
        {
            if (!WriteScene(*prepared.scene_, result.assetPath, result.error))
            {
                RecordStage(result.assetPath, "fail_wiscene_write");
                return result;
            }
        }
        catch (const std::exception& exception)
        {
            RecordStage(result.assetPath, "fail_wiscene_write_exception");
            result.error = "WISCENE serialization failed: " +
                std::string(exception.what());
            return result;
        }
        catch (...)
        {
            RecordStage(result.assetPath, "fail_wiscene_write_exception");
            result.error = "WISCENE serialization failed with an unknown error.";
            return result;
        }
        RecordStage(result.assetPath, "wiscene_write_complete");
        prepared.scene_.reset();
        RecordStage(result.assetPath, "imported_scene_released");

        auto reloadedScene =
            wi::allocator::make_shared_single<wi::scene::Scene>();
        RecordStage(result.assetPath, "reload_scene_allocated");
        try
        {
            if (!ReadScene(result.assetPath, *reloadedScene, result.error))
            {
                RecordStage(result.assetPath, "fail_wiscene_reload");
                return result;
            }
        }
        catch (const std::exception& exception)
        {
            RecordStage(result.assetPath, "fail_wiscene_reload_exception");
            result.error = "WISCENE reload failed: " +
                std::string(exception.what());
            return result;
        }
        catch (...)
        {
            RecordStage(result.assetPath, "fail_wiscene_reload_exception");
            result.error = "WISCENE reload failed with an unknown error.";
            return result;
        }
        RecordStage(result.assetPath, "wiscene_reload_complete");
        result.reloaded = Summarize(*reloadedScene);
        RecordStage(result.assetPath, "reload_summary_complete");
        if (!(result.imported == result.reloaded))
        {
            RecordStage(result.assetPath, "fail_round_trip_difference");
            result.error =
                "Imported WISCENE structure changed during save and reload. " +
                DescribeSummaryDifference(result.imported, result.reloaded);
            return result;
        }

        result.succeeded = true;
        RecordStage(result.assetPath, "pass");
        return result;
    }

    float ImportService::ResolveScaleFactor(
        const ModelScaleMode mode,
        const wi::scene::Scene& preparedScene) noexcept
    {
        switch (mode)
        {
            case ModelScaleMode::Centimeters:
                return 0.01f;
            case ModelScaleMode::Inches:
                return 0.0254f;
            case ModelScaleMode::Automatic:
                return ComputeAutomaticScaleFactor(preparedScene);
            case ModelScaleMode::Meters:
            case ModelScaleMode::Original:
            default:
                return 1.0f;
        }
    }

    PlaceImportedModelCommand::PlaceImportedModelCommand(
        wi::scene::Scene& targetScene,
        wi::allocator::shared_ptr<wi::scene::Scene> preparedScene,
        const XMFLOAT3& placementPosition,
        const float scaleFactor)
        : scene_(&targetScene)
        , preparedScene_(std::move(preparedScene))
        , placementPosition_(placementPosition)
        , scaleFactor_(scaleFactor > 0.0f ? scaleFactor : 1.0f)
    {
    }

    bool PlaceImportedModelCommand::Execute()
    {
        if (!hasSnapshot_)
        {
            // First execution: merge the freshly converted scene in.
            if (scene_ == nullptr || !preparedScene_.IsValid())
            {
                return false;
            }

            const std::size_t transformCountBefore =
                scene_->transforms.GetCount();

            // Scene::Merge() moves preparedScene_'s contents into *scene_ and
            // leaves preparedScene_ empty; release our reference immediately
            // afterward so nothing holds a stale handle to it.
            scene_->Merge(*preparedScene_);
            preparedScene_.reset();

            if (scene_->transforms.GetCount() <= transformCountBefore)
            {
                return false;
            }

            // Matches Wicked's own convention (Editor.cpp: "Imported models
            // always have a root transform entity") -- the first newly added
            // transform is the imported model's root.
            entity_ = scene_->transforms.GetEntity(transformCountBefore);
            if (auto* transform = scene_->transforms.GetComponent(entity_))
            {
                transform->translation_local = placementPosition_;
                transform->scale_local = XMFLOAT3(
                    scaleFactor_,
                    scaleFactor_,
                    scaleFactor_);
                transform->SetDirty();
            }

            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, entity_);
            hasSnapshot_ = true;
            return true;
        }

        // Redo: Undo removed the root and its whole imported hierarchy;
        // restore them from the snapshot with their original entity IDs.
        if (scene_ == nullptr || EntityExists(*scene_, entity_))
        {
            return false;
        }

        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        const auto restored = scene_->Entity_Serialize(snapshot_, serializer);
        return restored == entity_;
    }

    void PlaceImportedModelCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, entity_))
        {
            // Recursive by default: removes the whole imported hierarchy,
            // not just the root.
            scene_->Entity_Remove(entity_);
        }
    }

    wi::ecs::Entity PlaceImportedModelCommand::PlacedEntity() const noexcept
    {
        return entity_;
    }
}
