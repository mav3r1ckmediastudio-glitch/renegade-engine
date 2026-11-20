namespace wi::physics
{
    void GameplayContactListener::Enable(bool value)
    {
        std::scoped_lock lock(mutex_);
        if (value == enabled_.load(std::memory_order_relaxed))
            return;
        active_.clear();
        batch_.events.clear();
        batch_.dropped = 0;
        enabled_.store(value, std::memory_order_relaxed);
    }

    renegade::bridge::ContactEventBatch GameplayContactListener::Drain()
    {
        std::scoped_lock lock(mutex_);
        return std::exchange(batch_, {});
    }

    void GameplayContactListener::Drop()
    {
        if (batch_.dropped != UINT32_MAX)
            ++batch_.dropped;
    }

    void GameplayContactListener::Queue(renegade::bridge::ContactEvent event)
    {
        if (batch_.events.size() < 4096)
            batch_.events.push_back(event);
        else
            Drop();
    }

    void GameplayContactListener::Remember(const JPH::Body& a, const JPH::Body& b,
        const JPH::ContactManifold& manifold)
    {
        if (!enabled_.load(std::memory_order_relaxed))
            return;
        const auto entityA = terrain_props::EntityOf(a);
        const auto entityB = terrain_props::EntityOf(b);
        if (entityA == INVALID_ENTITY || entityB == INVALID_ENTITY)
            return;

        const JPH::SubShapeIDPair key(a.GetID(), manifold.mSubShapeID1, b.GetID(), manifold.mSubShapeID2);
        std::scoped_lock lock(mutex_);
        if (active_.find(key) != active_.end())
            return;
        if (active_.size() >= 65536)
        {
            Drop();
            return;
        }
        const renegade::bridge::ContactEvent event{
            entityA, entityB, manifold.mSubShapeID1.GetValue(), manifold.mSubShapeID2.GetValue(),
            a.IsSensor() || b.IsSensor(), true};
        active_.emplace(key, event);
        Queue(event);
    }

    void GameplayContactListener::OnContactAdded(const JPH::Body& a, const JPH::Body& b,
        const JPH::ContactManifold& manifold, JPH::ContactSettings&)
    {
        if (sample_diagnostics.load(std::memory_order_relaxed))
            diagnostic_contacts.fetch_add(1, std::memory_order_relaxed);
        RecordImpact(a, b, manifold);
        Remember(a, b, manifold);
    }

    void GameplayContactListener::OnContactPersisted(const JPH::Body& a, const JPH::Body& b,
        const JPH::ContactManifold& manifold, JPH::ContactSettings&)
    {
        if (sample_diagnostics.load(std::memory_order_relaxed))
            diagnostic_contacts.fetch_add(1, std::memory_order_relaxed);
        Remember(a, b, manifold);
    }

    void GameplayContactListener::OnContactRemoved(const JPH::SubShapeIDPair& key)
    {
        if (!enabled_.load(std::memory_order_relaxed))
            return;
        std::scoped_lock lock(mutex_);
        const auto found = active_.find(key);
        if (found == active_.end())
            return;
        auto event = found->second;
        active_.erase(found);
        event.began = false;
        Queue(event);
    }
}

namespace renegade::bridge
{
    bool SetBodyCollisionFilter(wi::scene::Scene& scene, wi::ecs::Entity entity, BodyCollisionFilter filter)
    {
        auto* body = scene.rigidbodies.GetComponent(entity);
        if (!body || !SupportsBodyCollisionFilter(*body) ||
            filter.layer < 0 || filter.layer >= BodyCollisionLayerCount ||
            filter.mask < 0 || filter.mask > AllBodyCollisionLayers ||
            filter == GetBodyCollisionFilter(scene, entity))
            return false;

        auto* metadata = scene.metadatas.GetComponent(entity);
        if (!metadata)
            metadata = &scene.metadatas.Create(entity);
        metadata->int_values.set(BodyCollisionLayerKey, filter.layer);
        metadata->int_values.set(BodyCollisionMaskKey, filter.mask);

        if (body->physicsobject && scene.physics_scene)
        {
            const auto& native = wi::physics::GetRigidBody(*body);
            auto& nativeScene = *static_cast<wi::physics::PhysicsScene*>(native.physics_scene.get());
            auto& system = nativeScene.physics_system;
            auto& bodies = system.GetBodyInterface();
            const JPH::ObjectLayer current = bodies.GetObjectLayer(native.bodyID);
            if (current == JPH::cObjectLayerInvalid)
                return true;
            const JPH::ObjectLayer updated = wi::physics::EncodeCollisionLayer(wi::physics::MotionLayer(current), filter);
            bodies.SetObjectLayer(native.bodyID, updated);
            bodies.InvalidateContactCache(native.bodyID);

            // Wake bodies affected by a change to a resting or static collider.
            JPH::AABox bounds;
            {
                const JPH::BodyLockRead lock(system.GetBodyLockInterface(), native.bodyID);
                if (!lock.Succeeded())
                    return true;
                bounds = lock.GetBody().GetWorldSpaceBounds();
            }
            const JPH::DefaultBroadPhaseLayerFilter moving(nativeScene.object_vs_broadphase_layer_filter, wi::physics::Layers::NON_MOVING);
            bodies.ActivateBodiesInAABox(bounds, moving, JPH::ObjectLayerFilter());
        }
        return true;
    }

    bool EnableContactEvents(wi::scene::Scene& scene, bool enabled)
    {
        if (!scene.physics_scene && (!enabled || JPH::Factory::sInstance == nullptr))
            return false;
        auto& native = wi::physics::GetPhysicsScene(scene);
        native.gameplay_contacts.Enable(enabled);
        native.physics_system.SetContactListener(native.gameplay_contacts.NeedsListener() ? &native.gameplay_contacts : nullptr);
        return true;
    }

    ContactEventBatch DrainContactEvents(const wi::scene::Scene& scene)
    {
        if (!scene.physics_scene)
            return {};
        auto& native = *static_cast<wi::physics::PhysicsScene*>(scene.physics_scene.get());
        return native.gameplay_contacts.Drain();
    }
}
