#include "RuntimeApplication.h"
#include "renegade/bridge/TerrainPropCollisionWorld.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <string_view>
#include <unordered_set>

namespace renegade::runtime
{
    // Opt-in native acceptance driver, restricted to disposable TerrainProof
    // projects. It uses the real Runtime player, native NPC, terrain and Jolt
    // update; it adds no physics step or substitute controller.
    void RuntimeApplication::TickTerrainPropCollisionProof()
    {
        const char* path=std::getenv("RENEGADE_P3_RUNTIME_PROOF");
        if(!path||!*path)return;
        struct State
        {
            int phase=0,frames=0,waits=0;
            std::uint64_t revision=UINT64_MAX;
            std::uint32_t box=0;
            wi::ecs::Entity boulderAnchor = wi::ecs::INVALID_ENTITY;
            std::set<wi::ecs::Entity> nearBoulder;
            std::set<std::pair<int,int>> walkedCells;
            float seconds=0,top=0,npcStart=0; XMFLOAT3 walkStart{}, walkEnd{};
            XMFLOAT3 landing{},destination{},source{};
            wi::ecs::Entity npc=wi::ecs::INVALID_ENTITY;
        };
        static State state;
        if(state.phase<0||state.phase==9)return;
        const auto log=[&](const std::string& message){std::ofstream(path,std::ios::app)<<message<<'\n';};
        const auto fail=[&](const std::string& message){proofWalkForward_=false;log("P3_RUNTIME_PROOF_FAIL "+message);state.phase=-1;};
        const auto root=std::filesystem::u8path(startupResult_.project.rootPath).filename().u8string();
        if(root.find("TerrainProof")==std::string::npos)
        {fail("Use a dedicated disposable TerrainProof project.");return;}
        if(++state.frames>6000){fail("Native frame budget exceeded.");return;}
        auto& scene=scenes_.GetScene();
        const auto world=scenes_.TerrainPropCollisions();
        if(!world||!player_.IsSpawned()||!bridge::HasLivePhysicsBody(scene,player_.entity))return;
        if(state.revision==UINT64_MAX)state.revision=scenes_.Revision();
        if(state.revision!=scenes_.Revision()){fail("Scene changed during proof.");return;}
        XMFLOAT3 position;
        if(!bridge::GetPhysicsPosition(scene,player_.entity,position))return;
        auto* body=scene.rigidbodies.GetComponent(player_.entity);
        if(!body)return;
        if(state.npc!=wi::ecs::INVALID_ENTITY)
            if(const auto* npc=scene.characters.GetComponent(state.npc))
            {
                const auto p=npc->GetPosition();
                wi::renderer::DrawCapsule(wi::primitive::Capsule(p,
                    XMFLOAT3{p.x,p.y+npc->height,p.z},npc->width),XMFLOAT4{1,.45f,.1f,1},false);
            }
        const auto sampleVisuals = [&](const char* milestone) {
            if (!std::getenv("RENEGADE_P4_RESIDENCY_PROOF")) return true;
            std::size_t count = 0;
            std::unordered_set<wi::ecs::Entity> meshes;
            for (std::size_t i = 0; i < scene.objects.GetCount(); ++i) {
                const auto entity = scene.objects.GetEntity(i);
                const auto* metadata = scene.metadatas.GetComponent(entity);
                if (!metadata || !metadata->runtimeProjectionOnly ||
                    !metadata->string_values.has(bridge::TerrainPropPlacementMetadataKey)) continue;
                if (std::string_view(milestone) == "boulder") state.nearBoulder.insert(entity);
                if (std::string_view(milestone) == "return" && !state.nearBoulder.count(entity))
                    return false;
                ++count;
                meshes.insert(scene.objects[i].meshID);
            }
            if (std::string_view(milestone) == "return" && count != state.nearBoulder.size()) return false;
            log("P4_GPU_BYTES " + std::string(milestone) + " " + std::to_string(wi::graphics::GetDevice()->GetMemoryUsage().usage));
            log(std::string("P4_SAMPLE ") + milestone + " x=" + std::to_string(position.x)
                + " visual_objects=" + std::to_string(count) + " distinct_meshes=" + std::to_string(meshes.size()));
            return true;
        };
        if(state.phase==0)
        {
            if(!world->Ready() || !propVisuals_.Settled())return;
            if (std::getenv("RENEGADE_P4_RESIDENCY_PROOF")) {
                const auto plane = scene.Entity_FindByName("Proof ground");
                if (auto* object = scene.objects.GetComponent(plane)) object->SetRenderable(false);
            }
            float nearest=std::numeric_limits<float>::max();bool found=false;
            const auto index=world->Index();
            for(std::uint32_t i=0;i<index->Count();++i)
            {
                const auto& record=index->Placement(i);
                if(index->GetProfile(record.profile).source.collision.shape!=bridge::TerrainPropShape::BoulderBox)continue;
                const float d=record.center.x*record.center.x+record.center.z*record.center.z;
                if(d<nearest){nearest=d;state.box=i;found=true;}
            }
            if(!found){fail("Fixture needs an authored boulder collision profile.");return;}
            const auto& record=index->Placement(state.box);
            const auto& c=index->GetProfile(record.profile).source.collision;
            state.top=record.center.y+c.halfExtents.y*record.scale;
            state.landing={record.center.x,state.top+2,record.center.z};
            sampleVisuals("start"); state.source=position;state.phase=1;
            log("P3_RUNTIME_PROOF_BEGIN indexed="+std::to_string(index->Count()));
        }
        if(state.phase==1)
        {
            if(!bridge::SetPhysicsPosition(scene,player_.entity,state.landing)){++state.waits;return;}
            (void)bridge::SetLinearVelocity(scene,player_.entity,XMFLOAT3{0,0,0});
            state.phase=2;state.seconds=0;return;
        }
        state.seconds+=std::max(0.f,scene.dt);
        if(state.phase==2)
        {
            if (!propVisuals_.Settled()) return;
            if(!wi::physics::IsCharacterGroundSupported(*body))
            {if(state.seconds>8)fail("Player did not land on the authored boulder.");return;}
            // Wicked offsets the native character shape upward: GetPosition
            // reports its foot origin, not the capsule centre.
            const auto& supportingRecord=world->Index()->Placement(state.box);
            const auto supportingEntity=wi::physics::terrain_props::CharacterSupportEntity(scene,player_.entity);
            const auto* support=scene.metadatas.GetComponent(supportingEntity);
            if(!support||!support->string_values.has(bridge::TerrainPropPlacementMetadataKey)||
                support->string_values.get(bridge::TerrainPropPlacementMetadataKey)!=std::to_string(supportingRecord.id)||
                !support->string_values.has("renegade.terrain_props.layer")||
                support->string_values.get("renegade.terrain_props.layer")!=world->Index()->Layer(supportingRecord.layer)||
                std::abs(position.y-state.top)>.25f)
            {
                if(state.seconds>8)fail("Player support identity/height disagrees with the selected boulder: y="+
                    std::to_string(position.y)+" top="+std::to_string(state.top));
                return;
            }
            sampleVisuals("boulder");
            player_.yaw=-DirectX::XM_PIDIV2;player_.pitch=.55f;
            log("PLAYER_BOULDER_SUPPORTED y="+std::to_string(position.y)+" top="+std::to_string(state.top)+
                " allocated="+std::to_string(world->Stats().nativeAllocated));
            const auto& record=world->Index()->Placement(state.box);
            const auto bounds=world->Index()->Bounds(state.box);
            state.npcStart=bounds.minimum.x-3;
            state.npc=scene.Entity_CreateTransform("P3 Native NPC");
            auto& npc=scene.characters.Create(state.npc);
            npc.height=1.8f;npc.width=.35f;npc.ground_friction=.8f;npc.SetActive(true);npc.SetFootPlacementEnabled(false);
            const XMFLOAT3 start{state.npcStart,bounds.minimum.y+.05f,record.center.z};
            npc.SetPosition(start);
            auto& transform=*scene.transforms.GetComponent(state.npc);
            transform.Translate(start);transform.UpdateTransform();
            state.phase=3;state.seconds=0;return;
        }
        if(state.phase==3)
        {
            auto* npc=scene.characters.GetComponent(state.npc);
            if(!npc){fail("Native NPC disappeared.");return;}
            npc->Move(XMFLOAT3{.4f,0,0});
            if(state.seconds<4)return;
            const auto p=npc->GetPosition();
            const auto& record=world->Index()->Placement(state.box);
            if(p.x<=state.npcStart+.5f||p.x>=record.center.x+.1f)
            {fail("NPC did not walk into/block against native prop collision: x="+std::to_string(p.x)+
                " y="+std::to_string(p.y)+" start="+std::to_string(state.npcStart)+
                " center="+std::to_string(record.center.x)+" wall="+std::to_string(npc->IsWallIntersect()));return;}
            npc->Move(XMFLOAT3{0,0,0});
            log("NPC_PROP_CONTACT x="+std::to_string(p.x)+" start="+std::to_string(state.npcStart)+
                " wall="+std::to_string(npc->IsWallIntersect()));
            state.source=position;
            state.destination={record.center.x+198,state.top+6,record.center.z};
            state.phase=4;state.seconds=0;state.waits=0;return;
        }
        if(state.phase==4)
        {
            if(!bridge::SetPhysicsPosition(scene,player_.entity,state.destination))
            {
                ++state.waits;
                if(std::abs(position.x-state.source.x)>.2f||std::abs(position.z-state.source.z)>.2f)
                {fail("Pending teleport moved the source player.");return;}
                if(state.waits==1)log("TELEPORT_WAIT_SOURCE_HELD terrain_required="+
                    std::to_string(world->Stats().terrainRequired));
                return;
            }
            (void)bridge::SetLinearVelocity(scene,player_.entity,XMFLOAT3{0,0,0});
            state.phase=5;state.seconds=0;return;
        }
        if (state.phase == 10 || state.phase == 11) {
            state.walkedCells.emplace(int(std::floor(position.x/32.0f)), int(std::floor(position.z/32.0f)));
            const auto target = state.phase == 10 ? state.walkEnd : state.walkStart;
            const float dx = target.x-position.x, dz = target.z-position.z;
            if (dx*dx + dz*dz > .75f*.75f) {
                player_.yaw = std::atan2(dx,dz);
                proofWalkForward_ = true;
                if (state.seconds > 35) fail("Continuous traversal failed to reach target at " + std::to_string(position.x) + "," + std::to_string(position.z));
                return;
            }
            proofWalkForward_ = false;
            if (!propVisuals_.Settled()) return;
            if (state.walkedCells.size() < 2) { fail("Walk did not cross a cell boundary"); return; }
            log("P4_WALK cells=" + std::to_string(state.walkedCells.size()) + " z=" + std::to_string(position.z));
            sampleVisuals(state.phase == 10 ? "walk_out" : "walk_back");
            state.phase = state.phase == 10 ? 11 : 6;
            state.seconds = 0;
            return;
        }
        if (state.phase == 6) {
            if (!bridge::SetPhysicsPosition(scene, player_.entity, state.landing)) return;
            (void)bridge::SetLinearVelocity(scene, player_.entity, XMFLOAT3{0,0,0});
            state.destination = state.landing;
            state.phase = 7;
            state.seconds = 0;
            return;
        }
        if(state.phase==5 || state.phase==7)
        {
            if (!propVisuals_.Settled()) return;
            if(!wi::physics::IsCharacterGroundSupported(*body))
            {if(state.seconds>10)fail("Player did not land after terrain-prefetched teleport.");return;}
            if(state.waits==0){fail("Teleport did not exercise pending destination readiness.");return;}
            if(std::abs(position.x-state.destination.x)>.5f||std::abs(position.z-state.destination.z)>.5f)
            {fail("Teleported player landed at the wrong destination.");return;}
            if (std::getenv("RENEGADE_P4_RESIDENCY_PROOF") && state.phase == 5) {
                sampleVisuals("outbound");
                const auto& record = world->Index()->Placement(state.box);
                bridge::TerrainPropCollisionHit hit;
                hit.layer = world->Index()->Layer(record.layer);
                hit.placement = record.id;
                state.boulderAnchor = world->HitAnchor(hit);
                if (state.boulderAnchor == wi::ecs::INVALID_ENTITY) { fail("Distant boulder lost its hit anchor"); return; }
                state.walkStart = position;
                std::string queryError;
                for (int i = 0; i < 96; ++i) {
                    const float yaw = i * DirectX::XM_2PI / 96;
                    state.walkEnd = {position.x + std::sin(yaw)*48, position.y, position.z + std::cos(yaw)*48};
                    auto from = position, to = state.walkEnd;
                    from.y += .7f; to.y += .7f;
                    bridge::TerrainPropCollisionHit obstacle;
                    if (!world->Index()->Cast(from,to,.5f,obstacle,queryError)) { fail(queryError); return; }
                    if (!obstacle.hit) { state.phase = 10; break; }
                }
                if (state.phase != 10) { fail("Could not find clear continuous traversal corridor"); return; }
                state.seconds = 0;
                return;
            }
            if (state.phase == 7) {
                if (!scene.objects.Contains(state.boulderAnchor)) { fail("Returned boulder lost its render object"); return; }
                if (!sampleVisuals("return")) { fail("Returned terrain object membership differs"); return; }
            }
            const auto stats=world->Stats();
            log("P3_RUNTIME_PROOF_PASS waits="+std::to_string(state.waits)+" player="+
                std::to_string(position.x)+","+std::to_string(position.y)+","+std::to_string(position.z)+
                " indexed="+std::to_string(stats.indexed)+" resident="+std::to_string(stats.resident)+
                " native_allocated="+std::to_string(stats.nativeAllocated)+" terrain="+
                std::to_string(stats.terrainReady)+"/"+std::to_string(stats.terrainRequired));
            state.phase=9;
        }
    }
}
