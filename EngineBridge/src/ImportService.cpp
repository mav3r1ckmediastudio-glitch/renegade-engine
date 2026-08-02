#include "renegade/bridge/ImportService.h"

#include <filesystem>

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

    bool WriteScene(
        wi::scene::Scene& scene,
        const std::string& path,
        std::string& error)
    {
        wi::Archive archive(path, false, false);
        if (!archive.IsOpen())
        {
            error = "Could not create the imported WISCENE asset: " + path;
            return false;
        }
        scene.Serialize(archive);
        archive.Close();

        wi::Archive validation(path, true, false);
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
        wi::Archive archive(path, true, false);
        if (!archive.IsOpen())
        {
            error = "Could not read the imported WISCENE asset: " + path;
            return false;
        }
        scene.Serialize(archive);
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
            HashValue(fingerprint, scene.names.GetEntity(index));
            HashString(fingerprint, scene.names[index].name);
        }
        for (std::size_t index = 0; index < scene.transforms.GetCount(); ++index)
        {
            HashValue(fingerprint, scene.transforms.GetEntity(index));
            const auto& transform = scene.transforms[index];
            HashValue(fingerprint, transform.scale_local);
            HashValue(fingerprint, transform.rotation_local);
            HashValue(fingerprint, transform.translation_local);
        }
        for (std::size_t index = 0; index < scene.hierarchy.GetCount(); ++index)
        {
            HashValue(fingerprint, scene.hierarchy.GetEntity(index));
            HashValue(fingerprint, scene.hierarchy[index].parentID);
        }
        for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
        {
            HashValue(fingerprint, scene.objects.GetEntity(index));
            HashValue(fingerprint, scene.objects[index].meshID);
        }
        for (std::size_t index = 0; index < scene.meshes.GetCount(); ++index)
        {
            HashValue(fingerprint, scene.meshes.GetEntity(index));
            const auto& mesh = scene.meshes[index];
            HashValue(fingerprint, mesh.vertex_positions.size());
            HashValue(fingerprint, mesh.indices.size());
            HashValue(fingerprint, mesh.subsets.size());
            for (const auto& subset : mesh.subsets)
            {
                HashValue(fingerprint, subset.materialID);
                HashValue(fingerprint, subset.indexOffset);
                HashValue(fingerprint, subset.indexCount);
            }
        }

        for (std::size_t index = 0; index < scene.materials.GetCount(); ++index)
        {
            const auto& material = scene.materials[index];
            HashValue(fingerprint, scene.materials.GetEntity(index));
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

    ImportResult ImportService::ImportGltfAsset(
        const std::string& sourcePath,
        const std::string& assetPath) const
    {
        ImportResult result;
        result.sourcePath = sourcePath;
        result.assetPath = assetPath;

        if (sourcePath.empty() || assetPath.empty())
        {
            result.error = "A source GLB/GLTF path and destination WISCENE path are required.";
            return result;
        }

        const auto sourceExtension = wi::helper::toUpper(
            wi::helper::GetExtensionFromFileName(sourcePath));
        if (sourceExtension != "GLB" && sourceExtension != "GLTF")
        {
            result.error = "Model Import V1 only accepts .glb and .gltf files: " + sourcePath;
            return result;
        }
        if (wi::helper::toUpper(
                wi::helper::GetExtensionFromFileName(assetPath)) != "WISCENE")
        {
            result.error = "The reusable imported asset must be a .wiscene file: " + assetPath;
            return result;
        }
        if (!wi::helper::FileExists(sourcePath))
        {
            result.error = "Source model does not exist: " + sourcePath;
            return result;
        }
        if (wi::graphics::GetDevice() == nullptr)
        {
            result.error = "Wicked GLTF conversion requires an initialized graphics device.";
            return result;
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
            return result;
        }

        wi::scene::Scene importedScene;
        ImportModel_GLTF(sourcePath, importedScene);
        result.imported = Summarize(importedScene);
        if (result.imported.meshes == 0 || result.imported.objects == 0)
        {
            result.error = "Wicked did not produce a mesh and object from the source model.";
            return result;
        }

        if (!WriteScene(importedScene, assetPath, result.error))
        {
            return result;
        }

        wi::scene::Scene reloadedScene;
        if (!ReadScene(assetPath, reloadedScene, result.error))
        {
            return result;
        }
        result.reloaded = Summarize(reloadedScene);
        if (!(result.imported == result.reloaded))
        {
            result.error = "Imported WISCENE structure changed during save and reload.";
            return result;
        }

        result.succeeded = true;
        return result;
    }
}
