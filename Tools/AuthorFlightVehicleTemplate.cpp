
#include "renegade/bridge/FlightVehicleService.h"
#include "renegade/bridge/PlayerService.h"
#include <Windows.h>
#include <iostream>

namespace {
using Body = wi::scene::RigidBodyPhysicsComponent;

wi::ecs::Entity Box(wi::scene::Scene& scene, const char* name, XMFLOAT3 position,
                   XMFLOAT3 scale, XMFLOAT4 color, wi::ecs::Entity parent = wi::ecs::INVALID_ENTITY,
                   float yaw = 0.f, float emissive = 0.f)
{
    const auto entity = scene.Entity_CreateCube(name);
    if (parent != wi::ecs::INVALID_ENTITY)
        scene.Component_Attach(entity, parent, true);
    auto& transform = *scene.transforms.GetComponent(entity);
    transform.translation_local = position;
    transform.scale_local = scale;
    if (yaw != 0.f) transform.RotateRollPitchYaw(XMFLOAT3(0,yaw,0));
    transform.SetDirty();
    auto* material = scene.materials.GetComponent(entity);
    if (material)
    {
        material->SetBaseColor(color);
        material->SetRoughness(.54f);
        if (emissive > 0.f)
            material->SetEmissiveColor({color.x,color.y,color.z,emissive});
    }
    return entity;
}

wi::ecs::Entity RigidBox(wi::scene::Scene& scene, const char* name, XMFLOAT3 position,
                        XMFLOAT3 halfExtents, float mass)
{
    const auto entity = scene.Entity_CreateTransform(name);
    auto& transform = *scene.transforms.GetComponent(entity);
    transform.translation_local = position;
    transform.SetDirty();
    auto& rigid = scene.rigidbodies.Create(entity);
    rigid.shape = Body::CollisionShape::BOX;
    rigid.box.halfextents = halfExtents;
    rigid.mass = mass;
    rigid.friction = .85f;
    return entity;
}
}

int main(int argc,char** argv)
{
    if (argc != 2) return 2;
    HWND window = CreateWindowExW(0,L"STATIC",L"VTOL template authoring",WS_OVERLAPPEDWINDOW,
        0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application app;
    app.allow_hdr = false;
    app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain {
        ~Drain(){
            while(wi::renderer::IsPipelineCreationActive()) Sleep(10);
            wi::graphics::GetDevice()->WaitForGPU();
        }
    } drain;
    wi::physics::SetEnabled(true);
    wi::physics::SetSimulationEnabled(false);

    wi::scene::Scene scene;
    const auto hull = RigidBox(scene,"VTOL Flight Chassis",{0,.60f,0},{1.65f,.33f,1.58f},940.f);
    scene.metadatas.Create(hull).bool_values.set(renegade::bridge::FlightVehicleKey,true);
    const XMFLOAT4 hullPaint = {.24f,.31f,.38f,1};
    const XMFLOAT4 gray = {.11f,.16f,.20f,1};
    const XMFLOAT4 orange = {.91f,.25f,.07f,1};
    const XMFLOAT4 glass = {.048f,.15f,.22f,1};
    const XMFLOAT4 thrust = {.16f,.62f,1.f,1};

    Box(scene,"Central fuselage",{0,.08f,0},{.57f,.28f,1.19f},hullPaint,hull);
    Box(scene,"Nose",{0,.04f,1.24f},{.43f,.19f,.40f},hullPaint,hull);
    Box(scene,"Canopy",{0,.40f,.52f},{.43f,.22f,.53f},glass,hull);
    Box(scene,"Dorsal equipment",{0,.37f,-.60f},{.39f,.13f,.49f},gray,hull);
    Box(scene,"Wing spar",{0,.015f,-.25f},{1.29f,.10f,.36f},gray,hull);
    for(float side : {-1.f,1.f})
    {
        Box(scene,"Thruster housing",{side*1.22f,.015f,-.30f},{.35f,.23f,.86f},hullPaint,hull);
        Box(scene,"Thruster stripes",{side*1.22f,.257f,-.30f},{.26f,.018f,.44f},orange,hull);
        Box(scene,"Thrust nozzle",{side*1.22f,-.22f,-.30f},{.26f,.026f,.57f},thrust,hull,0.f,.65f);
        Box(scene,"Landing skid",{side*.92f,-.30f,.01f},{.11f,.035f,1.01f},gray,hull);
        Box(scene,"Navigation strip",{side*.33f,.09f,1.61f},{.085f,.049f,.024f},thrust,hull,0.f,1.f);
        Box(scene,"Tail", {side*.42f,.38f,-1.17f},{.09f,.36f,.32f},hullPaint,hull);
    }
    Box(scene,"Flight range",{0,-.22f,0},{190.f,.20f,190.f},{.23f,.26f,.30f,1});
    RigidBox(scene,"Flight Range Collision",{0,-.5f,0},{190,.5f,190},0.f);
    const XMFLOAT4 paint={.90f,.75f,.21f,1};
    for(float x : {-3.8f,3.8f})
        Box(scene,"Pad edge",{x,.007f,0},{.055f,.012f,3.8f},paint);
    for(float z : {-3.8f,3.8f})
        Box(scene,"Pad edge",{0,.007f,z},{3.8f,.012f,.055f},paint);
    for(float x : {-1.15f,1.15f})
        Box(scene,"Landing marker",{x,.008f,0},{.12f,.012f,1.16f},paint);
    Box(scene,"Landing marker",{0,.008f,0},{1.15f,.011f,.12f},paint);
    for(float x : {-19.f,19.f})
    {
        Box(scene,"Flight course pylon",{x,2.0f,30.f},{.32f,2.0f,.32f},orange);
        RigidBox(scene,"Pylon Collision",{x,2.0f,30.f},{.32f,2.0f,.32f},0.f);
        Box(scene,"Flight course beacon",{x,4.15f,30.f},{.36f,.15f,.36f},thrust,wi::ecs::INVALID_ENTITY,0.f,1.5f);
    }

    renegade::bridge::TransformState spawn;
    spawn.translation={-3.15f,.37f,.5f};
    renegade::bridge::CreatePlayerStartCommand player(scene,spawn);
    if(!player.Execute()) return 3;
    const auto sun = scene.Entity_CreateLight("Daylight",{0,80,-20},{1,.95f,.85f},2.2f,200,
        wi::scene::LightComponent::DIRECTIONAL);
    scene.transforms.GetComponent(sun)->RotateRollPitchYaw(XMFLOAT3(1.15f,-.65f,0));
    const auto sky = scene.Entity_CreateTransform("Daylight Sky");
    auto& weather = scene.weathers.Create(sky);
    weather.ambient={.18f,.19f,.21f};
    weather.horizon={.67f,.76f,.87f};
    weather.zenith={.24f,.43f,.68f};
    scene.Update(0);
    wi::graphics::GetDevice()->SubmitCommandLists();
    wi::Archive output;
    scene.Serialize(output);
    if(!output.SaveFile(argv[1])) return 4;

    wi::Archive input(argv[1],true,false);
    if(!input.IsOpen()) return 5;
    wi::scene::Scene reopened;
    reopened.Serialize(input);
    bool hasFlightTag = false;
    for(size_t i=0;i<reopened.metadatas.GetCount();++i)
    {
        const auto& flags = reopened.metadatas[i].bool_values;
        if(flags.has("renegade.vehicle.flight") && flags.get("renegade.vehicle.flight"))
        {
            const auto* body = reopened.rigidbodies.GetComponent(reopened.metadatas.GetEntity(i));
            hasFlightTag = body && body->mass > 0.f;
        }
    }
    if(!hasFlightTag ||
       renegade::bridge::ResolvePlayerStart(reopened).resolution!=renegade::bridge::PlayerStartResolution::Success)
        return 6;
    std::cout << "VTOL template save/reopen PASS: flight metadata, dynamic hull and Player Start" << std::endl;
    return 0;
}
