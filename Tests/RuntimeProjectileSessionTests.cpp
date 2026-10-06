#include "RuntimeEquipmentLoadout.h"
#include "RuntimeProjectileSession.h"
#include "RuntimeProjectileVisuals.h"
#include <iostream>
#include <memory>
using namespace renegade;
int main()
{
    const auto fail=[](const char* s){std::cerr<<s<<'\n';return 1;};
    runtime::RuntimeEquipmentLoadout equipment;
    equipment.authored=equipment.ready=true;
    equipment.primary.equipment.assetId=bridge::GenerateStableId();
    equipment.primary.equipment.name="Shotgun";
    equipment.primary.equipment.handUse=bridge::EquipmentHandUse::TwoHanded;
    equipment.primary.equipment.actions.push_back(
        {bridge::EquipmentAction::PrimaryUse,"Attack",0,0,.5f,0,false,true});
    auto bullet=bridge::MakeProjectilePreset(bridge::ProjectilePreset::Bullet);
    bullet.projectId=bridge::GenerateStableId();bullet.assetId=bridge::GenerateStableId();
    equipment.resolvedProjectiles.push_back(
        {equipment.primary.equipment.assetId,bridge::EquipmentAction::PrimaryUse,bullet});
    bridge::GameplayInputFrame input;input.firePressed=input.fireDown=true;
    equipment.RouteStaged(input,true,false,0);
    if(!equipment.TakeProjectileRequests().empty())return fail("pause launched");
    equipment.RouteStaged(input,false,false,.01f);
    if(!equipment.TakeProjectileRequests().empty())return fail("holster launched");
    equipment.RouteStaged(input,true,false,.01f);
    auto requests=equipment.TakeProjectileRequests();
    if(requests.size()!=1||requests[0].equipmentId!=equipment.primary.equipment.assetId)
        return fail("accepted primary action not dispatched once");
    equipment.RouteStaged(input,true,true,.01f);
    if(!equipment.TakeProjectileRequests().empty())return fail("busy action duplicated");
    if(!equipment.TakeProjectileRequests().empty())return fail("request not consumed");
    equipment.actions.Reset();equipment.dispatched=false;
    equipment.primary.equipment.actions[0].windupSeconds=.2f;
    equipment.RouteStaged(input,true,false,.01f);
    if(!equipment.TakeProjectileRequests().empty())return fail("windup launched early");
    input.firePressed=false;input.cancelEquipmentPressed=true;
    equipment.RouteStaged(input,true,false,.3f);
    if(!equipment.TakeProjectileRequests().empty())return fail("cancel launched");

    auto scene=std::make_unique<wi::scene::Scene>();
    auto owner=scene->Entity_CreateTransform("Player");
    auto wall=scene->Entity_CreateTransform("Wall");
    const auto wallId=bridge::GenerateStableId();std::string error;
    if(!bridge::AssignPersistentEntityId(*scene,wall,wallId,error))return fail("wall id");
    auto collider=scene->colliders.Create(wall);
    collider.shape=wi::scene::ColliderComponent::Shape::Sphere;
    collider.sphere=wi::primitive::Sphere({0,0,5},.25f);
    scene->colliders_cpu=&collider;
    wi::primitive::AABB bounds({-.25f,-.25f,4.75f},{.25f,.25f,5.25f});
    scene->collider_bvh.Build(&bounds,1);
    runtime::RuntimeCharacterSystemState characters;
    runtime::ProjectileOwnerBinding binding{runtime::RuntimePlayerKnowledgeId,owner};
    bridge::ProjectileSource source{binding.subjectId,equipment.primary.equipment.assetId,
        "Player",{0,0,0},{0,0,0}};
    runtime::RuntimeProjectileSession session;
    std::uint64_t id=0;
    if(!session.Launch(bullet,source,{0,0,0},{0,0,1},id))return fail("launch");
    const auto query=[&](const auto& record,const auto& from,const auto& to){
        return runtime::QueryProjectileSceneSegment(*scene,characters,binding,record,from,to);
    };
    std::vector<bridge::ProjectileImpact> impacts;
    if(!session.Update(0,query,impacts)||!session.simulation.Records().size()||session.impacted)
        return fail("pause flight");
    if(!session.Update(.1f,query,impacts)||impacts.size()!=1||session.impacted!=1||
       session.simulation.Records().size()||session.markers.size()!=1||
       impacts[0].contact.targetSubjectId!=wallId||
       std::abs(impacts[0].contact.position.z-4.75f)>.001f)
        return fail("native wall contact");
    const auto seconds=session.markers[0].seconds;
    if(!session.Update(0,query,impacts)||session.markers[0].seconds!=seconds)
        return fail("pause feedback");
    if(!session.Update(.1f,query,impacts)||!impacts.empty()||session.impacted!=1)
        return fail("duplicate impact");
    if(!session.Update(2.1f,query,impacts)||!session.markers.empty())
        return fail("long frame feedback expiry");
    const auto generation=session.generation;
    session.Reset();
    if(session.launched||session.impacted||!session.traces.empty()||
       !session.markers.empty()||session.generation!=generation+1)
        return fail("reset transient state");
    bullet.speedMetresPerSecond=1;
    if(!session.Launch(bullet,source,{0,0,0},{0,0,1},id)||
       !session.Update(1.5f,query,impacts)||
       session.simulation.Records().size()!=1||
       std::abs(session.simulation.Records()[0].launch.position.z-1.5f)>.001f)
        return fail("long frame substeps");
    const auto age=session.simulation.Records()[0].ageSeconds;
    if(session.Update(-1,query,impacts)||session.simulation.Records()[0].ageSeconds!=age)
        return fail("invalid dt mutation");
    session.Reset();
    // Native cached instantiation follows simulation without file IO.
    runtime::RuntimeProjectileVisuals visuals;
    auto model=wi::allocator::make_shared<wi::scene::Scene>();
    const auto object=model->Entity_CreateTransform("Visual arrow");
    model->objects.Create(object);
    bullet.meshAssetId=bridge::GenerateStableId();
    bullet.visualScale=.5f;bullet.visualRotationDegrees={0,90,0};
    visuals.templates.emplace(bullet.assetId,
        runtime::RuntimeProjectileVisuals::Template{std::move(model),bullet});
    const auto transformsBefore=scene->transforms.GetCount();
    if(!session.Launch(bullet,source,{0,0,0},{0,0,1},id) ||
        !visuals.Spawn(*scene,bullet.assetId,id))return fail("visual instantiate");
    const auto clearFlight=[](const auto&,const auto&,const auto&){return bridge::ProjectileQueryResult{};};
    if(!session.Update(.5f,clearFlight,impacts))return fail("visual movement");
    if(session.meshProjectiles.count(id)!=1 || session.traces.empty() ||
        std::any_of(session.traces.begin(),session.traces.end(),[](const auto& t){return t.meshless;}))
        return fail("mesh projectile draws fallback flight feedback");
    visuals.Sync(*scene,session.simulation);
    if(visuals.instances.size()!=1 || visuals.roots.size()!=1 ||
        std::abs(scene->transforms.GetComponent(visuals.roots[0])->GetPosition().z-.5f)>.001f)
        return fail("visual follows simulation");
    session.Reset();visuals.Sync(*scene,session.simulation);
    if(!visuals.instances.empty() || !visuals.roots.empty() ||
        !visuals.instanceAssets.empty() || scene->transforms.GetCount()!=transformsBefore)
        return fail("visual retirement leaks hierarchy");
    visuals.Reset(*scene);
    if(!visuals.templates.empty())return fail("visual template reset");
    std::cout<<"Runtime projectile acceptance, native contact, visuals, pause and reset PASS\n";
    return 0;
}
