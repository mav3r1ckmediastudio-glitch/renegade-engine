#include "renegade/bridge/PhysicsLuaService.h"

#include "renegade/bridge/PhysicsService.h"

#include <wiLua.h>

namespace
{
    wi::scene::Scene* boundScene = nullptr;

    wi::scene::Scene* Scene() noexcept
    {
        return boundScene;
    }

    wi::ecs::Entity EntityArg(lua_State* L, const int index)
    {
        return static_cast<wi::ecs::Entity>(luaL_checkinteger(L, index));
    }

    XMFLOAT3 Vector3Arg(lua_State* L, const int first)
    {
        return XMFLOAT3(
            static_cast<float>(luaL_checknumber(L, first)),
            static_cast<float>(luaL_checknumber(L, first + 1)),
            static_cast<float>(luaL_checknumber(L, first + 2)));
    }

    XMFLOAT4 Vector4Arg(lua_State* L, const int first)
    {
        return XMFLOAT4(
            static_cast<float>(luaL_checknumber(L, first)),
            static_cast<float>(luaL_checknumber(L, first + 1)),
            static_cast<float>(luaL_checknumber(L, first + 2)),
            static_cast<float>(luaL_checknumber(L, first + 3)));
    }

    void PushVector(lua_State* L, const XMFLOAT3& value)
    {
        lua_createtable(L, 0, 3);
        lua_pushnumber(L, value.x); lua_setfield(L, -2, "x");
        lua_pushnumber(L, value.y); lua_setfield(L, -2, "y");
        lua_pushnumber(L, value.z); lua_setfield(L, -2, "z");
    }

    void PushVector(lua_State* L, const XMFLOAT4& value)
    {
        lua_createtable(L, 0, 4);
        lua_pushnumber(L, value.x); lua_setfield(L, -2, "x");
        lua_pushnumber(L, value.y); lua_setfield(L, -2, "y");
        lua_pushnumber(L, value.z); lua_setfield(L, -2, "z");
        lua_pushnumber(L, value.w); lua_setfield(L, -2, "w");
    }

    void PushBool(lua_State* L, const bool value)
    {
        lua_pushboolean(L, value ? 1 : 0);
    }

    int LuaGetWorld(lua_State* L)
    {
        const auto state = renegade::bridge::CapturePhysicsWorldState();
        lua_createtable(L, 0, 9);
#define RENEGADE_LUA_SET_BOOL(name, value) \
        lua_pushboolean(L, (value) ? 1 : 0); lua_setfield(L, -2, name)
#define RENEGADE_LUA_SET_NUMBER(name, value) \
        lua_pushnumber(L, static_cast<lua_Number>(value)); lua_setfield(L, -2, name)
        RENEGADE_LUA_SET_BOOL("enabled", state.enabled);
        RENEGADE_LUA_SET_BOOL("simulation_enabled", state.simulationEnabled);
        RENEGADE_LUA_SET_BOOL("interpolation_enabled", state.interpolationEnabled);
        RENEGADE_LUA_SET_BOOL("debug_draw_enabled", state.debugDrawEnabled);
        RENEGADE_LUA_SET_NUMBER("accuracy", state.accuracy);
        RENEGADE_LUA_SET_NUMBER("frame_rate", state.frameRate);
        RENEGADE_LUA_SET_NUMBER("constraint_debug_size", state.constraintDebugSize);
        RENEGADE_LUA_SET_NUMBER("debug_draw_max_distance", state.debugDrawMaxDistance);
        RENEGADE_LUA_SET_NUMBER("character_collision_tolerance", state.characterCollisionTolerance);
#undef RENEGADE_LUA_SET_BOOL
#undef RENEGADE_LUA_SET_NUMBER
        return 1;
    }

    template<typename Mutator>
    int MutateWorld(lua_State* L, Mutator&& mutator)
    {
        auto state = renegade::bridge::CapturePhysicsWorldState();
        mutator(state);
        renegade::bridge::ApplyPhysicsWorldState(state);
        return 0;
    }

    int LuaSetEnabled(lua_State* L)
    {
        return MutateWorld(L, [=](auto& state) {
            state.enabled = lua_toboolean(L, 1) != 0;
        });
    }

    int LuaSetSimulationEnabled(lua_State* L)
    {
        return MutateWorld(L, [=](auto& state) {
            state.simulationEnabled = lua_toboolean(L, 1) != 0;
        });
    }

    int LuaSetInterpolationEnabled(lua_State* L)
    {
        return MutateWorld(L, [=](auto& state) {
            state.interpolationEnabled = lua_toboolean(L, 1) != 0;
        });
    }

    int LuaSetDebugDrawEnabled(lua_State* L)
    {
        return MutateWorld(L, [=](auto& state) {
            state.debugDrawEnabled = lua_toboolean(L, 1) != 0;
        });
    }

    int LuaSetAccuracy(lua_State* L)
    {
        return MutateWorld(L, [=](auto& state) {
            state.accuracy = static_cast<int>(luaL_checkinteger(L, 1));
        });
    }

    int LuaSetFrameRate(lua_State* L)
    {
        return MutateWorld(L, [=](auto& state) {
            state.frameRate = static_cast<float>(luaL_checknumber(L, 1));
        });
    }

    int LuaSetConstraintDebugSize(lua_State* L)
    {
        return MutateWorld(L, [=](auto& state) {
            state.constraintDebugSize = static_cast<float>(luaL_checknumber(L, 1));
        });
    }

    int LuaSetDebugDrawMaxDistance(lua_State* L)
    {
        return MutateWorld(L, [=](auto& state) {
            state.debugDrawMaxDistance = static_cast<float>(luaL_checknumber(L, 1));
        });
    }

    int LuaSetCharacterCollisionTolerance(lua_State* L)
    {
        return MutateWorld(L, [=](auto& state) {
            state.characterCollisionTolerance =
                static_cast<float>(luaL_checknumber(L, 1));
        });
    }

    int LuaGetGravity(lua_State* L)
    {
        const auto* scene = Scene();
        if (scene == nullptr)
        {
            lua_pushnil(L);
            return 1;
        }
        PushVector(L, renegade::bridge::GetPhysicsGravity(*scene));
        return 1;
    }

    int LuaSetGravity(lua_State* L)
    {
        auto* scene = Scene();
        if (scene == nullptr)
        {
            PushBool(L, false);
            return 1;
        }
        renegade::bridge::SetPhysicsGravity(*scene, Vector3Arg(L, 1));
        PushBool(L, true);
        return 1;
    }

    int LuaHasBody(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr &&
            renegade::bridge::HasLivePhysicsBody(*scene, EntityArg(L, 1)));
        return 1;
    }

    int LuaSetPosition(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::SetPhysicsPosition(
            *scene, EntityArg(L, 1), Vector3Arg(L, 2)));
        return 1;
    }

    int LuaSetPositionAndRotation(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr &&
            renegade::bridge::SetPhysicsPositionAndRotation(
                *scene,
                EntityArg(L, 1),
                Vector3Arg(L, 2),
                Vector4Arg(L, 5)));
        return 1;
    }

    int LuaGetPosition(lua_State* L)
    {
        auto* scene = Scene();
        XMFLOAT3 value;
        if (scene == nullptr ||
            !renegade::bridge::GetPhysicsPosition(*scene, EntityArg(L, 1), value))
        {
            lua_pushnil(L);
            return 1;
        }
        PushVector(L, value);
        return 1;
    }

    int LuaGetRotation(lua_State* L)
    {
        auto* scene = Scene();
        XMFLOAT4 value;
        if (scene == nullptr ||
            !renegade::bridge::GetPhysicsRotation(*scene, EntityArg(L, 1), value))
        {
            lua_pushnil(L);
            return 1;
        }
        PushVector(L, value);
        return 1;
    }

    int LuaSetLinearVelocity(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::SetLinearVelocity(
            *scene, EntityArg(L, 1), Vector3Arg(L, 2)));
        return 1;
    }

    int LuaGetLinearVelocity(lua_State* L)
    {
        auto* scene = Scene();
        XMFLOAT3 value;
        if (scene == nullptr ||
            !renegade::bridge::GetLinearVelocity(*scene, EntityArg(L, 1), value))
        {
            lua_pushnil(L);
            return 1;
        }
        PushVector(L, value);
        return 1;
    }

    int LuaSetAngularVelocity(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::SetAngularVelocity(
            *scene, EntityArg(L, 1), Vector3Arg(L, 2)));
        return 1;
    }

    int LuaApplyForce(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::ApplyForce(
            *scene, EntityArg(L, 1), Vector3Arg(L, 2)));
        return 1;
    }

    int LuaApplyForceAt(lua_State* L)
    {
        auto* scene = Scene();
        const bool local = lua_gettop(L) < 8 || lua_toboolean(L, 8) != 0;
        PushBool(L, scene != nullptr && renegade::bridge::ApplyForceAt(
            *scene, EntityArg(L, 1), Vector3Arg(L, 2), Vector3Arg(L, 5), local));
        return 1;
    }

    int LuaApplyImpulse(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::ApplyImpulse(
            *scene, EntityArg(L, 1), Vector3Arg(L, 2)));
        return 1;
    }

    int LuaApplyImpulseAt(lua_State* L)
    {
        auto* scene = Scene();
        const bool local = lua_gettop(L) < 8 || lua_toboolean(L, 8) != 0;
        PushBool(L, scene != nullptr && renegade::bridge::ApplyImpulseAt(
            *scene, EntityArg(L, 1), Vector3Arg(L, 2), Vector3Arg(L, 5), local));
        return 1;
    }

    int LuaApplyTorque(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::ApplyTorque(
            *scene, EntityArg(L, 1), Vector3Arg(L, 2)));
        return 1;
    }

    int LuaSetActive(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::SetBodyActive(
            *scene, EntityArg(L, 1), lua_toboolean(L, 2) != 0));
        return 1;
    }

    int LuaSetGhost(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::SetGhostMode(
            *scene, EntityArg(L, 1), lua_toboolean(L, 2) != 0));
        return 1;
    }

    int LuaActivateAll(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr &&
            renegade::bridge::ActivateAllRigidBodies(*scene));
        return 1;
    }

    int LuaResetAll(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr &&
            renegade::bridge::ResetPhysicsObjects(*scene));
        return 1;
    }

    int LuaOptimizeBroadphase(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr &&
            renegade::bridge::OptimizePhysicsBroadPhase(*scene));
        return 1;
    }

    int LuaRaycast(lua_State* L)
    {
        auto* scene = Scene();
        if (scene == nullptr)
        {
            lua_pushnil(L);
            return 1;
        }
        const auto hit = renegade::bridge::Raycast(
            *scene,
            wi::primitive::Ray(Vector3Arg(L, 1), Vector3Arg(L, 4)));
        if (!hit.IsValid())
        {
            lua_pushnil(L);
            return 1;
        }
        lua_createtable(L, 0, 6);
        lua_pushinteger(L, static_cast<lua_Integer>(hit.entity));
        lua_setfield(L, -2, "entity");
        PushVector(L, hit.position); lua_setfield(L, -2, "position");
        PushVector(L, hit.localPosition); lua_setfield(L, -2, "local_position");
        PushVector(L, hit.normal); lua_setfield(L, -2, "normal");
        lua_pushinteger(L, static_cast<lua_Integer>(hit.ragdollEntity));
        lua_setfield(L, -2, "ragdoll_entity");
        lua_pushinteger(L, hit.softBodyTriangle);
        lua_setfield(L, -2, "soft_body_triangle");
        return 1;
    }

    int LuaHasCharacter(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::HasLiveCharacterPhysics(
            *scene, EntityArg(L, 1)));
        return 1;
    }

    int LuaCharacterMove(lua_State* L)
    {
        auto* scene = Scene();
        const bool controlInAir = lua_gettop(L) < 7 || lua_toboolean(L, 7) != 0;
        PushBool(L, scene != nullptr && renegade::bridge::MovePhysicsCharacter(
            *scene,
            EntityArg(L, 1),
            Vector3Arg(L, 2),
            static_cast<float>(luaL_checknumber(L, 5)),
            static_cast<float>(luaL_checknumber(L, 6)),
            controlInAir));
        return 1;
    }

    int LuaCharacterGround(lua_State* L)
    {
        auto* scene = Scene();
        renegade::bridge::CharacterGroundInfo info;
        if (scene == nullptr || !renegade::bridge::GetCharacterGroundInfo(
                *scene, EntityArg(L, 1), info))
        {
            lua_pushnil(L);
            return 1;
        }
        lua_createtable(L, 0, 5);
        PushVector(L, info.position); lua_setfield(L, -2, "position");
        PushVector(L, info.normal); lua_setfield(L, -2, "normal");
        PushVector(L, info.velocity); lua_setfield(L, -2, "velocity");
        PushBool(L, info.supported); lua_setfield(L, -2, "supported");
        const char* state = "not_supported";
        switch (info.state)
        {
        case renegade::bridge::CharacterGroundState::OnGround:
            state = "on_ground";
            break;
        case renegade::bridge::CharacterGroundState::OnSteepGround:
            state = "on_steep_ground";
            break;
        case renegade::bridge::CharacterGroundState::InAir:
            state = "in_air";
            break;
        default:
            break;
        }
        lua_pushstring(L, state); lua_setfield(L, -2, "state");
        return 1;
    }

    int LuaCharacterCapsule(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::ChangeCharacterCapsule(
            *scene,
            EntityArg(L, 1),
            static_cast<float>(luaL_checknumber(L, 2)),
            static_cast<float>(luaL_checknumber(L, 3))));
        return 1;
    }

    int LuaHasVehicle(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::HasLiveVehiclePhysics(
            *scene, EntityArg(L, 1)));
        return 1;
    }

    int LuaVehicleDrive(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::DrivePhysicsVehicle(
            *scene,
            EntityArg(L, 1),
            static_cast<float>(luaL_checknumber(L, 2)),
            static_cast<float>(luaL_checknumber(L, 3)),
            static_cast<float>(luaL_checknumber(L, 4)),
            static_cast<float>(luaL_checknumber(L, 5))));
        return 1;
    }

    int LuaVehicleVelocity(lua_State* L)
    {
        auto* scene = Scene();
        float velocity = 0.0f;
        if (scene == nullptr || !renegade::bridge::GetVehicleForwardVelocity(
                *scene, EntityArg(L, 1), velocity))
        {
            lua_pushnil(L);
            return 1;
        }
        lua_pushnumber(L, velocity);
        return 1;
    }

    int LuaVehicleUpdateWheels(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr &&
            renegade::bridge::UpdateVehicleWheelTransforms(*scene));
        return 1;
    }

    int LuaHasRagdoll(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::HasLiveRagdollPhysics(
            *scene, EntityArg(L, 1)));
        return 1;
    }

    bool RagdollBoneArg(
        lua_State* L,
        const int index,
        wi::scene::HumanoidComponent::HumanoidBone& bone)
    {
        const auto raw = static_cast<int>(luaL_checkinteger(L, index));
        if (raw < 0 || raw >= static_cast<int>(
                wi::scene::HumanoidComponent::HumanoidBone::Count))
        {
            return false;
        }
        bone = static_cast<wi::scene::HumanoidComponent::HumanoidBone>(raw);
        return true;
    }

    int LuaRagdollImpulse(lua_State* L)
    {
        auto* scene = Scene();
        wi::scene::HumanoidComponent::HumanoidBone bone{};
        PushBool(L, scene != nullptr && RagdollBoneArg(L, 2, bone) &&
            renegade::bridge::ApplyRagdollImpulse(
                *scene, EntityArg(L, 1), bone, Vector3Arg(L, 3)));
        return 1;
    }

    int LuaRagdollImpulseAt(lua_State* L)
    {
        auto* scene = Scene();
        wi::scene::HumanoidComponent::HumanoidBone bone{};
        const bool local = lua_gettop(L) < 9 || lua_toboolean(L, 9) != 0;
        PushBool(L, scene != nullptr && RagdollBoneArg(L, 2, bone) &&
            renegade::bridge::ApplyRagdollImpulseAt(
                *scene,
                EntityArg(L, 1),
                bone,
                Vector3Arg(L, 3),
                Vector3Arg(L, 6),
                local));
        return 1;
    }

    int LuaRagdollGhost(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::SetRagdollGhostMode(
            *scene, EntityArg(L, 1), lua_toboolean(L, 2) != 0));
        return 1;
    }

    int LuaActivateAllRagdolls(lua_State* L)
    {
        auto* scene = Scene();
        if (scene == nullptr)
        {
            PushBool(L, false);
            return 1;
        }
        renegade::bridge::ActivateAllRagdolls(*scene);
        PushBool(L, true);
        return 1;
    }

    int LuaHasSoftBody(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::HasLiveSoftBodyPhysics(
            *scene, EntityArg(L, 1)));
        return 1;
    }

    int LuaSoftBodyActive(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::SetSoftBodyActive(
            *scene, EntityArg(L, 1), lua_toboolean(L, 2) != 0));
        return 1;
    }

    int LuaSoftBodyNode(lua_State* L)
    {
        auto* scene = Scene();
        XMFLOAT3 position;
        if (scene == nullptr || !renegade::bridge::GetSoftBodyNodePosition(
                *scene,
                EntityArg(L, 1),
                static_cast<uint32_t>(luaL_checkinteger(L, 2)),
                position))
        {
            lua_pushnil(L);
            return 1;
        }
        PushVector(L, position);
        return 1;
    }

    int LuaSoftBodyReset(lua_State* L)
    {
        auto* scene = Scene();
        PushBool(L, scene != nullptr && renegade::bridge::ResetSoftBodyPhysics(
            *scene, EntityArg(L, 1)));
        return 1;
    }

    int LuaConstraintBroken(lua_State* L)
    {
        auto* scene = Scene();
        if (scene == nullptr)
        {
            lua_pushnil(L);
            return 1;
        }
        const auto* constraint = scene->constraints.GetComponent(EntityArg(L, 1));
        if (constraint == nullptr || constraint->physicsobject == nullptr)
        {
            lua_pushnil(L);
            return 1;
        }

        // Pinned upstream currently returns the native constraint's enabled
        // state from IsConstraintBroken(), while SetConstraintBroken(true)
        // disables that same constraint. Renegade's public contract keeps the
        // word "broken" semantically consistent without patching upstream.
        PushBool(L, !wi::physics::IsConstraintBroken(*constraint));
        return 1;
    }

    int LuaSetConstraintBroken(lua_State* L)
    {
        auto* scene = Scene();
        if (scene == nullptr)
        {
            PushBool(L, false);
            return 1;
        }
        auto* constraint = scene->constraints.GetComponent(EntityArg(L, 1));
        if (constraint == nullptr || constraint->physicsobject == nullptr)
        {
            PushBool(L, false);
            return 1;
        }
        wi::physics::SetConstraintBroken(
            *constraint,
            lua_gettop(L) < 2 || lua_toboolean(L, 2) != 0);
        PushBool(L, true);
        return 1;
    }

    void SetFunction(lua_State* L, const char* name, lua_CFunction function)
    {
        lua_pushcfunction(L, function);
        lua_setfield(L, -2, name);
    }

    void InstallNamespace(lua_State* L)
    {
        lua_getglobal(L, "renegade");
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setglobal(L, "renegade");
        }

        lua_getfield(L, -1, "physics");
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, -3, "physics");
        }

        lua_pushinteger(L, 1);
        lua_setfield(L, -2, "contract_version");

        SetFunction(L, "get_world", LuaGetWorld);
        SetFunction(L, "set_enabled", LuaSetEnabled);
        SetFunction(L, "set_simulation_enabled", LuaSetSimulationEnabled);
        SetFunction(L, "set_interpolation_enabled", LuaSetInterpolationEnabled);
        SetFunction(L, "set_debug_draw_enabled", LuaSetDebugDrawEnabled);
        SetFunction(L, "set_accuracy", LuaSetAccuracy);
        SetFunction(L, "set_frame_rate", LuaSetFrameRate);
        SetFunction(L, "set_constraint_debug_size", LuaSetConstraintDebugSize);
        SetFunction(L, "set_debug_draw_max_distance", LuaSetDebugDrawMaxDistance);
        SetFunction(L, "set_character_collision_tolerance", LuaSetCharacterCollisionTolerance);
        SetFunction(L, "get_gravity", LuaGetGravity);
        SetFunction(L, "set_gravity", LuaSetGravity);

        SetFunction(L, "has_body", LuaHasBody);
        SetFunction(L, "set_position", LuaSetPosition);
        SetFunction(L, "set_position_and_rotation", LuaSetPositionAndRotation);
        SetFunction(L, "get_position", LuaGetPosition);
        SetFunction(L, "get_rotation", LuaGetRotation);
        SetFunction(L, "set_linear_velocity", LuaSetLinearVelocity);
        SetFunction(L, "get_linear_velocity", LuaGetLinearVelocity);
        SetFunction(L, "set_angular_velocity", LuaSetAngularVelocity);
        SetFunction(L, "apply_force", LuaApplyForce);
        SetFunction(L, "apply_force_at", LuaApplyForceAt);
        SetFunction(L, "apply_impulse", LuaApplyImpulse);
        SetFunction(L, "apply_impulse_at", LuaApplyImpulseAt);
        SetFunction(L, "apply_torque", LuaApplyTorque);
        SetFunction(L, "set_active", LuaSetActive);
        SetFunction(L, "set_ghost", LuaSetGhost);
        SetFunction(L, "activate_all", LuaActivateAll);
        SetFunction(L, "reset_all", LuaResetAll);
        SetFunction(L, "optimize_broadphase", LuaOptimizeBroadphase);
        SetFunction(L, "raycast", LuaRaycast);

        SetFunction(L, "has_character", LuaHasCharacter);
        SetFunction(L, "character_move", LuaCharacterMove);
        SetFunction(L, "character_ground", LuaCharacterGround);
        SetFunction(L, "character_change_capsule", LuaCharacterCapsule);

        SetFunction(L, "has_vehicle", LuaHasVehicle);
        SetFunction(L, "vehicle_drive", LuaVehicleDrive);
        SetFunction(L, "vehicle_velocity", LuaVehicleVelocity);
        SetFunction(L, "vehicle_update_wheels", LuaVehicleUpdateWheels);

        SetFunction(L, "has_ragdoll", LuaHasRagdoll);
        SetFunction(L, "ragdoll_impulse", LuaRagdollImpulse);
        SetFunction(L, "ragdoll_impulse_at", LuaRagdollImpulseAt);
        SetFunction(L, "ragdoll_set_ghost", LuaRagdollGhost);
        SetFunction(L, "ragdoll_activate_all", LuaActivateAllRagdolls);

        SetFunction(L, "has_soft_body", LuaHasSoftBody);
        SetFunction(L, "soft_body_set_active", LuaSoftBodyActive);
        SetFunction(L, "soft_body_node_position", LuaSoftBodyNode);
        SetFunction(L, "soft_body_reset", LuaSoftBodyReset);

        SetFunction(L, "constraint_broken", LuaConstraintBroken);
        SetFunction(L, "constraint_set_broken", LuaSetConstraintBroken);

        lua_pop(L, 2); // physics, renegade
    }
}

namespace renegade::bridge
{
    bool BindPhysicsLua(
        wi::scene::Scene& scene,
        lua_State* state) noexcept
    {
        if (state == nullptr)
        {
            return false;
        }

        // The engine owns creation/destruction and initialization ordering for
        // its global VM. JP01 only installs functions into a VM that exists.
        boundScene = &scene;
        InstallNamespace(state);
        return true;
    }

    bool BindPhysicsLua(wi::scene::Scene& scene) noexcept
    {
        return BindPhysicsLua(scene, wi::lua::GetLuaState());
    }

    void UnbindPhysicsLua(const wi::scene::Scene* scene) noexcept
    {
        if (scene == nullptr || scene == boundScene)
            boundScene = nullptr;
    }

    bool IsPhysicsLuaBound() noexcept
    {
        return boundScene != nullptr;
    }

    bool IsPhysicsLuaBoundTo(const wi::scene::Scene& scene) noexcept
    {
        return boundScene == &scene;
    }
}
