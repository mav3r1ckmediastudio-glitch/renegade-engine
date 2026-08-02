#include "renegade/bridge/ImportService.h"

#include <exception>
#include <filesystem>
#include <fstream>

#include <ModelImporter.h>

namespace
{
    namespace fs = std::filesystem;

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
                HashString(fingerprint, texture.name);
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
            result.error = "Imported WISCENE structure changed during save and reload.";
            return result;
        }

        result.succeeded = true;
        RecordStage(result.assetPath, "pass");
        return result;
    }
}
