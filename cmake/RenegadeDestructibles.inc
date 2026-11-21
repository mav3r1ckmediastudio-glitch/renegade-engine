#include "../EngineBridge/include/renegade/bridge/DestructibleBlastPreset.h"

namespace wi::physics
{
    void GameplayContactListener::SetDestructible(wi::ecs::Entity entity, float threshold, float hitPoints)
    {
        std::scoped_lock lock(mutex_);
        if (threshold > 0.f || hitPoints > 0.f)
        {
            auto& target = destructibles_[entity];
            target.impactSpeed = threshold;
            if (hitPoints != target.hitPoints)
                target.remaining = target.hitPoints = hitPoints;
        }
        else
        {
            impacts_.erase(entity);
            rattles_.erase(entity);
            destructibles_.erase(entity);
        }
        has_destructibles_.store(!destructibles_.empty(), std::memory_order_relaxed);
    }

    void GameplayContactListener::ResetDestructibles()
    {
        std::scoped_lock lock(mutex_);
        destructibles_.clear();
        impacts_.clear();
        rattles_.clear();
        breaks_.clear();
        has_destructibles_.store(false, std::memory_order_relaxed);
    }

    bool GameplayContactListener::NeedsListener() const
    {
        return enabled_.load(std::memory_order_relaxed) || has_destructibles_.load(std::memory_order_relaxed);
    }

    bool GameplayContactListener::RequestBreak(wi::ecs::Entity entity)
    {
        std::scoped_lock lock(mutex_);
        if (destructibles_.find(entity) == destructibles_.end())
            return false;
        impacts_.try_emplace(entity, DestructibleImpact{entity, INVALID_ENTITY, 0.f, JPH::Vec3::sZero()});
        return true;
    }

    bool GameplayContactListener::DamageDestructible(wi::ecs::Entity entity, wi::ecs::Entity impactor,
        float damage, JPH::Vec3Arg direction)
    {
        std::scoped_lock lock(mutex_);
        const auto target = destructibles_.find(entity);
        if (target == destructibles_.end() || target->second.remaining <= 0.f || impacts_.count(entity) != 0)
            return false;
        auto& remaining = target->second.remaining;
        remaining = std::max(remaining - damage, 0.f);
        if (remaining == 0.f)
        {
            rattles_.erase(entity);
            impacts_.emplace(entity, DestructibleImpact{entity, impactor, 0.f, direction});
        }
        return true;
    }

    bool GameplayContactListener::PopImpact(DestructibleImpact& impact, bool rattle)
    {
        if (!has_destructibles_.load(std::memory_order_relaxed))
            return false;
        std::scoped_lock lock(mutex_);
        auto& pending = rattle ? rattles_ : impacts_;
        if (pending.empty())
            return false;
        const auto found = pending.begin();
        impact = found->second;
        pending.erase(found);
        return true;
    }

    void GameplayContactListener::RecordImpact(const JPH::Body& a, const JPH::Body& b,
        const JPH::ContactManifold& manifold)
    {
        if (!has_destructibles_.load(std::memory_order_relaxed) || a.IsSensor() || b.IsSensor())
            return;
        const auto entityA = terrain_props::EntityOf(a);
        const auto entityB = terrain_props::EntityOf(b);
        std::scoped_lock lock(mutex_);
        const auto targetA = destructibles_.find(entityA);
        const auto targetB = destructibles_.find(entityB);
        if ((targetA == destructibles_.end() && targetB == destructibles_.end()) ||
            manifold.mRelativeContactPointsOn1.empty())
            return;

        const auto pointA = manifold.GetWorldSpaceContactPointOn1(0);
        const auto pointB = manifold.GetWorldSpaceContactPointOn2(0);
        const float speed = (a.GetPointVelocity(pointA) - b.GetPointVelocity(pointB)).Dot(manifold.mWorldSpaceNormal);
        const auto queue = [&](const auto& target, wi::ecs::Entity impactor, JPH::Vec3Arg direction)
        {
            constexpr float RattleMinSpeed = 0.5f;
            if (target == destructibles_.end() || target->second.impactSpeed <= 0.f ||
                !(speed > 0.f) || !std::isfinite(speed))
                return;
            const bool breaks = speed >= target->second.impactSpeed;
            if (breaks)
                rattles_.erase(target->first);
            else if (speed < RattleMinSpeed || impacts_.find(target->first) != impacts_.end())
                return;
            auto& pending = breaks ? impacts_ : rattles_;
            const auto previous = pending.find(target->first);
            if (previous == pending.end() || (previous->second.speed > 0.f && previous->second.speed < speed))
                pending.insert_or_assign(target->first, DestructibleImpact{target->first, impactor, speed, direction});
        };
        queue(targetA, entityB, -manifold.mWorldSpaceNormal);
        queue(targetB, entityA, manifold.mWorldSpaceNormal);
    }

    std::vector<renegade::bridge::DestructibleBreakEvent> GameplayContactListener::DrainBreaks()
    {
        return std::exchange(breaks_, {});
    }

    void GameplayContactListener::ReportBreak(const renegade::bridge::DestructibleBreakEvent& event)
    {
        if (breaks_.size() < 1024)
            breaks_.push_back(event);
    }
}

namespace renegade::bridge
{
    namespace
    {
        constexpr float MaxBreakSpeed = 100.f;
        constexpr float MaxBreakHitPoints = 10000.f;
        constexpr size_t MaxFragments = 16;
        constexpr float RattleSeconds = .25f;

        float AuthoredThreshold(const wi::scene::Scene& scene, wi::ecs::Entity entity)
        {
            const auto* body = scene.rigidbodies.GetComponent(entity);
            const float speed = GetDestructibleImpactSpeed(scene, entity);
            return body && SupportsDestructible(*body) && !IsBodySensor(scene, entity) &&
                std::isfinite(speed) && speed > 0.f && speed <= MaxBreakSpeed ? speed : 0.f;
        }

        float AuthoredHitPoints(const wi::scene::Scene& scene, wi::ecs::Entity entity)
        {
            const auto* body = scene.rigidbodies.GetComponent(entity);
            const float hitPoints = GetDestructibleHitPoints(scene, entity);
            return body && SupportsDestructible(*body) && !IsBodySensor(scene, entity) &&
                std::isfinite(hitPoints) && hitPoints > 0.f && hitPoints <= MaxBreakHitPoints ? hitPoints : 0.f;
        }

        void RefreshContactListener(wi::physics::PhysicsScene& native)
        {
            native.physics_system.SetContactListener(native.gameplay_contacts.NeedsListener() ? &native.gameplay_contacts : nullptr);
        }

        // Imported models may have transform groups between their placed asset
        // root and render objects, plus mesh/material resource-only entities.
        struct DestructibleGeometry
        {
            wi::ecs::Entity intactVisual = wi::ecs::INVALID_ENTITY;
            std::vector<wi::ecs::Entity> pieces;
        };

        DestructibleGeometry CollectDestructibleGeometry(const wi::scene::Scene& scene, wi::ecs::Entity root,
            size_t maxPieces = MaxFragments)
        {
            DestructibleGeometry geometry;
            if (!scene.transforms.Contains(root) || scene.objects.Contains(root))
                return geometry;
            for (size_t i = 0; i < scene.objects.GetCount(); ++i)
            {
                const auto entity = scene.objects.GetEntity(i);
                auto parent = entity;
                bool childOfRoot = false;
                bool carriesBody = false;
                for (size_t depth = 0; depth < scene.hierarchy.GetCount(); ++depth)
                {
                    carriesBody = carriesBody || scene.rigidbodies.Contains(parent);
                    const auto* node = scene.hierarchy.GetComponent(parent);
                    if (!node)
                        break;
                    parent = node->parentID;
                    if (parent == root)
                    {
                        childOfRoot = true;
                        break;
                    }
                }
                if (!childOfRoot)
                    continue;
                const auto* mesh = scene.meshes.GetComponent(scene.objects[i].meshID);
                if (carriesBody || !mesh || mesh->vertex_positions.empty() || mesh->IsSkinned() ||
                    !scene.transforms.Contains(entity))
                    return {};
                if (IsDestructibleIntactVisual(scene, entity))
                {
                    if (geometry.intactVisual != wi::ecs::INVALID_ENTITY)
                        return {};
                    geometry.intactVisual = entity;
                }
                else
                {
                    if (geometry.pieces.size() == maxPieces)
                        return {};
                    geometry.pieces.push_back(entity);
                }
            }
            return geometry;
        }

        bool HasDestructibleConstraint(const wi::scene::Scene& scene, wi::ecs::Entity root)
        {
            if (scene.constraints.Contains(root))
                return true;
            for (size_t i = 0; i < scene.constraints.GetCount(); ++i)
            {
                const auto& constraint = scene.constraints[i];
                if (constraint.bodyA == root || constraint.bodyB == root)
                    return true;
            }
            return false;
        }

        struct DestructibleFragment
        {
            wi::ecs::Entity entity;
            XMFLOAT3 half;
            XMFLOAT3 offset;
            XMFLOAT4X4 world;
            const wi::scene::MeshComponent* collisionMesh = nullptr;
        };

        bool DescribeDestructibleFragment(const wi::scene::Scene& scene, wi::ecs::Entity entity,
            const wi::scene::MeshComponent& mesh, DestructibleFragment& piece)
        {
            using namespace DirectX;
            const auto* transform = scene.transforms.GetComponent(entity);
            const auto low = XMLoadFloat3(&mesh.aabb._min);
            const auto high = XMLoadFloat3(&mesh.aabb._max);
            const XMMATRIX world = transform->GetLocalMatrix() * scene.ComputeParentMatrixRecursive(entity);
            XMVECTOR scale, rotation, translation;
            if (!XMMatrixDecompose(&scale, &rotation, &translation, world))
                return false;
            XMStoreFloat3(&piece.half, (high - low) * .49f);
            XMStoreFloat3(&piece.offset, (high + low) * .5f);
            XMStoreFloat4x4(&piece.world, world);
            const XMVECTOR extents = XMVectorAbs(scale) * XMLoadFloat3(&piece.half);
            return !XMVector3IsNaN(extents) && !XMVector3IsInfinite(extents) &&
                XMVector3GreaterOrEqual(extents, XMVectorReplicate(.01f));
        }

        void SetIntactVisualRole(wi::scene::Scene& scene, wi::ecs::Entity entity, bool intact)
        {
            if (IsDestructibleIntactVisual(scene, entity) == intact)
                return;
            auto* metadata = scene.metadatas.GetComponent(entity);
            if (!metadata)
                metadata = &scene.metadatas.Create(entity);
            metadata->int_values.set(DestructibleIntactVisualKey, intact ? 1 : 0);
        }

        void AgeDebris(wi::scene::Scene& scene, std::vector<wi::physics::DestructibleDebris>& debris, float dt)
        {
            if (dt <= 0.f)
                return;
            for (size_t i = 0; i < debris.size();)
            {
                auto& piece = debris[i];
                auto* object = scene.objects.GetComponent(piece.entity);
                piece.stayRemaining -= dt;
                if (!object || piece.stayRemaining <= -piece.fadeSeconds)
                {
                    if (object)
                        scene.Entity_Remove(piece.entity);
                    else
                        scene.rigidbodies.Remove(piece.entity);
                    piece = debris.back();
                    debris.pop_back();
                    continue;
                }
                if (piece.stayRemaining < 0.f)
                    object->color.w = piece.baseOpacity * (1.f + piece.stayRemaining / piece.fadeSeconds);
                ++i;
            }
        }

        void PoseRattle(wi::scene::Scene& scene, wi::physics::DestructibleRattle& rattle, float amount)
        {
            for (auto& piece : rattle.pieces)
            {
                auto* transform = scene.transforms.GetComponent(piece.entity);
                if (!transform)
                    continue;
                const auto& position = transform->translation_local;
                if (position.x != piece.displayed.x || position.y != piece.displayed.y || position.z != piece.displayed.z)
                    piece.rest = position;
                piece.displayed = {
                    piece.rest.x + piece.offset.x * amount,
                    piece.rest.y + piece.offset.y * amount,
                    piece.rest.z + piece.offset.z * amount};
                transform->translation_local = piece.displayed;
                transform->SetDirty();
            }
        }

        void StartRattle(wi::scene::Scene& scene, wi::physics::PhysicsScene& native,
            const wi::physics::DestructibleImpact& impact)
        {
            using namespace DirectX;
            auto& rattles = native.destructible_rattles;
            if (auto previous = rattles.find(impact.target); previous != rattles.end())
            {
                PoseRattle(scene, previous->second, 0.f);
                rattles.erase(previous);
            }

            wi::physics::DestructibleRattle rattle{};
            auto geometry = CollectDestructibleGeometry(scene, impact.target);
            if (geometry.pieces.size() < 2)
                return;
            if (geometry.intactVisual != wi::ecs::INVALID_ENTITY)
                geometry.pieces = {geometry.intactVisual};
            const auto direction = wi::physics::cast(impact.direction);
            const float amplitude = std::clamp(impact.speed * .022f, .012f, .065f);
            for (const auto entity : geometry.pieces)
            {
                const auto& transform = *scene.transforms.GetComponent(entity);
                const auto parent = scene.ComputeParentMatrixRecursive(entity);
                const auto local = XMVector3TransformNormal(XMLoadFloat3(&direction) * amplitude, XMMatrixInverse(nullptr, parent));
                if (XMVector3IsNaN(local) || XMVector3IsInfinite(local))
                    return;
                wi::physics::DestructibleRattle::Piece piece{entity, transform.translation_local, transform.translation_local, {}};
                XMStoreFloat3(&piece.offset, local);
                rattle.pieces.push_back(piece);
            }
            PoseRattle(scene, rattle, 1.f);
            rattles.emplace(impact.target, std::move(rattle));
        }

        uint32_t BreakDestructible(wi::scene::Scene& scene, wi::physics::PhysicsScene& physics,
            const wi::physics::DestructibleImpact& impact, std::vector<XMFLOAT3>* blastPositions)
        {
            using namespace DirectX;
            using namespace wi::physics;
            const auto geometry = CollectDestructibleGeometry(scene, impact.target);
            const auto* root = scene.transforms.GetComponent(impact.target);
            if (geometry.pieces.size() < 2 || !root || HasDestructibleConstraint(scene, impact.target))
                return 0;
            const bool hasIntactVisual = geometry.intactVisual != wi::ecs::INVALID_ENTITY;
            std::vector<DestructibleFragment> pieces;
            pieces.reserve(geometry.pieces.size());
            for (const auto entity : geometry.pieces)
            {
                const auto* object = scene.objects.GetComponent(entity);
                const auto* mesh = scene.meshes.GetComponent(object->meshID);
                DestructibleFragment piece{entity};
                if (!DescribeDestructibleFragment(scene, entity, *mesh, piece))
                    return 0;
                if (hasIntactVisual)
                {
                    // Keep a box fallback for fragments whose points cannot form a volume.
                    JPH::Array<JPH::Vec3> points;
                    points.reserve(mesh->vertex_positions.size());
                    for (const auto& position : mesh->vertex_positions)
                        points.emplace_back(position.x, position.y, position.z);
                    JPH::ConvexHullShapeSettings hull(points);
                    if (hull.Create().IsValid())
                        piece.collisionMesh = mesh;
                }
                pieces.push_back(piece);
            }
            const XMFLOAT3 center = root->GetPosition();
            float blastSpeed = GetDestructibleBlastSpeed(scene, impact.target);
            if (!std::isfinite(blastSpeed) || blastSpeed < 0.f || blastSpeed > MaxDestructibleBlastSpeed)
                blastSpeed = 0.f;
            XMFLOAT3 blastCenter = center;
            if (blastSpeed > 0.f)
            {
                const auto& offset = scene.rigidbodies.GetComponent(impact.target)->local_offset;
                XMStoreFloat3(&blastCenter, XMVector3TransformCoord(XMLoadFloat3(&offset),
                    root->GetLocalMatrix() * scene.ComputeParentMatrixRecursive(impact.target)));
            }

            const auto debrisTiming = GetDestructibleDebrisTiming(scene, impact.target);

            // Replace the intact collider and visual only after validating every fragment.
            scene.rigidbodies.Remove(impact.target);
            if (hasIntactVisual)
                scene.objects.GetComponent(geometry.intactVisual)->SetRenderable(false);
            for (const auto& piece : pieces)
            {
                auto& object = *scene.objects.GetComponent(piece.entity);
                if (hasIntactVisual)
                    object.SetRenderable(true);
                if (debrisTiming.staySeconds > 0.f)
                    physics.destructible_debris.push_back({piece.entity, debrisTiming.staySeconds,
                        debrisTiming.fadeSeconds, object.color.w});
                auto& transform = *scene.transforms.GetComponent(piece.entity);
                transform.world = piece.world;
                scene.Component_Detach(piece.entity);

                auto& body = scene.rigidbodies.Create(piece.entity);
                if (piece.collisionMesh)
                {
                    body.shape = wi::scene::RigidBodyPhysicsComponent::CONVEX_HULL;
                }
                else
                {
                    body.shape = wi::scene::RigidBodyPhysicsComponent::BOX;
                    body.box.halfextents = piece.half;
                    body.local_offset = piece.offset;
                }
                body.mass = 2.f;
                body.friction = .65f;
                AddRigidBody(scene, piece.entity, body, transform, piece.collisionMesh);

                const XMFLOAT3 position = transform.GetPosition();
                const auto away = JPH::Vec3(position.x - center.x, 0.f, position.z - center.z);
                auto& native = GetRigidBody(body);
                if (!native.bodyID.IsInvalid())
                {
                    const float impactVelocity = hasIntactVisual ? std::min(impact.speed * .22f, 1.5f)
                        : std::min(impact.speed * .45f, 6.f);
                    const float spread = hasIntactVisual ? .85f : 2.5f;
                    const float lift = hasIntactVisual ? .45f : 1.1f;
                    auto velocity = impact.direction * impactVelocity +
                        away.NormalizedOr(JPH::Vec3::sZero()) * spread + JPH::Vec3(0.f, lift, 0.f);
                    if (blastSpeed > 0.f)
                    {
                        XMFLOAT3 pieceCenter;
                        XMStoreFloat3(&pieceCenter, XMVector3TransformCoord(XMLoadFloat3(&piece.offset),
                            XMLoadFloat4x4(&piece.world)));
                        // Send the lid up and the sides out without driving lower fragments into the floor.
                        const JPH::Vec3 radial(pieceCenter.x - blastCenter.x,
                            std::max(pieceCenter.y - blastCenter.y, .10f), pieceCenter.z - blastCenter.z);
                        velocity = radial.Normalized() * blastSpeed + JPH::Vec3(0.f, blastSpeed * .18f, 0.f);
                    }
                    wi::physics::SetLinearVelocity(body, cast(velocity));
                }
            }
            if (blastSpeed > 0.f && blastPositions)
                blastPositions->push_back(blastCenter);
            return static_cast<uint32_t>(pieces.size());
        }
    }

    wi::ecs::Entity ResolveAuthoredDestructibleRoot(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        for (size_t depth = 0; entity != wi::ecs::INVALID_ENTITY && depth <= scene.hierarchy.GetCount(); ++depth)
        {
            if (scene.rigidbodies.Contains(entity))
                return (AuthoredThreshold(scene, entity) > 0.f || AuthoredHitPoints(scene, entity) > 0.f)
                    ? entity : wi::ecs::INVALID_ENTITY;
            const auto* parent = scene.hierarchy.GetComponent(entity);
            entity = parent ? parent->parentID : wi::ecs::INVALID_ENTITY;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    bool CanUseDestructibleBlastPreset(const wi::scene::Scene& scene, wi::ecs::Entity root)
    {
        if (root == wi::ecs::INVALID_ENTITY || ResolveAuthoredDestructibleRoot(scene, root) != root ||
            HasDestructibleConstraint(scene, root))
            return false;
        const auto geometry = CollectDestructibleGeometry(scene, root);
        if (geometry.pieces.size() < 2)
            return false;
        const bool hasIntactVisual = geometry.intactVisual != wi::ecs::INVALID_ENTITY;
        if (hasIntactVisual && !scene.objects.GetComponent(geometry.intactVisual)->IsRenderable())
            return false;
        for (const auto entity : geometry.pieces)
        {
            const auto* object = scene.objects.GetComponent(entity);
            const auto* mesh = scene.meshes.GetComponent(object->meshID);
            DestructibleFragment piece{entity};
            if (object->IsRenderable() == hasIntactVisual ||
                !DescribeDestructibleFragment(scene, entity, *mesh, piece))
                return false;
        }
        return true;
    }

    bool SetDestructibleBlastPresetCommand::Execute()
    {
        return (!enabled_ || CanUseDestructibleBlastPreset(scene_, entity_)) &&
            SetDestructibleBlastSpeed(scene_, entity_, enabled_ ? StandardDestructibleBlastSpeed : 0.f);
    }

    DestructibleDebrisTiming GetDestructibleDebrisTiming(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        DestructibleDebrisTiming timing;
        if (const auto* metadata = scene.metadatas.GetComponent(entity))
        {
            const float stay = metadata->float_values.get(DestructibleDebrisStayKey);
            if (std::isfinite(stay) && stay >= 0.f && stay <= MaxDestructibleDebrisStay)
                timing.staySeconds = stay;
            if (metadata->float_values.has(DestructibleDebrisFadeKey))
            {
                const float fade = metadata->float_values.get(DestructibleDebrisFadeKey);
                if (std::isfinite(fade) && fade >= 0.f && fade <= MaxDestructibleDebrisFade)
                    timing.fadeSeconds = fade;
            }
        }
        return timing;
    }

    bool SetDestructibleDebrisTiming(wi::scene::Scene& scene, wi::ecs::Entity entity, DestructibleDebrisTiming timing)
    {
        const auto* body = scene.rigidbodies.GetComponent(entity);
        if (!body || !SupportsDestructible(*body) || IsBodySensor(scene, entity) ||
            !std::isfinite(timing.staySeconds) || timing.staySeconds < 0.f || timing.staySeconds > MaxDestructibleDebrisStay ||
            !std::isfinite(timing.fadeSeconds) || timing.fadeSeconds < 0.f || timing.fadeSeconds > MaxDestructibleDebrisFade ||
            GetDestructibleDebrisTiming(scene, entity) == timing)
            return false;
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (!metadata)
            metadata = &scene.metadatas.Create(entity);
        metadata->float_values.set(DestructibleDebrisStayKey, timing.staySeconds);
        metadata->float_values.set(DestructibleDebrisFadeKey, timing.fadeSeconds);
        return true;
    }

    bool SetDestructibleIntactMeshCommand::Execute()
    {
        const auto* body = scene_.rigidbodies.GetComponent(root_);
        if (!body || !SupportsDestructible(*body) || IsBodySensor(scene_, root_))
            return false;

        auto geometry = CollectDestructibleGeometry(scene_, root_, MaxFragments + 1);
        if (geometry.intactVisual != wi::ecs::INVALID_ENTITY)
            geometry.pieces.push_back(geometry.intactVisual);
        if (geometry.pieces.size() < 3 || geometry.pieces.size() > MaxFragments + 1 ||
            std::find(geometry.pieces.begin(), geometry.pieces.end(), intact_) == geometry.pieces.end())
            return false;

        before_.clear();
        for (const auto entity : geometry.pieces)
        {
            auto& object = *scene_.objects.GetComponent(entity);
            const bool intact = entity == intact_;
            const bool wasIntact = IsDestructibleIntactVisual(scene_, entity);
            const bool wasVisible = (object._flags & wi::scene::ObjectComponent::RENDERABLE) != 0;
            if (intact == wasIntact && intact == wasVisible)
                continue;
            before_.push_back({entity, wasIntact, wasVisible});
            SetIntactVisualRole(scene_, entity, intact);
            object.SetRenderable(intact);
        }
        return !before_.empty();
    }

    void SetDestructibleIntactMeshCommand::Undo()
    {
        for (const auto& state : before_)
        {
            if (auto* object = scene_.objects.GetComponent(state.entity))
            {
                SetIntactVisualRole(scene_, state.entity, state.intact);
                object->SetRenderable(state.visible);
            }
        }
    }

    bool SetDestructibleBlastSpeed(wi::scene::Scene& scene, wi::ecs::Entity entity, float speed)
    {
        const auto* body = scene.rigidbodies.GetComponent(entity);
        if (!body || !SupportsDestructible(*body) || IsBodySensor(scene, entity) ||
            !std::isfinite(speed) || speed < 0.f || speed > MaxDestructibleBlastSpeed ||
            GetDestructibleBlastSpeed(scene, entity) == speed)
            return false;
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (!metadata)
            metadata = &scene.metadatas.Create(entity);
        metadata->float_values.set(DestructibleBlastSpeedKey, speed);
        return true;
    }

    bool SetDestructibleImpactSpeed(wi::scene::Scene& scene, wi::ecs::Entity entity, float speed)
    {
        const auto* body = scene.rigidbodies.GetComponent(entity);
        if (!body || !std::isfinite(speed) || speed < 0.f || speed > MaxBreakSpeed ||
            (speed > 0.f && (!SupportsDestructible(*body) || IsBodySensor(scene, entity))) ||
            GetDestructibleImpactSpeed(scene, entity) == speed)
            return false;
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (!metadata)
            metadata = &scene.metadatas.Create(entity);
        metadata->float_values.set(DestructibleImpactSpeedKey, speed);
        if (scene.physics_scene)
        {
            auto& native = wi::physics::GetPhysicsScene(scene);
            if (native.destructibles_prepared)
            {
                native.gameplay_contacts.SetDestructible(entity, speed, AuthoredHitPoints(scene, entity));
                RefreshContactListener(native);
            }
        }
        return true;
    }

    bool SetDestructibleHitPoints(wi::scene::Scene& scene, wi::ecs::Entity entity, float hitPoints)
    {
        const auto* body = scene.rigidbodies.GetComponent(entity);
        if (!body || !std::isfinite(hitPoints) || hitPoints < 0.f || hitPoints > MaxBreakHitPoints ||
            (hitPoints > 0.f && (!SupportsDestructible(*body) || IsBodySensor(scene, entity))) ||
            GetDestructibleHitPoints(scene, entity) == hitPoints)
            return false;
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (!metadata)
            metadata = &scene.metadatas.Create(entity);
        metadata->float_values.set(DestructibleHitPointsKey, hitPoints);
        if (scene.physics_scene)
        {
            auto& native = wi::physics::GetPhysicsScene(scene);
            if (native.destructibles_prepared)
            {
                native.gameplay_contacts.SetDestructible(entity, AuthoredThreshold(scene, entity), hitPoints);
                RefreshContactListener(native);
            }
        }
        return true;
    }

    bool DamageDestructible(wi::scene::Scene& scene, wi::ecs::Entity hit, wi::ecs::Entity impactor,
        float damage, const XMFLOAT3& direction)
    {
        if (!scene.physics_scene || !std::isfinite(damage) || damage <= 0.f ||
            !std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z))
            return false;
        auto& listener = wi::physics::GetPhysicsScene(scene).gameplay_contacts;
        const auto incoming = wi::physics::cast(direction).NormalizedOr(JPH::Vec3::sZero());
        for (size_t depth = 0; hit != wi::ecs::INVALID_ENTITY && depth <= scene.hierarchy.GetCount(); ++depth)
        {
            if (AuthoredHitPoints(scene, hit) > 0.f)
                return listener.DamageDestructible(hit, impactor, damage, incoming);
            const auto* parent = scene.hierarchy.GetComponent(hit);
            hit = parent ? parent->parentID : wi::ecs::INVALID_ENTITY;
        }
        return false;
    }

    void PrepareDestructibles(wi::scene::Scene& scene)
    {
        if (JPH::Factory::sInstance == nullptr)
            return;
        auto& native = wi::physics::GetPhysicsScene(scene);
        native.destructibles_prepared = true;
        for (auto& entry : native.destructible_rattles)
            PoseRattle(scene, entry.second, 0.f);
        native.destructible_rattles.clear();
        native.gameplay_contacts.ResetDestructibles();
        for (size_t i = 0; i < scene.metadatas.GetCount(); ++i)
        {
            const auto entity = scene.metadatas.GetEntity(i);
            const float threshold = AuthoredThreshold(scene, entity);
            const float hitPoints = AuthoredHitPoints(scene, entity);
            if (threshold > 0.f || hitPoints > 0.f)
                native.gameplay_contacts.SetDestructible(entity, threshold, hitPoints);
        }
        RefreshContactListener(native);
    }

    bool RequestDestructibleBreak(wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        return scene.physics_scene && wi::physics::GetPhysicsScene(scene).gameplay_contacts.RequestBreak(entity);
    }

    void StepDestructibles(wi::scene::Scene& scene, float dt)
    {
        StepDestructibles(scene, dt, nullptr);
    }

    void StepDestructibles(wi::scene::Scene& scene, float dt, std::vector<XMFLOAT3>* blastPositions)
    {
        if (!scene.physics_scene)
            return;
        auto& native = wi::physics::GetPhysicsScene(scene);
        auto& listener = native.gameplay_contacts;
        const float advance = std::isfinite(dt) && dt > 0.f ? dt : 0.f;
        AgeDebris(scene, native.destructible_debris, advance);
        wi::physics::DestructibleImpact impact{};
        int processed = 0;
        for (; processed < 4 && listener.PopImpact(impact); ++processed)
        {
            const bool configured = AuthoredThreshold(scene, impact.target) > 0.f || AuthoredHitPoints(scene, impact.target) > 0.f;
            listener.SetDestructible(impact.target, 0.f);
            if (auto rattle = native.destructible_rattles.find(impact.target); rattle != native.destructible_rattles.end())
            {
                PoseRattle(scene, rattle->second, 0.f);
                native.destructible_rattles.erase(rattle);
            }
            if (!configured)
                continue;
            if (const uint32_t count = BreakDestructible(scene, native, impact, blastPositions); count > 0)
                listener.ReportBreak({impact.target, impact.impactor, impact.speed, count});
            else
                wi::backlog::post("Destructible requires an unconstrained static collider with two to sixteen valid mesh pieces.");
        }
        if (processed > 0)
            RefreshContactListener(native);

        auto& rattles = native.destructible_rattles;
        for (auto it = rattles.begin(); it != rattles.end();)
        {
            auto& rattle = it->second;
            rattle.elapsed += advance;
            if (rattle.elapsed >= RattleSeconds || AuthoredThreshold(scene, it->first) <= 0.f)
            {
                PoseRattle(scene, rattle, 0.f);
                it = rattles.erase(it);
                continue;
            }
            const float decay = 1.f - rattle.elapsed / RattleSeconds;
            PoseRattle(scene, rattle, decay * decay * std::cos(rattle.elapsed * 62.83f));
            ++it;
        }
        for (int i = 0; i < 4 && listener.PopImpact(impact, true); ++i)
        {
            if (AuthoredThreshold(scene, impact.target) > 0.f)
                StartRattle(scene, native, impact);
        }
    }

    std::vector<DestructibleBreakEvent> DrainDestructibleBreakEvents(const wi::scene::Scene& scene)
    {
        if (!scene.physics_scene)
            return {};
        return static_cast<wi::physics::PhysicsScene*>(scene.physics_scene.get())->gameplay_contacts.DrainBreaks();
    }
}
