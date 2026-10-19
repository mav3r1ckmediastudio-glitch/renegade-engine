#pragma once
#include "../Runtime/src/RuntimeChannelSession.h"
#include "../Runtime/src/RuntimeChannelScorchTrail.h"
static void CheckChannelSpell() {
    using namespace renegade::runtime;
    // Beam contact is immediate; animation start plays concurrently, not as a
    // hidden eight-tenths-second gameplay delay before a first damage tick.
    EquipmentDefinition ability{GenerateStableId(),"Channel","",EquipmentHandUse::PrimaryOnly,
        {{EquipmentAction::Cast,"Cast",0,0,0,.2f,false,true,true}}};
    EquipmentProjectileBinding binding;binding.action=EquipmentAction::Cast;binding.fireMode=EquipmentFireMode::Channel;
    binding.launchSocketName="Palm";binding.hitscanRangeMetres=25;binding.channelDamagePerSecond=20;
    ability.projectiles={binding};
    std::string text,error;EquipmentAssetDocument parsed;
    Check(SerializeEquipmentAsset({GenerateStableId(),ability},text,error) &&
        DeserializeEquipmentAsset(text,parsed,error) && parsed.equipment.projectiles[0].channelDamagePerSecond==20,
        "channel schema roundtrip "+error);
    auto bad=nlohmann::json::parse(text);bad["schema_version"]=4;
    Check(!DeserializeEquipmentAsset(bad.dump(),parsed,error),"channel accepted in old schema");
    bad=nlohmann::json::parse(text);bad["projectiles"][0]["channel_damage_per_second"]=-1;
    Check(!DeserializeEquipmentAsset(bad.dump(),parsed,error),"negative channel DPS");
    auto invalid=ability;invalid.actions[0].activeWhileHeld=false;
    Check(!ValidateEquipmentDefinition(invalid),"non-held channel accepted");
    invalid=ability;invalid.actions.push_back({EquipmentAction::PrimaryUse,"Attack"});
    Check(!ValidateEquipmentDefinition(invalid),"ambiguous channel and primary action accepted");
    RuntimeEquipmentLoadout loadout;loadout.authored=loadout.ready=true;loadout.primary.equipment=ability;
    RuntimeEquipmentLoadout::ProjectileRequest request{};request.action=EquipmentAction::Cast;
    request.fireMode=EquipmentFireMode::Channel;request.equipmentId=ability.assetId;
    loadout.resolvedProjectiles={request};
    GameplayInputFrame hold;hold.firePressed=hold.fireDown=true;
    loadout.RouteStaged(hold,true,false,1.f/60.f);hold.firePressed=false;
    Check(loadout.chargePresentation && loadout.channelActive &&
        loadout.TakeProjectileRequests().empty(),"immediate channel onset failed");
    loadout.RouteStaged(hold,true,false,.25f);
    Check(loadout.channelActive && loadout.TakeProjectileRequests().empty(),"channel not active or became projectile");
    loadout.RouteStaged({},true,false,0);
    Check(loadout.channelActive,"pause stopped channel");
    loadout.RouteStaged({},true,false,.01f);
    Check(!loadout.channelActive && loadout.releasePresentation,"release failed to stop channel");
    loadout.actions.Reset();hold.firePressed=true;loadout.RouteStaged(hold,true,false,.4f);
    hold.firePressed=false;hold.cancelEquipmentPressed=true;loadout.RouteStaged(hold,true,false,.01f);
    Check(!loadout.channelActive && !loadout.actions.ReservedHands(),"active cancel retained channel");
    RuntimeChannelSession session;renegade::bridge::ProjectileSource source;source.sourceAssetId=ability.assetId;source.ownerSubjectId="Player";
    renegade::bridge::ProjectileQueryResult hit;hit.status=renegade::bridge::ProjectileQueryStatus::Hit;hit.contact.targetSubjectId="A";
    float a=0,b=0;const auto apply=[&](const auto& impact){(impact.contact.targetSubjectId=="A"?a:b)+=impact.damage;};
    session.Update(.06f,source,hit,20,apply);
    session.Update(0,source,hit,20,apply);Check(a==0,"paused partial tick drained");
    hit.contact.targetSubjectId="B";session.Update(.04f,source,hit,20,apply);
    Check(std::abs(a-1.2f)<.001f && b==0,"aim change reassigned accrued damage");
    session.Stop(apply);Check(std::abs(b-.8f)<.001f && !session.active,"release lost partial tick");
    session.Reset();a=b=0;hit.contact.targetSubjectId="A";
    for(int i=0;i<60;++i)session.Update(1.f/60,source,hit,20,apply);session.Stop(apply);
    Check(std::abs(a-20.f)<.001f,"channel damage depended on frame rate");
    session.Reset();a=0;session.Update(1,source,hit,20,apply);session.Stop(apply);
    Check(std::abs(a-20)<.001f,"long frame channel dose incorrect");
    // A solid world receiver blocks the ray even without a damageable Character id.
    // Settle prior Character contact exactly once; do not pass its pending dose
    // through the wall or keep applying damage after the contact changes.
    session.Reset();a=b=0;hit.contact.targetSubjectId="A";
    session.Update(.07f,source,hit,20,apply);
    renegade::bridge::ProjectileQueryResult wall;
    wall.status=renegade::bridge::ProjectileQueryStatus::Hit;
    session.Update(.5f,source,wall,20,apply);
    Check(std::abs(a-1.4f)<.001f && b==0,"world obstruction passed through target damage");
    session.Stop(apply);
    Check(std::abs(a-1.4f)<.001f && b==0 && !session.active,
        "channel wall release produced phantom damage");
    session.Reset();Check(!session.active && session.ticks==0 && session.pendingSeconds==0,"reset retained channel state");
    RuntimeChannelScorchTrail trail;
    renegade::bridge::ProjectileQueryResult surface;
    surface.status=renegade::bridge::ProjectileQueryStatus::Hit;
    surface.contact.surfaceType=renegade::bridge::ImpactSurfaceType::Concrete;
    surface.contact.surfaceId="wall";
    surface.contact.normal={0,0,-1};
    surface.contact.position={0,1,0};
    unsigned paint=0;
    const auto stamp=[&](const XMFLOAT3&,const XMFLOAT3&,wi::ecs::Entity){++paint;};
    Check(trail.Update(surface,42,stamp)==1 && paint==1,"channel scorch failed first contact");
    for(int i=0;i<60;++i)trail.Update(surface,42,stamp);
    Check(paint==1,"stationary beam spammed scorch decals");
    surface.contact.position.x=.45f;
    Check(trail.Update(surface,42,stamp)>=4 && paint>=5,"moving beam failed to paint wall");
    const auto previous=paint;
    surface.contact.position.x=7.f;
    Check(trail.Update(surface,42,stamp)==1 && paint==previous+1,"beam drew across a contact discontinuity");
    surface.contact.surfaceType=renegade::bridge::ImpactSurfaceType::Character;
    Check(trail.Update(surface,42,stamp)==0 && !trail.touching,"beam marked flesh as wall");
    surface.contact.surfaceType=renegade::bridge::ImpactSurfaceType::Water;
    Check(trail.Update(surface,42,stamp)==0,"beam marked water");
    surface.contact.surfaceType=renegade::bridge::ImpactSurfaceType::Concrete;
    surface.contact.position.x=0;
    Check(trail.Update(surface,42,stamp)==1,"beam did not resume after a nonmarkable target");
    trail.Reset();Check(!trail.touching,"beam mark reset retained contact");
}
