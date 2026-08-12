#include "renegade/bridge/MaterialTextureAssetService.h"

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/MaterialService.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    bool IsWithin(const fs::path& child, const fs::path& parent)
    {
        auto childPart = child.begin();
        for (auto parentPart = parent.begin(); parentPart != parent.end();
            ++parentPart, ++childPart)
        {
            if (childPart == child.end() || *childPart != *parentPart)
                return false;
        }
        return true;
    }

    const AssetRecord* FindAssetRecord(
        const AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            registry.records.begin(), registry.records.end(),
            [&assetId](const AssetRecord& record)
            {
                return record.assetId == assetId;
            });
        return found == registry.records.end() ? nullptr : &*found;
    }

    const ImportedProductRecord* FindImportedProduct(
        const AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            registry.importedProducts.begin(), registry.importedProducts.end(),
            [&assetId](const ImportedProductRecord& record)
            {
                return record.productAssetId == assetId;
            });
        return found == registry.importedProducts.end() ? nullptr : &*found;
    }

    std::string ExtensionForFormat(const ResourceSourceFormat format)
    {
        for (const auto& capability : GetSupportedResourceFormats())
        {
            if (capability.format == format)
                return capability.extension;
        }
        return {};
    }

    std::string LogicalResourceName(
        const StableId& assetId,
        std::string hash,
        const ResourceSourceFormat format)
    {
        hash.erase(std::remove(hash.begin(), hash.end(), ':'), hash.end());
        std::replace(hash.begin(), hash.end(), '/', '_');
        std::replace(hash.begin(), hash.end(), '\\', '_');
        return "renegade_" + assetId + "_" + hash +
            ExtensionForFormat(format);
    }

    bool ApplyBinding(
        wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity,
        const PreparedMaterialTextureAsset& prepared,
        MaterialTextureResourceLoader& loader,
        std::string& error)
    {
        if (materialEntity == wi::ecs::INVALID_ENTITY ||
            IsTerrainOwnedMaterial(scene, materialEntity))
        {
            error = "Texture assignment requires one editable non-terrain material.";
            return false;
        }
        auto* material = scene.materials.GetComponent(materialEntity);
        if (material == nullptr)
        {
            error = "The target material no longer exists.";
            return false;
        }
        if (!IsValidStableId(prepared.assetId) || prepared.payload.empty() ||
            prepared.sourceFormat == ResourceSourceFormat::Unknown)
        {
            error = "The prepared texture asset is invalid.";
            return false;
        }

        if (!loader)
            loader = LoadPreparedMaterialTextureAsset;
        wi::Resource resource = loader(prepared, error);
        if (!resource.IsValid())
        {
            if (error.empty())
                error = "Wicked did not create a valid resource from the governed texture payload.";
            return false;
        }

        auto& texture = material->textures[
            wi::scene::MaterialComponent::BASECOLORMAP];
        texture.name.clear();
        texture.resource = std::move(resource);
        material->SetDirty();

        auto* metadata = scene.metadatas.GetComponent(materialEntity);
        if (metadata == nullptr)
            metadata = &scene.metadatas.Create(materialEntity);
        metadata->int_values.set(
            MaterialTextureAssetBindingVersionMetadataKey,
            MaterialTextureAssetBindingVersion);
        metadata->string_values.set(
            MaterialBaseColorTextureAssetIdMetadataKey,
            prepared.assetId);
        error.clear();
        return true;
    }
}

namespace renegade::bridge
{
    bool PrepareMaterialTextureAsset(
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& textureAssetId,
        PreparedMaterialTextureAsset& prepared,
        std::string& error)
    {
        prepared = {};
        if (projectRoot.empty() || !IsValidStableId(projectId) ||
            !IsValidStableId(textureAssetId))
        {
            error = "Texture binding requires valid project and texture asset identity.";
            return false;
        }

        AssetRegistry registry;
        if (!ReadAssetRegistry(projectRoot, projectId, registry, error))
            return false;
        const AssetRecord* record = FindAssetRecord(registry, textureAssetId);
        if (record == nullptr || record->dependencyClass != DependencyClass::Texture ||
            record->requirement != DependencyRequirement::Required ||
            !record->sourceAvailable || record->provider != "lp08.rasset" ||
            record->providerVersion != 1)
        {
            error = "The selected stable ID is not an available governed texture product.";
            return false;
        }
        const ImportedProductRecord* provenance =
            FindImportedProduct(registry, textureAssetId);
        if (provenance == nullptr || provenance->importer != "wicked.resourcemanager" ||
            provenance->importerVersion != 1)
        {
            error = "The governed texture is missing its accepted Wicked import provenance.";
            return false;
        }

        std::error_code ec;
        const fs::path root = fs::weakly_canonical(
            fs::absolute(fs::u8path(projectRoot), ec), ec);
        if (ec || root.empty() || !fs::is_directory(root, ec) || ec)
        {
            error = "The texture project root is unavailable.";
            return false;
        }
        const fs::path content = fs::weakly_canonical(root / "Content", ec);
        if (ec || !fs::is_directory(content, ec) || ec)
        {
            error = "The project Content directory is unavailable.";
            return false;
        }
        const fs::path product = fs::weakly_canonical(
            root / fs::u8path(record->projectRelativePath), ec);
        if (ec || !fs::is_regular_file(product, ec) || ec ||
            !IsWithin(product, content))
        {
            error = "The governed texture product is unavailable or outside project Content.";
            return false;
        }

        ResourceAssetDocument document;
        if (!ReadResourceAssetDocument(product.generic_u8string(), document, error))
            return false;
        if (document.manifest.projectId != projectId ||
            document.manifest.assetId != textureAssetId ||
            document.manifest.resourceClass != ResourceClass::Texture ||
            document.payload.empty())
        {
            error = "The governed texture product does not match its LC01 stable identity.";
            return false;
        }

        prepared.projectId = projectId;
        prepared.assetId = textureAssetId;
        prepared.productProjectRelativePath = record->projectRelativePath;
        prepared.sourceFormat = document.manifest.sourceFormat;
        prepared.payloadHash = document.manifest.payloadHash;
        prepared.logicalResourceName = LogicalResourceName(
            textureAssetId, document.manifest.payloadHash,
            document.manifest.sourceFormat);
        prepared.payload = std::move(document.payload);
        if (prepared.logicalResourceName.empty() ||
            ExtensionForFormat(prepared.sourceFormat).empty())
        {
            prepared = {};
            error = "The governed texture source format cannot be handed to Wicked.";
            return false;
        }
        error.clear();
        return true;
    }

    wi::Resource LoadPreparedMaterialTextureAsset(
        const PreparedMaterialTextureAsset& prepared,
        std::string& error)
    {
        if (prepared.logicalResourceName.empty() || prepared.payload.empty())
        {
            error = "The prepared texture payload is empty.";
            return {};
        }
        wi::Resource resource = wi::resourcemanager::Load(
            prepared.logicalResourceName,
            wi::resourcemanager::Flags::NONE,
            prepared.payload.data(),
            prepared.payload.size());
        if (!resource.IsValid() || !resource.GetTexture().IsValid())
        {
            error = "Wicked could not decode the governed texture payload.";
            return {};
        }
        error.clear();
        return resource;
    }

    bool InspectMaterialTextureBindings(
        const wi::scene::Scene& scene,
        std::vector<MaterialTextureBindingRecord>& bindings,
        std::string& error)
    {
        bindings.clear();
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const auto& metadata = scene.metadatas[index];
            if (!metadata.string_values.has(
                    MaterialBaseColorTextureAssetIdMetadataKey))
                continue;
            if (!metadata.int_values.has(
                    MaterialTextureAssetBindingVersionMetadataKey) ||
                metadata.int_values.get(
                    MaterialTextureAssetBindingVersionMetadataKey) !=
                    MaterialTextureAssetBindingVersion)
            {
                error = "Material texture binding metadata has an unsupported version.";
                return false;
            }
            MaterialTextureBindingRecord record;
            record.materialEntity = scene.metadatas.GetEntity(index);
            record.baseColorTextureAssetId = metadata.string_values.get(
                MaterialBaseColorTextureAssetIdMetadataKey);
            if (!IsValidStableId(record.baseColorTextureAssetId) ||
                scene.materials.GetComponent(record.materialEntity) == nullptr ||
                IsTerrainOwnedMaterial(scene, record.materialEntity))
            {
                error = "Material texture binding metadata targets an invalid material or asset ID.";
                return false;
            }
            bindings.push_back(std::move(record));
        }
        error.clear();
        return true;
    }

    MaterialTextureRestoreResult RestoreMaterialTextureBindings(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        MaterialTextureResourceLoader loader)
    {
        MaterialTextureRestoreResult result;
        std::vector<MaterialTextureBindingRecord> bindings;
        if (!InspectMaterialTextureBindings(scene, bindings, result.error))
            return result;
        result.discovered = bindings.size();
        if (!loader)
            loader = LoadPreparedMaterialTextureAsset;

        for (const auto& binding : bindings)
        {
            auto* material = scene.materials.GetComponent(binding.materialEntity);
            if (material == nullptr)
            {
                result.error = "A persisted texture binding lost its material target.";
                return result;
            }
            auto& texture = material->textures[
                wi::scene::MaterialComponent::BASECOLORMAP];
            if (texture.resource.IsValid() && texture.name.empty())
                continue;

            PreparedMaterialTextureAsset prepared;
            if (!PrepareMaterialTextureAsset(
                    projectRoot, projectId, binding.baseColorTextureAssetId,
                    prepared, result.error))
                return result;
            if (!ApplyBinding(
                    scene, binding.materialEntity, prepared, loader,
                    result.error))
                return result;
            ++result.restored;
        }
        result.succeeded = true;
        result.error.clear();
        return result;
    }

    SetMaterialBaseColorTextureAssetCommand::SetMaterialBaseColorTextureAssetCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity,
        PreparedMaterialTextureAsset prepared,
        MaterialTextureResourceLoader loader)
        : scene_(&scene)
        , materialEntity_(materialEntity)
        , prepared_(std::move(prepared))
        , loader_(std::move(loader))
    {
    }

    void SetMaterialBaseColorTextureAssetCommand::CaptureBefore()
    {
        if (capturedBefore_ || scene_ == nullptr)
            return;
        const auto* material = scene_->materials.GetComponent(materialEntity_);
        if (material == nullptr)
            return;
        beforeTexture_ = material->textures[
            wi::scene::MaterialComponent::BASECOLORMAP];
        const auto* metadata = scene_->metadatas.GetComponent(materialEntity_);
        hadMetadata_ = metadata != nullptr;
        if (metadata != nullptr)
        {
            hadVersion_ = metadata->int_values.has(
                MaterialTextureAssetBindingVersionMetadataKey);
            if (hadVersion_)
            {
                beforeVersion_ = metadata->int_values.get(
                    MaterialTextureAssetBindingVersionMetadataKey);
            }
            hadAssetId_ = metadata->string_values.has(
                MaterialBaseColorTextureAssetIdMetadataKey);
            if (hadAssetId_)
            {
                beforeAssetId_ = metadata->string_values.get(
                    MaterialBaseColorTextureAssetIdMetadataKey);
            }
        }
        capturedBefore_ = true;
    }

    bool SetMaterialBaseColorTextureAssetCommand::ApplyPrepared()
    {
        if (scene_ == nullptr ||
            IsTerrainOwnedMaterial(*scene_, materialEntity_) ||
            scene_->materials.GetComponent(materialEntity_) == nullptr)
        {
            error_ = "Texture assignment target is no longer editable.";
            return false;
        }
        const auto* metadata = scene_->metadatas.GetComponent(materialEntity_);
        if (metadata != nullptr &&
            metadata->string_values.has(
                MaterialBaseColorTextureAssetIdMetadataKey) &&
            metadata->string_values.get(
                MaterialBaseColorTextureAssetIdMetadataKey) == prepared_.assetId)
        {
            const auto* material = scene_->materials.GetComponent(materialEntity_);
            if (material->textures[
                    wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid())
            {
                error_ = "The selected texture is already assigned to this material.";
                return false;
            }
        }
        return ApplyBinding(
            *scene_, materialEntity_, prepared_, loader_, error_);
    }

    bool SetMaterialBaseColorTextureAssetCommand::Execute()
    {
        CaptureBefore();
        if (!capturedBefore_)
        {
            error_ = "Texture assignment could not capture the target material.";
            return false;
        }
        return ApplyPrepared();
    }

    void SetMaterialBaseColorTextureAssetCommand::RestoreBefore() noexcept
    {
        if (scene_ == nullptr || !capturedBefore_)
            return;
        auto* material = scene_->materials.GetComponent(materialEntity_);
        if (material == nullptr)
            return;
        material->textures[
            wi::scene::MaterialComponent::BASECOLORMAP] = beforeTexture_;
        material->SetDirty();

        if (!hadMetadata_)
        {
            scene_->metadatas.Remove(materialEntity_);
            return;
        }
        auto* metadata = scene_->metadatas.GetComponent(materialEntity_);
        if (metadata == nullptr)
            metadata = &scene_->metadatas.Create(materialEntity_);
        if (hadVersion_)
        {
            metadata->int_values.set(
                MaterialTextureAssetBindingVersionMetadataKey,
                beforeVersion_);
        }
        else
        {
            metadata->int_values.erase(
                MaterialTextureAssetBindingVersionMetadataKey);
        }
        if (hadAssetId_)
        {
            metadata->string_values.set(
                MaterialBaseColorTextureAssetIdMetadataKey,
                beforeAssetId_);
        }
        else
        {
            metadata->string_values.erase(
                MaterialBaseColorTextureAssetIdMetadataKey);
        }
    }

    void SetMaterialBaseColorTextureAssetCommand::Undo()
    {
        RestoreBefore();
    }
}
