    // Each settings table matches the fields of the corresponding public
    // authoring state. A patch changes only fields present in the Lua table.
    template <typename Visit>
    void VisitPhysicsSettings(renegade::bridge::CollisionState& state, const Visit& visit)
    {
        visit("shape", state.shape);
        visit("mass", state.mass);
        visit("friction", state.friction);
        visit("restitution", state.restitution);
        visit("damping_linear", state.dampingLinear);
        visit("damping_angular", state.dampingAngular);
        visit("buoyancy", state.buoyancy);
        visit("local_offset", state.localOffset);
        visit("mesh_lod", state.meshLod);
        visit("kinematic", state.kinematic);
        visit("locked_2d", state.locked2D);
        visit("disable_deactivation", state.disableDeactivation);
        visit("start_deactivated", state.startDeactivated);
        visit("box_half_extents", state.boxHalfExtents);
        visit("sphere_radius", state.sphereRadius);
        visit("capsule_radius", state.capsuleRadius);
        visit("capsule_height", state.capsuleHeight);
    }

    template <typename Visit>
    void VisitPhysicsSettings(renegade::bridge::ConstraintState& state, const Visit& visit)
    {
        visit("type", state.type);
        visit("body_a", state.bodyA);
        visit("body_b", state.bodyB);
        visit("disable_self_collision", state.disableSelfCollision);
        visit("break_distance", state.breakDistance);
        visit("distance_min", state.distanceMin);
        visit("distance_max", state.distanceMax);
        visit("hinge_min_angle", state.hingeMinAngle);
        visit("hinge_max_angle", state.hingeMaxAngle);
        visit("hinge_target_angular_velocity", state.hingeTargetAngularVelocity);
        visit("cone_half_angle", state.coneHalfAngle);
        visit("six_dof_min_translation", state.sixDofMinTranslation);
        visit("six_dof_max_translation", state.sixDofMaxTranslation);
        visit("six_dof_min_rotation", state.sixDofMinRotation);
        visit("six_dof_max_rotation", state.sixDofMaxRotation);
        visit("swing_twist_normal_half_cone_angle", state.swingTwistNormalHalfConeAngle);
        visit("swing_twist_plane_half_cone_angle", state.swingTwistPlaneHalfConeAngle);
        visit("swing_twist_min_angle", state.swingTwistMinAngle);
        visit("swing_twist_max_angle", state.swingTwistMaxAngle);
        visit("slider_min_limit", state.sliderMinLimit);
        visit("slider_max_limit", state.sliderMaxLimit);
        visit("slider_target_velocity", state.sliderTargetVelocity);
        visit("slider_max_force", state.sliderMaxForce);
    }

    template <typename Visit>
    void VisitPhysicsSettings(renegade::bridge::CharacterPhysicsState& state, const Visit& visit)
    {
        visit("enabled", state.enabled);
        visit("max_slope_angle", state.maxSlopeAngle);
        visit("gravity_factor", state.gravityFactor);
    }

    template <typename Visit>
    void VisitPhysicsSettings(renegade::bridge::VehiclePhysicsState& state, const Visit& visit)
    {
        visit("type", state.type);
        visit("collision_mode", state.collisionMode);
        visit("chassis_half_width", state.chassisHalfWidth);
        visit("chassis_half_height", state.chassisHalfHeight);
        visit("chassis_half_length", state.chassisHalfLength);
        visit("front_wheel_offset", state.frontWheelOffset);
        visit("rear_wheel_offset", state.rearWheelOffset);
        visit("wheel_radius", state.wheelRadius);
        visit("wheel_width", state.wheelWidth);
        visit("max_engine_torque", state.maxEngineTorque);
        visit("clutch_strength", state.clutchStrength);
        visit("max_roll_angle", state.maxRollAngle);
        visit("max_steering_angle", state.maxSteeringAngle);
        visit("front_suspension_min_length", state.frontSuspensionMinLength);
        visit("front_suspension_max_length", state.frontSuspensionMaxLength);
        visit("front_suspension_frequency", state.frontSuspensionFrequency);
        visit("front_suspension_damping", state.frontSuspensionDamping);
        visit("rear_suspension_min_length", state.rearSuspensionMinLength);
        visit("rear_suspension_max_length", state.rearSuspensionMaxLength);
        visit("rear_suspension_frequency", state.rearSuspensionFrequency);
        visit("rear_suspension_damping", state.rearSuspensionDamping);
        visit("four_wheel_drive", state.fourWheelDrive);
        visit("motorcycle_front_suspension_angle", state.motorcycleFrontSuspensionAngle);
        visit("motorcycle_front_brake_torque", state.motorcycleFrontBrakeTorque);
        visit("motorcycle_rear_brake_torque", state.motorcycleRearBrakeTorque);
        visit("motorcycle_lean_control", state.motorcycleLeanControl);
        visit("wheel_entity_front_left", state.wheelEntityFrontLeft);
        visit("wheel_entity_front_right", state.wheelEntityFrontRight);
        visit("wheel_entity_rear_left", state.wheelEntityRearLeft);
        visit("wheel_entity_rear_right", state.wheelEntityRearRight);
    }

    template <typename Visit>
    void VisitPhysicsSettings(renegade::bridge::RagdollPhysicsState& state, const Visit& visit)
    {
        visit("disabled", state.disabled);
        visit("physics_enabled", state.physicsEnabled);
        visit("locked_2d", state.locked2D);
        visit("fatness", state.fatness);
        visit("head_size", state.headSize);
    }

    template <typename Visit>
    void VisitPhysicsSettings(renegade::bridge::SoftBodyPhysicsState& state, const Visit& visit)
    {
        visit("detail", state.detail);
        visit("mass", state.mass);
        visit("friction", state.friction);
        visit("restitution", state.restitution);
        visit("pressure", state.pressure);
        visit("vertex_radius", state.vertexRadius);
        visit("wind_enabled", state.windEnabled);
    }

    template <typename Visit>
    void VisitPhysicsSettings(renegade::bridge::WickedColliderState& state, const Visit& visit)
    {
        visit("shape", state.shape);
        visit("cpu_enabled", state.cpuEnabled);
        visit("gpu_enabled", state.gpuEnabled);
        visit("radius", state.radius);
        visit("offset", state.offset);
        visit("tail", state.tail);
    }

    template <typename Value>
    void PushPhysicsField(lua_State* L, const Value& value)
    {
        if constexpr (std::is_same_v<Value, XMFLOAT3>)
            PushVector(L, value);
        else if constexpr (std::is_same_v<Value, bool>)
            PushBool(L, value);
        else if constexpr (std::is_floating_point_v<Value>)
            lua_pushnumber(L, value);
        else
            lua_pushinteger(L, static_cast<lua_Integer>(value));
    }

    float PhysicsNumber(lua_State* L, int index)
    {
        const auto number = luaL_checknumber(L, index);
        if (!std::isfinite(number) || std::abs(number) > (std::numeric_limits<float>::max)())
            luaL_error(L, "physics setting is outside the finite float range");
        return static_cast<float>(number);
    }

    template <typename Number>
    Number PhysicsInteger(lua_State* L)
    {
        const auto number = luaL_checkinteger(L, -1);
        const auto rangeValue = static_cast<long double>(number);
        if (rangeValue < std::numeric_limits<Number>::lowest() ||
            rangeValue > (std::numeric_limits<Number>::max)())
            luaL_error(L, "physics setting is outside the range of the field");
        return static_cast<Number>(number);
    }

    template <typename Value>
    void ReadPhysicsField(lua_State* L, Value& value)
    {
        if constexpr (std::is_same_v<Value, XMFLOAT3>)
        {
            luaL_checktype(L, -1, LUA_TTABLE);
            const auto coordinate = [L](const char* axis, float& destination) {
                lua_getfield(L, -1, axis);
                if (!lua_isnil(L, -1))
                    destination = PhysicsNumber(L, -1);
                lua_pop(L, 1);
            };
            coordinate("x", value.x);
            coordinate("y", value.y);
            coordinate("z", value.z);
        }
        else if constexpr (std::is_same_v<Value, bool>)
        {
            luaL_checktype(L, -1, LUA_TBOOLEAN);
            value = lua_toboolean(L, -1) != 0;
        }
        else if constexpr (std::is_floating_point_v<Value>)
            value = PhysicsNumber(L, -1);
        else if constexpr (std::is_enum_v<Value>)
            value = static_cast<Value>(PhysicsInteger<std::underlying_type_t<Value>>(L));
        else
            value = PhysicsInteger<Value>(L);
    }

    template <typename State>
    void ReadPhysicsPatch(lua_State* L, State& state)
    {
        luaL_checktype(L, 2, LUA_TTABLE);
        VisitPhysicsSettings(state, [L](const char* key, auto& value) {
            lua_getfield(L, 2, key);
            if (!lua_isnil(L, -1))
                ReadPhysicsField(L, value);
            lua_pop(L, 1);
        });
    }

    const auto* FindRigidBody(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        return scene.rigidbodies.GetComponent(entity);
    }

    const auto* FindConstraint(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        return scene.constraints.GetComponent(entity);
    }

    const auto* FindHumanoid(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        return scene.humanoids.GetComponent(entity);
    }

    const auto* FindSoftBody(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        return scene.softbodies.GetComponent(renegade::bridge::ResolveSoftBodyMeshEntity(scene, entity));
    }

    const auto* FindCollider(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        return scene.colliders.GetComponent(entity);
    }

    template <auto Find, auto Capture>
    int GetPhysicsSettings(lua_State* L)
    {
        const auto* scene = Scene();
        const auto* component = scene ? Find(*scene, EntityArg(L, 1)) : nullptr;
        if (!component)
        {
            lua_pushnil(L);
            return 1;
        }
        auto state = Capture(*component);
        lua_newtable(L);
        VisitPhysicsSettings(state, [L](const char* key, const auto& value) {
            PushPhysicsField(L, value);
            lua_setfield(L, -2, key);
        });
        return 1;
    }

    template <auto Find, auto Capture, typename Command>
    int SetPhysicsSettings(lua_State* L)
    {
        auto* scene = Scene();
        const auto entity = EntityArg(L, 1);
        const auto* component = scene ? Find(*scene, entity) : nullptr;
        if (!component)
        {
            PushBool(L, false);
            return 1;
        }
        const auto before = Capture(*component);
        auto after = before;
        ReadPhysicsPatch(L, after);
        Command command(*scene, entity, before, after);
        PushBool(L, command.Execute());
        return 1;
    }

    template <typename State, typename Command>
    int AddPhysicsComponent(lua_State* L)
    {
        auto* scene = Scene();
        const auto entity = EntityArg(L, 1);
        if (!scene)
        {
            PushBool(L, false);
            return 1;
        }
        State initial{};
        if (!lua_isnoneornil(L, 2))
            ReadPhysicsPatch(L, initial);
        if constexpr (std::is_same_v<State, renegade::bridge::CollisionState>)
        {
            // Explicit dimensions in a Lua settings table take precedence
            // over the editor's default primitive bounds fitting.
            Command command(*scene, entity, initial, lua_isnoneornil(L, 2));
            PushBool(L, command.Execute());
        }
        else
        {
            Command command(*scene, entity, initial);
            PushBool(L, command.Execute());
        }
        return 1;
    }

    template <typename Command>
    int RemovePhysicsComponent(lua_State* L)
    {
        auto* scene = Scene();
        const auto entity = EntityArg(L, 1);
        PushBool(L, scene && Command(*scene, entity).Execute());
        return 1;
    }

    void InstallPhysicsAuthoring(lua_State* L)
    {
        SetFunction(L, "get_rigid_body_settings", GetPhysicsSettings<FindRigidBody, renegade::bridge::CaptureCollision>);
        SetFunction(L, "set_rigid_body_settings", SetPhysicsSettings<FindRigidBody, renegade::bridge::CaptureCollision, renegade::bridge::SetCollisionCommand>);
        SetFunction(L, "add_rigid_body", AddPhysicsComponent<renegade::bridge::CollisionState, renegade::bridge::CreateCollisionCommand>);
        SetFunction(L, "remove_rigid_body", RemovePhysicsComponent<renegade::bridge::RemoveCollisionCommand>);

        SetFunction(L, "get_constraint_settings", GetPhysicsSettings<FindConstraint, renegade::bridge::CaptureConstraint>);
        SetFunction(L, "set_constraint_settings", SetPhysicsSettings<FindConstraint, renegade::bridge::CaptureConstraint, renegade::bridge::SetConstraintCommand>);
        SetFunction(L, "add_constraint", AddPhysicsComponent<renegade::bridge::ConstraintState, renegade::bridge::CreateConstraintCommand>);
        SetFunction(L, "remove_constraint", RemovePhysicsComponent<renegade::bridge::RemoveConstraintCommand>);

        SetFunction(L, "get_character_settings", GetPhysicsSettings<FindRigidBody, renegade::bridge::CaptureCharacterPhysics>);
        SetFunction(L, "set_character_settings", SetPhysicsSettings<FindRigidBody, renegade::bridge::CaptureCharacterPhysics, renegade::bridge::SetCharacterPhysicsCommand>);

        SetFunction(L, "get_vehicle_settings", GetPhysicsSettings<FindRigidBody, renegade::bridge::CaptureVehiclePhysics>);
        SetFunction(L, "set_vehicle_settings", SetPhysicsSettings<FindRigidBody, renegade::bridge::CaptureVehiclePhysics, renegade::bridge::SetVehiclePhysicsCommand>);

        SetFunction(L, "get_ragdoll_settings", GetPhysicsSettings<FindHumanoid, renegade::bridge::CaptureRagdollPhysics>);
        SetFunction(L, "set_ragdoll_settings", SetPhysicsSettings<FindHumanoid, renegade::bridge::CaptureRagdollPhysics, renegade::bridge::SetRagdollPhysicsCommand>);

        SetFunction(L, "get_soft_body_settings", GetPhysicsSettings<FindSoftBody, renegade::bridge::CaptureSoftBodyPhysics>);
        SetFunction(L, "set_soft_body_settings", SetPhysicsSettings<FindSoftBody, renegade::bridge::CaptureSoftBodyPhysics, renegade::bridge::SetSoftBodyPhysicsCommand>);
        SetFunction(L, "add_soft_body", AddPhysicsComponent<renegade::bridge::SoftBodyPhysicsState, renegade::bridge::CreateSoftBodyPhysicsCommand>);
        SetFunction(L, "remove_soft_body", RemovePhysicsComponent<renegade::bridge::RemoveSoftBodyPhysicsCommand>);

        SetFunction(L, "get_collider_settings", GetPhysicsSettings<FindCollider, renegade::bridge::CaptureWickedCollider>);
        SetFunction(L, "set_collider_settings", SetPhysicsSettings<FindCollider, renegade::bridge::CaptureWickedCollider, renegade::bridge::SetWickedColliderCommand>);
        SetFunction(L, "add_collider", AddPhysicsComponent<renegade::bridge::WickedColliderState, renegade::bridge::CreateWickedColliderCommand>);
        SetFunction(L, "remove_collider", RemovePhysicsComponent<renegade::bridge::RemoveWickedColliderCommand>);
    }
