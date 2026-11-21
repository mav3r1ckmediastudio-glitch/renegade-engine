#include "../EngineBridge/include/renegade/bridge/FlightVehicleService.h"

namespace wi::physics::jolt
{
    namespace
    {
        using namespace renegade::bridge;

        FlightVector ReadFlightVector(Vec3Arg value)
        {
            return {value.GetX(), value.GetY(), value.GetZ()};
        }

        Vec3 WriteFlightVector(FlightVector value)
        {
            return Vec3(value.x, value.y, value.z);
        }

        class AircraftGroundFilter : public BodyFilter
        {
        public:
            explicit AircraftGroundFilter(BodyID aircraft) : aircraft(aircraft) {}
            bool ShouldCollide(const BodyID& id) const override { return id != aircraft; }
            bool ShouldCollideLocked(const Body& body) const override
            {
                return body.GetID() != aircraft && !body.IsSensor() &&
                    MotionLayer(body.GetObjectLayer()) != Layers::GHOST;
            }

        private:
            BodyID aircraft;
        };

    }

    bool IsParkedFixedWing(const Scene& scene, Entity entity)
    {
        if (!renegade::bridge::IsFixedWing(scene, entity))
            return false;
        const auto* physics = static_cast<const PhysicsScene*>(scene.physics_scene.get());
        return physics == nullptr || physics->fixed_wings.find(entity) == physics->fixed_wings.end();
    }

    void StepFixedWing(Scene& scene, PhysicsScene& physics, float dt)
    {
        auto& system = physics.physics_system;
        auto& bodies = system.GetBodyInterface();
        for (auto it = physics.fixed_wings.begin(); it != physics.fixed_wings.end();)
        {
            const auto* authored = scene.rigidbodies.GetComponent(it->first);
            if (!authored || authored->mass <= 0.f || authored->IsKinematic() ||
                authored->IsCharacterPhysics() || authored->IsVehicle())
            {
                it = physics.fixed_wings.erase(it);
                continue;
            }
            auto& state = (it++)->second;
            if (!authored->physicsobject)
                continue;

            const auto id = GetRigidBody(*authored).bodyID;
            {
                BodyLockWrite lock(system.GetBodyLockInterface(), id);
                if (!lock.Succeeded() || !lock.GetBody().IsDynamic()) continue;
                auto* motion = lock.GetBody().GetMotionProperties();
                // Include the spanwise mass of the wings, which the simplified
                // fuselage collision shape does not represent.
                const auto inertia = Vec3(1.f / 2.4f, 1.f / 3.6f, 1.f / 1.7f) * motion->GetInverseMass();
                if ((motion->GetInverseInertiaDiagonal() - inertia).LengthSq() > 1.e-14f)
                    motion->SetInverseInertia(inertia, Quat::sIdentity());
            }
            const auto rotation = bodies.GetRotation(id);
            const auto inverse = rotation.Conjugated();
            const auto position = bodies.GetCenterOfMassPosition(id);
            const auto angular = bodies.GetAngularVelocity(id);
            const auto velocity = bodies.GetLinearVelocity(id);
            const auto wind = WriteFlightVector(state.aircraft.wind);
            const auto relative = velocity - wind;
            const auto loads = EvaluateFixedWing(state.aircraft.aerodynamics, state.input,
                ReadFlightVector(inverse * relative), ReadFlightVector(inverse * angular),
                state.aircraft.airDensity);
            state.telemetry = {relative.Length(), 0};
            for (const auto& load : loads)
            {
                const auto force = WriteFlightVector(load.force);
                if (!force.IsNearZero())
                    bodies.AddForce(id, rotation * force, position + rotation * WriteFlightVector(load.position));
            }

            constexpr float travel = .22f;
            const float spring = authored->mass * 43.f * state.aircraft.gearSpringScale;
            const float damper = authored->mass * 4.f * state.aircraft.gearDampingScale;
            const auto up = rotation * Vec3::sAxisY();
            const AircraftGroundFilter ignore(id);
            for (const auto& wheel : state.aircraft.gear)
            {
                const auto offset = rotation * WriteFlightVector(wheel.position);
                const RRayCast ray{position + offset + up * travel, -up * travel};
                RayCastResult hit;
                if (!system.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, ignore))
                    continue;
                const auto contact = ray.GetPointOnRay(hit.mFraction);
                Vec3 normal;
                {
                    BodyLockRead lock(system.GetBodyLockInterface(), hit.mBodyID);
                    if (!lock.Succeeded()) continue;
                    normal = lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, contact);
                }
                if (normal.Dot(up) < .5f)
                    continue;
                ++state.telemetry.groundContacts;
                const Vec3 pointVelocity = velocity + angular.Cross(Vec3(contact - position));
                const float speedDown = -pointVelocity.Dot(normal);
                const float load = std::clamp(spring * travel * (1.f-hit.mFraction) +
                    damper * speedDown, 0.f, authored->mass * 34.f);
                const float steer = wheel.steerable ? FlightControl(state.input.yaw) * .32f : 0.f;
                Vec3 forward = rotation * Vec3(std::sin(steer), 0, std::cos(steer));
                forward = (forward - normal * forward.Dot(normal)).NormalizedOr(Vec3::sAxisZ());
                const auto right = normal.Cross(forward);
                const float lateralSpeed = pointVelocity.Dot(right);
                const float forwardSpeed = pointVelocity.Dot(forward);
                const float effectiveMass = authored->mass / 4.f;
                const float rolling = wheel.steerable ? .022f : .022f + state.brakes * .72f;
                const float lateralForce = std::clamp(lateralSpeed * effectiveMass * 11.f,
                    -load * .82f, load * .82f);
                const float brakingForce = std::clamp(forwardSpeed * effectiveMass / dt,
                    -load * rolling, load * rolling);
                bodies.AddForce(id, normal * load - right * lateralForce - forward * brakingForce, contact);
            }
        }
    }
}

bool renegade::bridge::RegisterFixedWing(wi::scene::Scene& scene, wi::ecs::Entity entity,
    const FixedWingAircraft& aircraft)
{
    const auto* body = scene.rigidbodies.GetComponent(entity);
    if (!body || body->IsKinematic() || body->IsVehicle() || body->IsCharacterPhysics() ||
        !std::isfinite(body->mass) || body->mass <= 0.f)
        return false;
    const auto finite = [](FlightVector v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    const auto nonnegative = [](float value) { return std::isfinite(value) && value >= 0.f; };
    const auto validSurface = [&](const FixedWingSurface& surface) {
        return finite(surface.centre) && nonnegative(surface.area) && std::isfinite(surface.incidence);
    };
    const auto& setup = aircraft.aerodynamics;
    if (!nonnegative(aircraft.airDensity) || !finite(aircraft.wind) ||
        !validSurface(setup.portWing) || !validSurface(setup.starboardWing) ||
        !validSurface(setup.tail) || !validSurface(setup.fin) || !finite(setup.propeller) ||
        !nonnegative(setup.maxThrust) || !nonnegative(setup.propellerPitchSpeed) ||
        !nonnegative(setup.fuselageDragArea) || !nonnegative(setup.wingStallAngle) ||
        !nonnegative(aircraft.gearSpringScale) || !nonnegative(aircraft.gearDampingScale) ||
        std::any_of(aircraft.gear.begin(), aircraft.gear.end(),
            [&](const FixedWingGearPoint& wheel) { return !finite(wheel.position); }))
        return false;
    auto& state = wi::physics::GetPhysicsScene(scene).fixed_wings[entity];
    state = {aircraft, {}, 1.f, {}};
    return true;
}

bool renegade::bridge::CommandFixedWing(wi::scene::Scene& scene, wi::ecs::Entity entity,
    FixedWingInput input, float brakes)
{
    if (!scene.physics_scene) return false;
    auto& aircraft = wi::physics::GetPhysicsScene(scene).fixed_wings;
    const auto it = aircraft.find(entity);
    if (it == aircraft.end()) return false;
    it->second.input = input;
    it->second.brakes = std::max(0.f, FlightControl(brakes));
    return true;
}

renegade::bridge::FixedWingTelemetry renegade::bridge::ReadFixedWing(
    const wi::scene::Scene& scene, wi::ecs::Entity entity)
{
    if (!scene.physics_scene) return {};
    const auto& aircraft = static_cast<const wi::physics::PhysicsScene*>(scene.physics_scene.get())->fixed_wings;
    const auto it = aircraft.find(entity);
    return it == aircraft.end() ? FixedWingTelemetry{} : it->second.telemetry;
}
