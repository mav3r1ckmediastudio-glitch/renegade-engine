#include "renegade/bridge/MaterialTextureAssetService.h"

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/MaterialService.h"
#include "renegade/bridge/ProjectLifecycleDiagnostics.h"
#include "renegade/bridge/ResourceAssetCacheIdentityService.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    struct SlotDescriptor
    {
        MaterialTextureSlot slot;
        wi::scene::MaterialComponent::TEXTURESLOT wickedSlot;
        const char* metadataKey;
        const char* label;
    };

    constexpr std::array<SlotDescriptor, 5> SlotDescriptors = {{
        {MaterialTextureSlot::BaseColor,
            wi::scene::MaterialComponent::BASECOLORMAP,
            MaterialBaseColorTextureAssetIdMetadataKey, "Base Color"},
        {MaterialTextureSlot::Normal,
            wi::scene::MaterialComponent::NORMALMAP,
            MaterialNormalTextureAssetIdMetadataKey, "Normal"},
        {MaterialTextureSlot::Surface,
            wi::scene::MaterialComponent::SURFACEMAP,
            MaterialSurfaceTextureAssetIdMetadataKey, "Surface"},
        {MaterialTextureSlot::Emissive,
            wi::scene::MaterialComponent::EMISSIVEMAP,
            MaterialEmissiveTextureAssetIdMetadataKey, "Emissive"},
        {MaterialTextureSlot::Occlusion,
            wi::scene::MaterialComponent::OCCLUSIONMAP,
            MaterialOcclusionTextureAssetIdMetadataKey, "AO"},
    }};

    const SlotDescriptor& Descriptor(const MaterialTextureSlot slot) noexcept
    {
        const auto found = std::find_if(
            SlotDescriptors.begin(), SlotDescriptors.end(),
            [slot](const SlotDescriptor& descriptor)
            { return descriptor.slot == slot; });
        return found == SlotDescriptors.end() ? SlotDescriptors.front() : *found;
    }

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
            { return record.assetId == assetId; });
        return found == registry.records.end() ? nullptr : &*found;
    }

    const ImportedProductRecord* FindImportedProduct(
        const AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            registry.importedProducts.begin(), registry.importedProducts.end(),
            [&assetId](const ImportedProductRecord& record)
            { return record.productAssetId == assetId; });
        return found == registry.importedProducts.end() ? nullptr : &*found;
    }

    bool MetadataEmpty(const wi::scene::MetadataComponent& metadata)
    {
        return metadata.bool_values.names.empty() &&
            metadata.int_values.names.empty() &&
            metadata.float_values.names.empty() &&
            metadata.string_values.names.empty();
    }
}

namespace renegade::bridge
{
    wi::scene::MaterialComponent::TEXTURESLOT WickedTextureSlot(
        const MaterialTextureSlot slot) noexcept
    {
        return Descriptor(slot).wickedSlot;
    }

    const char* MaterialTextureSlotMetadataKey(
        const MaterialTextureSlot slot) noexcept
    {
        return Descriptor(slot).metadataKey;
    }

    const char* MaterialTextureSlotLabel(
        const MaterialTextureSlot slot) noexcept
    {
        return Descriptor(slot).label;
    }

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
        prepared.payload = std::move(document.payload);
        if (!BuildResourcePayloadCacheName(
                "renegade_", textureAssetId, prepared.sourceFormat,
                prepared.payload, prepared.logicalResourceName, error))
        {
            prepared = {};
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
            prepared.payload.data(), prepared.payload.size());
        if (!resource.IsValid() || !resource.GetTexture().IsValid())
        {
            error = "Wicked could not decode the governed texture payload.";
            return {};
        }
        error.clear();
        return resource;
    }

    bool ApplyPreparedMaterialTextureAsset(
        wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity,
        const MaterialTextureSlot slot,
        const PreparedMaterialTextureAsset& prepared,
        MaterialTextureResourceLoader loader,
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

        auto& texture = material->textures[WickedTextureSlot(slot)];
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
            MaterialTextureSlotMetadataKey(slot), prepared.assetId);
        error.clear();
        return true;
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
            bool hasAnyBinding = false;
            for (const auto& descriptor : SlotDescriptors)
                hasAnyBinding = hasAnyBinding || metadata.string_values.has(descriptor.metadataKey);
            if (!hasAnyBinding)
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

            const wi::ecs::Entity entity = scene.metadatas.GetEntity(index);
            if (scene.materials.GetComponent(entity) == nullptr ||
                IsTerrainOwnedMaterial(scene, entity))
            {
                error = "Material texture binding metadata targets an invalid material.";
                return false;
            }

            for (const auto& descriptor : SlotDescriptors)
            {
                if (!metadata.string_values.has(descriptor.metadataKey))
                    continue;
                MaterialTextureBindingRecord record;
                record.materialEntity = entity;
                record.slot = descriptor.slot;
                record.textureAssetId = metadata.string_values.get(descriptor.metadataKey);
                if (!IsValidStableId(record.textureAssetId))
                {
                    error = std::string("Material ") + descriptor.label +
                        " texture binding contains an invalid stable asset ID.";
                    return false;
                }
                if (record.slot == MaterialTextureSlot::BaseColor)
                    record.baseColorTextureAssetId = record.textureAssetId;
                bindings.push_back(std::move(record));
            }
        }
        error.clear();
        return true;
    }

    MaterialTextureRestoreResult RestoreMaterialTextureBindings(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        MaterialTextureResourceLoader loader,
        MaterialTextureRestoreProgress progress)
    {
        const auto restoreStarted = diagnostics::LifecycleClock::now();
        MaterialTextureRestoreResult result;
        std::vector<MaterialTextureBindingRecord> bindings;
        const auto inspectStarted = diagnostics::LifecycleClock::now();
        if (!InspectMaterialTextureBindings(scene, bindings, result.error))
        {
            diagnostics::LogProjectLifecycleTiming(
                "TEXTURE RESTORE",
                diagnostics::MillisecondsSince(restoreStarted),
                "binding inspection failed // " + result.error);
            return result;
        }
        const double inspectMilliseconds =
            diagnostics::MillisecondsSince(inspectStarted);
        result.discovered = bindings.size();
        if (!loader)
            loader = LoadPreparedMaterialTextureAsset;

        std::set<StableId> uniqueTextureAssetIds;
        for (const auto& binding : bindings)
            uniqueTextureAssetIds.insert(binding.textureAssetId);
        result.uniqueAssetIds = uniqueTextureAssetIds.size();
        std::size_t progressedUnique = 0;
        if (progress)
            progress(0, result.uniqueAssetIds);

        struct CachedRestoreAsset
        {
            bool prepareAttempted = false;
            bool preparedSuccessfully = false;
            bool loadAttempted = false;
            bool loadedSuccessfully = false;
            PreparedMaterialTextureAsset prepared;
            wi::Resource resource;
            std::string error;
        };
        std::map<StableId, CachedRestoreAsset> cache;

        std::size_t failureCount = 0;
        double prepareMilliseconds = 0.0;
        double loadMilliseconds = 0.0;
        double applyMilliseconds = 0.0;
        std::string firstFailure;
        for (const auto& binding : bindings)
        {
            auto* material = scene.materials.GetComponent(binding.materialEntity);
            if (material == nullptr)
            {
                ++failureCount;
                if (firstFailure.empty())
                    firstFailure = binding.textureAssetId +
                        ": persisted texture binding lost its material target";
                continue;
            }
            auto& texture = material->textures[WickedTextureSlot(binding.slot)];
            if (texture.resource.IsValid() && texture.name.empty())
            {
                ++result.alreadyLive;
                continue;
            }

            auto& cached = cache[binding.textureAssetId];
            if (!cached.prepareAttempted)
            {
                cached.prepareAttempted = true;
                const auto prepareStarted = diagnostics::LifecycleClock::now();
                cached.preparedSuccessfully = PrepareMaterialTextureAsset(
                    projectRoot, projectId, binding.textureAssetId,
                    cached.prepared, cached.error);
                prepareMilliseconds += diagnostics::MillisecondsSince(prepareStarted);
                if (cached.preparedSuccessfully)
                    ++result.preparedUnique;
                ++progressedUnique;
                if (progress)
                    progress(progressedUnique, result.uniqueAssetIds);
            }

            if (cached.preparedSuccessfully && !cached.loadAttempted)
            {
                cached.loadAttempted = true;
                const auto loadStarted = diagnostics::LifecycleClock::now();
                cached.resource = loader(cached.prepared, cached.error);
                loadMilliseconds += diagnostics::MillisecondsSince(loadStarted);
                cached.loadedSuccessfully = cached.resource.IsValid();
                if (cached.loadedSuccessfully)
                    ++result.loadedUnique;
                else if (cached.error.empty())
                    cached.error =
                        "Wicked did not create a valid resource from the governed texture payload.";
            }

            if (!cached.preparedSuccessfully || !cached.loadedSuccessfully)
            {
                ++failureCount;
                if (firstFailure.empty())
                    firstFailure = binding.textureAssetId + ": " + cached.error;
                continue;
            }

            const auto applyStarted = diagnostics::LifecycleClock::now();
            texture.name.clear();
            texture.resource = cached.resource;
            material->SetDirty();
            auto* metadata = scene.metadatas.GetComponent(binding.materialEntity);
            if (metadata == nullptr)
                metadata = &scene.metadatas.Create(binding.materialEntity);
            metadata->int_values.set(
                MaterialTextureAssetBindingVersionMetadataKey,
                MaterialTextureAssetBindingVersion);
            metadata->string_values.set(
                MaterialTextureSlotMetadataKey(binding.slot),
                cached.prepared.assetId);
            applyMilliseconds += diagnostics::MillisecondsSince(applyStarted);
            ++result.restored;
        }

        std::ostringstream restoreDetail;
        restoreDetail << "bindings=" << bindings.size()
                      << " // unique_ids=" << result.uniqueAssetIds
                      << " // prepared_unique=" << result.preparedUnique
                      << " // loaded_unique=" << result.loadedUnique
                      << " // already_live=" << result.alreadyLive
                      << " // restored=" << result.restored
                      << " // failures=" << failureCount
                      << " // inspect=" << std::fixed << std::setprecision(2)
                      << inspectMilliseconds << " ms"
                      << " // prepare=" << prepareMilliseconds << " ms"
                      << " // load=" << loadMilliseconds << " ms"
                      << " // apply=" << applyMilliseconds << " ms";
        diagnostics::LogProjectLifecycleTiming(
            "TEXTURE RESTORE",
            diagnostics::MillisecondsSince(restoreStarted),
            restoreDetail.str());

        if (failureCount != 0)
        {
            result.error = std::to_string(failureCount) +
                " governed material texture binding(s) could not be restored. First failure: " +
                firstFailure;
            return result;
        }
        result.succeeded = true;
        result.error.clear();
        return result;
    }

    SetMaterialTextureAssetCommand::SetMaterialTextureAssetCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity,
        const MaterialTextureSlot slot,
        PreparedMaterialTextureAsset prepared,
        MaterialTextureResourceLoader loader)
        : scene_(&scene)
        , materialEntity_(materialEntity)
        , slot_(slot)
        , prepared_(std::move(prepared))
        , loader_(std::move(loader))
    {
    }

    void SetMaterialTextureAssetCommand::CaptureBefore()
    {
        if (capturedBefore_ || scene_ == nullptr)
            return;
        const auto* material = scene_->materials.GetComponent(materialEntity_);
        if (material == nullptr)
            return;
        beforeTexture_ = material->textures[WickedTextureSlot(slot_)];
        const auto* metadata = scene_->metadatas.GetComponent(materialEntity_);
        hadMetadata_ = metadata != nullptr;
        if (metadata != nullptr)
        {
            hadVersion_ = metadata->int_values.has(
                MaterialTextureAssetBindingVersionMetadataKey);
            if (hadVersion_)
                beforeVersion_ = metadata->int_values.get(
                    MaterialTextureAssetBindingVersionMetadataKey);
            hadAssetId_ = metadata->string_values.has(
                MaterialTextureSlotMetadataKey(slot_));
            if (hadAssetId_)
                beforeAssetId_ = metadata->string_values.get(
                    MaterialTextureSlotMetadataKey(slot_));
        }
        capturedBefore_ = true;
    }

    bool SetMaterialTextureAssetCommand::Execute()
    {
        CaptureBefore();
        if (!capturedBefore_)
        {
            error_ = "Texture assignment could not capture the target material.";
            return false;
        }
        if (scene_ == nullptr ||
            IsTerrainOwnedMaterial(*scene_, materialEntity_) ||
            scene_->materials.GetComponent(materialEntity_) == nullptr)
        {
            error_ = "Texture assignment target is no longer editable.";
            return false;
        }
        const auto* metadata = scene_->metadatas.GetComponent(materialEntity_);
        if (metadata != nullptr &&
            metadata->string_values.has(MaterialTextureSlotMetadataKey(slot_)) &&
            metadata->string_values.get(MaterialTextureSlotMetadataKey(slot_)) ==
                prepared_.assetId)
        {
            const auto* material = scene_->materials.GetComponent(materialEntity_);
            if (material->textures[WickedTextureSlot(slot_)].resource.IsValid())
            {
                error_ = std::string("The selected texture is already assigned to the ") +
                    MaterialTextureSlotLabel(slot_) + " slot.";
                return false;
            }
        }
        return ApplyPreparedMaterialTextureAsset(
            *scene_, materialEntity_, slot_, prepared_, loader_, error_);
    }

    void SetMaterialTextureAssetCommand::RestoreBefore() noexcept
    {
        if (scene_ == nullptr || !capturedBefore_)
            return;
        auto* material = scene_->materials.GetComponent(materialEntity_);
        if (material == nullptr)
            return;
        material->textures[WickedTextureSlot(slot_)] = beforeTexture_;
        material->SetDirty();

        auto* metadata = scene_->metadatas.GetComponent(materialEntity_);
        if (!hadMetadata_)
        {
            if (metadata == nullptr)
                return;
            metadata->int_values.erase(MaterialTextureAssetBindingVersionMetadataKey);
            metadata->string_values.erase(MaterialTextureSlotMetadataKey(slot_));
            if (MetadataEmpty(*metadata))
                scene_->metadatas.Remove(materialEntity_);
            return;
        }
        if (metadata == nullptr)
            metadata = &scene_->metadatas.Create(materialEntity_);
        if (hadVersion_)
            metadata->int_values.set(
                MaterialTextureAssetBindingVersionMetadataKey, beforeVersion_);
        else
            metadata->int_values.erase(MaterialTextureAssetBindingVersionMetadataKey);
        if (hadAssetId_)
            metadata->string_values.set(
                MaterialTextureSlotMetadataKey(slot_), beforeAssetId_);
        else
            metadata->string_values.erase(MaterialTextureSlotMetadataKey(slot_));
    }

    void SetMaterialTextureAssetCommand::Undo()
    {
        RestoreBefore();
    }

    SetMaterialBaseColorTextureAssetCommand::SetMaterialBaseColorTextureAssetCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity,
        PreparedMaterialTextureAsset prepared,
        MaterialTextureResourceLoader loader)
        : command_(scene, materialEntity, MaterialTextureSlot::BaseColor,
            std::move(prepared), std::move(loader))
    {
    }

    bool SetMaterialBaseColorTextureAssetCommand::Execute()
    {
        return command_.Execute();
    }

    void SetMaterialBaseColorTextureAssetCommand::Undo()
    {
        command_.Undo();
    }
}
