#include "renegade/bridge/ResourceAssetRuntimeService.h"

#include "renegade/bridge/ResourceAssetCacheIdentityService.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <utility>

#include "json.hpp"

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        struct PackagedAssetEntry
        {
            StableId assetId;
            std::string packagedPath;
            std::string sourceHash;
        };

        bool IsWithin(const fs::path& root, const fs::path& candidate)
        {
            auto rootPart = root.begin();
            auto candidatePart = candidate.begin();
            for (; rootPart != root.end(); ++rootPart, ++candidatePart)
            {
                if (candidatePart == candidate.end() || *rootPart != *candidatePart)
                    return false;
            }
            return true;
        }

        bool IsSafeRelativePackagePath(const std::string& value)
        {
            if (value.empty() || value.find('\\') != std::string::npos)
                return false;
            const fs::path path = fs::u8path(value);
            if (path.empty() || path.is_absolute() || path.has_root_name() ||
                path.generic_u8string() != value ||
                path.lexically_normal().generic_u8string() != value)
            {
                return false;
            }
            return std::none_of(path.begin(), path.end(),
                [](const fs::path& part)
                { return part.empty() || part == "." || part == ".."; });
        }

        bool IsRegularNonSymlink(const fs::path& path)
        {
            std::error_code ec;
            const fs::file_status link = fs::symlink_status(path, ec);
            if (ec || fs::is_symlink(link))
                return false;
            return fs::is_regular_file(path, ec) && !ec;
        }

        bool ReadPackageAssetMap(
            const fs::path& packageRoot,
            const StableId& projectId,
            std::map<StableId, PackagedAssetEntry>& entries,
            std::string& error)
        {
            entries.clear();
            const fs::path manifestPath =
                packageRoot / "GameData" / "content-manifest.json";
            if (!IsRegularNonSymlink(manifestPath))
            {
                error =
                    "Packaged resource content manifest is missing or symlinked.";
                return false;
            }

            std::ifstream stream(manifestPath, std::ios::binary);
            if (!stream)
            {
                error = "Could not read packaged resource content manifest.";
                return false;
            }
            const std::string text{
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>()};
            if (!stream && !stream.eof())
            {
                error =
                    "Could not read complete packaged resource content manifest.";
                return false;
            }

            const nlohmann::json manifest = nlohmann::json::parse(
                text, nullptr, false);
            if (manifest.is_discarded() || !manifest.is_object() ||
                manifest.value("format", std::string{}) !=
                    "renegade-content-manifest" ||
                manifest.value("schema_version", 0) != 1 ||
                manifest.value("project_id", std::string{}) != projectId ||
                !manifest.contains("files") || !manifest["files"].is_array())
            {
                error =
                    "Packaged resource content manifest has invalid identity or schema.";
                return false;
            }

            for (const auto& item : manifest["files"])
            {
                if (!item.is_object())
                {
                    error =
                        "Packaged resource content manifest contains a non-object file record.";
                    return false;
                }
                PackagedAssetEntry entry;
                entry.assetId = item.value("asset_id", std::string{});
                entry.packagedPath = item.value("path", std::string{});
                entry.sourceHash = item.value("source_hash", std::string{});
                if (!IsValidStableId(entry.assetId) ||
                    !IsSafeRelativePackagePath(entry.packagedPath) ||
                    entry.packagedPath.rfind("GameData/", 0) != 0 ||
                    entry.sourceHash.empty())
                {
                    error =
                        "Packaged resource content manifest contains an invalid asset/path/hash record.";
                    return false;
                }
                if (!entries.emplace(entry.assetId, std::move(entry)).second)
                {
                    error =
                        "Packaged resource content manifest contains duplicate stable asset IDs.";
                    return false;
                }
            }
            error.clear();
            return true;
        }
    }

    bool PreparePackagedResourceAsset(
        const std::string& packageRootText,
        const StableId& projectId,
        const StableId& assetId,
        PackagedResourceAsset& prepared,
        std::string& error)
    {
        prepared = {};
        if (packageRootText.empty() || !IsValidStableId(projectId) ||
            !IsValidStableId(assetId))
        {
            error =
                "Packaged resource resolution requires a package root and valid project/asset IDs.";
            return false;
        }

        std::error_code ec;
        const fs::path packageRoot = fs::weakly_canonical(
            fs::u8path(packageRootText), ec);
        if (ec || !fs::is_directory(packageRoot, ec) || ec)
        {
            error = "Packaged resource package root is unavailable.";
            return false;
        }

        std::map<StableId, PackagedAssetEntry> entries;
        if (!ReadPackageAssetMap(packageRoot, projectId, entries, error))
            return false;
        const auto entry = entries.find(assetId);
        if (entry == entries.end())
        {
            error =
                "Governed resource stable ID is absent from the packaged content manifest: " +
                assetId;
            return false;
        }
        if (fs::u8path(entry->second.packagedPath).extension() !=
            ResourceAssetExtension)
        {
            error =
                "Governed resource stable ID does not map to a packaged .rasset product.";
            return false;
        }

        const fs::path assetPath = fs::weakly_canonical(
            packageRoot / fs::u8path(entry->second.packagedPath), ec);
        if (ec || !IsWithin(packageRoot, assetPath) ||
            !IsRegularNonSymlink(assetPath))
        {
            error =
                "Packaged governed resource product is missing, symlinked or outside the package root.";
            return false;
        }

        ResourceAssetDocument document;
        if (!ReadResourceAssetDocument(
                assetPath.generic_u8string(), document, error))
        {
            error = "Packaged governed resource product was rejected: " + error;
            return false;
        }
        if (document.manifest.projectId != projectId ||
            document.manifest.assetId != assetId ||
            document.manifest.payloadFormat != ResourceAssetPayloadFormat ||
            document.manifest.resourceClass == ResourceClass::Unknown ||
            document.payload.empty())
        {
            error =
                "Packaged governed resource manifest contradicts package/project stable identity.";
            return false;
        }

        prepared.projectId = projectId;
        prepared.assetId = assetId;
        prepared.sourceAssetId = document.manifest.sourceAssetId;
        prepared.packagedAssetPath = entry->second.packagedPath;
        prepared.resourceClass = document.manifest.resourceClass;
        prepared.sourceFormat = document.manifest.sourceFormat;
        prepared.payloadHash = document.manifest.payloadHash;
        prepared.payload = std::move(document.payload);
        error.clear();
        return true;
    }

    bool RefreshPackagedMaterialTextureAssets(
        wi::scene::Scene& scene,
        const std::string& packageRoot,
        const StableId& projectId,
        PackagedMaterialTextureRefreshResult& result,
        std::string& error,
        MaterialTextureResourceLoader loader)
    {
        result = {};
        error.clear();

        std::vector<MaterialTextureBindingRecord> bindings;
        if (!InspectMaterialTextureBindings(scene, bindings, error))
        {
            error =
                "Packaged material texture binding metadata was rejected: " + error;
            return false;
        }
        result.discoveredBindingCount = bindings.size();
        if (bindings.empty())
            return true;
        if (!loader)
            loader = LoadPreparedMaterialTextureAsset;

        struct LoadedTexture
        {
            PreparedMaterialTextureAsset prepared;
            std::string packagedPath;
            wi::Resource resource;
        };
        std::map<StableId, LoadedTexture> loadedById;

        // Resolve and decode every distinct required texture before touching a
        // target material. This keeps packaged Runtime replacement atomic at
        // the scene-binding level.
        for (const auto& binding : bindings)
        {
            if (loadedById.find(binding.baseColorTextureAssetId) !=
                loadedById.end())
                continue;

            PackagedResourceAsset packaged;
            if (!PreparePackagedResourceAsset(
                    packageRoot, projectId,
                    binding.baseColorTextureAssetId, packaged, error))
                return false;
            if (packaged.resourceClass != ResourceClass::Texture)
            {
                error =
                    "Material texture stable ID resolves to a packaged non-texture resource: " +
                    binding.baseColorTextureAssetId;
                return false;
            }

            LoadedTexture loaded;
            loaded.packagedPath = packaged.packagedAssetPath;
            loaded.prepared.projectId = projectId;
            loaded.prepared.assetId = packaged.assetId;
            loaded.prepared.productProjectRelativePath =
                packaged.packagedAssetPath;
            loaded.prepared.sourceFormat = packaged.sourceFormat;
            loaded.prepared.payloadHash = packaged.payloadHash;
            loaded.prepared.payload = std::move(packaged.payload);
            if (!BuildResourcePayloadCacheName(
                    "renegade_runtime_",
                    loaded.prepared.assetId,
                    loaded.prepared.sourceFormat,
                    loaded.prepared.payload,
                    loaded.prepared.logicalResourceName,
                    error))
            {
                return false;
            }
            loaded.resource = loader(loaded.prepared, error);
            if (!loaded.resource.IsValid())
            {
                if (error.empty())
                    error =
                        "Wicked did not create a valid resource from the packaged governed texture.";
                return false;
            }
            loadedById.emplace(
                binding.baseColorTextureAssetId, std::move(loaded));
        }

        for (const auto& binding : bindings)
        {
            auto* material = scene.materials.GetComponent(binding.materialEntity);
            const auto loaded = loadedById.find(binding.baseColorTextureAssetId);
            if (material == nullptr || loaded == loadedById.end())
            {
                error =
                    "Prepared packaged texture binding lost its material or loaded resource before commit.";
                return false;
            }

            auto& texture = material->textures[
                wi::scene::MaterialComponent::BASECOLORMAP];
            texture.name.clear();
            texture.resource = loaded->second.resource;
            material->SetDirty();

            PackagedMaterialTextureRefreshRecord record;
            record.assetId = binding.baseColorTextureAssetId;
            record.packagedAssetPath = loaded->second.packagedPath;
            record.payloadHash = loaded->second.prepared.payloadHash;
            record.materialEntity = binding.materialEntity;
            result.records.push_back(std::move(record));
        }

        result.refreshedBindingCount = result.records.size();
        error.clear();
        return true;
    }
}
