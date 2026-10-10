#include "renegade/bridge/EquipmentAssetService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/PlayerPrefabService.h"
#include "renegade/bridge/ReusableAssetDependencyService.h"
#include "json.hpp"
#include "../Runtime/src/RuntimeEquipmentLoadout.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
using namespace renegade::bridge;
namespace fs=std::filesystem;
void Check(bool ok,const std::string& message) {
    if(!ok){std::cerr<<"EQUIPMENT ASSET FAIL // "<<message<<"\n";std::exit(1);}
}
#include "EquipmentMeleeChainTests.h"
int main() {
    const auto root=fs::temp_directory_path()/("renegade-equipment-"+GenerateStableId());
    fs::create_directories(root);const auto project=GenerateStableId();
    AssetRegistry registry;registry.projectId=project;
    Check(WriteAssetRegistry(root.generic_u8string(),registry).success,"registry");
    EquipmentDefinition sword{GenerateStableId(),"Sword","",EquipmentHandUse::PrimaryOnly,
        {{EquipmentAction::PrimaryUse,"Attack",0.1f,0.2f,0.3f,0.4f,false,true}}};
    std::string text,error;EquipmentAssetDocument parsed;
    Check(SerializeEquipmentAsset({project,sword},text,error)&&DeserializeEquipmentAsset(text,parsed,error)&&
        parsed.equipment.actions[0].recoverySeconds==0.4f,"schema roundtrip "+error);
    const auto canonical=text;auto malformed=nlohmann::json::parse(text);
    malformed["schema_version"]=2;
    Check(!DeserializeEquipmentAsset(malformed.dump(),parsed,error),"future equipment schema");
    malformed=nlohmann::json::parse(text);malformed["actions"][0]["hold_until_release"]=1;
    Check(!DeserializeEquipmentAsset(malformed.dump(),parsed,error),"numeric boolean accepted");
    auto invalid=sword;invalid.actions.push_back(invalid.actions[0]);
    Check(!SerializeEquipmentAsset({project,invalid},text,error),"duplicate action");
    auto hitscan=sword;hitscan.name="Hitscan rifle";
    hitscan.projectiles={{EquipmentAction::PrimaryUse,"","",0,0,"",EquipmentFireMode::Hitscan,125.0f,18.0f}};
    Check(SerializeEquipmentAsset({project,hitscan},text,error),"hitscan serialize "+error);
    auto hitscanJson=nlohmann::json::parse(text);
    Check(hitscanJson.at("schema_version")==3 &&
        hitscanJson.at("projectiles")[0].at("fire_mode")=="Hitscan","hitscan schema v3");
    Check(DeserializeEquipmentAsset(text,parsed,error)&&
        parsed.equipment.projectiles.size()==1&&
        parsed.equipment.projectiles[0].fireMode==EquipmentFireMode::Hitscan&&
        parsed.equipment.projectiles[0].projectileAssetId.empty()&&
        parsed.equipment.projectiles[0].hitscanRangeMetres==125.0f&&
        parsed.equipment.projectiles[0].hitscanDamage==18.0f,"hitscan roundtrip "+error);
    auto invalidHitscan=hitscan;invalidHitscan.projectiles[0].projectileAssetId=GenerateStableId();
    Check(!SerializeEquipmentAsset({project,invalidHitscan},text,error),"hitscan accepted projectile asset");
    invalidHitscan=hitscan;invalidHitscan.projectiles[0].hitscanRangeMetres=0;
    Check(!SerializeEquipmentAsset({project,invalidHitscan},text,error),"zero hitscan range accepted");
    auto savedHitscan=SaveEquipmentAsset(root.generic_u8string(),project,hitscan);
    Check(savedHitscan.succeeded&&LoadEquipmentAsset(root.generic_u8string(),project,
        savedHitscan.document.equipment.assetId,parsed,error)&&
        parsed.equipment.projectiles[0].fireMode==EquipmentFireMode::Hitscan,
        "hitscan save/reopen "+error);
    renegade::runtime::RuntimeEquipmentLoadout hitscanRuntime;
    PlayerControllerSettings hitscanSettings;hitscanSettings.primaryEquipmentAssetId=savedHitscan.document.equipment.assetId;
    Check(hitscanRuntime.Load(root.generic_u8string(),project,hitscanSettings)&&
        hitscanRuntime.resolvedProjectiles.size()==1&&
        hitscanRuntime.resolvedProjectiles[0].fireMode==EquipmentFireMode::Hitscan&&
        hitscanRuntime.resolvedProjectiles[0].projectile.assetId.empty()&&
        hitscanRuntime.resolvedProjectiles[0].hitscanRangeMetres==125.0f,
        "hitscan Runtime resolution "+hitscanRuntime.error);
    auto saved=SaveEquipmentAsset(root.generic_u8string(),project,sword);
    Check(saved.succeeded&&LoadEquipmentAsset(root.generic_u8string(),project,saved.document.equipment.assetId,parsed,error),"save/reopen "+error);
    Check(parsed.equipment.assetId!=sword.assetId,"immutable save identity");
    Check(!LoadEquipmentAsset(root.generic_u8string(),GenerateStableId(),parsed.equipment.assetId,parsed,error),"foreign project accepted");
    auto shield=sword;shield.name="Shield";shield.handUse=EquipmentHandUse::OffHandOnly;
    shield.actions={{EquipmentAction::Block,"Block",0,0,0,.2f,false,true,true}};
    auto savedShield=SaveEquipmentAsset(root.generic_u8string(),project,shield);
    Check(savedShield.succeeded && LoadEquipmentAsset(root.generic_u8string(),project,
        savedShield.document.equipment.assetId,parsed,error) &&
        parsed.equipment.actions[0].activeWhileHeld,"active held shield persistence");
    Check(SerializeEquipmentAsset(savedShield.document,text,error),"held serialize");
    auto oldHeld=nlohmann::json::parse(text);oldHeld["actions"][0].erase("active_while_held");
    Check(DeserializeEquipmentAsset(oldHeld.dump(),parsed,error) &&
        !parsed.equipment.actions[0].activeWhileHeld,"old actions gain no held-active policy");
    oldHeld=nlohmann::json::parse(text);oldHeld["actions"][0]["active_while_held"]=1;
    Check(!DeserializeEquipmentAsset(oldHeld.dump(),parsed,error),"numeric active-held boolean");
    oldHeld=nlohmann::json::parse(text);oldHeld["actions"][0]["unknown"]=true;
    Check(!DeserializeEquipmentAsset(oldHeld.dump(),parsed,error),"unknown held action field");
    auto bow=sword;bow.name="Bow";bow.handUse=EquipmentHandUse::TwoHanded;
    auto savedBow=SaveEquipmentAsset(root.generic_u8string(),project,bow);
    const auto p=saved.document.equipment.assetId,o=savedShield.document.equipment.assetId,b=savedBow.document.equipment.assetId;
    Check(savedShield.succeeded&&savedBow.succeeded&&ValidateStartingEquipment(root.generic_u8string(),project,p,o,error),"independent slots");
    Check(!ValidateStartingEquipment(root.generic_u8string(),project,b,o,error)&&
        !ValidateStartingEquipment(root.generic_u8string(),project,o,"",error)&&
        !ValidateStartingEquipment(root.generic_u8string(),project,"",b,error),"hand exclusions");
    auto missing=sword;missing.presentationAssetId=GenerateStableId();
    Check(!SaveEquipmentAsset(root.generic_u8string(),project,missing).succeeded,"missing presentation");
    const auto before=ListEquipmentAssets(root.generic_u8string(),project,error).size();
    auto failed=SaveEquipmentAsset(root.generic_u8string(),project,sword,
        [](ProjectDocumentTransactionStage stage,size_t,const std::string&,std::string& error){
            if(stage==ProjectDocumentTransactionStage::AfterReplace){error="Injected";return ProjectDocumentTransactionHookAction::Fail;}
            return ProjectDocumentTransactionHookAction::Continue;
        });
    Check(!failed.succeeded&&failed.transaction.rolledBack&&!fs::exists(root/failed.projectRelativePath)&&
        ListEquipmentAssets(root.generic_u8string(),project,error).size()==before,"rollback");
    PlayerControllerSettings settings;settings.primaryEquipmentAssetId=p;settings.offHandEquipmentAssetId=o;
    auto prefab=SavePlayerPrefab(root.generic_u8string(),project,"Sword and Shield",settings);
    PlayerPrefabDocument loaded;
    Check(prefab.succeeded&&LoadPlayerPrefab(root.generic_u8string(),project,prefab.document.assetId,loaded,error)&&
        PlayerSettingsEqual(loaded.settings,settings),"prefab loadout "+error);
    Check(SerializePlayerPrefab(prefab.document,text,error),"prefab serialize");
    auto legacy=nlohmann::json::parse(text);legacy["schema_version"]=1;
    legacy["settings"].erase("primary_equipment_asset_id");legacy["settings"].erase("off_hand_equipment_asset_id");
    Check(DeserializePlayerPrefab(legacy.dump(),loaded,error)&&loaded.settings.primaryEquipmentAssetId.empty()&&
        loaded.settings.offHandEquipmentAssetId.empty(),"v1 compatibility");
    wi::scene::Scene scene;CreatePlayerStartCommand create(scene,{});
    Check(create.Execute(),"start");const auto entity=create.CreatedEntity();CommandService commands;
    Check(commands.Execute(std::make_unique<ApplyPlayerPrefabCommand>(scene,entity,prefab.document)),"apply loadout prefab");
    auto single=settings;single.offHandEquipmentAssetId.clear();
    Check(commands.Execute(std::make_unique<SetPlayerControllerSettingsCommand>(scene,entity,single))&&
        CapturePlayerControllerSettings(scene,entity).offHandEquipmentAssetId.empty()&&commands.Undo()&&
        CapturePlayerControllerSettings(scene,entity).offHandEquipmentAssetId==o,"loadout undo");
    wi::Archive archive;scene.Serialize(archive);archive.SetReadModeAndResetPos(true);
    wi::scene::Scene reopened;reopened.Serialize(archive);const auto start=ResolvePlayerStart(reopened);
    Check(start.resolution==PlayerStartResolution::Success&&PlayerSettingsEqual(start.start.settings,settings),"scene cold reopen");
    DependencyNode source;source.projectRelativePath=prefab.projectRelativePath;source.dependencyClass=DependencyClass::Data;
    std::vector<DependencyCandidate> candidates;
    Check(ReusableAssetDependencyProvider(project).Discover({root.generic_u8string(),&source},
        [&](const auto& c){candidates.push_back(c);},{},error)&&candidates.size()==2,"prefab dependency discovery "+error);
    Check(ReadAssetRegistry(root.generic_u8string(),project,registry,error),"registry reread");
    bool hasEdges=false;
    for(const auto& r:registry.records)if(r.assetId==prefab.document.assetId)hasEdges=r.dependencyAssetIds.size()==2;
    Check(hasEdges,"prefab loadout registry edges");
    settings.primaryEquipmentAssetId="invalid";
    Check(!SerializePlayerPrefab({project,GenerateStableId(),"Bad",settings},text,error),"invalid loadout ID");
    renegade::runtime::RuntimeEquipmentLoadout runtime;
    GameplayInputFrame input;
    input.firePressed=input.reloadPressed=input.aimDown=input.toggleEquipmentPressed=true;
    input.player.moveForward=1;
    Check(runtime.Load(root.generic_u8string(),project,{}) &&
        !runtime.authored && runtime.Route(input,true).firePressed &&
        runtime.Presentation("legacy")=="legacy","legacy equipment compatibility");
    PlayerControllerSettings loadout;
    loadout.primaryEquipmentAssetId=p;loadout.offHandEquipmentAssetId=o;
    Check(runtime.Load(root.generic_u8string(),project,loadout)&&runtime.ready&&
        runtime.primary.equipment.assetId==p&&runtime.offHand.equipment.assetId==o&&
        runtime.Presentation("legacy").empty(),"loadout ownership and no inherited weapon");
    auto routed=runtime.Route(input,true);
    Check(!routed.firePressed&&!routed.reloadPressed&&!routed.aimDown&&
        !routed.toggleEquipmentPressed&&routed.player.moveForward==1,
        "staged or missing actions bypassed admission");
    auto immediate=sword;
    immediate.actions={{EquipmentAction::PrimaryUse,"Attack",0,0,1,0,false,true},
        {EquipmentAction::Unequip,"Unequip",0,0,1,0,false,true}};
    const auto savedImmediate=SaveEquipmentAsset(root.generic_u8string(),project,immediate);
    loadout.primaryEquipmentAssetId=savedImmediate.document.equipment.assetId;
    loadout.offHandEquipmentAssetId.clear();
    Check(savedImmediate.succeeded&&runtime.Load(root.generic_u8string(),project,loadout),
        "immediate Runtime loadout");
    routed=runtime.Route(input,true);
    Check(routed.firePressed&&routed.toggleEquipmentPressed&&!routed.reloadPressed&&
        !routed.aimDown&&!runtime.Route(input,false).toggleEquipmentPressed,
        "semantic primary and holster admission");
    auto& staged = runtime.primary.equipment.actions[0];
    staged.prepareSeconds=.1f; staged.windupSeconds=.2f; staged.recoverySeconds=.3f;
    GameplayInputFrame press;press.firePressed=true;
    Check(!runtime.RouteStaged(press,true,false,.05f).firePressed &&
        runtime.actions.ReservedHands()==1,"prepare reservation");
    GameplayInputFrame quiet;
    Check(!runtime.RouteStaged(quiet,true,false,0).firePressed,"paused preparation");
    Check(!runtime.RouteStaged(quiet,true,true,1).firePressed,"native busy stole staged action");
    Check(!runtime.RouteStaged(quiet,true,false,.1f).firePressed,"windup dispatched early");
    Check(runtime.RouteStaged(quiet,true,false,.2f).firePressed,"active dispatch missing");
    Check(!runtime.RouteStaged(press,true,true,10).firePressed &&
        runtime.actions.ReservedHands()==1,"native active duration released ownership");
    Check(!runtime.RouteStaged(press,true,false,.1f).firePressed &&
        runtime.actions.ReservedHands()==1,"recovery bypassed");
    Check(!runtime.RouteStaged(quiet,true,false,.3f).firePressed &&
        runtime.actions.ReservedHands()==0,"recovery reservation retained");
    Check(runtime.RouteStaged(press,true,false,.4f).firePressed,"next action not admitted");
    runtime.actions.Reset();runtime.dispatched=false;
    staged.holdUntilRelease=true;
    press.fireDown=true;
    Check(!runtime.RouteStaged(press,true,false,1).firePressed &&
        runtime.actions.Channels()[0].phase==EquipmentActionPhase::Hold,"held action did not wait");
    auto held=quiet;held.fireDown=true;
    Check(!runtime.RouteStaged(held,true,false,2).firePressed &&
        runtime.actions.ReservedHands()==1,"held action dispatched without release");
    Check(!runtime.RouteStaged(quiet,true,false,0).firePressed,"pause released held action");
    Check(runtime.RouteStaged(quiet,true,false,.01f).firePressed,"release did not dispatch");
    Check(!runtime.RouteStaged(quiet,true,false,.01f).firePressed,"release dispatched twice");
    runtime.actions.Reset();runtime.dispatched=false;
    GameplayInputFrame cancel;cancel.cancelEquipmentPressed=true;
    runtime.RouteStaged(press,true,false,.05f);
    Check(!runtime.RouteStaged(cancel,true,false,.01f).firePressed &&
        runtime.actions.ReservedHands()==0,"prepare cancellation failed");
    runtime.RouteStaged(press,true,false,1);
    Check(!runtime.RouteStaged(cancel,true,false,.01f).firePressed &&
        runtime.actions.ReservedHands()==0,"hold cancellation failed");
    staged.cancellableBeforeActive=false;
    runtime.RouteStaged(press,true,false,1);
    cancel.fireDown=true;
    runtime.RouteStaged(cancel,true,false,.01f);
    Check(runtime.actions.ReservedHands()==1,"uncancellable hold was cancelled");
    runtime.actions.Reset();runtime.dispatched=false;
    staged.cancellableBeforeActive=true;
    press.fireDown=false;
    Check(runtime.RouteStaged(press,true,false,1).firePressed,"short tap lost release during windup");
    runtime.RouteStaged(cancel,true,true,.01f);
    Check(runtime.actions.ReservedHands()==1,"cancel interrupted active playback");
    runtime.actions.Reset();runtime.dispatched=false;
    staged.holdUntilRelease=false;
    runtime.primary.equipment.actions[0].animationAction="Reload";
    Check(!runtime.Route(input,true).firePressed,"semantic mismatch admitted as attack");
    runtime.actions.Reset();runtime.dispatched=false;
    runtime.primary.equipment.actions={
        {EquipmentAction::Charge,"Charge",.1f,.1f,0,0,true,true},
        {EquipmentAction::Release,"Release",0,0,1,.2f,false,true}};
    press.fireDown=true;
    Check(runtime.HasChargeRelease(),"explicit charge/release not admitted");
    runtime.RouteStaged(press,true,false,1,false,false);
    Check(runtime.actions.ReservedHands()==0,"missing native charge pairs admitted");
    runtime.RouteStaged(press,true,false,1,false,true);
    Check(runtime.chargePresentation && !runtime.releasePresentation &&
        runtime.actions.Channels()[0].phase==EquipmentActionPhase::Hold,"charge hold presentation missing");
    runtime.RouteStaged(quiet,true,false,0,false,true);
    Check(runtime.chargePresentation && !runtime.releasePresentation,"pause lost charge pose");
    runtime.RouteStaged(quiet,true,false,.01f,false,true);
    Check(!runtime.chargePresentation && runtime.releasePresentation &&
        runtime.actions.ReservedHands()==1 &&
        runtime.actions.Channels()[0].definition.action==EquipmentAction::Release,"release lost reservation");
    runtime.RouteStaged(press,true,true,10,false,true);
    Check(!runtime.releasePresentation && runtime.actions.ReservedHands()==1,"release repeated or freed early");
    runtime.RouteStaged(quiet,true,false,.3f,false,true);
    Check(runtime.actions.ReservedHands()==0,"release recovery stuck");
    runtime.RouteStaged(press,true,false,1,false,true);
    cancel.fireDown=true;
    runtime.RouteStaged(cancel,true,false,.01f,false,true);
    Check(!runtime.chargePresentation && !runtime.releasePresentation &&
        runtime.actions.ReservedHands()==0,"charge cancellation retained pose");
    runtime.primary.equipment.actions[1].windupSeconds=.1f;
    Check(!runtime.HasChargeRelease(),"unsupported release windup admitted");
    runtime.primary.equipment.actions[1].windupSeconds=0;
    runtime.primary.equipment.actions.push_back({EquipmentAction::PrimaryUse,"Attack"});
    Check(!runtime.HasChargeRelease(),"primary-use precedence changed");

    // Native off-hand capability is deliberately false in live Runtime until
    // the supplied pack is verified. These checks prove the action adapter only.
    Check(runtime.Load(root.generic_u8string(),project,{}) ,"router reset");
    runtime.authored=runtime.ready=true;
    runtime.primary.equipment=sword;
    runtime.primary.equipment.actions={{EquipmentAction::PrimaryUse,"Attack",0,0,1,.1f}};
    runtime.offHand.equipment=shield;
    GameplayInputFrame both;both.firePressed=both.fireDown=true;
    both.offHandUsePressed=both.offHandUseDown=both.aimDown=true;
    runtime.RouteStaged(both,true,false,.01f,false,false,false);
    Check(runtime.actions.ReservedHands()==1 && !runtime.offHandBlockPresentation,
        "unavailable native off-hand admitted");
    runtime.actions.Reset();runtime.dispatched=false;
    routed=runtime.RouteStaged(both,true,false,.01f,false,false,true);
    Check(routed.firePressed && !routed.aimDown && runtime.offHandBlockPresentation &&
        runtime.actions.ReservedHands()==3,"independent attack and held block dispatch");
    GameplayInputFrame block;block.offHandUseDown=true;
    runtime.RouteStaged(block,true,true,1,false,false,true);
    Check(runtime.offHandBlockPresentation && runtime.actions.ReservedHands()==3,
        "primary native playback stole shield hold");
    runtime.RouteStaged(block,true,false,.2f,false,false,true);
    Check(runtime.offHandBlockPresentation && runtime.actions.ReservedHands()==2,
        "primary completion changed off-hand");
    runtime.RouteStaged(quiet,true,false,0,false,false,true);
    Check(runtime.offHandBlockPresentation,"pause released active block");
    runtime.RouteStaged(quiet,true,false,.01f,false,false,true);
    Check(!runtime.offHandBlockPresentation && runtime.actions.ReservedHands()==2,
        "off-hand release did not enter its recovery");
    runtime.RouteStaged(quiet,true,false,.3f,false,false,true);
    Check(runtime.actions.ReservedHands()==0,"shield recovery stuck");
    runtime.actions.Reset();runtime.dispatched=false;
    runtime.primary.equipment.actions[0].windupSeconds=.5f;
    runtime.RouteStaged(both,true,false,.01f,false,false,true);
    const auto primaryPhase=runtime.actions.Channels()[1].phase;
    runtime.RouteStaged(block,true,true,1,false,false,true);
    Check(runtime.offHandBlockPresentation && runtime.actions.Channels()[1].phase==primaryPhase,
        "primary busy paused off-hand or advanced primary");
    runtime.actions.Reset();runtime.dispatched=false;
    runtime.offHand.equipment.actions[0].windupSeconds=.5f;
    auto offPress=block;offPress.offHandUsePressed=true;
    runtime.RouteStaged(offPress,true,true,.01f,false,false,true);
    runtime.RouteStaged(cancel,true,true,.01f,false,false,true);
    Check(runtime.actions.ReservedHands()==0,"off-hand preparation cancel failed");
    runtime.actions.Reset();runtime.dispatched=false;
    runtime.primary.equipment.actions[0].activeWhileHeld=true;
    runtime.primary.equipment.actions[0].activeSeconds=0;
    Check(!runtime.Route(input,true).firePressed &&
        !runtime.RouteStaged(press,true,false,1).firePressed,
        "unsupported held primary silently admitted");

    loadout.primaryEquipmentAssetId=GenerateStableId();
    Check(!runtime.Load(root.generic_u8string(),project,loadout)&&runtime.authored&&
        !runtime.ready&&runtime.primary.equipment.assetId.empty()&&
        !runtime.error.empty()&&!runtime.Route(input,true).firePressed&&
        runtime.Presentation("legacy").empty(),"failed load retained stale or legacy equipment");

    TestEquipmentMeleeChains(sword,shield);
    fs::remove_all(root);
    std::cout<<"EQUIPMENT ASSET PASS // schema, identity, journal rollback, hand admission, prefab migration, undo/reopen, dependency edges\n";
}
