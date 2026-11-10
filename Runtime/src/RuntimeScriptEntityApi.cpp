#include "RuntimeScriptEntityApi.h"

#include <cmath>
#include <algorithm>

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

    bool RuntimeScriptEntityApi::GetWorldPosition(
        const RuntimeScriptEntityReference& reference,
        XMFLOAT3& position,
        std::string& error) const
    {
        wi::scene::TransformComponent* transform = nullptr;
        if (!ResolveTransform(reference, transform, error))
            return false;
        position = transform->GetPosition();
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
        transform->UpdateTransform();
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
        transform->UpdateTransform();
        error.clear();
        return true;
    }

    bool RuntimeScriptEntityApi::GetLocalScale(
        const RuntimeScriptEntityReference& reference,
        XMFLOAT3& scale,
        std::string& error) const
    {
        wi::scene::TransformComponent* transform = nullptr;
        if (!ResolveTransform(reference, transform, error))
            return false;
        scale = transform->scale_local;
        error.clear();
        return true;
    }

    bool RuntimeScriptEntityApi::SetLocalScale(
        const RuntimeScriptEntityReference& reference,
        const XMFLOAT3& scale,
        std::string& error) const
    {
        if (!IsFinite(scale))
        {
            error = "Transform scale must contain finite numbers.";
            return false;
        }

        wi::scene::TransformComponent* transform = nullptr;
        if (!ResolveTransform(reference, transform, error))
            return false;
        transform->scale_local = scale;
        transform->SetDirty();
        transform->UpdateTransform();
        error.clear();
        return true;
    }


    bool RuntimeScriptEntityApi::GetLightState(const RuntimeScriptEntityReference& ref,RuntimeLightAnimationState& s,std::string& error) const {
        wi::ecs::Entity e;if(!Resolve(ref,e,error))return false;
        const auto* l=scene_->lights.GetComponent(e);const auto* t=scene_->transforms.GetComponent(e);
        if(!l||!t){error="EntityRef does not reference a Light with a Transform.";return false;}
        s.intensity=l->intensity;s.r=l->color.x;s.g=l->color.y;s.b=l->color.z;
        s.x=t->translation_local.x;s.y=t->translation_local.y;s.z=t->translation_local.z;
        s.qx=t->rotation_local.x;s.qy=t->rotation_local.y;s.qz=t->rotation_local.z;s.qw=t->rotation_local.w;
        uint32_t hash=2166136261u;for(unsigned char c:ref.stableId){hash^=c;hash*=16777619u;}
        s.seed=float(hash&0xffffffu)/16777216.0f;error.clear();return true;
    }
    bool RuntimeScriptEntityApi::SetLightState(const RuntimeScriptEntityReference& ref,const RuntimeLightAnimationState& s,std::string& error) const {
        for(float v:{s.intensity,s.r,s.g,s.b,s.x,s.y,s.z,s.qx,s.qy,s.qz,s.qw})
            if(!std::isfinite(v)){error="Light animation values must be finite.";return false;}
        const float norm=s.qx*s.qx+s.qy*s.qy+s.qz*s.qz+s.qw*s.qw;
        if(!std::isfinite(norm)||norm<0.000001f){error="Light rotation must be a finite nonzero quaternion.";return false;}
        wi::ecs::Entity e;if(!Resolve(ref,e,error))return false;
        auto* l=scene_->lights.GetComponent(e);auto* t=scene_->transforms.GetComponent(e);
        if(!l||!t){error="EntityRef does not reference a Light with a Transform.";return false;}
        l->intensity=std::clamp(s.intensity,0.0f,100000.0f);
        l->color={std::clamp(s.r,0.0f,1.0f),std::clamp(s.g,0.0f,1.0f),std::clamp(s.b,0.0f,1.0f)};
        t->translation_local={s.x,s.y,s.z};const float inv=1/std::sqrt(norm);
        t->rotation_local={s.qx*inv,s.qy*inv,s.qz*inv,s.qw*inv};t->SetDirty();t->UpdateTransform();
        error.clear();return true;
    }

    bool RuntimeScriptEntityApi::IsFinite(const XMFLOAT3& value) noexcept
    {
        return std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }
}
