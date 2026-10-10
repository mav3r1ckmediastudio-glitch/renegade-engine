#include "renegade/bridge/ProjectileAssetService.h"
#include "renegade/bridge/EquipmentAssetService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ReusableAssetDependencyService.h"
#include "renegade/bridge/TestLevelSnapshotService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/ProjectService.h"
#include "json.hpp"
#include "renegade/bridge/ReusableAssetService.h"
#include <iomanip>
#include <sstream>
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
    // Every starting preset must save without a model, and retain its feedback on reload.
    for(unsigned i=0;i<5;++i) {
        auto preset=MakeProjectilePreset(static_cast<ProjectilePreset>(i));
        preset.projectId=project;preset.assetId=GenerateStableId();
        ProjectileAssetDocument copy;
        if(!SerializeProjectileAsset(preset,text,error)||
           !DeserializeProjectileAsset(text,copy,error)||
           copy.name!=preset.name||copy.flightEffects.size()!=preset.flightEffects.size()||
           copy.impactEffect!=preset.impactEffect||copy.gravityScale!=preset.gravityScale)
            return fail("preset serialization");
        if((i==0||i==4) && (copy.flightEffects.empty()||
           copy.flightEffects[0].kind==ProjectileEffectKind::None))
            return fail("model-free preset feedback");
    }
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
    corrupt=nlohmann::json::parse(text); corrupt["schema_version"]=4;
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
    // Mesh appearance is a governed dependency, including legacy migration.
    if(!SerializeProjectileAsset(saved.document,text,error))return fail("appearance serialize");
    auto legacy=nlohmann::json::parse(text);
    legacy["schema_version"]=1;legacy.erase("mesh_asset_id");
    legacy.erase("visual_scale");legacy.erase("visual_rotation_degrees");
    for(const char* field:{"flight_effects","impact_effect","stick_on_impact","stuck_lifetime_seconds","embed_depth_metres"})legacy.erase(field);
    if(!DeserializeProjectileAsset(legacy.dump(),reopened,error)||!reopened.meshAssetId.empty()||
        reopened.visualScale!=1)return fail("legacy appearance defaults");
    auto invalidMesh=arrow;invalidMesh.meshAssetId=GenerateStableId();
    if(SaveProjectileAsset(root.generic_u8string(),project,invalidMesh).succeeded)
        return fail("missing appearance accepted");
    ReusableModelAssetDocument modelDocument;
    modelDocument.manifest.projectId=project;modelDocument.manifest.assetId=presentation;
    modelDocument.manifest.sourceAssetId=GenerateStableId();modelDocument.manifest.sourceFormat="fbx";
    modelDocument.manifest.importer="wicked.ufbx";
    modelDocument.manifest.settingsJson=R"({"options":{},"source_format":"fbx"})";
    modelDocument.payload={0x57,0x49,0x53,0x43,0x45,0x4e,0x45,0x01};
    std::uint64_t hash=1469598103934665603ull;
    for(auto byte:modelDocument.payload){hash^=byte;hash*=1099511628211ull;}
    std::ostringstream hashText;hashText<<"fnv1a64:"<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;
    modelDocument.manifest.payloadHash=hashText.str();
    std::vector<std::uint8_t> bytes;
    if(!SerializeReusableModelAssetDocument(modelDocument,bytes,error))return fail("mesh document");
    {std::ofstream out(root/"Content/Test.rasset",std::ios::binary|std::ios::trunc);
     out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
    auto visible=arrow;visible.meshAssetId=presentation;visible.visualScale=.5f;
    visible.visualRotationDegrees={90,0,-45};
    visible.flightEffects={{ProjectileEffectKind::Flame,{0,0,.3f},.08f,60,.3f},
                           {ProjectileEffectKind::Smoke,{0,0,.3f},.08f,60,.3f}};
    visible.impactEffect=ProjectileEffectKind::Sparks;visible.stickOnImpact=true;
    visible.stuckLifetimeSeconds=20;visible.embedDepthMetres=.04f;
    const auto visualSaved=SaveProjectileAsset(root.generic_u8string(),project,visible);
    if(!visualSaved.succeeded||!LoadProjectileAsset(root.generic_u8string(),project,
        visualSaved.document.assetId,reopened,error)||reopened.meshAssetId!=presentation||
        reopened.visualScale!=.5f||reopened.visualRotationDegrees!=visible.visualRotationDegrees)
        return fail("appearance save/reopen");
    if(reopened.flightEffects.size()!=2||reopened.flightEffects[0].kind!=ProjectileEffectKind::Flame||
       !reopened.stickOnImpact||reopened.impactEffect!=ProjectileEffectKind::Sparks||
       reopened.stuckLifetimeSeconds!=20||reopened.embedDepthMetres!=.04f)
        return fail("effects and stick persistence");
    auto invalidEffect=reopened;invalidEffect.flightEffects[0].particlesPerSecond=501;
    if(ValidateProjectileAsset(invalidEffect,error))return fail("effect budget validation");
    invalidEffect=reopened;invalidEffect.meshAssetId.clear();
    if(ValidateProjectileAsset(invalidEffect,error))return fail("meshless stick validation");
    if(!SerializeProjectileAsset(reopened,text,error))return fail("v3 serial");
    auto old2=nlohmann::json::parse(text);old2["schema_version"]=2;
    for(const char* field:{"flight_effects","impact_effect","stick_on_impact","stuck_lifetime_seconds","embed_depth_metres"})old2.erase(field);
    ProjectileAssetDocument oldAppearance;
    if(!DeserializeProjectileAsset(old2.dump(),oldAppearance,error)||oldAppearance.stickOnImpact||
       !oldAppearance.flightEffects.empty())return fail("v2 policy defaults");
    source.projectRelativePath=visualSaved.projectRelativePath;candidates.clear();
    if(!ReusableAssetDependencyProvider(project).Discover({root.generic_u8string(),&source},
        [&](const auto& c){candidates.push_back(c);},{},error)||
        candidates.size()!=1||candidates[0].declaredPath!="Content/Test.rasset"||
        candidates[0].requirement!=DependencyRequirement::Required)
        return fail("appearance package dependency");
    if(!SerializeProjectileAsset(reopened,text,error))return fail("appearance json");
    auto invalidAppearance=nlohmann::json::parse(text);invalidAppearance["visual_scale"]=true;
    if(DeserializeProjectileAsset(invalidAppearance.dump(),reopened,error))return fail("boolean scale");
    invalidAppearance=nlohmann::json::parse(text);invalidAppearance["visual_rotation_degrees"]={0,0,361};
    if(DeserializeProjectileAsset(invalidAppearance.dump(),reopened,error))return fail("rotation bounds");
    // Build Game records the dependency discovery provider while retaining
    // authored IDs, paths and dependencies. Saved assets must still load/list.
    AssetRegistry packagedRegistry;
    if(!ReadAssetRegistry(root.generic_u8string(),project,packagedRegistry,error))
        return fail("package registry");
    for(auto& record:packagedRegistry.records)
        if(record.provider=="renegade.equipment" || record.provider=="renegade.projectile")
            record.provider="lp07.rasset";
    if(!WriteAssetRegistry(root.generic_u8string(),packagedRegistry).success ||
       !LoadEquipmentAsset(root.generic_u8string(),project,equipped.document.equipment.assetId,item,error) ||
       !LoadProjectileAsset(root.generic_u8string(),project,visualSaved.document.assetId,reopened,error) ||
       ListProjectileAssets(root.generic_u8string(),project,error).empty())
        return fail("build discovery provider rejected saved equipment/projectiles");
    AssetRegistry tampered;
    if(!ReadAssetRegistry(root.generic_u8string(),project,tampered,error))return fail("mesh registry");
    for(auto& record:tampered.records)
        if(record.assetId==visualSaved.document.assetId)record.dependencyAssetIds.clear();
    if(!WriteAssetRegistry(root.generic_u8string(),tampered).success ||
        LoadProjectileAsset(root.generic_u8string(),project,visualSaved.document.assetId,reopened,error))
        return fail("appearance registry mismatch accepted");
    fs::remove(root/saved.projectRelativePath);
    if(LoadEquipmentAsset(root.generic_u8string(),project,equipped.document.equipment.assetId,item,error))
        return fail("missing bound projectile silently loaded");
    fs::remove_all(root);
    std::cout<<"ProjectileAssetTests passed\n";
    return 0;
}
