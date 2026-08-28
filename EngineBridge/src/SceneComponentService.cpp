#include "renegade/bridge/SceneComponentService.h"

#include "renegade/bridge/ReusableAssetInstanceService.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        bool IsReusableRoot(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity) noexcept
        {
            const auto* metadata = scene.metadatas.GetComponent(entity);
            return metadata != nullptr && metadata->string_values.has(
                ReusableAssetInstanceIdMetadataKey);
        }

        std::string TrimName(std::string value)
        {
            const auto notSpace = [](const unsigned char character)
            {
                return !std::isspace(character);
            };
            const auto begin = std::find_if(value.begin(), value.end(), notSpace);
            const auto end = std::find_if(value.rbegin(), value.rend(), notSpace).base();
            if (begin >= end)
                return {};
            return std::string(begin, end);
        }

        void AppendUnique(
            std::vector<wi::ecs::Entity>& values,
            const wi::ecs::Entity entity)
        {
            if (entity == wi::ecs::INVALID_ENTITY)
                return;
            if (std::find(values.begin(), values.end(), entity) == values.end())
                values.push_back(entity);
        }

        std::vector<wi::ecs::Entity> CollectLayerTargets(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity selected)
        {
            std::vector<wi::ecs::Entity> targets;
            const wi::ecs::Entity root =
                ResolveSceneComponentAuthoringRoot(scene, selected);
            if (root == wi::ecs::INVALID_ENTITY)
                return targets;

            AppendUnique(targets, root);
            if (!IsReusableRoot(scene, root))
                return targets;

            for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
            {
                const wi::ecs::Entity entity = scene.objects.GetEntity(index);
                if (entity == root || scene.Entity_IsDescendant(entity, root))
                    AppendUnique(targets, entity);
            }
            return targets;
        }

        std::vector<wi::ecs::Entity> CollectObjectTargets(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity selected)
        {
            std::vector<wi::ecs::Entity> targets;
            const wi::ecs::Entity root =
                ResolveSceneComponentAuthoringRoot(scene, selected);
            if (root == wi::ecs::INVALID_ENTITY)
                return targets;

            if (!IsReusableRoot(scene, root))
            {
                if (scene.objects.Contains(root))
                    targets.push_back(root);
                return targets;
            }

            for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
            {
                const wi::ecs::Entity entity = scene.objects.GetEntity(index);
                if (entity == root || scene.Entity_IsDescendant(entity, root))
                    targets.push_back(entity);
            }
            return targets;
        }

        bool ReadObjectProperty(
            const wi::scene::ObjectComponent& object,
            const ObjectParticipationProperty property) noexcept
        {
            switch (property)
            {
            case ObjectParticipationProperty::Renderable:
                return object.IsRenderable();
            case ObjectParticipationProperty::CastShadow:
                return object.IsCastingShadow();
            case ObjectParticipationProperty::Foreground:
                return object.IsForeground();
            case ObjectParticipationProperty::VisibleInMainCamera:
                return !object.IsNotVisibleInMainCamera();
            case ObjectParticipationProperty::VisibleInReflections:
                return !object.IsNotVisibleInReflections();
            case ObjectParticipationProperty::Wetmap:
                return object.IsWetmapEnabled();
            }
            return false;
        }

        void ApplyObjectProperty(
            wi::scene::ObjectComponent& object,
            const ObjectParticipationProperty property,
            const bool value) noexcept
        {
            switch (property)
            {
            case ObjectParticipationProperty::Renderable:
                object.SetRenderable(value);
                break;
            case ObjectParticipationProperty::CastShadow:
                object.SetCastShadow(value);
                break;
            case ObjectParticipationProperty::Foreground:
                object.SetForeground(value);
                break;
            case ObjectParticipationProperty::VisibleInMainCamera:
                object.SetNotVisibleInMainCamera(!value);
                break;
            case ObjectParticipationProperty::VisibleInReflections:
                object.SetNotVisibleInReflections(!value);
                break;
            case ObjectParticipationProperty::Wetmap:
                object.SetWetmapEnabled(value);
                break;
            }
        }
    }

    wi::ecs::Entity ResolveSceneComponentAuthoringRoot(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selected) noexcept
    {
        if (selected == wi::ecs::INVALID_ENTITY)
            return wi::ecs::INVALID_ENTITY;

        wi::ecs::Entity current = selected;
        const std::size_t maximumDepth = scene.hierarchy.GetCount() + 1;
        for (std::size_t depth = 0;
            current != wi::ecs::INVALID_ENTITY && depth <= maximumDepth;
            ++depth)
        {
            if (IsReusableRoot(scene, current))
                return current;
            const auto* hierarchy = scene.hierarchy.GetComponent(current);
            if (hierarchy == nullptr ||
                hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                hierarchy->parentID == current)
            {
                break;
            }
            current = hierarchy->parentID;
        }
        return selected;
    }

    SceneLayerMaskState InspectSceneLayerMask(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selected)
    {
        SceneLayerMaskState result;
        const auto targets = CollectLayerTargets(scene, selected);
        result.targetCount = targets.size();
        if (targets.empty())
            return result;

        const auto maskFor = [&scene](const wi::ecs::Entity entity)
        {
            const auto* layer = scene.layers.GetComponent(entity);
            return layer != nullptr ? layer->layerMask : ~0u;
        };

        result.mask = maskFor(targets.front());
        for (std::size_t index = 1; index < targets.size(); ++index)
        {
            if (maskFor(targets[index]) != result.mask)
            {
                result.mixed = true;
                break;
            }
        }
        return result;
    }

    ObjectParticipationState InspectObjectParticipation(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selected,
        const ObjectParticipationProperty property)
    {
        ObjectParticipationState result;
        const auto targets = CollectObjectTargets(scene, selected);
        result.targetCount = targets.size();
        if (targets.empty())
            return result;

        const auto* first = scene.objects.GetComponent(targets.front());
        if (first == nullptr)
            return result;
        result.value = ReadObjectProperty(*first, property);
        for (std::size_t index = 1; index < targets.size(); ++index)
        {
            const auto* object = scene.objects.GetComponent(targets[index]);
            if (object == nullptr || ReadObjectProperty(*object, property) != result.value)
            {
                result.mixed = true;
                break;
            }
        }
        return result;
    }

    SetSceneNameCommand::SetSceneNameCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity selected,
        std::string name)
        : scene_(&scene)
        , entity_(ResolveSceneComponentAuthoringRoot(scene, selected))
        , after_(TrimName(std::move(name)))
    {
        const auto* existing = scene.names.GetComponent(entity_);
        hadName_ = existing != nullptr;
        if (existing != nullptr)
            before_ = existing->name;
    }

    bool SetSceneNameCommand::Apply(const std::string& value)
    {
        if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY || value.empty())
            return false;
        auto* name = scene_->names.GetComponent(entity_);
        if (name == nullptr)
            name = &scene_->names.Create(entity_);
        name->name = value;
        return true;
    }

    bool SetSceneNameCommand::Execute()
    {
        if (after_.empty() || (hadName_ && before_ == after_))
            return false;
        return Apply(after_);
    }

    void SetSceneNameCommand::Undo()
    {
        if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY)
            return;
        if (!hadName_)
        {
            scene_->names.Remove(entity_);
            return;
        }
        (void)Apply(before_);
    }

    SetSceneLayerMaskCommand::SetSceneLayerMaskCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity selected,
        const std::uint32_t mask)
        : scene_(&scene)
        , after_(mask)
    {
        const auto targets = CollectLayerTargets(scene, selected);
        before_.reserve(targets.size());
        for (const wi::ecs::Entity entity : targets)
        {
            TargetState state;
            state.entity = entity;
            if (const auto* layer = scene.layers.GetComponent(entity); layer != nullptr)
            {
                state.hadLayer = true;
                state.mask = layer->layerMask;
            }
            if (const auto* hierarchy = scene.hierarchy.GetComponent(entity);
                hierarchy != nullptr)
            {
                state.hadHierarchy = true;
                state.hierarchyBind = hierarchy->layerMask_bind;
            }
            before_.push_back(state);
        }
    }

    bool SetSceneLayerMaskCommand::Apply(const std::uint32_t mask)
    {
        if (scene_ == nullptr || before_.empty())
            return false;
        for (const auto& target : before_)
        {
            auto* layer = scene_->layers.GetComponent(target.entity);
            if (layer == nullptr)
                layer = &scene_->layers.Create(target.entity);
            layer->layerMask = mask;
            if (auto* hierarchy = scene_->hierarchy.GetComponent(target.entity);
                hierarchy != nullptr)
            {
                hierarchy->layerMask_bind = mask;
            }
            if (auto* material = scene_->materials.GetComponent(target.entity);
                material != nullptr)
            {
                material->SetDirty();
            }
        }
        return true;
    }

    bool SetSceneLayerMaskCommand::Execute()
    {
        if (before_.empty())
            return false;
        bool changed = false;
        for (const auto& target : before_)
        {
            const std::uint32_t current = target.hadLayer ? target.mask : ~0u;
            if (current != after_)
            {
                changed = true;
                break;
            }
        }
        return changed && Apply(after_);
    }

    void SetSceneLayerMaskCommand::Undo()
    {
        if (scene_ == nullptr)
            return;
        for (const auto& target : before_)
        {
            if (!target.hadLayer)
            {
                scene_->layers.Remove(target.entity);
            }
            else
            {
                auto* layer = scene_->layers.GetComponent(target.entity);
                if (layer == nullptr)
                    layer = &scene_->layers.Create(target.entity);
                layer->layerMask = target.mask;
            }
            if (target.hadHierarchy)
            {
                if (auto* hierarchy = scene_->hierarchy.GetComponent(target.entity);
                    hierarchy != nullptr)
                {
                    hierarchy->layerMask_bind = target.hierarchyBind;
                }
            }
            if (auto* material = scene_->materials.GetComponent(target.entity);
                material != nullptr)
            {
                material->SetDirty();
            }
        }
    }

    SetMetadataPresetCommand::SetMetadataPresetCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity selected,
        const wi::scene::MetadataComponent::Preset preset)
        : scene_(&scene)
        , entity_(ResolveSceneComponentAuthoringRoot(scene, selected))
        , after_(preset)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity_);
        hadMetadata_ = metadata != nullptr;
        if (metadata != nullptr)
            before_ = metadata->preset;
    }

    bool SetMetadataPresetCommand::Apply(
        const wi::scene::MetadataComponent::Preset preset)
    {
        if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY)
            return false;
        auto* metadata = scene_->metadatas.GetComponent(entity_);
        if (metadata == nullptr)
            metadata = &scene_->metadatas.Create(entity_);
        metadata->preset = preset;
        return true;
    }

    bool SetMetadataPresetCommand::Execute()
    {
        if (hadMetadata_ && before_ == after_)
            return false;
        if (!hadMetadata_ && after_ == wi::scene::MetadataComponent::Preset::Custom)
            return false;
        return Apply(after_);
    }

    void SetMetadataPresetCommand::Undo()
    {
        if (scene_ == nullptr || entity_ == wi::ecs::INVALID_ENTITY)
            return;
        if (!hadMetadata_)
        {
            scene_->metadatas.Remove(entity_);
            return;
        }
        (void)Apply(before_);
    }

    SetObjectParticipationCommand::SetObjectParticipationCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity selected,
        const ObjectParticipationProperty property,
        const bool value)
        : scene_(&scene)
        , property_(property)
        , after_(value)
    {
        const auto targets = CollectObjectTargets(scene, selected);
        before_.reserve(targets.size());
        for (const wi::ecs::Entity entity : targets)
        {
            const auto* object = scene.objects.GetComponent(entity);
            if (object == nullptr)
                continue;
            before_.push_back({entity, ReadObjectProperty(*object, property_)});
        }
    }

    bool SetObjectParticipationCommand::Apply(const bool value)
    {
        if (scene_ == nullptr || before_.empty())
            return false;
        for (const auto& target : before_)
        {
            auto* object = scene_->objects.GetComponent(target.entity);
            if (object != nullptr)
                ApplyObjectProperty(*object, property_, value);
        }
        return true;
    }

    bool SetObjectParticipationCommand::Execute()
    {
        if (before_.empty())
            return false;
        const bool changed = std::any_of(
            before_.begin(),
            before_.end(),
            [this](const TargetState& target)
            {
                return target.value != after_;
            });
        return changed && Apply(after_);
    }

    void SetObjectParticipationCommand::Undo()
    {
        if (scene_ == nullptr)
            return;
        for (const auto& target : before_)
        {
            auto* object = scene_->objects.GetComponent(target.entity);
            if (object != nullptr)
                ApplyObjectProperty(*object, property_, target.value);
        }
    }
}
