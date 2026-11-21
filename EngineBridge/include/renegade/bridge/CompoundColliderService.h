#pragma once

#include "CommandService.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

namespace renegade::bridge
{
    enum class CompoundPartShape { Box = 1, Sphere, Capsule };

    inline constexpr size_t MaxCompoundParts = 8;
    inline constexpr char CompoundPartKey[] = "renegade.physics.compound_part";
    inline constexpr char CompoundColliderKey[] = "renegade.physics.compound_collider";

    struct CompoundPart
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        CompoundPartShape shape = CompoundPartShape::Box;
        XMFLOAT3 position{};
        XMFLOAT4 orientation{0.f, 0.f, 0.f, 1.f};
        XMFLOAT3 halfExtents{.5f, .5f, .5f};
    };

    struct CompoundParts
    {
        std::array<CompoundPart, MaxCompoundParts> parts;
        size_t count = 0;
        bool valid = true;
    };

    inline bool SupportsCompoundCollider(const wi::scene::RigidBodyPhysicsComponent& body)
    {
        return !body.IsVehicle() && !body.IsCharacterPhysics();
    }

    inline bool HasCompoundCollider(const wi::scene::Scene& scene, wi::ecs::Entity root)
    {
        const auto* metadata = scene.metadatas.GetComponent(root);
        return metadata && metadata->int_values.get(CompoundColliderKey) == 1;
    }

    inline int CompoundPartKind(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata ? metadata->int_values.get(CompoundPartKey) : 0;
    }

    inline wi::ecs::Entity GetCompoundPartParent(const wi::scene::Scene& scene, wi::ecs::Entity part)
    {
        const auto* hierarchy = scene.hierarchy.GetComponent(part);
        return hierarchy && CompoundPartKind(scene, part) ? hierarchy->parentID : wi::ecs::INVALID_ENTITY;
    }

    // Read only this collider's children, from the current native hierarchy index.
    inline CompoundParts GetCompoundParts(const wi::scene::Scene& scene, wi::ecs::Entity root)
    {
        CompoundParts result;
        if (!HasCompoundCollider(scene, root))
            return result;
        scene.WaitBuildTopDownHierarchy();
        const auto children = scene.topdown_hierarchy.find(root);
        if (children == scene.topdown_hierarchy.end())
            return result;
        for (const auto entity : children->second)
        {
            const int kind = CompoundPartKind(scene, entity);
            if (kind == 0)
                continue;
            const auto* transform = scene.transforms.GetComponent(entity);
            if (!transform || scene.rigidbodies.Contains(entity) ||
                kind < int(CompoundPartShape::Box) || kind > int(CompoundPartShape::Capsule) ||
                result.count == MaxCompoundParts)
            {
                result.valid = false;
                break;
            }

            const auto position = transform->translation_local;
            const auto scale = transform->scale_local;
            const auto rotation = XMLoadFloat4(&transform->rotation_local);
            const float length = XMVectorGetX(XMVector4LengthSq(rotation));
            const float values[]{position.x, position.y, position.z, scale.x, scale.y, scale.z, length};
            if (!std::all_of(std::begin(values), std::end(values), [](float value) { return std::isfinite(value); }) ||
                length < 0.000001f)
            {
                result.valid = false;
                break;
            }
            auto& part = result.parts[result.count++];
            part.entity = entity;
            part.shape = static_cast<CompoundPartShape>(kind);
            part.position = position;
            XMStoreFloat4(&part.orientation, XMVector4Normalize(rotation));
            const auto dimension = [](float value) { return std::clamp(std::abs(value), .015f, 100.f); };
            part.halfExtents = {dimension(scale.x), dimension(scale.y), dimension(scale.z)};
            if (part.shape == CompoundPartShape::Sphere)
            {
                const float radius = std::max({part.halfExtents.x, part.halfExtents.y, part.halfExtents.z});
                part.halfExtents = {radius, radius, radius};
            }
            if (part.shape == CompoundPartShape::Capsule)
                part.halfExtents.x = part.halfExtents.z = std::max(part.halfExtents.x, part.halfExtents.z);
        }
        return result;
    }

    class AddCompoundPartCommand final : public ICommand
    {
    public:
        AddCompoundPartCommand(wi::scene::Scene& scene, wi::ecs::Entity root, CompoundPartShape shape)
            : scene_(scene), root_(root), part_(wi::ecs::CreateEntity()), shape_(shape),
              previouslyMarked_(HasCompoundCollider(scene, root))
        {}

        bool Execute() override
        {
            const auto* body = scene_.rigidbodies.GetComponent(root_);
            if (!body || !SupportsCompoundCollider(*body) || !scene_.transforms.Contains(root_) ||
                scene_.transforms.Contains(part_) || shape_ < CompoundPartShape::Box || shape_ > CompoundPartShape::Capsule)
                return false;

            size_t count = 0;
            for (size_t i = 0; i < scene_.hierarchy.GetCount(); ++i)
            {
                if (scene_.hierarchy[i].parentID == root_ &&
                    CompoundPartKind(scene_, scene_.hierarchy.GetEntity(i)) && ++count >= MaxCompoundParts)
                    return false;
            }

            scene_.names.Create(part_).name = shape_ == CompoundPartShape::Box ? "Compound Box" :
                shape_ == CompoundPartShape::Sphere ? "Compound Sphere" : "Compound Capsule";
            auto& transform = scene_.transforms.Create(part_);
            transform.translation_local = {static_cast<float>(count) * .75f, 0.f, 0.f};
            transform.scale_local = {.5f, .5f, .5f};
            transform.SetDirty();
            scene_.Component_Attach(part_, root_, true);
            scene_.metadatas.Create(part_).int_values.set(CompoundPartKey, static_cast<int>(shape_));
            SetRootMarker(true);
            return true;
        }

        void Undo() override
        {
            if (GetCompoundPartParent(scene_, part_) == root_)
                scene_.Entity_Remove(part_);
            if (scene_.transforms.Contains(root_))
                SetRootMarker(previouslyMarked_);
        }

        wi::ecs::Entity PartEntity() const { return part_; }

    private:
        void SetRootMarker(bool enabled)
        {
            auto* metadata = scene_.metadatas.GetComponent(root_);
            if (!metadata)
                metadata = &scene_.metadatas.Create(root_);
            metadata->int_values.set(CompoundColliderKey, enabled ? 1 : 0);
        }

        wi::scene::Scene& scene_;
        wi::ecs::Entity root_;
        wi::ecs::Entity part_;
        CompoundPartShape shape_;
        bool previouslyMarked_;
    };
}
