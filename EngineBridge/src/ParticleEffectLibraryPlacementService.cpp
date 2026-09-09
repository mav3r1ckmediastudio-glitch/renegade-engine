#include "renegade/bridge/ParticleEffectLibraryService.h"

#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/MaterialService.h"
#include "renegade/bridge/ParticleBlendModeService.h"

#include <filesystem>
#include <string>
#include <unordered_set>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    bool EntityExists(const wi::scene::Scene& scene, const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    std::string UniqueName(const wi::scene::Scene& scene, const std::string& base)
    {
        auto collision = [&](const std::string& candidate)
        {
            for (std::size_t index = 0; index < scene.names.GetCount(); ++index)
            {
                if (scene.names[index].name == candidate)
                    return true;
            }
            return false;
        };
        if (!collision(base))
            return base;
        for (int suffix = 2;; ++suffix)
        {
            const std::string candidate = base + " " + std::to_string(suffix);
            if (!collision(candidate))
                return candidate;
        }
    }

    bool AssignIdentity(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        std::string& error)
    {
        if (renegade::bridge::AssignNewPersistentEntityId(scene, entity, error))
            return true;
        scene.Entity_Remove(entity);
        return false;
    }

    void SetMetadata(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const char* key,
        const char* value)
    {
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr)
            metadata = &scene.metadatas.Create(entity);
        metadata->string_values.set(key, value);
    }

    void ApplyLocalTransform(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const renegade::bridge::TransformState& state) noexcept
    {
        auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
            return;
        transform->translation_local = state.translation;
        transform->rotation_local = state.rotation;
        transform->scale_local = state.scale;
        transform->SetDirty();
        transform->UpdateTransform();
        const auto* hierarchy = scene.hierarchy.GetComponent(entity);
        if (hierarchy != nullptr && hierarchy->parentID != wi::ecs::INVALID_ENTITY)
        {
            const auto* parent = scene.transforms.GetComponent(hierarchy->parentID);
            if (parent != nullptr)
                transform->UpdateTransform_Parented(*parent);
        }
    }

    bool ApplyBlendMode(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const wi::enums::BLENDMODE blendMode) noexcept
    {
        auto* material = scene.materials.GetComponent(entity);
        if (material == nullptr ||
            !renegade::bridge::IsSupportedParticleBlendMode(blendMode))
            return false;
        auto state = renegade::bridge::CaptureMaterial(*material);
        state.blendMode = blendMode;
        renegade::bridge::ApplyMaterial(
            *material,
            renegade::bridge::SanitizeMaterialState(state));
        return true;
    }
}

namespace renegade::bridge
{
    PrepareParticleEffectPresetResult PrepareParticleEffectPresetForProject(
        const std::string& packagePath,
        const std::string& projectRoot,
        const StableId& projectId)
    {
        PrepareParticleEffectPresetResult result;
        if (packagePath.empty() || projectRoot.empty() || projectId.empty())
        {
            result.error = "Particle effect placement requires package and project identity.";
            return result;
        }

        ParticleEffectPreset loaded;
        if (!LoadParticleEffectPreset(packagePath, loaded, result.error))
            return result;

        result.preset.name = loaded.name;
        result.preset.layers.reserve(loaded.layers.size());
        CreatorTextureWorkflowService workflow;
        const fs::path package = fs::u8path(packagePath);

        for (const auto& layer : loaded.layers)
        {
            PreparedParticleEffectPresetLayer preparedLayer;
            preparedLayer.preset = layer;
            if (!layer.textureLibraryRelativePath.empty())
            {
                const fs::path texture =
                    package / fs::u8path(layer.textureLibraryRelativePath);
                const auto imported = workflow.ImportTexture(
                    projectRoot,
                    projectId,
                    texture.generic_u8string());
                if (!imported.succeeded)
                {
                    result.error = "Particle effect texture import failed: " + imported.error;
                    result.preset = {};
                    return result;
                }

                std::string prepareError;
                if (!PrepareMaterialTextureAsset(
                        projectRoot,
                        projectId,
                        imported.assetId,
                        preparedLayer.texture,
                        prepareError))
                {
                    result.error = "Particle effect texture preparation failed: " + prepareError;
                    result.preset = {};
                    return result;
                }
                preparedLayer.hasTexture = true;
                ++result.importedTextureCount;
            }
            result.preset.layers.push_back(std::move(preparedLayer));
        }

        if (result.preset.layers.empty())
        {
            result.error = "Particle effect preset contains no emitter layers.";
            result.preset = {};
            return result;
        }
        result.succeeded = true;
        return result;
    }

    CreateParticleEffectFromPresetCommand::CreateParticleEffectFromPresetCommand(
        wi::scene::Scene& scene,
        const XMFLOAT3& position,
        PreparedParticleEffectPreset preset)
        : scene_(&scene), position_(position), preset_(std::move(preset))
    {
    }

    bool CreateParticleEffectFromPresetCommand::Execute()
    {
        error_.clear();
        if (scene_ == nullptr || preset_.layers.empty())
        {
            error_ = "Particle effect preset is empty.";
            return false;
        }
        return hasSnapshot_ ? RestoreSnapshot() : CreateFirstTime();
    }

    bool CreateParticleEffectFromPresetCommand::CreateFirstTime()
    {
        const std::string rootName = UniqueName(
            *scene_, preset_.name.empty() ? "Particle Effect" : preset_.name);
        root_ = scene_->Entity_CreateTransform(rootName);
        if (root_ == wi::ecs::INVALID_ENTITY || !AssignIdentity(*scene_, root_, error_))
        {
            root_ = wi::ecs::INVALID_ENTITY;
            if (error_.empty())
                error_ = "Could not create particle effect root.";
            return false;
        }

        auto* rootTransform = scene_->transforms.GetComponent(root_);
        if (rootTransform == nullptr)
        {
            scene_->Entity_Remove(root_);
            root_ = wi::ecs::INVALID_ENTITY;
            error_ = "Particle effect root has no transform.";
            return false;
        }
        rootTransform->translation_local = position_;
        rootTransform->SetDirty();
        rootTransform->UpdateTransform();
        SetMetadata(
            *scene_, root_, ParticleEffectRootMetadataKey, ParticleEffectMetadataVersion);

        layerEntities_.clear();
        layerEntities_.reserve(preset_.layers.size());
        for (std::size_t index = 0; index < preset_.layers.size(); ++index)
        {
            const auto& prepared = preset_.layers[index];
            const std::string fallback =
                rootName + " / Layer " + std::to_string(index + 1);
            const std::string layerName = UniqueName(
                *scene_, prepared.preset.name.empty() ? fallback : prepared.preset.name);
            const auto layer = scene_->Entity_CreateEmitter(layerName, position_);
            if (layer == wi::ecs::INVALID_ENTITY || !AssignIdentity(*scene_, layer, error_))
            {
                scene_->Entity_Remove(root_);
                root_ = wi::ecs::INVALID_ENTITY;
                layerEntities_.clear();
                if (error_.empty())
                    error_ = "Could not create particle effect emitter layer.";
                return false;
            }

            SetMetadata(
                *scene_, layer, ParticleEffectLayerMetadataKey, ParticleEffectMetadataVersion);
            ApplyParticleEmitter(*scene_, layer, prepared.preset.emitter);
            if (!ApplyBlendMode(*scene_, layer, prepared.preset.blendMode))
            {
                scene_->Entity_Remove(root_);
                root_ = wi::ecs::INVALID_ENTITY;
                layerEntities_.clear();
                error_ = "Could not apply particle effect blend mode.";
                return false;
            }

            scene_->Component_Attach(layer, root_);
            ApplyLocalTransform(*scene_, layer, prepared.preset.localTransform);
            layerEntities_.push_back(layer);
        }

        if (!ApplyPreparedTextures())
        {
            scene_->Entity_Remove(root_);
            root_ = wi::ecs::INVALID_ENTITY;
            layerEntities_.clear();
            return false;
        }

        snapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        if (scene_->Entity_Serialize(snapshot_, serializer, root_) != root_)
        {
            scene_->Entity_Remove(root_);
            root_ = wi::ecs::INVALID_ENTITY;
            layerEntities_.clear();
            error_ = "Could not snapshot particle effect for Undo/Redo.";
            return false;
        }
        hasSnapshot_ = true;
        return true;
    }

    bool CreateParticleEffectFromPresetCommand::RestoreSnapshot()
    {
        if (root_ == wi::ecs::INVALID_ENTITY || EntityExists(*scene_, root_))
        {
            error_ = "Particle effect Redo target already exists.";
            return false;
        }
        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        if (scene_->Entity_Serialize(snapshot_, serializer) != root_)
        {
            error_ = "Could not restore particle effect hierarchy.";
            return false;
        }
        if (!ApplyPreparedTextures())
        {
            scene_->Entity_Remove(root_);
            return false;
        }
        return true;
    }

    bool CreateParticleEffectFromPresetCommand::ApplyPreparedTextures()
    {
        if (layerEntities_.size() != preset_.layers.size())
        {
            error_ = "Particle effect layer identity does not match its preset.";
            return false;
        }

        for (std::size_t index = 0; index < preset_.layers.size(); ++index)
        {
            const auto& prepared = preset_.layers[index];
            if (!prepared.hasTexture)
                continue;
            if (!EntityExists(*scene_, layerEntities_[index]))
            {
                error_ = "Particle effect texture target no longer exists.";
                return false;
            }
            std::string textureError;
            if (!ApplyPreparedMaterialTextureAsset(
                    *scene_,
                    layerEntities_[index],
                    MaterialTextureSlot::BaseColor,
                    prepared.texture,
                    {},
                    textureError))
            {
                error_ = "Could not apply particle effect texture: " + textureError;
                return false;
            }
        }
        return true;
    }

    void CreateParticleEffectFromPresetCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, root_))
            scene_->Entity_Remove(root_);
    }
}
