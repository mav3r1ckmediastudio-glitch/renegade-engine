#include "RuntimeScriptEntityApi.h"

#include <cmath>

namespace renegade::runtime
{
    RuntimeScriptEntityApi::RuntimeScriptEntityApi(
        wi::scene::Scene& scene,
        const std::unordered_map<bridge::StableId, wi::ecs::Entity>& entitiesById,
        const std::uint64_t generation) noexcept
        : scene_(&scene)
        , entitiesById_(&entitiesById)
        , generation_(generation)
    {
    }

    bool RuntimeScriptEntityApi::Resolve(
        const RuntimeScriptEntityReference& reference,
        wi::ecs::Entity& entity,
        std::string& error) const
    {
        entity = wi::ecs::INVALID_ENTITY;
        if (scene_ == nullptr || entitiesById_ == nullptr)
        {
            error = "Runtime entity API is not bound to an active Level.";
            return false;
        }
        if (!bridge::IsValidStableId(reference.stableId))
        {
            error = "EntityRef does not contain a valid Renegade identity.";
            return false;
        }
        if (reference.generation != generation_)
        {
            error = "EntityRef belongs to a stale Level generation.";
            return false;
        }

        const auto found = entitiesById_->find(reference.stableId);
        if (found == entitiesById_->end() ||
            found->second == wi::ecs::INVALID_ENTITY)
        {
            error = "EntityRef no longer resolves in the active Level.";
            return false;
        }

        // Never trust the map as a permanent liveness cache. A runtime mutation
        // may have removed/replaced the ECS entity without changing generation.
        if (bridge::PersistentEntityId(*scene_, found->second) != reference.stableId)
        {
            error = "EntityRef no longer resolves to the same live Renegade entity.";
            return false;
        }

        entity = found->second;
        error.clear();
        return true;
    }

    bool RuntimeScriptEntityApi::GetName(
        const RuntimeScriptEntityReference& reference,
        std::string& name,
        std::string& error) const
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        if (!Resolve(reference, entity, error))
            return false;

        const auto* component = scene_->names.GetComponent(entity);
        name = component == nullptr ? std::string{} : component->name;
        error.clear();
        return true;
    }

    bool RuntimeScriptEntityApi::ResolveTransform(
        const RuntimeScriptEntityReference& reference,
        wi::scene::TransformComponent*& transform,
        std::string& error) const
    {
        transform = nullptr;
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        if (!Resolve(reference, entity, error))
            return false;

        transform = scene_->transforms.GetComponent(entity);
        if (transform == nullptr)
        {
            error = "EntityRef does not reference an entity with a Transform.";
            return false;
        }
        error.clear();
        return true;
    }

    bool RuntimeScriptEntityApi::GetLocalPosition(
        const RuntimeScriptEntityReference& reference,
        XMFLOAT3& position,
        std::string& error) const
    {
        wi::scene::TransformComponent* transform = nullptr;
        if (!ResolveTransform(reference, transform, error))
            return false;
        position = transform->translation_local;
        error.clear();
        return true;
    }

    bool RuntimeScriptEntityApi::SetLocalPosition(
        const RuntimeScriptEntityReference& reference,
        const XMFLOAT3& position,
        std::string& error) const
    {
        if (!IsFinite(position))
        {
            error = "Transform position must contain finite numbers.";
            return false;
        }

        wi::scene::TransformComponent* transform = nullptr;
        if (!ResolveTransform(reference, transform, error))
            return false;
        transform->translation_local = position;
        transform->SetDirty();
        error.clear();
        return true;
    }

    bool RuntimeScriptEntityApi::TranslateLocal(
        const RuntimeScriptEntityReference& reference,
        const XMFLOAT3& delta,
        std::string& error) const
    {
        if (!IsFinite(delta))
        {
            error = "Transform delta must contain finite numbers.";
            return false;
        }

        wi::scene::TransformComponent* transform = nullptr;
        if (!ResolveTransform(reference, transform, error))
            return false;
        const XMFLOAT3 translated(
            transform->translation_local.x + delta.x,
            transform->translation_local.y + delta.y,
            transform->translation_local.z + delta.z);
        if (!IsFinite(translated))
        {
            error = "Transform translation overflowed the finite runtime range.";
            return false;
        }
        transform->translation_local = translated;
        transform->SetDirty();
        error.clear();
        return true;
    }

    bool RuntimeScriptEntityApi::IsFinite(const XMFLOAT3& value) noexcept
    {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }
}
