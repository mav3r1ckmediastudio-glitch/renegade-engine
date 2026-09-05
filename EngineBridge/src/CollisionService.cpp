#include "renegade/bridge/CollisionService.h"

#include "renegade/bridge/ReusableAssetInstanceService.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr float Epsilon = 0.00001f;
    constexpr float MinimumDimension = 0.01f;
    constexpr uint32_t MaximumEditorMeshLod = 6;

    bool NearlyEqual(const float left, const float right) noexcept
    {
        return std::abs(left - right) <= Epsilon;
    }

    bool NearlyEqual(
        const XMFLOAT3& left,
        const XMFLOAT3& right) noexcept
    {
        return NearlyEqual(left.x, right.x) &&
            NearlyEqual(left.y, right.y) &&
            NearlyEqual(left.z, right.z);
    }

    bool IsSupportedShape(
        const wi::scene::RigidBodyPhysicsComponent::CollisionShape shape) noexcept
    {
        using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
        switch (shape)
        {
        case Shape::BOX:
        case Shape::SPHERE:
        case Shape::CAPSULE:
        case Shape::CYLINDER:
        case Shape::CONVEX_HULL:
        case Shape::TRIANGLE_MESH:
        case Shape::HEIGHTFIELD:
            return true;
        default:
            return false;
        }
    }

    bool IsMeshDerivedShape(
        const wi::scene::RigidBodyPhysicsComponent::CollisionShape shape) noexcept
    {
        using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
        return shape == Shape::CONVEX_HULL ||
            shape == Shape::TRIANGLE_MESH ||
            shape == Shape::HEIGHTFIELD;
    }

    bool RequiresPhysicsObjectRecreation(
        const renegade::bridge::CollisionState& before,
        const renegade::bridge::CollisionState& after) noexcept
    {
        using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
        if (before.shape != after.shape)
            return true;

        switch (after.shape)
        {
        case Shape::BOX:
            return !NearlyEqual(before.boxHalfExtents, after.boxHalfExtents);
        case Shape::SPHERE:
            return !NearlyEqual(before.sphereRadius, after.sphereRadius);
        case Shape::CAPSULE:
        case Shape::CYLINDER:
            return !NearlyEqual(before.capsuleRadius, after.capsuleRadius) ||
                !NearlyEqual(before.capsuleHeight, after.capsuleHeight);
        case Shape::TRIANGLE_MESH:
            return before.meshLod != after.meshLod;
        case Shape::CONVEX_HULL:
        case Shape::HEIGHTFIELD:
        default:
            return false;
        }
    }

    bool IsReusableAssetWrapper(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        if (entity == wi::ecs::INVALID_ENTITY ||
            !scene.transforms.Contains(entity))
        {
            return false;
        }
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr &&
            metadata->string_values.has(
                renegade::bridge::ReusableAssetInstanceIdMetadataKey);
    }

    bool RelativeMatrixToTarget(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const wi::ecs::Entity target,
        XMMATRIX& relative) noexcept
    {
        relative = XMMatrixIdentity();
        if (entity == target)
            return scene.transforms.Contains(entity);

        wi::ecs::Entity current = entity;
        const std::size_t maximumDepth = scene.hierarchy.GetCount() + 1;
        for (std::size_t depth = 0;
            current != wi::ecs::INVALID_ENTITY && depth <= maximumDepth; ++depth)
        {
            if (current == target)
                return true;

            const auto* transform = scene.transforms.GetComponent(current);
            if (transform == nullptr)
                return false;
            relative = relative * transform->GetLocalMatrix();

            const auto* hierarchy = scene.hierarchy.GetComponent(current);
            if (hierarchy == nullptr ||
                hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                hierarchy->parentID == current)
            {
                return false;
            }
            current = hierarchy->parentID;
        }
        return false;
    }

    bool MeasureTargetGeometryBounds(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity target,
        XMFLOAT3& minimum,
        XMFLOAT3& maximum) noexcept
    {
        if (target == wi::ecs::INVALID_ENTITY ||
            !scene.transforms.Contains(target))
        {
            return false;
        }

        const float highest = (std::numeric_limits<float>::max)();
        minimum = XMFLOAT3(highest, highest, highest);
        maximum = XMFLOAT3(-highest, -highest, -highest);
        bool found = false;

        for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
        {
            const wi::ecs::Entity objectEntity = scene.objects.GetEntity(index);
            if (objectEntity != target &&
                !scene.Entity_IsDescendant(objectEntity, target))
            {
                continue;
            }

            const auto& object = scene.objects[index];
            if (object.meshID == wi::ecs::INVALID_ENTITY)
                continue;
            const auto* mesh = scene.meshes.GetComponent(object.meshID);
            if (mesh == nullptr || mesh->vertex_positions.empty())
                continue;

            XMMATRIX localToTarget;
            if (!RelativeMatrixToTarget(
                    scene, objectEntity, target, localToTarget))
            {
                continue;
            }

            for (const auto& vertex : mesh->vertex_positions)
            {
                XMFLOAT3 point;
                XMStoreFloat3(
                    &point,
                    XMVector3TransformCoord(
                        XMLoadFloat3(&vertex),
                        localToTarget));
                minimum.x = std::min(minimum.x, point.x);
                minimum.y = std::min(minimum.y, point.y);
                minimum.z = std::min(minimum.z, point.z);
                maximum.x = std::max(maximum.x, point.x);
                maximum.y = std::max(maximum.y, point.y);
                maximum.z = std::max(maximum.z, point.z);
                found = true;
            }
        }
        return found;
    }
}

namespace renegade::bridge
{
    wi::ecs::Entity ResolveCollisionAuthoringTarget(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selectedEntity) noexcept
    {
        if (selectedEntity == wi::ecs::INVALID_ENTITY)
            return wi::ecs::INVALID_ENTITY;

        // Walk upward so nested reusable assets resolve to their nearest stable
        // root rather than whichever metadata component happens to appear first.
        wi::ecs::Entity current = selectedEntity;
        const std::size_t maximumDepth = scene.hierarchy.GetCount() + 1;
        for (std::size_t depth = 0;
            current != wi::ecs::INVALID_ENTITY && depth <= maximumDepth; ++depth)
        {
            if (IsReusableAssetWrapper(scene, current))
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
        return selectedEntity;
    }

    bool FitPrimitiveCollisionStateToTarget(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        CollisionState& state) noexcept
    {
        using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
        if (IsMeshDerivedShape(state.shape) || !IsSupportedShape(state.shape))
            return false;

        const wi::ecs::Entity target =
            ResolveCollisionAuthoringTarget(scene, entity);
        XMFLOAT3 minimum;
        XMFLOAT3 maximum;
        if (!MeasureTargetGeometryBounds(scene, target, minimum, maximum))
            return false;

        // Keep the body centred on the stable transform origin. This avoids
        // relying on backend local-offset semantics when the asset is rotated;
        // extents are conservatively expanded to contain an off-centre pivot.
        const XMFLOAT3 half(
            std::max(std::abs(minimum.x), std::abs(maximum.x)),
            std::max(std::abs(minimum.y), std::abs(maximum.y)),
            std::max(std::abs(minimum.z), std::abs(maximum.z)));

        CollisionState fitted = state;
        switch (state.shape)
        {
        case Shape::BOX:
            fitted.boxHalfExtents = half;
            break;
        case Shape::SPHERE:
            fitted.sphereRadius = std::sqrt(
                half.x * half.x + half.y * half.y + half.z * half.z);
            break;
        case Shape::CAPSULE:
        {
            const float radius = std::max(half.x, half.z);
            fitted.capsuleRadius = radius;
            // Jolt's capsule setting stores half-height of the cylinder portion.
            fitted.capsuleHeight = std::max(
                MinimumDimension,
                half.y - radius);
            break;
        }
        case Shape::CYLINDER:
            fitted.capsuleRadius = std::max(half.x, half.z);
            // The shared height field maps to Jolt CylinderShapeSettings half-height.
            fitted.capsuleHeight = half.y;
            break;
        default:
            return false;
        }

        state = SanitizeCollisionState(fitted);
        return true;
    }

    bool RequestCollisionShapeRefresh(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const wi::ecs::Entity target =
            ResolveCollisionAuthoringTarget(scene, entity);
        auto* rigidbody = scene.rigidbodies.GetComponent(target);
        if (rigidbody == nullptr)
            return false;

        // Transform scale is consumed when the backend creates its native
        // collision shape. A refresh flag alone only updates live parameters;
        // destroy the implementation-owned body handle so the next physics
        // update recreates the shape from the current authored root scale.
        rigidbody->physicsobject.reset();
        rigidbody->SetRefreshParametersNeeded(true);
        return true;
    }

    ReusableAssetCollisionRepairResult RepairReusableAssetCollisionTargets(
        wi::scene::Scene& scene) noexcept
    {
        ReusableAssetCollisionRepairResult result;

        // The same load/save canonicalization boundary also repairs only old
        // implementation-generated wrapper labels. Creator-authored names are
        // preserved by RepairReusableAssetInstanceNames().
        (void)RepairReusableAssetInstanceNames(scene);

        for (std::size_t metadataIndex = 0;
            metadataIndex < scene.metadatas.GetCount(); ++metadataIndex)
        {
            const wi::ecs::Entity wrapper =
                scene.metadatas.GetEntity(metadataIndex);
            if (!IsReusableAssetWrapper(scene, wrapper))
                continue;

            wi::ecs::Entity nestedBody = wi::ecs::INVALID_ENTITY;
            std::size_t nestedBodyCount = 0;
            for (std::size_t bodyIndex = 0;
                bodyIndex < scene.rigidbodies.GetCount(); ++bodyIndex)
            {
                const wi::ecs::Entity bodyEntity =
                    scene.rigidbodies.GetEntity(bodyIndex);
                if (bodyEntity != wrapper &&
                    ResolveCollisionAuthoringTarget(scene, bodyEntity) == wrapper)
                {
                    nestedBody = bodyEntity;
                    ++nestedBodyCount;
                }
            }

            if (nestedBodyCount == 0)
                continue;

            if (scene.rigidbodies.Contains(wrapper) || nestedBodyCount != 1)
            {
                result.conflictCount += nestedBodyCount;
                continue;
            }

            const auto* nested = scene.rigidbodies.GetComponent(nestedBody);
            if (nested == nullptr)
            {
                ++result.conflictCount;
                continue;
            }

            wi::scene::RigidBodyPhysicsComponent migrated;
            migrated = *nested;
            migrated.physicsobject.reset();
            migrated.SetRefreshParametersNeeded(true);

            scene.rigidbodies.Remove(nestedBody);
            auto& rootBody = scene.rigidbodies.Create(wrapper);
            rootBody = migrated;
            rootBody.physicsobject.reset();
            rootBody.SetRefreshParametersNeeded(true);
            ++result.migratedBodyCount;
        }

        return result;
    }

    CollisionTargetStatus CheckCollisionTarget(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const wi::scene::RigidBodyPhysicsComponent::CollisionShape shape) noexcept
    {
        using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;

        if (entity == wi::ecs::INVALID_ENTITY)
            return CollisionTargetStatus::InvalidEntity;
        if (!scene.transforms.Contains(entity))
            return CollisionTargetStatus::MissingTransform;
        if (!IsSupportedShape(shape))
            return CollisionTargetStatus::InvalidMeshData;
        if (!IsMeshDerivedShape(shape))
            return CollisionTargetStatus::Supported;

        const auto* object = scene.objects.GetComponent(entity);
        if (object == nullptr || object->meshID == wi::ecs::INVALID_ENTITY)
            return CollisionTargetStatus::MissingMesh;
        const auto* mesh = scene.meshes.GetComponent(object->meshID);
        if (mesh == nullptr)
            return CollisionTargetStatus::MissingMesh;

        if (shape == Shape::CONVEX_HULL)
        {
            return mesh->vertex_positions.empty()
                ? CollisionTargetStatus::InvalidMeshData
                : CollisionTargetStatus::Supported;
        }

        if (shape == Shape::TRIANGLE_MESH)
        {
            if (mesh->vertex_positions.empty() ||
                mesh->indices.size() < 3 ||
                mesh->subsets.empty())
            {
                return CollisionTargetStatus::InvalidMeshData;
            }
            return CollisionTargetStatus::Supported;
        }

        const size_t vertexCount = mesh->vertex_positions.size();
        const size_t dimension = static_cast<size_t>(
            std::sqrt(static_cast<double>(vertexCount)));
        if (dimension < 2 || dimension * dimension != vertexCount)
            return CollisionTargetStatus::InvalidHeightFieldGrid;
        return CollisionTargetStatus::Supported;
    }

    bool CanAuthorCollisionShape(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const wi::scene::RigidBodyPhysicsComponent::CollisionShape shape) noexcept
    {
        return CheckCollisionTarget(scene, entity, shape) ==
            CollisionTargetStatus::Supported;
    }

    CollisionState CaptureCollision(
        const wi::scene::RigidBodyPhysicsComponent& rigidbody) noexcept
    {
        CollisionState state;
        state.shape = rigidbody.shape;
        state.mass = rigidbody.mass;
        state.friction = rigidbody.friction;
        state.restitution = rigidbody.restitution;
        state.dampingLinear = rigidbody.damping_linear;
        state.dampingAngular = rigidbody.damping_angular;
        state.buoyancy = rigidbody.buoyancy;
        state.localOffset = rigidbody.local_offset;
        state.meshLod = rigidbody.mesh_lod;
        state.kinematic = rigidbody.IsKinematic();
        state.locked2D = rigidbody.IsLocked2D();
        state.disableDeactivation = rigidbody.IsDisableDeactivation();
        state.startDeactivated = rigidbody.IsStartDeactivated();
        state.boxHalfExtents = rigidbody.box.halfextents;
        state.sphereRadius = rigidbody.sphere.radius;
        state.capsuleRadius = rigidbody.capsule.radius;
        state.capsuleHeight = rigidbody.capsule.height;
        return state;
    }

    CollisionState SanitizeCollisionState(const CollisionState& state) noexcept
    {
        CollisionState result = state;
        if (!IsSupportedShape(result.shape))
        {
            result.shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;
        }

        result.mass = std::max(result.mass, 0.0f);
        result.friction = std::clamp(result.friction, 0.0f, 1.0f);
        result.restitution = std::clamp(result.restitution, 0.0f, 1.0f);
        result.dampingLinear = std::clamp(result.dampingLinear, 0.0f, 1.0f);
        result.dampingAngular = std::clamp(result.dampingAngular, 0.0f, 1.0f);
        result.buoyancy = std::clamp(result.buoyancy, 0.0f, 2.0f);
        result.meshLod = std::min(result.meshLod, MaximumEditorMeshLod);

        result.boxHalfExtents.x = std::max(result.boxHalfExtents.x, MinimumDimension);
        result.boxHalfExtents.y = std::max(result.boxHalfExtents.y, MinimumDimension);
        result.boxHalfExtents.z = std::max(result.boxHalfExtents.z, MinimumDimension);
        result.sphereRadius = std::max(result.sphereRadius, MinimumDimension);
        result.capsuleRadius = std::max(result.capsuleRadius, MinimumDimension);
        result.capsuleHeight = std::max(result.capsuleHeight, MinimumDimension);
        return result;
    }

    bool HasCollisionStateChange(
        const CollisionState& before,
        const CollisionState& after) noexcept
    {
        const auto left = SanitizeCollisionState(before);
        const auto right = SanitizeCollisionState(after);
        return left.shape != right.shape ||
            !NearlyEqual(left.mass, right.mass) ||
            !NearlyEqual(left.friction, right.friction) ||
            !NearlyEqual(left.restitution, right.restitution) ||
            !NearlyEqual(left.dampingLinear, right.dampingLinear) ||
            !NearlyEqual(left.dampingAngular, right.dampingAngular) ||
            !NearlyEqual(left.buoyancy, right.buoyancy) ||
            !NearlyEqual(left.localOffset, right.localOffset) ||
            left.meshLod != right.meshLod ||
            left.kinematic != right.kinematic ||
            left.locked2D != right.locked2D ||
            left.disableDeactivation != right.disableDeactivation ||
            left.startDeactivated != right.startDeactivated ||
            !NearlyEqual(left.boxHalfExtents, right.boxHalfExtents) ||
            !NearlyEqual(left.sphereRadius, right.sphereRadius) ||
            !NearlyEqual(left.capsuleRadius, right.capsuleRadius) ||
            !NearlyEqual(left.capsuleHeight, right.capsuleHeight);
    }

    void ApplyCollision(
        wi::scene::RigidBodyPhysicsComponent& rigidbody,
        const CollisionState& state) noexcept
    {
        const auto before = CaptureCollision(rigidbody);
        const auto safe = SanitizeCollisionState(state);

        if (RequiresPhysicsObjectRecreation(before, safe))
            rigidbody.physicsobject.reset();

        rigidbody.shape = safe.shape;
        rigidbody.mass = safe.mass;
        rigidbody.friction = safe.friction;
        rigidbody.restitution = safe.restitution;
        rigidbody.damping_linear = safe.dampingLinear;
        rigidbody.damping_angular = safe.dampingAngular;
        rigidbody.buoyancy = safe.buoyancy;
        rigidbody.local_offset = safe.localOffset;
        rigidbody.mesh_lod = safe.meshLod;
        rigidbody.SetKinematic(safe.kinematic);
        rigidbody.SetLocked2D(safe.locked2D);
        rigidbody.SetDisableDeactivation(safe.disableDeactivation);
        rigidbody.SetStartDeactivated(safe.startDeactivated);
        rigidbody.box.halfextents = safe.boxHalfExtents;
        rigidbody.sphere.radius = safe.sphereRadius;
        rigidbody.capsule.radius = safe.capsuleRadius;
        rigidbody.capsule.height = safe.capsuleHeight;
        rigidbody.SetRefreshParametersNeeded(true);
    }

    CreateCollisionCommand::CreateCollisionCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity targetEntity,
        const CollisionState& initial,
        const bool autoFitPrimitive)
        : scene_(&scene)
        , entity_(ResolveCollisionAuthoringTarget(scene, targetEntity))
        , initial_(SanitizeCollisionState(initial))
        , autoFitPrimitive_(autoFitPrimitive)
    {
        if (autoFitPrimitive_)
        {
            CollisionState fitted = initial_;
            if (FitPrimitiveCollisionStateToTarget(scene, entity_, fitted))
                initial_ = SanitizeCollisionState(fitted);
        }
    }

    bool CreateCollisionCommand::Execute()
    {
        if (scene_ == nullptr ||
            scene_->rigidbodies.Contains(entity_) ||
            !CanAuthorCollisionShape(*scene_, entity_, initial_.shape))
        {
            return false;
        }
        auto& rigidbody = scene_->rigidbodies.Create(entity_);
        ApplyCollision(rigidbody, initial_);
        return true;
    }

    void CreateCollisionCommand::Undo()
    {
        if (scene_ != nullptr)
            scene_->rigidbodies.Remove(entity_);
    }

    SetCollisionCommand::SetCollisionCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const CollisionState& collision)
        : scene_(&scene)
        , entity_(ResolveCollisionAuthoringTarget(scene, entity))
        , after_(SanitizeCollisionState(collision))
    {
        if (const auto* existing = scene.rigidbodies.GetComponent(entity_))
            before_ = CaptureCollision(*existing);
    }

    SetCollisionCommand::SetCollisionCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const CollisionState& before,
        const CollisionState& after)
        : scene_(&scene)
        , entity_(ResolveCollisionAuthoringTarget(scene, entity))
        , before_(SanitizeCollisionState(before))
        , after_(SanitizeCollisionState(after))
    {
    }

    bool SetCollisionCommand::Execute()
    {
        return HasCollisionStateChange(before_, after_) && Apply(after_);
    }

    void SetCollisionCommand::Undo()
    {
        Apply(before_);
    }

    bool SetCollisionCommand::Apply(const CollisionState& state) noexcept
    {
        if (scene_ == nullptr ||
            !CanAuthorCollisionShape(*scene_, entity_, state.shape))
        {
            return false;
        }
        auto* rigidbody = scene_->rigidbodies.GetComponent(entity_);
        if (rigidbody == nullptr)
            return false;
        ApplyCollision(*rigidbody, state);
        return true;
    }

    RemoveCollisionCommand::RemoveCollisionCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
        : scene_(&scene)
        , entity_(ResolveCollisionAuthoringTarget(scene, entity))
    {
    }

    bool RemoveCollisionCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;
        const auto* existing = scene_->rigidbodies.GetComponent(entity_);
        if (existing == nullptr)
            return false;

        removedNative_ = *existing;
        removedNative_.physicsobject.reset();
        removedNative_.SetRefreshParametersNeeded(true);
        hasRemoved_ = true;
        scene_->rigidbodies.Remove(entity_);
        return true;
    }

    void RemoveCollisionCommand::Undo()
    {
        if (scene_ == nullptr ||
            !hasRemoved_ ||
            scene_->rigidbodies.Contains(entity_))
        {
            return;
        }

        auto& rigidbody = scene_->rigidbodies.Create(entity_);
        rigidbody = removedNative_;
        rigidbody.physicsobject.reset();
        rigidbody.SetRefreshParametersNeeded(true);
    }
}
