#include "renegade/bridge/ProjectileAssetService.h"
#include "renegade/bridge/EquipmentAssetService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ReusableAssetDependencyService.h"
#include "renegade/bridge/TestLevelSnapshotService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/ProjectService.h"
#include "json.hpp"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/PlayerPrefabService.h"
#include "renegade/bridge/CommandService.h"
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace renegade::bridge;
namespace fs=std::filesystem;
int main()
{
    const auto root=fs::temp_directory_path()/("renegade-projectile-assets-"+GenerateStableId());
    fs::create_directories(root);
    const auto project=GenerateStableId();
    AssetRegistry registry; registry.projectId=project;
    // Equipment admission checks registry/path; this fixture is not a rendered model proof.
    const StableId presentation="ffffffff-ffff-4fff-8fff-ffffffffffff";
    fs::create_directories(root/"Content");
    std::ofstream(root/"Content/Test.rasset")<<"fixture";
    AssetRecord model;model.assetId=presentation;model.dependencyNodeId="fixture-model";
    model.projectRelativePath="Content/Test.rasset";
    model.dependencyClass=DependencyClass::ImportedContent;model.provider="fixture";model.contentHash="fnv1a64:0000000000000001";
    registry.records.push_back(model);
    auto fail=[](const char* reason) { std::cerr<<reason<<'\n'; return 1; };
    if(!WriteAssetRegistry(root.generic_u8string(),registry).success)return fail("registry");
    std::string error,text;
    auto arrow=MakeProjectilePreset(ProjectilePreset::Arrow);
    auto saved=SaveProjectileAsset(root.generic_u8string(),project,arrow);
    ProjectileAssetDocument reopened;
    if(!saved.succeeded||!LoadProjectileAsset(root.generic_u8string(),project,
        saved.document.assetId,reopened,error)||reopened.name!="Arrow"||
        reopened.speedMetresPerSecond!=45||reopened.gravityScale!=1)
        return fail("projectile save/reopen");
    const auto simulation=ProjectileSimulationDefinition(reopened);
    if(simulation.acceleration.y!=-9.81f||simulation.lifetimeSeconds!=5)
        return fail("simulation mapping");
    if(!SerializeProjectileAsset(reopened,text,error))return fail("serialize");
    auto corrupt=nlohmann::json::parse(text); corrupt["speed_metres_per_second"]=true;
    if(DeserializeProjectileAsset(corrupt.dump(),reopened,error))return fail("boolean speed");
    corrupt=nlohmann::json::parse(text); corrupt["schema_version"]=2;
    if(DeserializeProjectileAsset(corrupt.dump(),reopened,error))return fail("future projectile schema");
    auto bad=arrow; bad.name="   ";
    if(SaveProjectileAsset(root.generic_u8string(),project,bad).succeeded)return fail("blank name");
    if(LoadProjectileAsset(root.generic_u8string(),GenerateStableId(),saved.document.assetId,reopened,error))
        return fail("foreign project");
    const auto failed=SaveProjectileAsset(root.generic_u8string(),project,arrow,
        [](ProjectDocumentTransactionStage stage,std::size_t,const std::string&,std::string& error) {
            if(stage==ProjectDocumentTransactionStage::AfterReplace) {
                error="injected";return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        });
    if(failed.succeeded||!failed.transaction.rolledBack||fs::exists(root/failed.projectRelativePath)||
        ListProjectileAssets(root.generic_u8string(),project,error).size()!=1)
        return fail("projectile rollback");

    EquipmentDefinition weapon{GenerateStableId(),"Bow","",EquipmentHandUse::TwoHanded,
        {{EquipmentAction::Charge,"Charge",0,0,1,0,true,true},
         {EquipmentAction::Release,"Release",0,0,0.2f,0,false,true}}};
    const auto original=weapon;
    weapon.presentationAssetId=presentation;
    weapon.projectiles.push_back({EquipmentAction::Release,saved.document.assetId});
    auto equipped=SaveEquipmentAsset(root.generic_u8string(),project,weapon);
    EquipmentAssetDocument item;
    if(!equipped.succeeded||!LoadEquipmentAsset(root.generic_u8string(),project,
        equipped.document.equipment.assetId,item,error)||item.equipment.projectiles.size()!=1||
        item.equipment.handUse!=original.handUse||item.equipment.actions.size()!=2||
        item.equipment.actions[0].animationAction!="Charge")
        return fail("weapon binding preserves existing setup");
    if(!SerializeEquipmentAsset(item,text,error)||
        nlohmann::json::parse(text)["schema_version"]!=2)
        return fail("schema2 projectile binding");
    const auto schema2=text;
    wi::scene::Scene scene;
    CreatePlayerStartCommand create(scene,{});
    if(!create.Execute())return fail("player start");
    const auto player=create.CreatedEntity();
    CommandService commands;
    auto settings=CapturePlayerControllerSettings(scene,player);
    const auto before=settings;
    settings.primaryEquipmentAssetId=equipped.document.equipment.assetId;
    if(!commands.Execute(std::make_unique<SetPlayerControllerSettingsCommand>(scene,player,settings))||
       !commands.Undo()||!PlayerSettingsEqual(CapturePlayerControllerSettings(scene,player),before)||
       !commands.Redo()||!PlayerSettingsEqual(CapturePlayerControllerSettings(scene,player),settings))
        return fail("projectile assignment undo/redo");
    wi::Archive archive;scene.Serialize(archive);archive.SetReadModeAndResetPos(true);
    wi::scene::Scene cold;cold.Serialize(archive);
    const auto start=ResolvePlayerStart(cold);
    if(start.resolution!=PlayerStartResolution::Success||
       start.start.settings.primaryEquipmentAssetId!=settings.primaryEquipmentAssetId||
       !LoadEquipmentAsset(root.generic_u8string(),project,
           start.start.settings.primaryEquipmentAssetId,item,error)||
       item.equipment.projectiles[0].projectileAssetId!=saved.document.assetId)
        return fail("saved player cold reopen retains projectile binding");
    auto duplicate=weapon; duplicate.projectiles.push_back(duplicate.projectiles.front());
    if(SaveEquipmentAsset(root.generic_u8string(),project,duplicate).succeeded)return fail("duplicate binding");
    auto missing=weapon;missing.projectiles[0].projectileAssetId=GenerateStableId();
    if(SaveEquipmentAsset(root.generic_u8string(),project,missing).succeeded)return fail("missing projectile");
    auto wrongAction=weapon;wrongAction.projectiles[0].action=EquipmentAction::PrimaryUse;
    if(SaveEquipmentAsset(root.generic_u8string(),project,wrongAction).succeeded)return fail("unavailable action");
    if(!SerializeEquipmentAsset({project,original},text,error)||
        nlohmann::json::parse(text)["schema_version"]!=1||
        !DeserializeEquipmentAsset(text,item,error)||!item.equipment.projectiles.empty())
        return fail("legacy schema retained");

    DependencyNode source;source.projectRelativePath=equipped.projectRelativePath;
    source.dependencyClass=DependencyClass::Data;
    std::vector<DependencyCandidate> candidates;
    if(!ReusableAssetDependencyProvider(project).Discover({root.generic_u8string(),&source},
        [&](const auto& candidate){candidates.push_back(candidate);},{},error)||
        candidates.size()!=2||candidates[1].declaredPath!=saved.projectRelativePath||
        candidates[1].dependencyClass!=DependencyClass::Data)
        return fail("projectile packaging dependency");
    // Test Level must contain the actual projectile, not only registry edges.
    auto snapshotWeapon=weapon;snapshotWeapon.presentationAssetId.clear();
    auto snapshotItem=SaveEquipmentAsset(root.generic_u8string(),project,snapshotWeapon);
    if(!snapshotItem.succeeded)return fail("snapshot equipment fixture");
    SceneService snapshotScenes;
    CreatePlayerStartCommand snapshotPlayer(snapshotScenes.GetScene(),{});
    if(!snapshotPlayer.Execute())return fail("snapshot player");
    PlayerControllerSettings snapshotSettings;
    snapshotSettings.primaryEquipmentAssetId=snapshotItem.document.equipment.assetId;
    SetPlayerControllerSettingsCommand snapshotAssignment(snapshotScenes.GetScene(),
        snapshotPlayer.CreatedEntity(),snapshotSettings);
    if(!snapshotAssignment.Execute())return fail("snapshot assignment");
    ProjectMetadata metadata;metadata.projectId=project;metadata.rootPath=root.generic_u8string();
    metadata.name="Projectile snapshot";
    TestLevelSnapshotService snapshots(snapshotScenes,commands);
    TestLevelSnapshot snapshot;
    if(!snapshots.Create(metadata,snapshot,error)||
       !LoadEquipmentAsset(snapshot.sessionDirectory,project,snapshotSettings.primaryEquipmentAssetId,item,error)||
       !LoadProjectileAsset(snapshot.sessionDirectory,project,saved.document.assetId,reopened,error))
    {std::cerr<<error;return fail("Test Level projectile closure");}
    if(!snapshots.Cleanup(snapshot,error))return fail("snapshot cleanup");
    fs::remove(root/saved.projectRelativePath);
    if(LoadEquipmentAsset(root.generic_u8string(),project,equipped.document.equipment.assetId,item,error))
        return fail("missing bound projectile silently loaded");
    fs::remove_all(root);
    std::cout<<"ProjectileAssetTests passed\n";
    return 0;
}
