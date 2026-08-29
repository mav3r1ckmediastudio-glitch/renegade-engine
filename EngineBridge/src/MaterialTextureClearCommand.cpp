#include "renegade/bridge/MaterialTextureAssetService.h"

#include "renegade/bridge/MaterialService.h"

namespace renegade::bridge
{
    ClearMaterialTextureAssetCommand::ClearMaterialTextureAssetCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity materialEntity,
        const MaterialTextureSlot slot)
        : scene_(&scene)
        , materialEntity_(materialEntity)
        , slot_(slot)
    {
    }

    void ClearMaterialTextureAssetCommand::CaptureBefore()
    {
        if (capturedBefore_ || scene_ == nullptr)
            return;

        const auto* material = scene_->materials.GetComponent(materialEntity_);
        if (material == nullptr || IsTerrainOwnedMaterial(*scene_, materialEntity_))
            return;

        beforeTexture_ = material->textures[WickedTextureSlot(slot_)];
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
                MaterialTextureSlotMetadataKey(slot_));
            if (hadAssetId_)
            {
                beforeAssetId_ = metadata->string_values.get(
                    MaterialTextureSlotMetadataKey(slot_));
            }
        }
        capturedBefore_ = true;
    }

    bool ClearMaterialTextureAssetCommand::Execute()
    {
        CaptureBefore();
        if (!capturedBefore_ || scene_ == nullptr)
            return false;

        auto* material = scene_->materials.GetComponent(materialEntity_);
        if (material == nullptr || IsTerrainOwnedMaterial(*scene_, materialEntity_))
            return false;

        auto& texture = material->textures[WickedTextureSlot(slot_)];
        const bool hadNativeTexture =
            !texture.name.empty() || texture.resource.IsValid();
        if (!hadNativeTexture && !hadAssetId_)
            return false;

        texture = {};
        material->SetDirty();

        auto* metadata = scene_->metadatas.GetComponent(materialEntity_);
        if (metadata != nullptr)
        {
            metadata->string_values.erase(MaterialTextureSlotMetadataKey(slot_));

            const bool hasAnyGovernedBinding =
                metadata->string_values.has(MaterialBaseColorTextureAssetIdMetadataKey) ||
                metadata->string_values.has(MaterialNormalTextureAssetIdMetadataKey) ||
                metadata->string_values.has(MaterialSurfaceTextureAssetIdMetadataKey) ||
                metadata->string_values.has(MaterialEmissiveTextureAssetIdMetadataKey) ||
                metadata->string_values.has(MaterialOcclusionTextureAssetIdMetadataKey);
            if (!hasAnyGovernedBinding)
            {
                metadata->int_values.erase(
                    MaterialTextureAssetBindingVersionMetadataKey);
            }
        }
        return true;
    }

    void ClearMaterialTextureAssetCommand::RestoreBefore() noexcept
    {
        if (scene_ == nullptr || !capturedBefore_)
            return;

        auto* material = scene_->materials.GetComponent(materialEntity_);
        if (material == nullptr)
            return;

        material->textures[WickedTextureSlot(slot_)] = beforeTexture_;
        material->SetDirty();

        auto* metadata = scene_->metadatas.GetComponent(materialEntity_);
        if (!hadMetadata_ && metadata == nullptr)
            return;
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
                MaterialTextureSlotMetadataKey(slot_),
                beforeAssetId_);
        }
        else
        {
            metadata->string_values.erase(MaterialTextureSlotMetadataKey(slot_));
        }
    }

    void ClearMaterialTextureAssetCommand::Undo()
    {
        RestoreBefore();
    }
}
