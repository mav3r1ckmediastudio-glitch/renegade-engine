#include "renegade/bridge/PhysicsLuaService.h"

#include "renegade/bridge/CollisionService.h"
#include "renegade/bridge/ConstraintService.h"
#include "renegade/bridge/CharacterPhysicsService.h"
#include "renegade/bridge/VehiclePhysicsService.h"
#include "renegade/bridge/RagdollPhysicsService.h"
#include "renegade/bridge/SoftBodyPhysicsService.h"
#include "renegade/bridge/WickedColliderService.h"
#include "renegade/bridge/FlightVehicleService.h"
#include "renegade/bridge/PhysicsGameplayService.h"
#include "renegade/bridge/PhysicsService.h"

#include <limits>
#include <type_traits>
#include "renegade/bridge/TerrainPropCollisionWorld.h"

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

    int LuaTerrainInterest(lua_State* L)
    {
        const auto id=luaL_checkinteger(L,1);
        const auto position=Vector3Arg(L,2);
        const float radius=static_cast<float>(luaL_optnumber(L,5,1));
        const auto velocity=lua_gettop(L)>=8?Vector3Arg(L,6):XMFLOAT3{0,0,0};
        auto* scene=Scene();
        const auto world=scene?renegade::bridge::TerrainPropCollisionWorld::Find(*scene):nullptr;
        std::string error;
        const bool success=world&&id>0&&world->SetInterest(static_cast<std::uint64_t>(id),
            {position,velocity,radius},error);
        if(!success&&error.empty())error="Terrain collision interest requires a positive ID and active scene owner.";
        PushBool(L,success);lua_pushlstring(L,error.data(),error.size());return 2;
    }
    int LuaClearTerrainInterest(lua_State* L)
    {
        const auto id=luaL_checkinteger(L,1);
        auto* scene=Scene();
        const auto world=scene?renegade::bridge::TerrainPropCollisionWorld::Find(*scene):nullptr;
        if(world&&id>0)world->RemoveInterest(static_cast<std::uint64_t>(id));
        PushBool(L,world&&id>0);return 1;
    }
    int LuaCancelTerrainDestination(lua_State* L)
    {
        const auto actor=EntityArg(L,1);auto* scene=Scene();
        const auto world=scene?renegade::bridge::TerrainPropCollisionWorld::Find(*scene):nullptr;
        if(world)world->CancelDestination(actor);
        PushBool(L,world!=nullptr);return 1;
    }
    int LuaTerrainCollisionState(lua_State* L)
    {
        auto* scene=Scene();
        const auto world=scene?renegade::bridge::TerrainPropCollisionWorld::Find(*scene):nullptr;
        if(!world){lua_pushnil(L);return 1;}
        const auto state=world->Stats();
        lua_createtable(L,0,10);
        PushBool(L,state.ready);lua_setfield(L,-2,"ready");
        const auto number=[&](const char* name,std::size_t value){
            lua_pushinteger(L,static_cast<lua_Integer>(value));lua_setfield(L,-2,name);
        };
        number("indexed",state.indexed);number("resident",state.resident);number("required",state.required);
        number("native_allocated",state.nativeAllocated);number("native_capacity",state.nativeCapacity);
        number("terrain_required",state.terrainRequired);number("terrain_ready",state.terrainReady);
        number("query_work",state.queryNodes);
        lua_pushlstring(L,state.diagnostic.data(),state.diagnostic.size());lua_setfield(L,-2,"diagnostic");
        return 1;
    }

    int LuaBodySensor(lua_State* L)
    {
        const auto entity = EntityArg(L, 1);
        const auto* scene = Scene();
        if (!scene || !scene->rigidbodies.Contains(entity))
            lua_pushnil(L);
        else
            PushBool(L, renegade::bridge::IsBodySensor(*scene, entity));
        return 1;
    }

    int LuaSetBodySensor(lua_State* L)
    {
        const auto entity = EntityArg(L, 1);
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        auto* scene = Scene();
        PushBool(L, scene && renegade::bridge::SetBodySensor(*scene, entity, lua_toboolean(L, 2) != 0));
        return 1;
    }

    int LuaGetBodyCollisionFilter(lua_State* L)
    {
        const auto entity = EntityArg(L, 1);
        const auto* scene = Scene();
        const auto* body = scene ? scene->rigidbodies.GetComponent(entity) : nullptr;
        if (!body || !renegade::bridge::SupportsBodyCollisionFilter(*body))
        {
            lua_pushnil(L);
            return 1;
        }
        const auto filter = renegade::bridge::GetBodyCollisionFilter(*scene, entity);
        lua_pushinteger(L, filter.layer);
        lua_pushinteger(L, filter.mask);
        return 2;
    }

    int LuaSetBodyCollisionFilter(lua_State* L)
    {
        using namespace renegade::bridge;
        const auto entity = EntityArg(L, 1);
        int validLayer = 0, validMask = 0;
        const auto layer = lua_tointegerx(L, 2, &validLayer);
        const auto mask = lua_tointegerx(L, 3, &validMask);
        auto* scene = Scene();
        PushBool(L, scene && validLayer && validMask && layer >= 0 && layer < BodyCollisionLayerCount &&
            mask >= 0 && mask <= AllBodyCollisionLayers &&
            SetBodyCollisionFilter(*scene, entity, {static_cast<int>(layer), static_cast<int>(mask)}));
        return 1;
    }

    int LuaSetDestructibleImpactSpeed(lua_State* L)
    {
        const auto entity = EntityArg(L, 1);
        int valid = 0;
        const auto speed = lua_tonumberx(L, 2, &valid);
        auto* scene = Scene();
        PushBool(L, scene && valid &&
            renegade::bridge::SetDestructibleImpactSpeed(*scene, entity, static_cast<float>(speed)));
        return 1;
    }

    int LuaBreakDestructible(lua_State* L)
    {
        const auto entity = EntityArg(L, 1);
        auto* scene = Scene();
        PushBool(L, scene && renegade::bridge::RequestDestructibleBreak(*scene, entity));
        return 1;
    }

    int LuaDrainDestructibleBreaks(lua_State* L)
    {
        const auto* scene = Scene();
        const auto events = scene ? renegade::bridge::DrainDestructibleBreakEvents(*scene)
                                  : std::vector<renegade::bridge::DestructibleBreakEvent>{};
        lua_createtable(L, static_cast<int>(events.size()), 0);
        lua_Integer index = 1;
        for (const auto& event : events)
        {
            lua_createtable(L, 0, 4);
            lua_pushinteger(L, static_cast<lua_Integer>(event.entity)); lua_setfield(L, -2, "entity");
            lua_pushinteger(L, static_cast<lua_Integer>(event.impactor)); lua_setfield(L, -2, "impactor");
            lua_pushnumber(L, event.impactSpeed); lua_setfield(L, -2, "impact_speed");
            lua_pushinteger(L, event.pieceCount); lua_setfield(L, -2, "piece_count");
            lua_rawseti(L, -2, index++);
        }
        return 1;
    }

    int LuaEnableContactEvents(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TBOOLEAN);
        auto* scene = Scene();
        PushBool(L, scene && renegade::bridge::EnableContactEvents(*scene, lua_toboolean(L, 1) != 0));
        return 1;
    }

    int LuaDrainContactEvents(lua_State* L)
    {
        const auto* scene = Scene();
        const auto batch = scene ? renegade::bridge::DrainContactEvents(*scene)
                                 : renegade::bridge::ContactEventBatch{};
        lua_createtable(L, static_cast<int>(batch.events.size()), 0);
        lua_Integer index = 1;
        for (const auto& event : batch.events)
        {
            lua_createtable(L, 0, 6);
            lua_pushinteger(L, static_cast<lua_Integer>(event.bodyA)); lua_setfield(L, -2, "body_a");
            lua_pushinteger(L, static_cast<lua_Integer>(event.bodyB)); lua_setfield(L, -2, "body_b");
            lua_pushinteger(L, event.subshapeA); lua_setfield(L, -2, "subshape_a");
            lua_pushinteger(L, event.subshapeB); lua_setfield(L, -2, "subshape_b");
            PushBool(L, event.sensor); lua_setfield(L, -2, "sensor");
            lua_pushstring(L, event.began ? "begin" : "end"); lua_setfield(L, -2, "phase");
            lua_rawseti(L, -2, index++);
        }
        lua_pushinteger(L, batch.dropped);
        return 2;
    }

    int LuaRaycastBodies(lua_State* L)
    {
        const auto badArgument = [L](const char* message) {
            lua_pushnil(L);
            lua_pushstring(L, message);
            return 2;
        };
        const auto readCoordinate = [L](int index, float& value) {
            int numeric = 0;
            value = static_cast<float>(lua_tonumberx(L, index, &numeric));
            return numeric && std::isfinite(value);
        };

        XMFLOAT3 from, to;
        if (!readCoordinate(1, from.x) || !readCoordinate(2, from.y) || !readCoordinate(3, from.z) ||
            !readCoordinate(4, to.x) || !readCoordinate(5, to.y) || !readCoordinate(6, to.z))
            return badArgument("A ray requires six finite coordinates.");

        lua_Integer mask = UINT32_MAX;
        if (!lua_isnoneornil(L, 7))
        {
            int integer = 0;
            mask = lua_tointegerx(L, 7, &integer);
            if (!integer || mask < 0 || static_cast<uint64_t>(mask) > UINT32_MAX)
                return badArgument("Expected a 32-bit unsigned scene layer mask.");
        }

        auto exclude = wi::ecs::INVALID_ENTITY;
        if (!lua_isnoneornil(L, 8))
        {
            int integer = 0;
            const auto entity = lua_tointegerx(L, 8, &integer);
            if (!integer || entity < 0)
                return badArgument("Expected an entity ID.");
            exclude = static_cast<wi::ecs::Entity>(entity);
        }
        if (!lua_isnoneornil(L, 9) && !lua_isboolean(L, 9))
            return badArgument("Expected a boolean for sensor inclusion.");

        const auto* scene = Scene();
        wi::physics::terrain_props::NativeHit hit;
        std::string error;
        if (!scene || !renegade::bridge::RaycastBodies(*scene, from, to, static_cast<uint32_t>(mask),
                exclude, lua_toboolean(L, 9) != 0, hit, error))
        {
            lua_pushnil(L);
            lua_pushstring(L, scene ? error.c_str() : "No physics scene is bound.");
            return 2;
        }
        if (hit.entity == wi::ecs::INVALID_ENTITY)
        {
            lua_pushnil(L);
            return 1;
        }
        lua_createtable(L, 0, 4);
        lua_pushinteger(L, static_cast<lua_Integer>(hit.entity)); lua_setfield(L, -2, "entity");
        lua_pushnumber(L, hit.fraction); lua_setfield(L, -2, "fraction");
        PushVector(L, hit.position); lua_setfield(L, -2, "position");
        PushVector(L, hit.normal); lua_setfield(L, -2, "normal");
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
        if (!hit.queryReady)
            return luaL_error(L, "Terrain collision query is unavailable; query blocked.");
        if (!hit.IsValid())
        {
            lua_pushnil(L);
            return 1;
        }
        lua_createtable(L, 0, 8);
        lua_pushlstring(L,hit.terrainPropLayer.data(),hit.terrainPropLayer.size());
        lua_setfield(L,-2,"terrain_prop_layer");
        const auto placement=std::to_string(hit.terrainPropPlacement);
        lua_pushlstring(L,placement.data(),placement.size());
        lua_setfield(L,-2,"terrain_prop_placement");
        lua_pushstring(L,renegade::bridge::ImpactSurfaceTypeToken(hit.surface));lua_setfield(L,-2,"surface");
        lua_pushnumber(L,hit.friction);lua_setfield(L,-2,"friction");
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

    int LuaPhysicalVtolCommand(lua_State* L)
    {
        auto* scene = Scene();
        const auto entity = EntityArg(L, 1);
        const renegade::bridge::FlightVehicleInput input{
            static_cast<float>(luaL_checknumber(L, 2)),
            static_cast<float>(luaL_checknumber(L, 3)),
            static_cast<float>(luaL_checknumber(L, 4))};
        const bool enabled = lua_isnoneornil(L, 5) || lua_toboolean(L, 5);
        PushBool(L, scene && renegade::bridge::CommandPhysicalVtol(*scene, entity, input, enabled));
        return 1;
    }

    int LuaPhysicalVtolGrounded(lua_State* L)
    {
        const auto* scene = Scene();
        PushBool(L, scene && renegade::bridge::IsPhysicalVtolGrounded(*scene, EntityArg(L, 1)));
        return 1;
    }

    int LuaHelicopterCommand(lua_State* L)
    {
        auto* scene = Scene();
        const auto entity = EntityArg(L, 1);
        const renegade::bridge::FlightVehicleInput input{
            float(luaL_checknumber(L, 2)), float(luaL_checknumber(L, 3)), float(luaL_checknumber(L, 4))};
        const bool enabled = lua_isnoneornil(L, 5) || lua_toboolean(L, 5);
        PushBool(L, scene && renegade::bridge::CommandHelicopter(*scene, entity, input, enabled));
        return 1;
    }

    int LuaHelicopterGrounded(lua_State* L)
    {
        const auto* scene = Scene();
        PushBool(L, scene && renegade::bridge::IsHelicopterGrounded(*scene, EntityArg(L, 1)));
        return 1;
    }

    int LuaSpacecraftCommand(lua_State* L)
    {
        auto* scene = Scene();
        const auto entity = EntityArg(L, 1);
        const renegade::bridge::SpacecraftInput input{
            float(luaL_checknumber(L, 2)), float(luaL_checknumber(L, 3)), float(luaL_checknumber(L, 4)),
            float(luaL_checknumber(L, 5)), float(luaL_checknumber(L, 6)), float(luaL_checknumber(L, 7))};
        const bool enabled = lua_isnoneornil(L, 8) || lua_toboolean(L, 8);
        PushBool(L, scene && renegade::bridge::CommandSpacecraft(*scene, entity, input, enabled));
        return 1;
    }

    int LuaHasFlightVehicle(lua_State* L)
    {
        const auto* scene = Scene();
        PushBool(L, scene && renegade::bridge::HasLiveFlightVehicle(*scene, EntityArg(L, 1)));
        return 1;
    }

    int LuaFlightVehicleDrive(lua_State* L)
    {
        auto* scene = Scene();
        const auto entity = EntityArg(L, 1);
        const renegade::bridge::FlightVehicleInput input{
            static_cast<float>(luaL_checknumber(L, 2)),
            static_cast<float>(luaL_checknumber(L, 3)),
            static_cast<float>(luaL_checknumber(L, 4))};
        const float dt = static_cast<float>(luaL_checknumber(L, 5));
        float altitude = static_cast<float>(
            luaL_optnumber(L, 6, std::numeric_limits<float>::quiet_NaN()));
        if (scene && renegade::bridge::DriveFlightVehicle(*scene, entity, input, dt, altitude))
            lua_pushnumber(L, altitude);
        else
            lua_pushnil(L);
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

    int LuaSoftBodyGetPins(lua_State* L)
    {
        auto* scene = Scene();
        const auto entity = EntityArg(L, 1);
        wi::vector<uint32_t> pins;
        if (!scene || !renegade::bridge::GetSoftBodyPinnedVertices(*scene, entity, pins))
        {
            lua_pushnil(L);
            return 1;
        }

        lua_createtable(L, static_cast<int>(pins.size()), 0);
        lua_Integer slot = 1;
        for (auto index : pins)
        {
            lua_pushinteger(L, index);
            lua_rawseti(L, -2, slot++);
        }
        return 1;
    }

    int LuaSoftBodySetPins(lua_State* L)
    {
        auto* scene = Scene();
        const auto entity = EntityArg(L, 1);
        luaL_checktype(L, 2, LUA_TTABLE);
        const auto meshEntity = scene ? renegade::bridge::ResolveSoftBodyMeshEntity(*scene, entity)
                                      : wi::ecs::INVALID_ENTITY;
        const auto* mesh = scene ? scene->meshes.GetComponent(meshEntity) : nullptr;
        const auto length = lua_rawlen(L, 2);
        if (!mesh || length > mesh->vertex_positions.size())
        {
            PushBool(L, false);
            return 1;
        }
        wi::vector<uint32_t> pins;
        lua_pushnil(L);
        while (lua_next(L, 2) != 0)
        {
            const bool integer = lua_isinteger(L, -2);
            const auto key = lua_tointeger(L, -2);
            lua_pop(L, 1);
            if (!integer || key < 1 || static_cast<uint64_t>(key) > length)
            {
                lua_pop(L, 1);
                PushBool(L, false);
                return 1;
            }
        }
        pins.reserve(length);
        for (size_t slot = 1; slot <= length; ++slot)
        {
            lua_rawgeti(L, 2, static_cast<lua_Integer>(slot));
            const bool integer = lua_isinteger(L, -1);
            const auto index = lua_tointeger(L, -1);
            lua_pop(L, 1);
            if (!integer || index < 0 || static_cast<uint64_t>(index) >= mesh->vertex_positions.size() ||
                static_cast<uint64_t>(index) > UINT32_MAX)
            {
                PushBool(L, false);
                return 1;
            }
            pins.push_back(static_cast<uint32_t>(index));
        }
        renegade::bridge::SetSoftBodyPinsCommand command(*scene, entity, std::move(pins));
        PushBool(L, command.Execute());
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

#include "PhysicsLuaAuthoring.inc"

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

        InstallPhysicsAuthoring(L);
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
        SetFunction(L, "body_is_sensor", LuaBodySensor);
        SetFunction(L, "body_set_sensor", LuaSetBodySensor);
        SetFunction(L, "body_get_collision_filter", LuaGetBodyCollisionFilter);
        SetFunction(L, "body_set_collision_filter", LuaSetBodyCollisionFilter);
        SetFunction(L, "enable_contact_events", LuaEnableContactEvents);
        SetFunction(L, "set_destructible_impact_speed", LuaSetDestructibleImpactSpeed);
        SetFunction(L, "break_destructible", LuaBreakDestructible);
        SetFunction(L, "drain_destructible_breaks", LuaDrainDestructibleBreaks);
        SetFunction(L, "drain_contact_events", LuaDrainContactEvents);
        SetFunction(L, "raycast_bodies", LuaRaycastBodies);
        SetFunction(L, "raycast", LuaRaycast);
        SetFunction(L, "set_terrain_interest", LuaTerrainInterest);
        SetFunction(L, "clear_terrain_interest", LuaClearTerrainInterest);
        SetFunction(L, "cancel_terrain_destination", LuaCancelTerrainDestination);
        SetFunction(L, "terrain_collision_state", LuaTerrainCollisionState);

        SetFunction(L, "has_character", LuaHasCharacter);
        SetFunction(L, "character_move", LuaCharacterMove);
        SetFunction(L, "character_ground", LuaCharacterGround);
        SetFunction(L, "character_change_capsule", LuaCharacterCapsule);

        SetFunction(L, "has_vehicle", LuaHasVehicle);
        SetFunction(L, "has_flight_vehicle", LuaHasFlightVehicle);
        SetFunction(L, "flight_vehicle_drive", LuaFlightVehicleDrive);
        SetFunction(L, "physical_vtol_command", LuaPhysicalVtolCommand);
        SetFunction(L, "physical_vtol_grounded", LuaPhysicalVtolGrounded);
        SetFunction(L, "helicopter_command", LuaHelicopterCommand);
        SetFunction(L, "helicopter_grounded", LuaHelicopterGrounded);
        SetFunction(L, "spacecraft_command", LuaSpacecraftCommand);
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
        SetFunction(L, "soft_body_get_pinned_vertices", LuaSoftBodyGetPins);
        SetFunction(L, "soft_body_set_pinned_vertices", LuaSoftBodySetPins);
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
