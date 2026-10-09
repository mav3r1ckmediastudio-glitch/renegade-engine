#include "RuntimeEquipmentLoadout.h"
#include "RuntimeProjectileSession.h"
#include "RuntimeProjectileAim.h"
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
    const auto transformsBeforeHitscan=scene->transforms.GetCount();
    bridge::ProjectileQueryResult instant;std::string hitscanError;
    if(!runtime::QueryHitscan(source,{0,0,0},{0,0,1},10,query,instant,hitscanError)||
       instant.status!=bridge::ProjectileQueryStatus::Hit||
       instant.contact.targetSubjectId!=wallId||
       std::abs(instant.contact.position.z-4.75f)>.001f)
        return fail("hitscan shared native contact");
    if(scene->transforms.GetCount()!=transformsBeforeHitscan)return fail("hitscan query mutated scene hierarchy");
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
    runtime::RuntimeEquipmentLoadout release;
    runtime::RuntimeEquipmentLoadout::ProjectileRequest delayed{bullet.assetId,bridge::EquipmentAction::PrimaryUse,bullet,"Left",.2f,1,"Right"};
    release.ScheduleProjectile(delayed,101);
    if(!release.ReleaseProjectiles(101,.1f,false).empty())return fail("release marker early");
    auto first=release.ReleaseProjectiles(101,.2f,false);
    if(first.size()!=1||first[0].launchSocketName!="Left"||
       !release.ReleaseProjectiles(101,.5f,false).empty())return fail("release marker exactly once");
    release.ScheduleProjectile(delayed,101);
    auto second=release.ReleaseProjectiles(101,.3f,false);
    if(second.size()!=1||second[0].launchSocketName!="Right")return fail("accepted PSP alternation");
    delayed.socketPolicy=2;release.ScheduleProjectile(delayed,101);
    if(release.ReleaseProjectiles(101,.3f,false).size()!=2)return fail("both PSPs");
    release.ScheduleProjectile(delayed,101);
    if(!release.ReleaseProjectiles(102,.3f,false).empty()||!release.scheduledProjectiles.empty())
        return fail("changed animation retained pending projectile");
    release.ScheduleProjectile(delayed,101);
    if(!release.ReleaseProjectiles(101,.3f,true).empty())return fail("cancelled release");
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
        !visuals.instanceAssets.empty())
        return fail("visual retirement retained projectile hierarchy");
    // Refined presets can leave a bounded detached flight-effect tail after
    // the projectile mesh retires. It must clean up after its authored life.
    visuals.Sync(*scene,session.simulation,10.0f);
    if(!visuals.dyingEffects.empty() || scene->transforms.GetCount()!=transformsBefore)
        return fail("visual effect tail leaks hierarchy");
    visuals.Reset(*scene);
    if(!visuals.templates.empty())return fail("visual template reset");
    auto stickyModel=wi::allocator::make_shared<wi::scene::Scene>();
    stickyModel->objects.Create(stickyModel->Entity_CreateTransform("Sticky arrow"));
    bullet.stickOnImpact=true;bullet.stuckLifetimeSeconds=1;
    visuals.templates.emplace(bullet.assetId,runtime::RuntimeProjectileVisuals::Template{std::move(stickyModel),bullet});
    if(!session.Launch(bullet,source,{0,0,0},{0,0,1},id)||
       !visuals.Spawn(*scene,bullet.assetId,id))return fail("sticky spawn");
    bridge::ProjectileImpact stick;stick.projectileId=id;stick.contact.position={0,0,5};stick.incomingVelocity={0,0,1};
    {
        auto* receiver=scene->transforms.GetComponent(wall);
        const auto saved=*receiver;
        receiver->ClearTransform();receiver->Scale(XMFLOAT3{5,.2f,.05f});
        receiver->RotateRollPitchYaw(XMFLOAT3{.1f,.4f,.2f});receiver->UpdateTransform();
        stick.incomingVelocity={.96f,.1f,.25f};
        const auto expected=runtime::RuntimeProjectileVisuals::Pose(bullet,stick.incomingVelocity,stick.contact.position);
        visuals.Impact(*scene,stick,wall);
        visuals.Sync(*scene,session.simulation,0);
        auto* arrow=scene->transforms.GetComponent(visuals.retained.back().root);
        const auto actual=arrow->GetWorldMatrix();
        for(unsigned axis=0;axis<3;++axis)
            if(XMVectorGetX(XMVector3Length(actual.r[axis]-expected.r[axis]))>.001f)
                return fail("angled retained arrow inherits receiver shear or loses direction");
        receiver->RotateRollPitchYaw(XMFLOAT3{0,.5f,0});receiver->UpdateTransform();
        visuals.Sync(*scene,session.simulation,0);
        const auto moved=arrow->GetWorldMatrix();
        for(unsigned axis=0;axis<3;++axis)
            if(std::abs(XMVectorGetX(XMVector3Length(moved.r[axis]))-bullet.visualScale)>.001f)
                return fail("moving receiver stretches retained arrow");
        scene->Entity_Remove(visuals.retained.back().root);visuals.retained.clear();
        *receiver=saved;
        if(!visuals.Spawn(*scene,bullet.assetId,id))return fail("restore sticky test projectile");
        stick.incomingVelocity={0,0,1};
    }
    session.Reset();visuals.Impact(*scene,stick,wall);visuals.Sync(*scene,session.simulation);
    if(visuals.retained.size()!=1||visuals.instances.size()!=0||
       visuals.retained[0].receiver!=wall || scene->hierarchy.Contains(visuals.retained[0].root))
        return fail("stick at exact hit object");
    const auto stuck=visuals.retained[0].root;
    auto* wallTransform=scene->transforms.GetComponent(wall);
    wallTransform->Translate(XMFLOAT3{2,0,0});wallTransform->UpdateTransform();
    wi::jobsystem::Initialize();
    wi::jobsystem::context ctx;scene->RunTransformUpdateSystem(ctx);wi::jobsystem::Wait(ctx);
    scene->RunHierarchyUpdateSystem(ctx);wi::jobsystem::Wait(ctx);
    visuals.Sync(*scene,session.simulation,0);
    if(std::abs(scene->transforms.GetComponent(stuck)->GetPosition().x-2)>.001f)
        return fail("stuck projectile follows moving object");
    visuals.Sync(*scene,session.simulation,0);
    if(visuals.retained.size()!=1)return fail("pause retained lifetime");
    visuals.Sync(*scene,session.simulation,1.1f);
    if(!visuals.retained.empty()||scene->transforms.Contains(stuck))return fail("stuck expiry");
    if(!session.Launch(bullet,source,{0,0,0},{0,0,1},id) ||
        !visuals.Spawn(*scene,bullet.assetId,id))return fail("water arrow spawn");
    stick.projectileId=id;stick.contact.surfaceType=bridge::ImpactSurfaceType::Water;
    session.Reset();visuals.Impact(*scene,stick,wall);visuals.Sync(*scene,session.simulation);
    if(!visuals.retained.empty() || !visuals.instances.empty())
        return fail("arrow pins to water surface");
    bridge::ProjectileEffectLayer flame;flame.kind=bridge::ProjectileEffectKind::Flame;
    const auto effect=bridge::CreateProjectileEffectEmitter(*scene,flame,{0,0,0});
    if(!scene->emitters.Contains(effect)||scene->emitters.GetComponent(effect)->GetMaxParticleCount()!=256)
        return fail("native bounded effect emitter");
    scene->Entity_Remove(effect);
    const auto burstEffect=bridge::CreateProjectileEffectEmitter(*scene,flame,{0,0,0},true);
    const auto* burstEmitter=scene->emitters.GetComponent(burstEffect);
    if(!burstEmitter || burstEmitter->burst_on_create!=48 || !burstEmitter->IsInactive())
        return fail("impact burst stays inactive until native update initializes buffers");
    scene->Entity_Remove(burstEffect);visuals.Reset(*scene);

    // Shared impact presentation is surface-aware, bounded and independent of
    // whether the contact came from Hitscan or a travelling projectile.
    bridge::ProjectileImpact surfaceImpact;
    surfaceImpact.contact.position={0,0,1};
    surfaceImpact.contact.normal={0,0,-1};
    surfaceImpact.contact.surfaceType=bridge::ImpactSurfaceType::Metal;
    surfaceImpact.incomingVelocity={0,0,1};
    const auto emittersBeforeImpact=scene->emitters.GetCount();
    const auto decalsBeforeImpact=scene->decals.GetCount();
    visuals.PresentSurfaceImpact(*scene,surfaceImpact,wall);
    if(visuals.impactDecals.size()!=1 ||
       scene->emitters.GetCount()!=emittersBeforeImpact+1 ||
       scene->decals.GetCount()!=decalsBeforeImpact+1)
        return fail("metal impact presentation");
    const auto mark=visuals.impactDecals.front().entity;
    const auto* markHierarchy=scene->hierarchy.GetComponent(mark);
    if(markHierarchy==nullptr || markHierarchy->parentID!=wall)
        return fail("impact mark follows hit object");
    visuals.Sync(*scene,session.simulation,20.0f);
    if(!visuals.impactDecals.empty() ||
       scene->emitters.GetCount()!=emittersBeforeImpact ||
       scene->decals.GetCount()!=decalsBeforeImpact)
        return fail("impact presentation lifetime");

    surfaceImpact.contact.surfaceType=bridge::ImpactSurfaceType::Character;
    visuals.PresentSurfaceImpact(*scene,surfaceImpact,wall);
    if(!visuals.impactDecals.empty() ||
       scene->decals.GetCount()!=decalsBeforeImpact ||
       scene->emitters.GetCount()!=emittersBeforeImpact+1)
        return fail("character impact decal policy");
    if(visuals.blood.drops.size()!=60 || scene->rigidbodies.GetCount()!=0)
        return fail("character impact directional blood droplets without rigid bodies");
    const auto& spray=scene->emitters[scene->emitters.GetCount()-1];
    if(spray.framesX!=4 || spray.framesY!=4 || spray.frameCount!=16 ||
       spray.frameRate!=0 || !spray.IsFrameBlendingEnabled() || spray.burst_on_create!=1)
        return fail("blood requires one animated lifetime-driven atlas");
    const auto bloodDrop=visuals.blood.drops.front().entity;
    const auto bloodStart=scene->transforms.GetComponent(bloodDrop)->GetPosition();
    visuals.Sync(*scene,session.simulation,0);
    if(scene->transforms.GetComponent(bloodDrop)->GetPosition().y!=bloodStart.y)
        return fail("blood pause consumed droplet lifetime");
    visuals.Sync(*scene,session.simulation,.016f);
    if(scene->transforms.GetComponent(bloodDrop)->GetPosition().z>=bloodStart.z)
        return fail("blood spray ignores incoming hit direction");
    visuals.Sync(*scene,session.simulation,2.0f);
    if(scene->emitters.GetCount()!=emittersBeforeImpact)
        return fail("character impact effect lifetime");
    visuals.Reset(*scene);
    {
        runtime::RuntimeBloodEffects bloodProof;
        const auto baseObjects=scene->objects.GetCount(),baseMeshes=scene->meshes.GetCount(),
            baseMaterials=scene->materials.GetCount(),baseDecals=scene->decals.GetCount();
        for(unsigned i=0;i<8;++i)bloodProof.Spawn(*scene,{0,1,0},{0,0,-1},wall,1.f,i);
        if(bloodProof.drops.size()!=runtime::RuntimeBloodEffects::MaxDrops ||
            scene->meshes.GetCount()!=baseMeshes+1)return fail("blood cap or shared droplet mesh");
        for(unsigned i=0;i<runtime::RuntimeBloodEffects::MaxStains+12;++i)bloodProof.Mark(*scene,{0,0,0},{0,1,0},wall,.05f,float(i));
        if(bloodProof.stains.size()!=runtime::RuntimeBloodEffects::MaxStains)
            return fail("blood stain cap");
        const auto stain=bloodProof.stains.front().entity;
        if(scene->hierarchy.GetComponent(stain)->parentID!=wall)return fail("blood stain target attachment");
        const auto receiver=scene->Entity_CreateTransform("Transformed blood receiver");
        auto* receiverTransform=scene->transforms.GetComponent(receiver);
        receiverTransform->Scale(XMFLOAT3{2,.5f,3});receiverTransform->Translate(XMFLOAT3{1,-.5f,4});
        receiverTransform->UpdateTransform();
        bloodProof.Mark(*scene,{1,0,4},{0,1,0},receiver,.2f,0);
        const auto attached=bloodProof.stains.back().entity;
        scene->transforms.GetComponent(attached)->UpdateTransform_Parented(*scene->transforms.GetComponent(receiver));
        const auto beforeMove=scene->transforms.GetComponent(attached)->GetPosition();
        if(std::abs(beforeMove.x-1)>.001f || std::abs(beforeMove.y-.004f)>.001f ||
           std::abs(beforeMove.z-4)>.001f)return fail("blood attachment lost world contact on transformed receiver");
        receiverTransform=scene->transforms.GetComponent(receiver);
        receiverTransform->Translate(XMFLOAT3{0,1,0});receiverTransform->UpdateTransform();
        scene->transforms.GetComponent(attached)->UpdateTransform_Parented(*scene->transforms.GetComponent(receiver));
        if(std::abs(scene->transforms.GetComponent(attached)->GetPosition().y-1.004f)>.001f)
            return fail("blood stain does not follow receiver");
        // The oldest stain was evicted by the extra mark; select a surviving one.
        const auto surviving=bloodProof.stains.front().entity;
        const auto fresh=*scene->materials.GetComponent(surviving);
        bool variedTint=false,variedScale=false;
        const auto firstScale=scene->transforms.GetComponent(surviving)->GetScale();
        for(const auto& mark:bloodProof.stains) {
            const auto* m=scene->materials.GetComponent(mark.entity);
            const auto scale=scene->transforms.GetComponent(mark.entity)->GetScale();
            variedTint|=std::abs(m->baseColor.x-fresh.baseColor.x)>.005f;
            variedScale|=std::abs(scale.x-firstScale.x)>.005f;
        }
        if(!variedTint || !variedScale)return fail("repeated blood marks must retain size and tint variation");
        bloodProof.Update(*scene,0,{});
        if(bloodProof.stains.front().seconds!=runtime::RuntimeBloodEffects::StainLifetime)
            return fail("blood stain pause consumed lifetime");
        bloodProof.Update(*scene,31,{});
        const auto* aged=scene->materials.GetComponent(surviving);
        if(!bloodProof.drops.empty() || bloodProof.stains.size()!=runtime::RuntimeBloodEffects::MaxStains ||
            !aged || aged->baseColor.x>=fresh.baseColor.x || aged->roughness<=fresh.roughness ||
            aged->reflectance>=fresh.reflectance || aged->baseColor.w!=fresh.baseColor.w)
            return fail("blood stains must persist and dry after thirty seconds");
        bloodProof.Update(*scene,runtime::RuntimeBloodEffects::StainLifetime-46,{});
        if(scene->materials.GetComponent(surviving)->baseColor.w>=fresh.baseColor.w)
            return fail("blood stain final fade");
        bloodProof.Update(*scene,16,{});
        if(!bloodProof.stains.empty())return fail("blood presentation expiry");
        bloodProof.Reset(*scene);scene->Entity_Remove(receiver);
        if(scene->objects.GetCount()!=baseObjects || scene->meshes.GetCount()!=baseMeshes ||
            scene->materials.GetCount()!=baseMaterials || scene->decals.GetCount()!=baseDecals)
            return fail("blood reset orphaned resources");
    }
    // The directional prototype must carry its cone with the impact axis,
    // retain seeded variation, and continue gravity / pause / cleanup.
    {
        runtime::RuntimeBloodEffects candidate;candidate.directionalPrototype=true;
        candidate.Spawn(*scene,{0,10,0},{1,0,0},wall,1.f,17);
        float forward=0,transverse=0;
        const auto firstVelocity=candidate.drops.front().velocity;
        for(const auto& drop:candidate.drops){forward+=drop.velocity.x;transverse+=drop.velocity.z;}
        if(forward/candidate.drops.size()<2.f ||
           std::abs(transverse/candidate.drops.size())>1.f)
            return fail("directional blood cone must follow a side impact");
        const auto firstPosition=candidate.drops.front().position;
        candidate.Update(*scene,0,{});
        if(candidate.drops.front().position.x!=firstPosition.x)
            return fail("directional blood pause moves droplets");
        candidate.Update(*scene,.02f,{});
        if(candidate.drops.front().position.x<=firstPosition.x ||
           candidate.drops.front().velocity.y>=firstVelocity.y)
            return fail("directional blood must travel and fall");
        candidate.Reset(*scene);
        candidate.Spawn(*scene,{0,10,0},{0,0,-1},wall,1.f,23);
        float frontal=0;for(const auto& drop:candidate.drops)frontal+=drop.velocity.z;
        if(frontal/candidate.drops.size()>-2.f)
            return fail("frontal blood cone must rotate with impact direction");
        if(candidate.drops.front().velocity.x==firstVelocity.x &&
           candidate.drops.front().velocity.z==firstVelocity.z)
            return fail("directional blood trajectories repeat across seeds");
        candidate.Reset(*scene);
    }
    // Authored blood sheets use native object materials and face the incoming side,
    // including a contact whose mesh normal incorrectly points along the shot.
    {
        const auto donor=scene->Entity_CreateTransform("Blood preset test");
        scene->materials.Create(donor).baseColor={.35f,.008f,.016f,1};
        scene->metadatas.Create(donor).bool_values.set(runtime::RuntimeBloodSheets::MaterialKey,true);
        surfaceImpact.contact.surfaceType=bridge::ImpactSurfaceType::Character;
        surfaceImpact.contact.normal={0,0,1};surfaceImpact.incomingVelocity={0,0,10};
        visuals.PresentSurfaceImpact(*scene,surfaceImpact,wall);
        if(visuals.blood.liquidSheets.sheets.size()!=2 || scene->emitters.GetCount()!=emittersBeforeImpact)
            return fail("authored liquid uses native lit sheets instead of legacy spray");
        const auto entity=visuals.blood.liquidSheets.sheets.front().entity;
        const auto fresh=scene->transforms.GetComponent(entity)->GetPosition();
        if(fresh.z>=surfaceImpact.contact.position.z)
            return fail("blood must emerge on incoming-facing side despite reversed mesh normal");
        visuals.Sync(*scene,session.simulation,0);
        if(scene->materials.GetComponent(entity)->texMulAdd.z!=0)
            return fail("blood sheet pause advances animation");
        visuals.Sync(*scene,session.simulation,.2f);
        if(scene->materials.GetComponent(entity)->texMulAdd.z==0 &&
           scene->materials.GetComponent(entity)->texMulAdd.w==0)
            return fail("blood sheet animation remains frozen");
        visuals.Sync(*scene,session.simulation,.51f);
        if(!visuals.blood.liquidSheets.sheets.empty() || scene->objects.Contains(entity))
            return fail("blood spray must finish promptly and retire its objects");
        for(unsigned i=0;i<20;++i)visuals.PresentSurfaceImpact(*scene,surfaceImpact,wall);
        if(visuals.blood.liquidSheets.sheets.size()!=runtime::RuntimeBloodSheets::MaxSheets)
            return fail("blood sheet budget exceeded");
        visuals.Reset(*scene);
        if(!visuals.blood.liquidSheets.sheets.empty() || scene->meshes.Contains(entity) ||
           scene->materials.Contains(entity) || !scene->materials.Contains(donor))
            return fail("blood reset must remove transient sheets and retain authored material");
        scene->Entity_Remove(donor);
    }
    const auto objectsBefore=scene->objects.GetCount(),meshesBefore=scene->meshes.GetCount(),
        materialsBefore=scene->materials.GetCount();
    surfaceImpact.contact.surfaceType=bridge::ImpactSurfaceType::Glass;
    visuals.PresentSurfaceImpact(*scene,surfaceImpact,wall);
    if(visuals.impactGeometry.pieces.size()!=12 || visuals.impactDecals.size()!=1 ||
        scene->emitters.GetCount()!=emittersBeforeImpact)
        return fail("glass produces real shard meshes and crack mark without sparks");
    const auto shard=visuals.impactGeometry.pieces.front().entity;
    const auto initial=scene->transforms.GetComponent(shard)->GetPosition();
    visuals.Sync(*scene,session.simulation,0);
    if(scene->transforms.GetComponent(shard)->GetPosition().y!=initial.y ||
        std::find(visuals.roots.begin(),visuals.roots.end(),shard)==visuals.roots.end())
        return fail("glass pause and render-only projectile-query exclusion");
    visuals.Sync(*scene,session.simulation,.2f);
    const auto moved=scene->transforms.GetComponent(shard)->GetPosition();
    if(std::abs(moved.z-initial.z)<.01f)return fail("glass ballistic motion");
    visuals.Reset(*scene);
    if(scene->objects.GetCount()!=objectsBefore || scene->meshes.GetCount()!=meshesBefore ||
        scene->materials.GetCount()!=materialsBefore)return fail("glass reset orphaned mesh/material");

    surfaceImpact.contact.surfaceType=bridge::ImpactSurfaceType::Water;
    surfaceImpact.contact.normal={0,1,0};
    visuals.PresentSurfaceImpact(*scene,surfaceImpact,wall);
    if(visuals.impactGeometry.pieces.size()!=65 || !visuals.impactDecals.empty() ||
        scene->emitters.GetCount()!=emittersBeforeImpact+2)
        return fail("transmissive water crown, visible mesh drops and plume/skirt spray; no bullet-hole decal");
    visuals.Sync(*scene,session.simulation,.5f);
    if(visuals.impactGeometry.pieces.size()!=64)return fail("water crown expires before droplets");
    for(const auto& piece:visuals.impactGeometry.pieces) {
        const auto* material=scene->materials.GetComponent(piece.entity);
        if(piece.kind!=runtime::RuntimeImpactGeometry::Kind::WaterDrop ||
            material->transmission<.9f || material->emissiveColor.w!=0)
            return fail("water droplets require clear native transmission without emission");
    }
    visuals.Sync(*scene,session.simulation,2.f);
    if(!visuals.impactGeometry.pieces.empty() || scene->emitters.GetCount()!=emittersBeforeImpact ||
        scene->objects.GetCount()!=objectsBefore || scene->meshes.GetCount()!=meshesBefore ||
        scene->materials.GetCount()!=materialsBefore)return fail("water lifetime cleanup");
    for(unsigned i=0;i<14;++i)
    {
        surfaceImpact.contact.surfaceType=bridge::ImpactSurfaceType::Glass;
        visuals.PresentSurfaceImpact(*scene,surfaceImpact,wall);
    }
    if(visuals.impactGeometry.pieces.size()!=renegade::runtime::RuntimeImpactGeometry::MaxPieces)
        return fail("glass repeated-fire mesh cap");
    visuals.Reset(*scene);
    if(scene->objects.GetCount()!=objectsBefore || scene->meshes.GetCount()!=meshesBefore ||
        scene->materials.GetCount()!=materialsBefore)return fail("capped geometry reset cleanup");
    std::cout<<"Runtime projectile acceptance, native contact, visuals, pause and reset PASS\n";
    return 0;
}
