#include "renegade/bridge/TerrainPropPhysicsAdapter.h"
#include <wiPhysics.h>
#include <wiJobSystem.h>
#include <wiPrimitive.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace p = wi::physics::terrain_props;
namespace
{
    int checks = 0;
    void Require(bool value, const char* message)
    {
        ++checks;
        if (!value) throw std::runtime_error(message);
    }
    bool Near(float a, float b) { return std::abs(a-b) < .002f; }
    wi::ecs::Entity Body(wi::scene::Scene& scene, const XMFLOAT3& position)
    {
        auto entity = wi::ecs::CreateEntity();
        auto& transform = scene.transforms.Create(entity);
        transform.Translate(position);
        transform.UpdateTransform();
        scene.rigidbodies.Create(entity).mass = 0;
        return entity;
    }
}
int main()
{
    try
    {
        wi::physics::Initialize();
        struct Workers{~Workers(){wi::jobsystem::ShutDown();}} workers;
        p::Shape trunk, box;
        std::string error;
        Require(p::CreateShape(p::ShapeKind::Capsule, {.5f,1,1}, trunk,error), "capsule creation");
        Require(p::CreateShape(p::ShapeKind::Box, {1,1,1}, box,error), "box creation");
        p::CastHit hit;
        Require(p::CastSegment(trunk, {-2,0,0},{2,0,0},0,hit,error) &&
            hit.hit && Near(hit.fraction,.375f) && hit.normal.x<-.99f, "trunk ray");
        Require(p::CastSegment(trunk, {-2,4,0},{2,4,0},0,hit,error) &&
            !hit.hit, "canopy remains noncolliding");
        Require(p::CastSegment(trunk, {0,0,0},{2,0,0},0,hit,error) &&
            hit.hit && hit.fraction==0, "start inside trunk");
        Require(p::CastSegment(trunk, {-2,0,0},{2,0,0},.25f,hit,error) &&
            hit.hit && Near(hit.fraction,.3125f), "fast sphere sweep");
        Require(p::CastSegment(box, {-2,0,0},{-1,0,0},0,hit,error) &&
            hit.hit && Near(hit.fraction,1), "endpoint contact");
        const auto retained = hit;
        Require(!p::CastSegment(box,{0,0,0},{1,0,0},-1,hit,error) &&
            hit.hit==retained.hit && hit.fraction==retained.fraction, "invalid query retains output");
        Require(!p::CreateShape(p::ShapeKind::Capsule,{0,1,1},box,error) && box.IsValid(),
            "failed shape replacement retains previous shape");
        {
            wi::scene::Scene scene;
            const auto initial=p::BodyCapacity(scene);
            Require(initial.maximum==65536 && initial.allocated==0, "real capacity");
            auto entity=Body(scene,{100,0,0});
            Require(p::PrepareStaticBody(scene,entity,box,8,4096,error), "boulder scale body");
            Require(p::BodyCapacity(scene).allocated==1, "real body allocated immediately");
            auto native=wi::physics::Intersects(scene,wi::primitive::Ray(XMFLOAT3{80,0,0},XMFLOAT3{1,0,0},0,40));
            Require(native.entity==entity && Near(native.position.x,92), "native boulder collision agrees with scale");
            Require(!p::PrepareStaticBody(scene,entity,box,8,4096,error) &&
                p::BodyCapacity(scene).allocated==1, "duplicate preparation cannot mutate existing body");
            scene.Entity_Remove(entity,false);
            Require(p::BodyCapacity(scene).allocated==0, "cold eviction destroys body ID");
            for(int cycle=0;cycle<1024;++cycle)
            {
                entity=Body(scene,{0,0,0});
                Require(p::PrepareStaticBody(scene,entity,trunk,1,4096,error), "repeated residency create");
                Require(p::BodyCapacity(scene).allocated==1, "bounded allocated bodies");
                scene.Entity_Remove(entity,false);
                Require(p::BodyCapacity(scene).allocated==0, "repeated eviction reclaims native ID");
            }
            entity=Body(scene,{0,0,0});
            Require(!p::PrepareStaticBody(scene,entity,trunk,1,65536,error) &&
                p::BodyCapacity(scene).allocated==0, "capacity reserve fails closed");
            scene.Entity_Remove(entity,false);
            entity=Body(scene,{0,0,0});
            Require(p::PrepareStaticBody(scene,entity,trunk,1,4096,error),
                "capacity failure recovers without scene reset");
            scene.Clear();
            Require(p::BodyCapacity(scene).allocated==0, "scene replacement retires bodies");
        }
        {
            wi::physics::SetEnabled(true);wi::physics::SetSimulationEnabled(true);
            wi::scene::Scene scene;
            auto floor=Body(scene,{0,0,0});
            Require(p::PrepareStaticBody(scene,floor,box,4,4096,error),"support identity floor");
            scene.dt=1.f/60; // Normal Scene::Update publishes this before the native physics boundary.
            auto avatar=Body(scene,{0,4.02f,0});
            auto& body=*scene.rigidbodies.GetComponent(avatar);
            body.mass=80;body.shape=body.CAPSULE;body.capsule.height=.5f;body.capsule.radius=.4f;
            body.SetCharacterPhysics(true);body.SetRefreshParametersNeeded(true);
            wi::jobsystem::context physics;
            for(int frame=0;frame<240;++frame)
            {
                wi::physics::MoveCharacter(body,XMFLOAT3{0,0,0},0,0,true);
                wi::physics::RunPhysicsUpdateSystem(physics,scene,1.f/60);
                wi::jobsystem::Wait(physics);
            }
            const auto feet=wi::physics::GetPosition(body);
            std::cout<<"CHARACTER_SUPPORT live="<<p::HasLiveBody(scene,avatar)
                <<" supported="<<wi::physics::IsCharacterGroundSupported(body)
                <<" actual="<<p::CharacterSupportEntity(scene,avatar)<<" expected="<<floor
                <<" feet="<<feet.x<<","<<feet.y<<","<<feet.z
                <<" gravity="<<scene.weather.gravity.y<<std::endl;
            Require(p::HasLiveBody(scene,avatar)&&wi::physics::IsCharacterGroundSupported(body)&&
                p::CharacterSupportEntity(scene,avatar)==floor,
                "native character support resolves actual body ownership rather than pointer-valued userdata");
            Require(std::abs(wi::physics::GetPosition(body).y-4)<.1f,
                "native character position is the supported foot origin");
            XMFLOAT3 minimum,maximum;
            Require(p::BodyBounds(scene,avatar,minimum,maximum)&&maximum.y>5&&minimum.y>3.8f,
                "native posed character bounds include the upward-offset capsule");
            scene.Entity_Remove(floor,false);
            Require(p::CharacterSupportEntity(scene,avatar)==wi::ecs::INVALID_ENTITY,
                "removed support body cannot return a stale or dangling owner");
            scene.Clear();
            Require(p::BodyCapacity(scene).allocated==0,"character support test retires actual native IDs");
        }
        {
            wi::scene::Scene capacity;
            std::vector<wi::ecs::Entity> allocated;
            const auto maximum=p::BodyCapacity(capacity).maximum;
            allocated.reserve(maximum);
            for(std::uint32_t i=0;i<maximum;++i)
            {
                auto entity=Body(capacity,{float(i%256)*4,0,float(i/256)*4});
                if(!p::PrepareStaticBody(capacity,entity,box,1,0,error))
                    throw std::runtime_error("native full-capacity setup "+std::to_string(i)+": "+error);
                allocated.push_back(entity);
                if(i+1==maximum-4096)
                {
                    auto blocked=Body(capacity,{-10,0,0});
                    Require(!p::PrepareStaticBody(capacity,blocked,box,1,4096,error)&&
                        p::BodyCapacity(capacity).allocated==maximum-4096,
                        "real native allocations preserve ordinary reserve");
                    capacity.Entity_Remove(blocked,false);
                }
            }
            auto blocked=Body(capacity,{-10,0,0});
            Require(p::BodyCapacity(capacity).allocated==maximum&&
                !p::PrepareStaticBody(capacity,blocked,box,1,0,error)&&
                p::BodyCapacity(capacity).allocated==maximum,
                "actual full native capacity fails admission without an invalid body");
            capacity.Entity_Remove(allocated.back(),false);
            Require(p::PrepareStaticBody(capacity,blocked,box,1,0,error)&&
                p::BodyCapacity(capacity).allocated==maximum,
                "real capacity exhaustion recovers after native ID reclamation");
            capacity.Clear();
            Require(p::BodyCapacity(capacity).allocated==0,"full-capacity teardown reclaims every ID");
        }
        std::cout<<"PASS "<<checks<<" native physics assertions\n";
        return 0;
    }
    catch(const std::exception& e)
    {
        std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';
        return 1;
    }
}
