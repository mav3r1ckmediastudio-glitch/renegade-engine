namespace wi::physics::jolt
{
    using namespace renegade::bridge;

    void StepTrackedVehicles(Scene& scene, PhysicsScene& physics)
    {
        auto& system = physics.physics_system;
        auto& bodies = system.GetBodyInterface();
        for (auto it = physics.tracked_vehicles.begin(); it != physics.tracked_vehicles.end();)
        {
            const auto entity = it->first;
            auto* component = scene.rigidbodies.GetComponent(entity);
            if (!component)
            {
                it = physics.tracked_vehicles.erase(it);
                continue;
            }
            const auto& setup = (it++)->second;
            const auto& tuning = setup.tuning;
            if (!component->physicsobject)
                continue;

            auto& native = GetRigidBody(*component);
            if (native.bodyID.IsInvalid())
                continue;
            const bool valid = IsTrackedVehicle(scene, entity) && !component->IsVehicle() &&
                !component->IsCharacterPhysics() && !component->IsKinematic() && component->mass > 0.f;
            if (!valid)
            {
                native.RemoveTrackedVehicle(system);
                continue;
            }
            if (!native.tracked_constraint)
            {
                VehicleConstraintSettings settings;
                settings.mMaxPitchRollAngle = JPH::DegreesToRadians(tuning.maxTilt);
                auto* controller = new TrackedVehicleControllerSettings;
                settings.mController = controller;
                controller->mEngine.mMaxTorque = tuning.engineTorque;
                for (unsigned i = 0; i < TrackedWheelCount; ++i)
                {
                    const unsigned side = i / TrackedWheelsPerSide;
                    const unsigned station = i % TrackedWheelsPerSide;
                    const bool end = station == 0 || station == TrackedWheelsPerSide - 1;
                    auto* wheel = new WheelSettingsTV;
                    auto anchor = setup.anchors[i];
                    anchor.x *= tuning.widthScale;
                    anchor.y += tuning.anchorHeight;
                    anchor.z *= tuning.lengthScale;
                    wheel->mPosition = cast(anchor);
                    wheel->mRadius = tuning.radius;
                    wheel->mWidth = tuning.width;
                    // Skid steering needs lateral slip across the contact patches.
                    wheel->mLateralFriction = tuning.lateralGrip;
                    wheel->mLongitudinalFriction = tuning.longitudinalGrip;
                    wheel->mSuspensionMinLength = end ? tuning.endSuspension : tuning.suspensionMin;
                    wheel->mSuspensionMaxLength = end ? tuning.endSuspension : tuning.suspensionMin + tuning.suspensionTravel;
                    wheel->mSuspensionSpring.mFrequency = tuning.spring;
                    wheel->mSuspensionSpring.mDamping = tuning.damping;
                    settings.mWheels.push_back(wheel);
                    auto& track = controller->mTracks[side];
                    track.mWheels.push_back(i);
                    track.mMaxBrakeTorque = tuning.brakeTorque;
                    track.mDifferentialRatio = tuning.differentialRatio;
                    track.mInertia = tuning.trackInertia;
                    track.mAngularDamping = tuning.trackDamping;
                    if (station == TrackedWheelsPerSide - 1)
                        track.mDrivenWheel = i;
                }

                {
                    const BodyLockWrite lock(system.GetBodyLockInterface(), native.bodyID);
                    if (!lock.Succeeded() || !lock.GetBody().IsDynamic()) continue;
                    native.tracked_constraint = new VehicleConstraint(lock.GetBody(), settings);
                }
                using CollisionMode = RigidBodyPhysicsComponent::Vehicle::CollisionMode;
                RefConst<VehicleCollisionTester> tester;
                switch (component->vehicle.collision_mode)
                {
                case CollisionMode::Sphere:
                    tester = new VehicleCollisionTesterCastSphere(Layers::MOVING, .5f * tuning.width);
                    break;
                case CollisionMode::Cylinder:
                    tester = new VehicleCollisionTesterCastCylinder(Layers::MOVING);
                    break;
                default:
                    tester = new VehicleCollisionTesterRay(Layers::MOVING);
                    break;
                }
                native.tracked_constraint->SetVehicleCollisionTester(tester);
                system.AddConstraint(native.tracked_constraint);
                system.AddStepListener(native.tracked_constraint);
                bodies.ActivateBody(native.bodyID);
            }

            auto& controller = *static_cast<TrackedVehicleController*>(native.tracked_constraint->GetController());
            float forward = setup.forward;
            float left = 1.f;
            float right = 1.f;
            const float amount = std::abs(setup.turn);
            if (std::abs(forward) < .06f && amount > .06f)
            {
                forward = tuning.pivotPower * amount;
                left = setup.turn < 0.f ? -1.f : 1.f;
                right = -left;
            }
            else if (setup.turn < 0.f)
                left = 1.f - amount * tuning.steerReduction;
            else
                right = 1.f - amount * tuning.steerReduction;

            controller.SetDriverInput(forward * (1.f - setup.brake), left, right, setup.brake);
            if (std::abs(forward) > .02f && setup.brake < .99f)
                bodies.ActivateBody(native.bodyID);
        }
    }
}

bool renegade::bridge::RegisterTrackedVehicle(wi::scene::Scene& scene, wi::ecs::Entity entity,
    const TrackedWheelAnchors& anchors)
{
    auto* component = scene.rigidbodies.GetComponent(entity);
    if (!IsTrackedVehicle(scene, entity) || !component || component->IsKinematic() ||
        component->IsCharacterPhysics() || component->IsVehicle() || component->mass <= 0.f ||
        !std::all_of(anchors.begin(), anchors.end(), [](XMFLOAT3 anchor) {
            return std::isfinite(anchor.x) && std::isfinite(anchor.y) && std::isfinite(anchor.z);
        }))
        return false;
    auto& vehicles = wi::physics::GetPhysicsScene(scene).tracked_vehicles;
    const auto [entry, inserted] = vehicles.emplace(entity, wi::physics::TrackedVehicleSetup{});
    if (inserted)
    {
        entry->second.anchors = anchors;
        entry->second.tuning = ReadTrackedTuning(scene, entity);
    }
    return true;
}

bool renegade::bridge::DriveTrackedVehicle(wi::scene::Scene& scene, wi::ecs::Entity entity,
    float forward, float turn, float brake)
{
    if (!scene.physics_scene || !IsTrackedVehicle(scene, entity))
        return false;
    auto& states = wi::physics::GetPhysicsScene(scene).tracked_vehicles;
    auto found = states.find(entity);
    if (found == states.end()) return false;
    auto& setup = found->second;
    setup.forward = FlightControl(forward);
    setup.turn = FlightControl(turn);
    setup.brake = std::max(0.f, FlightControl(brake));
    return true;
}

renegade::bridge::TrackedVehicleState renegade::bridge::ReadTrackedVehicle(
    const wi::scene::Scene& scene, wi::ecs::Entity entity)
{
    TrackedVehicleState state;
    auto* component = scene.rigidbodies.GetComponent(entity);
    if (!component || !component->physicsobject) return state;
    const auto& native = *static_cast<const wi::physics::RigidBody*>(component->physicsobject.get());
    if (!native.tracked_constraint) return state;

    state.ready = true;
    const auto& controller = *static_cast<const JPH::TrackedVehicleController*>(
        native.tracked_constraint->GetController());
    for (unsigned side = 0; side < TrackSides; ++side)
        state.trackSpeed[side] = controller.GetTracks()[side].mAngularVelocity;
    for (unsigned i = 0; i < TrackedWheelCount; ++i)
    {
        const auto* wheel = native.tracked_constraint->GetWheel(i);
        state.contacts += wheel->HasContact() ? 1u : 0u;
        state.suspension[i] = wheel->GetSuspensionLength();
        state.wheelAngle[i] = wheel->GetRotationAngle();
    }
    return state;
}
