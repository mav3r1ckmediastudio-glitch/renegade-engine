namespace wi::physics::jolt
{
    namespace
    {
        // Order groups the main and reverse pairs, then the two fore and two aft pods.
        constexpr const char* SpacecraftNozzles[] = {
            "REF_THR_MAIN_L", "REF_THR_MAIN_R", "REF_THR_REV_L", "REF_THR_REV_R",
            "REF_THR_FWD_L_OUT", "REF_THR_FWD_L_UP", "REF_THR_FWD_L_DN",
            "REF_THR_FWD_R_OUT", "REF_THR_FWD_R_UP", "REF_THR_FWD_R_DN",
            "REF_THR_AFT_L_OUT", "REF_THR_AFT_L_UP", "REF_THR_AFT_L_DN",
            "REF_THR_AFT_R_OUT", "REF_THR_AFT_R_UP", "REF_THR_AFT_R_DN",
        };

        bool LocalAircraftMarker(const Scene& scene, Entity marker, Entity chassis, XMMATRIX& local)
        {
            const auto* markerPose = scene.transforms.GetComponent(marker);
            const auto* chassisPose = scene.transforms.GetComponent(chassis);
            if (!markerPose || !chassisPose) return false;
            local = XMLoadFloat4x4(&markerPose->world) * XMMatrixInverse(nullptr, XMLoadFloat4x4(&chassisPose->world));
            return true;
        }

        bool ResolveSpacecraftNozzles(Scene& scene, Entity entity, PhysicalVtolState& state)
        {
            for (size_t i = 0; i < std::size(SpacecraftNozzles); ++i)
            {
                const auto marker = scene.Entity_FindByName(SpacecraftNozzles[i], entity);
                XMMATRIX local;
                if (!LocalAircraftMarker(scene, marker, entity, local))
                    return false;
                state.nozzles[i] = {
                    Vec3(XMVectorGetX(local.r[3]), XMVectorGetY(local.r[3]), XMVectorGetZ(local.r[3])),
                    Vec3(XMVectorGetX(local.r[2]), XMVectorGetY(local.r[2]), XMVectorGetZ(local.r[2])).NormalizedOr(Vec3::sAxisZ()),
                };
            }
            return true;
        }

        void StepSpacecraft(BodyInterface& bodies, BodyID id, RVec3 position, Quat rotation,
            Vec3 gravity, float mass, const PhysicalVtolState& state)
        {
            const auto& command = state.spacecraft;
            std::array<float, 16> loads{};
            const auto mix = [&](float amount, std::initializer_list<size_t> jets) {
                for (size_t jet : jets)
                    loads[jet] += std::max(0.f, amount);
            };
            mix(command.forward, {0, 1});
            mix(-command.forward, {2, 3});
            mix(command.lateral, {4, 10});
            mix(-command.lateral, {7, 13});
            mix(command.vertical, {6, 9, 12, 15});
            mix(-command.vertical, {5, 8, 11, 14});
            mix(command.pitch * .62f, {6, 9, 11, 14});
            mix(-command.pitch * .62f, {5, 8, 12, 15});
            mix(command.yaw, {4, 13});
            mix(-command.yaw, {7, 10});
            mix(command.roll * .32f, {9, 15, 5, 11});
            mix(-command.roll * .32f, {6, 12, 8, 14});

            // A zero-gravity craft can share a scene with a normal-gravity docking deck.
            bodies.AddForce(id, -gravity * mass);
            for (size_t i = 0; i < loads.size(); ++i)
            {
                if (loads[i] <= 0.f) continue;
                const auto& nozzle = state.nozzles[i];
                const float acceleration = i < 2 ? 3.1f : i < 4 ? 2.2f : 1.0f;
                bodies.AddForce(id, rotation * nozzle.direction * (mass * acceleration * std::min(loads[i], 1.f)),
                    position + rotation * nozzle.position);
            }
        }
    }

    namespace
    {
        using FlightMode = renegade::bridge::PlayerVehicleMode;

        PhysicalVtolState* ConfigureFlightState(Scene& scene, Entity entity, FlightMode mode)
        {
            const auto* body = scene.rigidbodies.GetComponent(entity);
            if (!body || !renegade::bridge::HasLiveFlightVehicle(scene, entity) ||
                body->IsCharacterPhysics() || body->IsVehicle())
                return nullptr;

            auto& states = GetPhysicsScene(scene).physical_vtols;
            const auto current = states.find(entity);
            if (current != states.end() && current->second.mode == mode)
                return &current->second;

            PhysicalVtolState state{};
            state.mode = mode;
            if (mode == FlightMode::Spacecraft && !ResolveSpacecraftNozzles(scene, entity, state))
                return nullptr;
            if (mode == FlightMode::Helicopter)
            {
                const auto hub = scene.Entity_FindByName("REF_MainRotorHub", entity);
                XMMATRIX local;
                if (!LocalAircraftMarker(scene, hub, entity, local))
                    return nullptr;
                state.rotorHub = Vec3(XMVectorGetX(local.r[3]), XMVectorGetY(local.r[3]), XMVectorGetZ(local.r[3]));
            }
            return &(states[entity] = state);
        }

        bool CommandRotorcraft(Scene& scene, Entity entity, FlightMode mode,
            renegade::bridge::FlightVehicleInput input, bool enabled)
        {
            if (renegade::bridge::GetPlayerVehicleMode(scene, entity) != mode) return false;
            if (!enabled)
            {
                if (scene.physics_scene) GetPhysicsScene(scene).physical_vtols.erase(entity);
                return true;
            }
            auto* state = ConfigureFlightState(scene, entity, mode);
            if (!state) return false;
            using renegade::bridge::FlightControl;
            state->input = {FlightControl(input.thrust), FlightControl(input.yaw), FlightControl(input.climb)};
            return true;
        }

        bool Grounded(const Scene& scene, Entity entity, FlightMode mode)
        {
            if (!scene.physics_scene) return false;
            const auto& states = static_cast<const PhysicsScene*>(scene.physics_scene.get())->physical_vtols;
            const auto it = states.find(entity);
            return it != states.end() && it->second.mode == mode && it->second.grounded;
        }
    }

    void StepPhysicalVtol(Scene& scene, PhysicsScene& physics, float dt)
    {
        auto& system = physics.physics_system;
        auto& bodies = system.GetBodyInterface();
        auto& vehicles = physics.physical_vtols;
        for (auto it = vehicles.begin(); it != vehicles.end();)
        {
            const auto* component = scene.rigidbodies.GetComponent(it->first);
            if (!component || renegade::bridge::GetPlayerVehicleMode(scene, it->first) != it->second.mode ||
                component->mass <= 0.f || component->IsKinematic() ||
                component->IsCharacterPhysics() || component->IsVehicle())
            {
                it = vehicles.erase(it);
                continue;
            }

            auto& state = (it++)->second;
            if (!component->physicsobject)
                continue;

            const auto id = GetRigidBody(*component).bodyID;
            const auto position = bodies.GetCenterOfMassPosition(id);
            const auto rotation = bodies.GetRotation(id);
            const auto velocity = bodies.GetLinearVelocity(id);
            const auto angular = bodies.GetAngularVelocity(id);
            if (state.mode == FlightMode::Spacecraft)
            {
                StepSpacecraft(bodies, id, position, rotation, system.GetGravity(), component->mass, state);
                continue;
            }
            const bool helicopter = state.mode == FlightMode::Helicopter;
            float bottom;
            {
                const BodyLockRead lock(system.GetBodyLockInterface(), id);
                if (!lock.Succeeded() || !lock.GetBody().IsDynamic())
                    continue;
                bottom = lock.GetBody().GetWorldSpaceBounds().mMin.GetY();
            }

            const Vec3 vertical = Vec3::sAxisY();
            const float height = float(position.GetY() - bottom);
            const AircraftGroundFilter ignore(id);
            const RRayCast groundRay{position, -vertical * (height + .08f)};
            RayCastResult hit;
            state.grounded = false;
            if (system.GetNarrowPhaseQuery().CastRay(groundRay, hit, {}, {}, ignore))
            {
                const BodyLockRead lock(system.GetBodyLockInterface(), hit.mBodyID);
                if (lock.Succeeded())
                {
                    const auto normal = lock.GetBody().GetWorldSpaceSurfaceNormal(
                        hit.mSubShapeID2, groundRay.GetPointOnRay(hit.mFraction));
                    state.grounded = normal.Dot(vertical) > .65f;
                }
            }

            const float climb = state.input.climb;
            if (!state.started || state.grounded)
            {
                state.targetAltitude = position.GetY();
                state.started = true;
            }
            // Let the landing surface carry the weight at idle.
            if (state.grounded && climb <= 0.f)
                continue;

            const auto up = rotation * vertical;
            const auto bodyForward = rotation * Vec3::sAxisZ();
            const auto forward = (bodyForward - vertical * bodyForward.GetY()).NormalizedOr(Vec3::sAxisZ());
            const auto right = vertical.Cross(forward);
            const auto horizontal = velocity - vertical * velocity.GetY();
            const float forwardSpeed = horizontal.Dot(forward);
            const float yaw = state.input.yaw;
            const float pitch = state.input.thrust * (helicopter ? .23f : .35f);
            // A stationary pivot stays level. A moving turn leans into the turn
            // and the tilted lift vector supplies lateral acceleration.
            const float bank = yaw * std::clamp(forwardSpeed / 8.f, -1.f, 1.f) * .48f;
            const auto desiredUp = (vertical + forward * pitch + right * bank).Normalized();
            const float yawRate = angular.GetY();
            const auto tiltRate = angular - vertical * yawRate;
            const float mass = component->mass;
            const auto torque = mass * (up.Cross(desiredUp) * 20.f - tiltRate * 10.f +
                vertical * ((yaw * 1.05f - yawRate) * 14.f));

            const float climbRate = climb * 3.6f;
            const double altitude = position.GetY();
            state.targetAltitude = std::clamp(state.targetAltitude + climbRate * dt, altitude - 5.0, altitude + 5.0);
            const float heightError = float(state.targetAltitude - altitude);
            const float gravity = -system.GetGravity().GetY();
            const float acceleration = gravity + heightError * 2.4f + (climbRate - velocity.GetY()) * 3.0f;
            const auto liftAxis = helicopter
                ? (up + bodyForward * (state.input.thrust * .16f) + right * (bank * .24f)).Normalized()
                : up;
            const float lift = mass * std::clamp(acceleration / std::max(liftAxis.GetY(), .55f), 0.f, 24.f);
            if (helicopter)
            {
                bodies.AddForce(id, liftAxis * lift, position + rotation * state.rotorHub);
                bodies.AddForce(id, -horizontal * (mass * .36f));
            }
            else
                bodies.AddForce(id, liftAxis * lift - horizontal * (mass * .40f));
            bodies.AddTorque(id, torque);
        }
    }
}

bool renegade::bridge::CommandPhysicalVtol(wi::scene::Scene& scene, wi::ecs::Entity entity,
    FlightVehicleInput input, bool enabled)
{
    if (!enabled && !IsPhysicalVtol(scene, entity)) return true;
    return wi::physics::jolt::CommandRotorcraft(scene, entity, PlayerVehicleMode::PhysicalVTOL, input, enabled);
}

bool renegade::bridge::CommandHelicopter(wi::scene::Scene& scene, wi::ecs::Entity entity,
    FlightVehicleInput input, bool enabled)
{
    return wi::physics::jolt::CommandRotorcraft(scene, entity, PlayerVehicleMode::Helicopter, input, enabled);
}

bool renegade::bridge::CommandSpacecraft(wi::scene::Scene& scene, wi::ecs::Entity entity,
    SpacecraftInput input, bool enabled)
{
    if (!IsSpacecraft(scene, entity)) return false;
    if (!enabled)
    {
        if (scene.physics_scene)
            wi::physics::GetPhysicsScene(scene).physical_vtols.erase(entity);
        return true;
    }
    auto* state = wi::physics::jolt::ConfigureFlightState(scene, entity, PlayerVehicleMode::Spacecraft);
    if (!state) return false;
    state->spacecraft = {
        FlightControl(input.forward), FlightControl(input.lateral), FlightControl(input.vertical),
        FlightControl(input.pitch), FlightControl(input.yaw), FlightControl(input.roll),
    };
    return true;
}

bool renegade::bridge::IsHelicopterGrounded(const wi::scene::Scene& scene, wi::ecs::Entity entity)
{
    return IsHelicopter(scene, entity) && wi::physics::jolt::Grounded(scene, entity, PlayerVehicleMode::Helicopter);
}

bool renegade::bridge::IsPhysicalVtolGrounded(const wi::scene::Scene& scene, wi::ecs::Entity entity)
{
    return wi::physics::jolt::Grounded(scene, entity, PlayerVehicleMode::PhysicalVTOL);
}
