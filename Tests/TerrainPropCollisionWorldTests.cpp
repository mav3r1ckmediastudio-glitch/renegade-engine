#include "renegade/bridge/TerrainPropCollisionWorld.h"
#include "../Runtime/src/TerrainPropViewOrigin.h"
#include <wiPhysics.h>
#include <wiLua.h>
#include "renegade/bridge/PhysicsLuaService.h"
#include <wiJobSystem.h>
#include "renegade/bridge/PhysicsService.h"
#include "renegade/bridge/NavigationService.h"
#include "renegade/bridge/SceneService.h"
#include <iostream>
#include <stdexcept>

namespace b=renegade::bridge;
namespace n=wi::physics::terrain_props;
namespace
{
    // These owners are constructed before function-local registries and
    // destroyed after them. The executable's exit is part of this regression.
    std::unique_ptr<wi::scene::Scene> shutdownScene;
    std::shared_ptr<b::TerrainPropCollisionWorld> shutdownWorld;
    int checks=0;
    void Require(bool value,const char* message)
    { ++checks;if(!value)throw std::runtime_error(message); }
    b::TerrainPropDocument Document()
    {
        b::TerrainPropDocument d;
        d.layerId="11111111-1111-4111-8111-111111111111";d.nextPlacementId=10001;
        b::TerrainPropProfile profile;
        profile.id[0]=1;profile.modelAssetId="22222222-2222-4222-8222-222222222222";
        profile.collision={b::TerrainPropShape::BoulderBox,{0,0,0},{1,1,1}};
        d.profiles={profile};
        b::TerrainPropPage page;page.profiles={profile.id};
        for(std::uint64_t i=0;i<10000;++i)page.placements.push_back({i+1,0,{float(i)*10,0,0},0,1});
        d.pages={page};return d;
    }
    b::TerrainPropCollisionWorld::Interest At(float x) { return {{x,0,0},{0,0,0},0}; }
}
int main()
{
    try
    {
        wi::physics::Initialize();
        struct Workers{~Workers(){wi::jobsystem::ShutDown();}} workers;
        std::string error;
        {
            wi::scene::Scene travel;
            const auto seat = travel.Entity_CreateTransform("Moving vehicle chassis");
            const auto walking = travel.Entity_CreateTransform("Walking player");
            const XMFLOAT3 camera{2.f, 3.f, 4.f};
            auto* chassis = travel.transforms.GetComponent(seat);
            for (float x : {0.f, 64.f, 160.f, -64.f})
            {
                chassis->translation_local = {x, 2.f, 0.f};
                chassis->SetDirty();
                chassis->UpdateTransform();
                const auto origin = renegade::runtime::TerrainPropViewOrigin(travel, seat, walking, &camera);
                Require(origin && origin->x == x, "terrain detail must follow the vehicle across 32m cells");
            }
            // Register both bodies, then move the chassis between completed simulation steps.
            // Its native pose is authoritative even before the render transform catches up.
            for (auto id : {seat, walking})
            {
                auto& body = travel.rigidbodies.Create(id);
                body.mass = 1.f;
                body.shape = body.BOX;
                body.box.halfextents = {1.f, .5f, 1.f};
            }
            wi::jobsystem::context physics;
            travel.dt = 1.f / 60.f;
            wi::physics::RunPhysicsUpdateSystem(physics, travel, travel.dt);
            wi::jobsystem::Wait(physics);
            XMFLOAT3 foot{};
            Require(b::GetPhysicsPosition(travel, walking, foot), "walking body initialized");
            for (float x : {96.f, 256.f, -128.f})
            {
                Require(b::SetPhysicsPosition(travel, seat, {x, 2.f, 0.f}), "move live chassis");
                const auto aboard = renegade::runtime::TerrainPropViewOrigin(travel, seat, walking, &camera);
                Require(aboard && std::abs(aboard->x - x) < .001f, "live chassis owns terrain detail while riding");
            }
            const auto afterExit = renegade::runtime::TerrainPropViewOrigin(travel, wi::ecs::INVALID_ENTITY,
                walking, &camera);
            Require(afterExit && std::abs(afterExit->x - foot.x) < .001f, "walking body regains terrain detail after exit");
            const auto unboarded = renegade::runtime::TerrainPropViewOrigin(travel, wi::ecs::INVALID_ENTITY,
                wi::ecs::INVALID_ENTITY, &camera);
            Require(unboarded && unboarded->x == camera.x, "unspawned player uses camera after leaving vehicle");
            Require(!renegade::runtime::TerrainPropViewOrigin(travel, wi::ecs::INVALID_ENTITY,
                wi::ecs::INVALID_ENTITY, nullptr), "no resident view before scene and camera are available");
        }
        auto document=Document();
        wi::scene::Scene scene;
        auto world=b::TerrainPropCollisionWorld::Create(scene,{document},true,error);
        Require(world!=nullptr,"world initialization");
        b::TerrainPropCollisionWorld::Settings settings;
        settings.requestDistance=1;settings.releaseDistance=3;settings.releaseDelay=.5f;
        settings.maximumBodies=4;settings.changesPerFrame=1;
        Require(world->Configure(settings,error),"validated budgets");
        Require(world->Update(.016f,{At(0)})&&world->Stats().resident==1,
            "only near collision becomes resident");
        Require(n::BodyCapacity(scene).allocated==1,"actual near body count");
        const auto npcContact=scene.Intersects(wi::primitive::Capsule(
            XMFLOAT3{1.2f,-1,0},XMFLOAT3{1.2f,1,0},.5f),
            wi::enums::FILTER_NAVIGATION_MESH|wi::enums::FILTER_COLLIDER);
        Require(npcContact.entity!=wi::ecs::INVALID_ENTITY&&
            npcContact.depth>.25f&&npcContact.normal.x>.9f,
            "native NPC capsule resolves against the same resident Jolt convex shape");
        const auto foot=scene.Intersects(wi::primitive::Ray(
            XMFLOAT3{0,4,0},XMFLOAT3{0,-1,0},0,10),wi::enums::FILTER_NAVIGATION_MESH);
        Require(foot.entity!=wi::ecs::INVALID_ENTITY&&std::abs(foot.position.y-1)<.001f,
            "native NPC foot ray uses resident prop surface");
        const auto distant=b::Raycast(scene,wi::primitive::Ray(
            XMFLOAT3{99985,0,0},XMFLOAT3{3,0,0},0,15));
        Require(distant.queryReady&&distant.IsValid()&&distant.terrainPropPlacement==10000&&
            distant.terrainPropLayer==document.layerId&&distant.surface==b::ImpactSurfaceType::Stone,
            "public physics ray normalizes direction and returns nonresident stable identity");
        Require(!b::Raycast(scene,wi::primitive::Ray(XMFLOAT3{},XMFLOAT3{})).queryReady,
            "invalid public ray is blocked");
        b::NavigationGridSettings nav;
        nav.fitToSceneBounds=false;nav.resolutionX=24;nav.resolutionY=8;nav.resolutionZ=24;nav.voxelSize=.5f;
        auto navigation=b::CreateNavigationGrid(scene,nav,error);
        Require(navigation!=wi::ecs::INVALID_ENTITY&&b::IsNavigationGridCurrent(scene,navigation)&&
            scene.voxel_grids.GetComponent(navigation)->check_voxel(XMFLOAT3{0,0,0}),
            "navigation bakes authored collision without render geometry");
        const auto baked=scene.voxel_grids.GetComponent(navigation)->voxels;
        auto workerIndex=world->Index();
        bool wrongThreadBlocked=false,workerHit=false;
        std::thread worker([&]{
            std::string threadError;
            auto query=b::Raycast(scene,wi::primitive::Ray(XMFLOAT3{-2,0,0},XMFLOAT3{1,0,0},0,4));
            wrongThreadBlocked=!query.queryReady&&
                !b::RequireTerrainPropDestination(scene,wi::ecs::INVALID_ENTITY,{0,0,0},1,threadError);
            b::TerrainPropCollisionHit workerContact;
            workerHit=workerIndex->Cast({-2,0,0},{2,0,0},0,workerContact,threadError)&&workerContact.hit;
        });
        worker.join();
        Require(wrongThreadBlocked&&workerHit,
            "scene access fails closed on worker; explicit immutable snapshot remains queryable");

        b::TerrainPropCollisionHit hit;
        const auto anchor=[&]{
            const auto entity=scene.Entity_CreateTransform("Transient visual anchor");
            scene.objects.Create(entity);
            auto& metadata=scene.metadatas.Create(entity);metadata.runtimeProjectionOnly=true;
            metadata.string_values.set("renegade.terrain_props.layer",document.layerId);
            metadata.string_values.set(b::TerrainPropPlacementMetadataKey,"1");
            return entity;
        };
        const auto firstAnchor=anchor();world->RefreshAnchors();
        Require(world->Index()->Cast({-2,0,0},{2,0,0},0,hit,error)&&
            world->HitAnchor(hit)==firstAnchor,"stable collision resolves its visual anchor");
        scene.Entity_Remove(firstAnchor,false);
        const auto nextAnchor=anchor();
        auto unchangedVisualCollision=world->Stage(document,error);
        const auto sameIndex=world->Index();
        world->Commit(*unchangedVisualCollision);
        Require(world->Index()==sameIndex&&world->HitAnchor(hit)==nextAnchor&&
            n::BodyCapacity(scene).allocated==1,
            "changed visual anchors refresh without rebuilding identical collision or native IDs");
        scene.Entity_Remove(nextAnchor,false);
        Require(world->Index()->Cast({99985,0,0},{100000,0,0},0,hit,error)&&
            hit.hit&&hit.placement==10000,"distant hit remains queryable without body");
        Require(world->Update(.016f,{At(10)})&&n::BodyCapacity(scene).allocated==2,
            "release delay retains old body during transition");
        Require(world->Update(.6f,{At(10)})&&n::BodyCapacity(scene).allocated==1,
            "cold body destroyed after release delay");
        auto actor=wi::ecs::CreateEntity();scene.transforms.Create(actor);
        Require(!world->Destination(actor,{1000,0,0},0,error),"teleport waits for destination");
        Require(world->Update(.016f,{At(10)})&&n::BodyCapacity(scene).allocated==2,
            "destination prepared while source collision retained");
        const auto removed=n::IntersectRay(scene,wi::primitive::Ray(
            XMFLOAT3{998,0,0},XMFLOAT3{1,0,0},0,4),~0u).entity;
        Require(removed!=wi::ecs::INVALID_ENTITY,"destination fixture has a native prop body");
        scene.Entity_Remove(removed,false);
        Require(!world->Destination(actor,{1000,0,0},0,error),
            "stale resident map cannot admit a deleted destination native body");
        Require(!world->Update(.016f,{At(10)})&&world->Update(.016f,{At(10)}),
            "bounded destination repair destroys stale ownership before recreating a live body");
        Require(world->Destination(actor,{1000,0,0},0,error),"teleport admitted only after preparation");
        Require(world->Update(.6f,{At(1000)})&&n::BodyCapacity(scene).allocated==1,
            "source retires after successful transition");
        settings.maximumBodies=1;settings.releaseDistance=100;settings.releaseDelay=100;
        Require(world->Configure(settings,error),"pressure settings");
        Require(!world->Update(.016f,{At(1010)})&&n::BodyCapacity(scene).allocated==0,
            "required new region outranks optional warm retention");
        Require(world->Update(.016f,{At(1010)})&&n::BodyCapacity(scene).allocated==1,
            "bounded creation completes on next owner frame");
        settings.maximumBodies=4;settings.releaseDistance=3;settings.releaseDelay=0;
        settings.ordinaryBodyReserve=65535;
        Require(world->Configure(settings,error),"native reserve settings");
        Require(!world->Update(.016f,{At(1010),At(1020)})&&world->Stats().resident==1,
            "native capacity exhaustion blocks readiness without deleting source");
        settings.ordinaryBodyReserve=4096;
        Require(world->Configure(settings,error)&&world->Update(.016f,{At(1010),At(1020)}),
            "capacity admission recovers");
        settings.queryNodesPerFrame=1;
        const auto preserved=n::BodyCapacity(scene).allocated;
        Require(world->Configure(settings,error)&&!world->Update(.016f,{At(1010)})&&
            n::BodyCapacity(scene).allocated==preserved,"frame query budget fails closed before mutating bodies");
        settings.queryNodesPerFrame=262144;
        Require(world->Configure(settings,error),"restore frame query budget");
        const auto currentBodies=world->Stats().resident;
        Require(!world->Update(.016f,{At(0),At(10),At(20),At(30),At(40)})&&
            world->Stats().resident==currentBodies,"over-capacity required set leaves old display/physics intact");
        auto retainedIndex=world->Index();
        auto staged=world->Stage(document,error);
        auto stale=world->Stage(document,error);
        Require(staged&&stale&&world->CanCommit(*staged),"staged immutable query revision");
        const auto beforeUnchanged=n::BodyCapacity(scene).allocated;
        world->Commit(*staged);
        Require(!world->CanCommit(*stale)&&n::BodyCapacity(scene).allocated==beforeUnchanged&&
            b::IsNavigationGridCurrent(scene,navigation)&&scene.voxel_grids.GetComponent(navigation)->voxels==baked,
            "unchanged collision keeps native bodies and baked navigation; stale command rejected");
        auto moved=document;moved.pages[0].placements[0].position[0]=2;
        auto geometry=world->Stage(moved,error);
        Require(geometry!=nullptr,"prepare changed collision geometry");
        world->Commit(*geometry);
        Require(n::BodyCapacity(scene).allocated==0&&!b::IsNavigationGridCurrent(scene,navigation),
            "actual collision edit reclaims old bodies and marks navigation stale");
        Require(b::RebuildNavigationGrid(scene,navigation,nav,error)&&
            b::IsNavigationGridCurrent(scene,navigation)&&
            scene.voxel_grids.GetComponent(navigation)->check_voxel(XMFLOAT3{2,0,0}),
            "explicit navigation bake uses edited authored shape");

        Require(world->Update(.016f,{At(0)}),"new query revision can become ready");
        Require(world->SetInterest(1,At(500),error),"scripted collision region");
        Require(world->Update(.016f,{At(0)})&&world->Stats().resident==2,
            "scripted region independent of player/camera");
        world->RemoveInterest(1);
        Require(world->Update(.016f,{At(0)})&&world->Stats().resident==1,"scripted region releases");
        auto npc=scene.Entity_CreateTransform("Native NPC interest");
        scene.transforms.GetComponent(npc)->Translate(XMFLOAT3{300,0,0});
        scene.transforms.GetComponent(npc)->UpdateTransform();
        scene.characters.Create(npc).SetActive(true);
        (void)world->UpdateSceneInterests(.016f);
        Require(world->UpdateSceneInterests(.016f)&&world->Stats().resident==1,
            "native NPC region becomes resident without player or camera");
        scene.Entity_Remove(npc,false);
        world->Detach();
        Require(!b::TerrainPropCollisionWorld::Find(scene)&&n::BodyCapacity(scene).allocated==0,
            "detach destroys body IDs and registry lifetime");
        Require(!world->Update(.016f,{At(0)})&&!world->CanCommit(*stale),
            "detached controller rejects scene work");
        Require(retainedIndex->Cast({-2,0,0},{2,0,0},0,hit,error)&&hit.hit,
            "immutable worker snapshot remains valid without scene/body lifetime");
        world.reset();
        Require(n::BodyCapacity(scene).allocated==0,"world teardown has no retained native IDs");
        std::shared_ptr<b::TerrainPropCollisionWorld> detached;
        std::shared_ptr<const b::TerrainPropCollisionIndex> snapshot;
        {
            b::SceneService owned;owned.NewScene();
            detached=owned.TerrainPropCollisions();
            auto edit=detached->Stage(document,error);
            Require(edit!=nullptr,"scene-owned staged collision lifetime");
            detached->Commit(*edit);snapshot=detached->Index();
            owned.NewScene();
            Require(!detached->Ready()&&!detached->Update(0,{}),
                "scene replacement detaches externally retained world before scene clear");
        }
        Require(snapshot->Cast({-2,0,0},{2,0,0},0,hit,error)&&hit.hit,
            "scene-independent query survives SceneService destruction");
        detached.reset();
        {
            wi::jobsystem::Initialize();
            wi::scene::Scene support;
            auto floorDocument=Document();
            floorDocument.pages[0].placements.resize(2);
            floorDocument.profiles[0].collision.halfExtents={2,1,2};
            auto supportWorld=b::TerrainPropCollisionWorld::Create(support,{floorDocument},true,error);
            auto crate=support.Entity_CreateTransform("Sleeping supported crate");
            support.transforms.GetComponent(crate)->Translate(XMFLOAT3{0,1.5f,0});
            support.transforms.GetComponent(crate)->UpdateTransform();
            auto& body=support.rigidbodies.Create(crate);body.mass=1;body.shape=body.BOX;
            body.box.halfextents={12,.5f,.5f};body.restitution=0;
            Require(supportWorld!=nullptr,"supported-body collision world");
            b::TerrainPropCollisionWorld::Settings policy;policy.releaseDelay=0;
            Require(supportWorld->Configure(policy,error)&&supportWorld->UpdateSceneInterests(1.f/60),
                "dynamic body requests its own local collision support");
            wi::jobsystem::context physics;
            for(int frame=0;frame<240;++frame)
            {
                Require(supportWorld->UpdateSceneInterests(1.f/60),"support remains ready");
                wi::physics::RunPhysicsUpdateSystem(physics,support,1.f/60);
                wi::jobsystem::Wait(physics);
            }
            Require(n::IsBodySleeping(support,crate)&&n::BodyCapacity(support).allocated==3&&
                supportWorld->Stats().resident==2,
                "wide sleeping native body without a render root pins support across its actual bounds");
            Require(b::SetPhysicsPosition(support,crate,{500,1.5f,0}),
                "public teleport admits a prepared empty destination");
            Require(supportWorld->UpdateSceneInterests(.1f)&&supportWorld->Stats().resident==0&&
                n::BodyCapacity(support).allocated==1,
                "post-teleport interest uses actual Jolt pose before render transform catches up");
            support.Entity_Remove(crate,false);
            Require(supportWorld->UpdateSceneInterests(.1f)&&n::BodyCapacity(support).allocated==0,
                "support pin releases when the sleeping body is removed");
            supportWorld->Detach();
            wi::jobsystem::ShutDown();
        }
        {
            wi::scene::Scene ground;
            const auto terrainEntity=ground.Entity_CreateTransform("Native terrain readiness fixture");
            auto& terrain=ground.terrains.Create(terrainEntity);
            terrain.scene=&ground;terrain.terrainEntity=terrainEntity;
            terrain.SetPhysicsEnabled(true);terrain.SetCenterToCamEnabled(false);
            terrain.generation=4;terrain.chunk_scale=1;
            auto groundWorld=b::TerrainPropCollisionWorld::Create(ground,{},true,error);
            auto traveler=ground.Entity_CreateTransform("Terrain traveler");
            b::TerrainPropCollisionWorld::Interest source{{0,1,0},{0,0,0},1};
            Require(groundWorld&&!groundWorld->Update(.016f,{source})&&
                groundWorld->Stats().terrainRequired==9&&!n::AreTerrainRequestsReady(ground),
                "spawn waits for existing terrain pump to publish its requested collision cells");
            Require(!b::Raycast(ground,wi::primitive::Ray(XMFLOAT3{-2,1,0},XMFLOAT3{1,0,0},0,4)).queryReady,
                "required terrain unknown is blocked rather than reported clear");
            // Publish the same chunk-ready events used by the native generation
            // hook. Box floors isolate ownership/admission from terrain meshing;
            // existing native terrain-surface proofs validate the real mesh.
            const auto publish=[&]{
                const auto requested=n::GetTerrainRequests(ground,terrainEntity);
                Require(requested!=nullptr,"native terrain queue snapshot");
                for(const auto& key:*requested)
                {
                    const auto previous=n::GetTerrainChunk(ground,terrainEntity,key);
                    if(previous.valid)continue;
                    const auto entity=ground.Entity_CreateTransform("Generated floor");
                    auto& transform=*ground.transforms.GetComponent(entity);
                    transform.Translate(XMFLOAT3{float(key.x)*66,-1,float(key.z)*66});transform.UpdateTransform();
                    auto& body=ground.rigidbodies.Create(entity);body.mass=0;body.shape=body.BOX;
                    body.box.halfextents={33,1,33};
                    n::PublishTerrainChunk(ground,terrainEntity,key,entity,true);
                }
            };
            publish();
            Require(groundWorld->Update(.016f,{source})&&n::AreTerrainRequestsReady(ground)&&
                n::BodyCapacity(ground).allocated==9,"terrain admission confirms actual native floor bodies");
            const auto sourceRecords=n::GetTerrainChunkRecords(ground,terrainEntity);
            Require(!groundWorld->Destination(traveler,{132,1,0},1,error),
                "teleport cannot outrun target terrain collision");
            Require(!groundWorld->Update(.016f,{source})&&groundWorld->Stats().terrainRequired==15&&
                n::BodyCapacity(ground).allocated==9,"source floor remains allocated while destination generates");
            for(const auto& floor:sourceRecords)Require(n::HasLiveBody(ground,floor.entity),"source support retained");
            publish();
            Require(groundWorld->Update(.016f,{source})&&
                groundWorld->Destination(traveler,{132,1,0},1,error)&&n::BodyCapacity(ground).allocated==15,
                "terrain and prop destination admission completes without another simulation step");
            const auto record=sourceRecords.front();
            n::PublishTerrainChunk(ground,terrainEntity,record.key,record.entity,false);
            Require(!n::AreTerrainRequestsReady(ground),"late native terrain invalidation closes the pre-physics gate");
            n::PublishTerrainChunk(ground,terrainEntity,record.key,record.entity,true);
            b::TerrainPropCollisionWorld::Settings limited;limited.maximumTerrainChunks=8;
            const auto unchanged=n::GetTerrainRequests(ground,terrainEntity);
            Require(groundWorld->Configure(limited,error)&&!groundWorld->Update(.016f,{source})&&
                n::GetTerrainRequests(ground,terrainEntity)==unchanged&&n::BodyCapacity(ground).allocated==15,
                "terrain request exhaustion preserves the previous source request set and bodies");
            ground.Entity_Remove(terrainEntity,false);
            Require(groundWorld->Update(.016f,{source})&&!n::HasTerrainRequests(ground),
                "actual terrain-owner deletion retires stale readiness registration");
            groundWorld->Detach();
            Require(!n::HasTerrainRequests(ground),"scene detach retires native terrain interest registration");
            ground.Clear();
            Require(n::BodyCapacity(ground).allocated==0,"terrain scene lifetime reclaims its native bodies");
        }
        {
            wi::scene::Scene precision;
            auto exact=Document();exact.pages[0].placements.resize(1);
            exact.nextPlacementId=UINT64_MAX;exact.pages[0].placements[0].id=UINT64_MAX-1;
            auto exactWorld=b::TerrainPropCollisionWorld::Create(precision,{exact},false,error);
            auto* vm=luaL_newstate();luaL_openlibs(vm);
            Require(exactWorld&&b::BindPhysicsLua(precision,vm),"private high-ID Lua query fixture");
            const char* script="local h=renegade.physics.raycast(-2,0,0,1,0,0); "
                "assert(h.terrain_prop_placement=='18446744073709551614'); "
                "assert(h.surface=='stone' and h.friction==0.5)";
            const bool loaded=luaL_loadstring(vm,script)==LUA_OK;
            const bool queried=loaded&&lua_pcall(vm,0,0,0)==LUA_OK;
            b::UnbindPhysicsLua(&precision);lua_close(vm);
            Require(queried,"Lua preserves full uint64 placement identity and authored collision surface");
            exactWorld->Detach();
        }
        shutdownScene=std::make_unique<wi::scene::Scene>();
        auto shutdownDocument=Document();shutdownDocument.pages[0].placements.resize(1);
        auto terrainOwner=shutdownScene->Entity_CreateTransform("Process teardown terrain owner");
        auto& teardownTerrain=shutdownScene->terrains.Create(terrainOwner);
        teardownTerrain.SetPhysicsEnabled(false);
        shutdownWorld=b::TerrainPropCollisionWorld::Create(*shutdownScene,{shutdownDocument},true,error);
        Require(shutdownWorld&&shutdownWorld->Update(.016f,{At(0)})&&
            n::BodyCapacity(*shutdownScene).allocated==1,
            "process-lifetime scene retains native ownership until static teardown");
        std::cout<<"PASS "<<checks<<" native residency assertions; peak resident <= 4; indexed=10000\n";
        return 0;
    }
    catch(const std::exception& e)
    { std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1; }
}
