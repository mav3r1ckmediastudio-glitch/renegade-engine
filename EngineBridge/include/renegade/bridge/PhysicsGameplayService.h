#pragma once

#include "CommandService.h"
#include "TerrainPropPhysicsAdapter.h"

namespace renegade::bridge
{
    inline constexpr char BodySensorKey[] = "renegade.physics.sensor";

    inline bool IsBodySensor(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata && metadata->bool_values.has(BodySensorKey) && metadata->bool_values.get(BodySensorKey);
    }

    inline bool SupportsBodySensor(const wi::scene::RigidBodyPhysicsComponent& body)
    {
        return !body.IsCharacterPhysics() && !body.IsVehicle();
    }

    inline bool SetBodySensor(wi::scene::Scene& scene, wi::ecs::Entity entity, bool enabled)
    {
        auto* body = scene.rigidbodies.GetComponent(entity);
        if (!body || (enabled && !SupportsBodySensor(*body)) || IsBodySensor(scene, entity) == enabled)
            return false;
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (!metadata)
            metadata = &scene.metadatas.Create(entity);
        metadata->bool_values.set(BodySensorKey, enabled);
        body->SetRefreshParametersNeeded(true);
        return true;
    }

    class SetBodySensorCommand final : public ICommand
    {
    public:
        SetBodySensorCommand(wi::scene::Scene& scene, wi::ecs::Entity entity, bool enabled)
            : scene_(scene), entity_(entity), enabled_(enabled)
        {}
        bool Execute() override { return SetBodySensor(scene_, entity_, enabled_); }
        void Undo() override { SetBodySensor(scene_, entity_, !enabled_); }
    private:
        wi::scene::Scene& scene_;
        wi::ecs::Entity entity_;
        bool enabled_;
    };

    inline constexpr int BodyCollisionLayerCount = 8;
    inline constexpr int AllBodyCollisionLayers = (1 << BodyCollisionLayerCount) - 1;

    struct BodyCollisionFilter
    {
        int layer = 0;
        int mask = AllBodyCollisionLayers;
        bool operator==(const BodyCollisionFilter& other) const
        {
            return layer == other.layer && mask == other.mask;
        }
    };

    inline bool SupportsBodyCollisionFilter(const wi::scene::RigidBodyPhysicsComponent& body)
    {
        return !body.IsCharacterPhysics() && !body.IsVehicle();
    }

    inline constexpr char BodyCollisionLayerKey[] = "renegade.physics.collision_layer";
    inline constexpr char BodyCollisionMaskKey[] = "renegade.physics.collision_mask";

    inline BodyCollisionFilter GetBodyCollisionFilter(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        BodyCollisionFilter filter;
        if (const auto* metadata = scene.metadatas.GetComponent(entity))
        {
            const auto& values = metadata->int_values;
            const int layer = values.has(BodyCollisionLayerKey) ? values.get(BodyCollisionLayerKey) : 0;
            const int mask = values.has(BodyCollisionMaskKey) ? values.get(BodyCollisionMaskKey) : AllBodyCollisionLayers;
            if (layer >= 0 && layer < BodyCollisionLayerCount)
                filter.layer = layer;
            if (mask >= 0 && mask <= AllBodyCollisionLayers)
                filter.mask = mask;
        }
        return filter;
    }

    // At a scene safe point, update the authoring settings and any existing native body.
    bool SetBodyCollisionFilter(wi::scene::Scene& scene, wi::ecs::Entity entity, BodyCollisionFilter filter);

    class SetBodyCollisionFilterCommand final : public ICommand
    {
    public:
        SetBodyCollisionFilterCommand(wi::scene::Scene& scene, wi::ecs::Entity entity, BodyCollisionFilter filter)
            : scene_(scene), entity_(entity), before_(GetBodyCollisionFilter(scene, entity)), after_(filter)
        {}
        bool Execute() override { return SetBodyCollisionFilter(scene_, entity_, after_); }
        void Undo() override { SetBodyCollisionFilter(scene_, entity_, before_); }
    private:
        wi::scene::Scene& scene_;
        wi::ecs::Entity entity_;
        BodyCollisionFilter before_;
        BodyCollisionFilter after_;
    };

    inline constexpr char DestructibleImpactSpeedKey[] = "renegade.physics.break_impact_speed";
    inline constexpr char DestructibleHitPointsKey[] = "renegade.physics.break_hit_points";
    inline constexpr char DestructibleBlastSpeedKey[] = "renegade.physics.break_blast_speed";
    inline constexpr float MaxDestructibleBlastSpeed = 24.f;
    inline constexpr char DestructibleDebrisStayKey[] = "renegade.physics.break_debris_stay";
    inline constexpr char DestructibleDebrisFadeKey[] = "renegade.physics.break_debris_fade";
    inline constexpr float MaxDestructibleDebrisStay = 120.f;
    inline constexpr float MaxDestructibleDebrisFade = 10.f;
    inline constexpr char DestructibleIntactVisualKey[] = "renegade.physics.break_intact_visual";

    // Set on an authored object below the static collider root. The remaining
    // mesh descendants are the pieces. Author them hidden for an intact swap.
    inline bool IsDestructibleIntactVisual(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata && metadata->int_values.get(DestructibleIntactVisualKey) == 1;
    }

    inline bool SupportsDestructible(const wi::scene::RigidBodyPhysicsComponent& body)
    {
        return SupportsBodyCollisionFilter(body) && body.mass == 0;
    }

    inline float GetDestructibleImpactSpeed(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata && metadata->float_values.has(DestructibleImpactSpeedKey)
            ? metadata->float_values.get(DestructibleImpactSpeedKey) : 0.f;
    }

    inline float GetDestructibleHitPoints(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata ? metadata->float_values.get(DestructibleHitPointsKey) : 0.f;
    }

    inline float GetDestructibleBlastSpeed(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata ? metadata->float_values.get(DestructibleBlastSpeedKey) : 0.f;
    }

    bool SetDestructibleBlastSpeed(wi::scene::Scene& scene, wi::ecs::Entity entity, float speed);

    struct DestructibleDebrisTiming
    {
        float staySeconds = 0.f;
        float fadeSeconds = 1.f;
        bool operator==(const DestructibleDebrisTiming& other) const
        {
            return staySeconds == other.staySeconds && fadeSeconds == other.fadeSeconds;
        }
        bool operator!=(const DestructibleDebrisTiming& other) const { return !(*this == other); }
    };

    DestructibleDebrisTiming GetDestructibleDebrisTiming(const wi::scene::Scene& scene, wi::ecs::Entity entity);
    bool SetDestructibleDebrisTiming(wi::scene::Scene& scene, wi::ecs::Entity entity, DestructibleDebrisTiming timing);

    // Set and break APIs run on the scene owner thread. Zero disables that input.
    bool SetDestructibleHitPoints(wi::scene::Scene& scene, wi::ecs::Entity entity, float hitPoints);
    bool DamageDestructible(wi::scene::Scene& scene, wi::ecs::Entity hit, wi::ecs::Entity impactor,
        float damage, const XMFLOAT3& direction);

    bool SetDestructibleImpactSpeed(wi::scene::Scene& scene, wi::ecs::Entity entity, float speed);
    void PrepareDestructibles(wi::scene::Scene& scene);
    void StepDestructibles(wi::scene::Scene& scene, float dt);
    void StepDestructibles(wi::scene::Scene& scene, float dt, std::vector<XMFLOAT3>* blastPositions);
    bool RequestDestructibleBreak(wi::scene::Scene& scene, wi::ecs::Entity entity);

    struct DestructibleBreakEvent
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity impactor = wi::ecs::INVALID_ENTITY;
        float impactSpeed = 0.f;
        uint32_t pieceCount = 0;
    };

    std::vector<DestructibleBreakEvent> DrainDestructibleBreakEvents(const wi::scene::Scene& scene);

    class SetDestructibleImpactSpeedCommand final : public ICommand
    {
    public:
        SetDestructibleImpactSpeedCommand(wi::scene::Scene& scene, wi::ecs::Entity entity, float speed)
            : scene_(scene), entity_(entity), before_(GetDestructibleImpactSpeed(scene, entity)), after_(speed)
        {}
        bool Execute() override { return SetDestructibleImpactSpeed(scene_, entity_, after_); }
        void Undo() override { SetDestructibleImpactSpeed(scene_, entity_, before_); }
    private:
        wi::scene::Scene& scene_;
        wi::ecs::Entity entity_;
        float before_;
        float after_;
    };

    class SetDestructibleHitPointsCommand final : public ICommand
    {
    public:
        SetDestructibleHitPointsCommand(wi::scene::Scene& scene, wi::ecs::Entity entity, float hitPoints)
            : scene_(scene), entity_(entity), before_(GetDestructibleHitPoints(scene, entity)), after_(hitPoints)
        {}
        bool Execute() override { return SetDestructibleHitPoints(scene_, entity_, after_); }
        void Undo() override { SetDestructibleHitPoints(scene_, entity_, before_); }
    private:
        wi::scene::Scene& scene_;
        wi::ecs::Entity entity_;
        float before_;
        float after_;
    };

    class SetDestructibleBlastSpeedCommand final : public ICommand
    {
    public:
        SetDestructibleBlastSpeedCommand(wi::scene::Scene& scene, wi::ecs::Entity entity, float speed)
            : scene_(scene), entity_(entity), before_(GetDestructibleBlastSpeed(scene, entity)), after_(speed)
        {}
        bool Execute() override { return SetDestructibleBlastSpeed(scene_, entity_, after_); }
        void Undo() override { SetDestructibleBlastSpeed(scene_, entity_, before_); }
    private:
        wi::scene::Scene& scene_;
        wi::ecs::Entity entity_;
        float before_;
        float after_;
    };

    class SetDestructibleDebrisTimingCommand final : public ICommand
    {
    public:
        SetDestructibleDebrisTimingCommand(wi::scene::Scene& scene, wi::ecs::Entity entity,
            DestructibleDebrisTiming timing)
            : scene_(scene), entity_(entity), before_(GetDestructibleDebrisTiming(scene, entity)), after_(timing)
        {}
        bool Execute() override { return SetDestructibleDebrisTiming(scene_, entity_, after_); }
        void Undo() override { SetDestructibleDebrisTiming(scene_, entity_, before_); }
    private:
        wi::scene::Scene& scene_;
        wi::ecs::Entity entity_;
        DestructibleDebrisTiming before_;
        DestructibleDebrisTiming after_;
    };

    // Designates one render object as intact and hides the other pieces below the body.
    class SetDestructibleIntactMeshCommand final : public ICommand
    {
    public:
        SetDestructibleIntactMeshCommand(wi::scene::Scene& scene, wi::ecs::Entity root, wi::ecs::Entity intact)
            : scene_(scene), root_(root), intact_(intact)
        {}
        bool Execute() override;
        void Undo() override;
    private:
        struct MeshState
        {
            wi::ecs::Entity entity;
            bool intact;
            bool visible;
        };
        wi::scene::Scene& scene_;
        wi::ecs::Entity root_;
        wi::ecs::Entity intact_;
        std::vector<MeshState> before_;
    };

    struct ContactEvent
    {
        wi::ecs::Entity bodyA = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity bodyB = wi::ecs::INVALID_ENTITY;
        uint32_t subshapeA = 0;
        uint32_t subshapeB = 0;
        bool sensor = false;
        bool began = false;
    };

    struct ContactEventBatch
    {
        std::vector<ContactEvent> events;
        uint32_t dropped = 0;
    };

    // Call at a scene safe point, after physics jobs have completed.
    bool EnableContactEvents(wi::scene::Scene& scene, bool enabled);
    ContactEventBatch DrainContactEvents(const wi::scene::Scene& scene);

    // Queries native gameplay bodies. Streamed terrain props use the existing
    // terrain-aware raycast instead.
    inline bool RaycastBodies(const wi::scene::Scene& scene, const XMFLOAT3& from,
        const XMFLOAT3& to, uint32_t mask, wi::ecs::Entity exclude, bool includeSensors,
        wi::physics::terrain_props::NativeHit& hit, std::string& error)
    {
        const std::vector<wi::ecs::Entity> excluded = exclude == wi::ecs::INVALID_ENTITY
            ? std::vector<wi::ecs::Entity>{} : std::vector<wi::ecs::Entity>{exclude};
        return wi::physics::terrain_props::CastNativeScene(
            scene, from, to, 0.0f, mask, excluded, false, hit, error, includeSensors);
    }
}
