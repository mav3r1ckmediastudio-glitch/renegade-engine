#include "renegade/bridge/ParticleEffectService.h"

#include "renegade/bridge/IdentityService.h"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace
{
    bool EntityExists(const wi::scene::Scene& scene, const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    bool HasMetadataValue(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const char* key,
        const char* value) noexcept
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr && metadata->string_values.has(key) &&
            metadata->string_values.get(key) == value;
    }

    void SetMetadataValue(
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

    std::string EntityName(const wi::scene::Scene& scene, const wi::ecs::Entity entity)
    {
        const auto* name = scene.names.GetComponent(entity);
        return name != nullptr && !name->name.empty()
            ? name->name
            : std::string("Particle Effect");
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

    bool AssignIdentityOrRemove(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        std::string error;
        if (renegade::bridge::AssignNewPersistentEntityId(scene, entity, error))
            return true;
        scene.Entity_Remove(entity);
        return false;
    }
}

namespace renegade::bridge
{
    bool IsParticleEffectRoot(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        return entity != wi::ecs::INVALID_ENTITY &&
            scene.transforms.GetComponent(entity) != nullptr &&
            HasMetadataValue(
                scene,
                entity,
                ParticleEffectRootMetadataKey,
                ParticleEffectMetadataVersion);
    }

    bool IsParticleEffectLayer(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        return IsParticleEmitter(scene, entity) &&
            HasMetadataValue(
                scene,
                entity,
                ParticleEffectLayerMetadataKey,
                ParticleEffectMetadataVersion);
    }

    wi::ecs::Entity ParticleEffectRootForLayer(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity layer) noexcept
    {
        if (!IsParticleEffectLayer(scene, layer))
            return wi::ecs::INVALID_ENTITY;

        wi::ecs::Entity current = layer;
        const std::size_t maximumDepth = scene.hierarchy.GetCount() + 1;
        for (std::size_t depth = 0; depth <= maximumDepth; ++depth)
        {
            const auto* hierarchy = scene.hierarchy.GetComponent(current);
            if (hierarchy == nullptr ||
                hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                hierarchy->parentID == current)
                break;
            current = hierarchy->parentID;
            if (IsParticleEffectRoot(scene, current))
                return current;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    std::vector<ParticleEffectLayer> CollectParticleEffectLayers(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity effectRoot)
    {
        std::vector<ParticleEffectLayer> result;
        if (!IsParticleEffectRoot(scene, effectRoot))
            return result;

        result.reserve(scene.emitters.GetCount());
        for (std::size_t index = 0; index < scene.emitters.GetCount(); ++index)
        {
            const auto entity = scene.emitters.GetEntity(index);
            if (!IsParticleEffectLayer(scene, entity))
                continue;
            const auto* hierarchy = scene.hierarchy.GetComponent(entity);
            if (hierarchy == nullptr || hierarchy->parentID != effectRoot)
                continue;
            result.push_back({entity, EntityName(scene, entity)});
        }

        std::sort(result.begin(), result.end(),
            [](const ParticleEffectLayer& left, const ParticleEffectLayer& right)
            {
                if (left.name != right.name)
                    return left.name < right.name;
                return left.entity < right.entity;
            });
        return result;
    }

    CreateParticleEffectCommand::CreateParticleEffectCommand(
        wi::scene::Scene& scene,
        const XMFLOAT3& position,
        std::string name)
        : scene_(&scene), position_(position), requestedName_(std::move(name))
    {
        if (requestedName_.empty())
            requestedName_ = "Particle Effect";
    }

    bool CreateParticleEffectCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;

        if (hasSnapshot_)
        {
            if (EntityExists(*scene_, root_) || EntityExists(*scene_, firstLayer_))
                return false;

            rootSnapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer rootSerializer;
            rootSerializer.allow_remap = false;
            if (scene_->Entity_Serialize(rootSnapshot_, rootSerializer) != root_)
                return false;

            layerSnapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer layerSerializer;
            layerSerializer.allow_remap = false;
            if (scene_->Entity_Serialize(layerSnapshot_, layerSerializer) != firstLayer_)
            {
                scene_->Entity_Remove(root_);
                return false;
            }
            return true;
        }

        const std::string effectName = UniqueName(*scene_, requestedName_);
        root_ = scene_->Entity_CreateTransform(effectName);
        if (root_ == wi::ecs::INVALID_ENTITY || !AssignIdentityOrRemove(*scene_, root_))
        {
            root_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        auto* rootTransform = scene_->transforms.GetComponent(root_);
        if (rootTransform == nullptr)
        {
            scene_->Entity_Remove(root_);
            root_ = wi::ecs::INVALID_ENTITY;
            return false;
        }
        rootTransform->translation_local = position_;
        rootTransform->SetDirty();
        rootTransform->UpdateTransform();
        SetMetadataValue(
            *scene_, root_, ParticleEffectRootMetadataKey, ParticleEffectMetadataVersion);

        firstLayer_ = scene_->Entity_CreateEmitter(
            UniqueName(*scene_, effectName + " / Layer 1"), position_);
        if (firstLayer_ == wi::ecs::INVALID_ENTITY ||
            !AssignIdentityOrRemove(*scene_, firstLayer_))
        {
            scene_->Entity_Remove(root_);
            root_ = wi::ecs::INVALID_ENTITY;
            firstLayer_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        SetMetadataValue(
            *scene_, firstLayer_, ParticleEffectLayerMetadataKey, ParticleEffectMetadataVersion);
        ApplyParticleEmitter(*scene_, firstLayer_, MakeNewParticleEmitterState());
        scene_->Component_Attach(firstLayer_, root_);

        rootSnapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer rootSerializer;
        scene_->Entity_Serialize(rootSnapshot_, rootSerializer, root_);
        layerSnapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer layerSerializer;
        scene_->Entity_Serialize(layerSnapshot_, layerSerializer, firstLayer_);
        hasSnapshot_ = true;
        return true;
    }

    void CreateParticleEffectCommand::Undo()
    {
        if (scene_ == nullptr)
            return;
        if (EntityExists(*scene_, firstLayer_))
            scene_->Entity_Remove(firstLayer_);
        if (EntityExists(*scene_, root_))
            scene_->Entity_Remove(root_);
    }

    AddParticleEffectLayerCommand::AddParticleEffectLayerCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity effectRoot,
        ParticleEmitterState state,
        TransformState localTransform)
        : scene_(&scene), root_(effectRoot),
          state_(SanitizeParticleEmitterState(state)),
          localTransform_(localTransform)
    {
    }

    bool AddParticleEffectLayerCommand::Execute()
    {
        if (scene_ == nullptr || !IsParticleEffectRoot(*scene_, root_))
            return false;

        if (hasSnapshot_)
        {
            if (EntityExists(*scene_, layer_))
                return false;
            snapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            return scene_->Entity_Serialize(snapshot_, serializer) == layer_;
        }

        const auto* rootTransform = scene_->transforms.GetComponent(root_);
        if (rootTransform == nullptr)
            return false;

        const auto existing = CollectParticleEffectLayers(*scene_, root_);
        const std::string base = EntityName(*scene_, root_) + " / Layer " +
            std::to_string(existing.size() + 1);
        layer_ = scene_->Entity_CreateEmitter(
            UniqueName(*scene_, base), rootTransform->GetPosition());
        if (layer_ == wi::ecs::INVALID_ENTITY || !AssignIdentityOrRemove(*scene_, layer_))
        {
            layer_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        SetMetadataValue(
            *scene_, layer_, ParticleEffectLayerMetadataKey, ParticleEffectMetadataVersion);
        ApplyParticleEmitter(*scene_, layer_, state_);
        scene_->Component_Attach(layer_, root_);
        ApplyLocalTransform(*scene_, layer_, localTransform_);

        snapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        scene_->Entity_Serialize(snapshot_, serializer, layer_);
        hasSnapshot_ = true;
        return true;
    }

    void AddParticleEffectLayerCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, layer_))
            scene_->Entity_Remove(layer_);
    }
}
