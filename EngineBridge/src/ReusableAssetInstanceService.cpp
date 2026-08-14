#include "renegade/bridge/ReusableAssetInstanceService.h"

#include "renegade/bridge/CreatorModelImportRecipe.h"

#include <utility>

namespace renegade::bridge
{
    namespace
    {
        bool WrapperExists(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity) noexcept
        {
            return entity != wi::ecs::INVALID_ENTITY &&
                scene.transforms.GetComponent(entity) != nullptr;
        }

        bool ReadInstanceAssetId(
            const wi::scene::MetadataComponent& metadata,
            StableId& assetId,
            std::string& error)
        {
            if (!metadata.string_values.has(ReusableAssetInstanceIdMetadataKey))
                return false;

            if (!metadata.int_values.has(
                    ReusableAssetInstanceVersionMetadataKey) ||
                metadata.int_values.get(
                    ReusableAssetInstanceVersionMetadataKey) !=
                    ReusableAssetInstanceVersion)
            {
                error =
                    "Reusable asset instance metadata has an unsupported version.";
                return false;
            }

            assetId = metadata.string_values.get(
                ReusableAssetInstanceIdMetadataKey);
            if (!IsValidStableId(assetId))
            {
                error =
                    "Reusable asset instance metadata contains an invalid stable asset ID.";
                return false;
            }
            return true;
        }

        bool IsPayloadRootMetadata(
            const wi::scene::MetadataComponent& metadata) noexcept
        {
            return metadata.bool_values.has(ReusableAssetPayloadRootMetadataKey) &&
                metadata.bool_values.get(ReusableAssetPayloadRootMetadataKey);
        }

        wi::ecs::Entity FindNewCreatorAuthoredRoot(
            const wi::scene::Scene& scene,
            const wi::unordered_set<wi::ecs::Entity>& entitiesBefore) noexcept
        {
            for (std::size_t index = 0; index < scene.names.GetCount(); ++index)
            {
                if (scene.names[index].name != CreatorAuthoredTransformRootName)
                    continue;
                const wi::ecs::Entity entity = scene.names.GetEntity(index);
                if (entitiesBefore.count(entity) == 0 &&
                    scene.transforms.GetComponent(entity) != nullptr)
                {
                    return entity;
                }
            }
            return wi::ecs::INVALID_ENTITY;
        }
    }

    bool InspectReusableAssetInstances(
        const wi::scene::Scene& scene,
        std::vector<ReusableAssetInstanceRecord>& instances,
        std::string& error)
    {
        instances.clear();

        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const wi::ecs::Entity wrapper = scene.metadatas.GetEntity(index);
            const auto& metadata = scene.metadatas[index];
            if (!metadata.string_values.has(ReusableAssetInstanceIdMetadataKey))
                continue;

            StableId assetId;
            if (!ReadInstanceAssetId(metadata, assetId, error))
                return false;
            if (!WrapperExists(scene, wrapper))
            {
                error =
                    "Reusable asset instance metadata is not attached to a transform entity.";
                return false;
            }

            wi::ecs::Entity payloadRoot = wi::ecs::INVALID_ENTITY;
            for (std::size_t candidateIndex = 0;
                candidateIndex < scene.metadatas.GetCount(); ++candidateIndex)
            {
                const auto& candidateMetadata = scene.metadatas[candidateIndex];
                if (!IsPayloadRootMetadata(candidateMetadata))
                    continue;

                const wi::ecs::Entity candidate =
                    scene.metadatas.GetEntity(candidateIndex);
                if (!scene.Entity_IsDescendant(candidate, wrapper))
                    continue;

                if (payloadRoot != wi::ecs::INVALID_ENTITY)
                {
                    error =
                        "Reusable asset instance contains more than one marked payload root.";
                    return false;
                }
                payloadRoot = candidate;
            }

            if (payloadRoot == wi::ecs::INVALID_ENTITY)
            {
                error =
                    "Reusable asset instance is missing its marked payload root.";
                return false;
            }

            instances.push_back({assetId, wrapper, payloadRoot});
        }

        error.clear();
        return true;
    }

    PlaceReusableModelCommand::PlaceReusableModelCommand(
        wi::scene::Scene& targetScene,
        wi::allocator::shared_ptr<wi::scene::Scene> preparedScene,
        StableId assetId,
        const XMFLOAT3& placementPosition,
        const float scaleFactor)
        : scene_(&targetScene)
        , preparedScene_(std::move(preparedScene))
        , assetId_(std::move(assetId))
        , placementPosition_(placementPosition)
        , scaleFactor_(scaleFactor > 0.0f ? scaleFactor : 1.0f)
    {
    }

    void PlaceReusableModelCommand::CaptureMaterialResources(
        const std::size_t firstMaterialIndex)
    {
        materialResources_.clear();
        if (scene_ == nullptr)
            return;

        for (std::size_t materialIndex = firstMaterialIndex;
            materialIndex < scene_->materials.GetCount(); ++materialIndex)
        {
            const wi::ecs::Entity materialEntity =
                scene_->materials.GetEntity(materialIndex);
            const auto& material = scene_->materials[materialIndex];
            for (std::uint32_t slot = 0;
                slot < wi::scene::MaterialComponent::TEXTURESLOT_COUNT; ++slot)
            {
                const auto& resource = material.textures[slot].resource;
                if (resource.IsValid())
                {
                    materialResources_.push_back(
                        {materialEntity, slot, resource});
                }
            }
        }
    }

    void PlaceReusableModelCommand::RestoreCapturedMaterialResources()
    {
        if (scene_ == nullptr)
            return;

        for (const auto& captured : materialResources_)
        {
            auto* material = scene_->materials.GetComponent(
                captured.materialEntity);
            if (material == nullptr ||
                captured.slot >= wi::scene::MaterialComponent::TEXTURESLOT_COUNT)
            {
                continue;
            }
            material->textures[captured.slot].resource = captured.resource;
            material->SetDirty();
        }
    }

    bool PlaceReusableModelCommand::Execute()
    {
        if (!hasSnapshot_)
        {
            if (scene_ == nullptr || !preparedScene_.IsValid() ||
                !IsValidStableId(assetId_))
            {
                return false;
            }

            wi::unordered_set<wi::ecs::Entity> entitiesBefore;
            scene_->FindAllEntities(entitiesBefore);
            const std::size_t transformCountBefore =
                scene_->transforms.GetCount();
            const std::size_t animationCountBefore =
                scene_->animations.GetCount();
            const std::size_t materialCountBefore =
                scene_->materials.GetCount();

            scene_->Merge(*preparedScene_);
            preparedScene_.reset();
            if (scene_->transforms.GetCount() <= transformCountBefore)
                return false;

            // Creator-authored products deliberately carry their approved
            // transform on one marked root. That root is the replaceable
            // payload boundary and must remain intact below the instance
            // wrapper. Legacy products predate the marker and retain the Gate
            // 6 normalization behavior.
            payloadRoot_ = FindNewCreatorAuthoredRoot(*scene_, entitiesBefore);
            const bool creatorAuthoredPayload =
                payloadRoot_ != wi::ecs::INVALID_ENTITY;
            if (!creatorAuthoredPayload)
            {
                payloadRoot_ =
                    scene_->transforms.GetEntity(transformCountBefore);
            }

            auto* payloadTransform =
                scene_->transforms.GetComponent(payloadRoot_);
            if (payloadTransform == nullptr)
                return false;

            if (!creatorAuthoredPayload)
            {
                payloadTransform->translation_local = XMFLOAT3(0.0f, 0.0f, 0.0f);
                payloadTransform->scale_local = XMFLOAT3(1.0f, 1.0f, 1.0f);
                payloadTransform->SetDirty();
            }

            entity_ = scene_->Entity_CreateTransform("Reusable Asset Instance");
            auto* instanceTransform = scene_->transforms.GetComponent(entity_);
            if (instanceTransform == nullptr)
                return false;
            instanceTransform->translation_local = placementPosition_;
            instanceTransform->scale_local = XMFLOAT3(
                scaleFactor_, scaleFactor_, scaleFactor_);
            instanceTransform->SetDirty();

            auto& instanceMetadata = scene_->metadatas.Create(entity_);
            instanceMetadata.string_values.set(
                ReusableAssetInstanceIdMetadataKey, assetId_);
            instanceMetadata.int_values.set(
                ReusableAssetInstanceVersionMetadataKey,
                ReusableAssetInstanceVersion);

            auto* payloadMetadata = scene_->metadatas.GetComponent(payloadRoot_);
            if (payloadMetadata == nullptr)
                payloadMetadata = &scene_->metadatas.Create(payloadRoot_);
            payloadMetadata->bool_values.set(
                ReusableAssetPayloadRootMetadataKey, true);

            // Both the legacy normalized root and the creator-authored root are
            // already in the local space Renegade wants to preserve below the
            // stable instance wrapper.
            scene_->Component_Attach(payloadRoot_, entity_, true);

            for (std::size_t index = animationCountBefore;
                index < scene_->animations.GetCount(); ++index)
            {
                scene_->animations[index].Play();
            }

            // Wicked Resource handles are deliberately non-serialized. Keep
            // the governed resources loaded by placement preparation alive so
            // Undo/Redo can restore the exact same live material state instead
            // of producing a grey instance until the next project reopen.
            CaptureMaterialResources(materialCountBefore);

            snapshot_.SetReadModeAndResetPos(false);
            wi::ecs::EntitySerializer serializer;
            scene_->Entity_Serialize(snapshot_, serializer, entity_);
            hasSnapshot_ = true;
            return true;
        }

        if (scene_ == nullptr || WrapperExists(*scene_, entity_))
            return false;

        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        const wi::ecs::Entity restored =
            scene_->Entity_Serialize(snapshot_, serializer);
        if (restored != entity_)
            return false;

        std::vector<ReusableAssetInstanceRecord> instances;
        std::string error;
        if (!InspectReusableAssetInstances(*scene_, instances, error))
            return false;
        for (const auto& instance : instances)
        {
            if (instance.instanceRoot == entity_)
            {
                payloadRoot_ = instance.payloadRoot;
                RestoreCapturedMaterialResources();
                return true;
            }
        }
        return false;
    }

    void PlaceReusableModelCommand::Undo()
    {
        if (scene_ != nullptr && WrapperExists(*scene_, entity_))
            scene_->Entity_Remove(entity_);
    }

    wi::ecs::Entity PlaceReusableModelCommand::PlacedEntity() const noexcept
    {
        return entity_;
    }

    wi::ecs::Entity PlaceReusableModelCommand::PayloadRootEntity() const noexcept
    {
        return payloadRoot_;
    }

    const StableId& PlaceReusableModelCommand::AssetId() const noexcept
    {
        return assetId_;
    }
}
