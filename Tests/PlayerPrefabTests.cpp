#include "renegade/bridge/PlayerPrefabService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/AssetCatalogueService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <cstdlib>

using namespace renegade::bridge;
namespace fs=std::filesystem;
void Check(bool ok,const std::string& message)
{if(!ok){std::cerr<<"PLAYER PREFAB FAIL // "<<message<<"\n";std::exit(1);}}
int main()
{
    std::string error,text;
    PlayerPrefabDocument d{GenerateStableId(),GenerateStableId(),"Shotgun Player",{}},parsed;
    d.settings.walkSpeed=3.25f;d.settings.sprintSpeed=6.5f;
    d.settings.capsuleRadius=0.45f;d.settings.capsuleHeight=0.55f;
    d.settings.firstPersonArmsAssetId=GenerateStableId();
    Check(SerializePlayerPrefab(d,text,error)&&DeserializePlayerPrefab(text,parsed,error)&&
        PlayerSettingsEqual(parsed.settings,d.settings),"typed settings roundtrip "+error);
    auto bad=text;auto pos=bad.find("\"schema_version\": 1");
    bad.replace(pos,19,"\"schema_version\": 2");
    Check(!DeserializePlayerPrefab(bad,parsed,error),"future schema accepted");
    auto invalid=d;invalid.settings.capsuleRadius=-1;
    Check(!SerializePlayerPrefab(invalid,bad,error),"negative capsule accepted");
    invalid=d;invalid.settings.walkSpeed=std::numeric_limits<float>::quiet_NaN();
    Check(!SerializePlayerPrefab(invalid,bad,error),"NaN accepted");
    invalid=d;invalid.settings.firstPersonArmsAssetId="broken";
    Check(!SerializePlayerPrefab(invalid,bad,error),"invalid arms ID accepted");

    wi::scene::Scene scene;
    TransformState pose;pose.translation=XMFLOAT3(5,2,-7);
    XMStoreFloat4(&pose.rotation,XMQuaternionRotationRollPitchYaw(0,1.2f,0));
    CreatePlayerStartCommand create(scene,pose);Check(create.Execute(),"create start");
    const auto entity=create.CreatedEntity();
    CommandService commands;
    Check(commands.Execute(std::make_unique<ApplyPlayerPrefabCommand>(scene,entity,d)),"apply");
    Check(CapturePlayerPrefabOrigin(scene,entity)==d.assetId&&
        PlayerSettingsEqual(CapturePlayerControllerSettings(scene,entity),d.settings),"assignment wrong");
    Check(commands.Undo()&&CapturePlayerPrefabOrigin(scene,entity).empty()&&
        CapturePlayerControllerSettings(scene,entity).walkSpeed==4.5f,"undo did not restore custom player");
    Check(commands.Redo(),"redo failed");
    Check(ResolvePlayerStart(scene).start.transform.translation.x==5&&
        ResolvePlayerStart(scene).start.transform.rotation.y==pose.rotation.y,"prefab changed spawn");
    auto local=d.settings;local.walkSpeed=2;
    Check(commands.Execute(std::make_unique<SetPlayerControllerSettingsCommand>(scene,entity,local)),"local edit");
    PlayerPrefabDocument baseline;
    Check(CapturePlayerPrefabBaseline(scene,entity,baseline,error)&&
        !PlayerSettingsEqual(local,baseline.settings),"override baseline lost");
    Check(commands.Execute(std::make_unique<ApplyPlayerPrefabCommand>(scene,entity,baseline)),"reset");
    Check(PlayerSettingsEqual(CapturePlayerControllerSettings(scene,entity),d.settings)&&commands.Undo()&&
        CapturePlayerControllerSettings(scene,entity).walkSpeed==2,"reset undo");
    Check(commands.Redo(),"reset redo");
    wi::Archive archive;scene.Serialize(archive);archive.SetReadModeAndResetPos(true);
    wi::scene::Scene reopened;reopened.Serialize(archive);
    const auto start=ResolvePlayerStart(reopened);
    Check(start.resolution==PlayerStartResolution::Success&&
        CapturePlayerPrefabBaseline(reopened,start.start.entity,baseline,error)&&
        PlayerSettingsEqual(start.start.settings,d.settings),"scene roundtrip");

    const auto root=fs::temp_directory_path()/("renegade-player-prefab-"+GenerateStableId());
    fs::create_directories(root);
    AssetRegistry registry;registry.projectId=d.projectId;
    Check(WriteAssetRegistry(root.generic_u8string(),registry).success,"registry fixture");
    auto settings=d.settings;settings.firstPersonArmsAssetId.clear();
    auto saved=SavePlayerPrefab(root.generic_u8string(),d.projectId,"Saved Player",settings);
    Check(saved.succeeded,"save "+saved.error);
    Check(LoadPlayerPrefab(root.generic_u8string(),d.projectId,saved.document.assetId,parsed,error)&&
        PlayerSettingsEqual(parsed.settings,settings),"cold load "+error);
    const auto count=ListPlayerPrefabs(root.generic_u8string(),d.projectId,error).size();
    Check(count==1,"list");
    auto failed=SavePlayerPrefab(root.generic_u8string(),d.projectId,"Must Roll Back",settings,
        [](ProjectDocumentTransactionStage stage,std::size_t,const std::string&,std::string& error){
            if(stage==ProjectDocumentTransactionStage::AfterReplace)
            {error="Injected prefab failure";return ProjectDocumentTransactionHookAction::Fail;}
            return ProjectDocumentTransactionHookAction::Continue;
        });
    Check(!failed.succeeded&&failed.transaction.rolledBack&&
        !fs::exists(root/failed.projectRelativePath)&&
        ListPlayerPrefabs(root.generic_u8string(),d.projectId,error).size()==count,"transaction rollback");
    Check(!LoadPlayerPrefab(root.generic_u8string(),GenerateStableId(),saved.document.assetId,parsed,error),
        "cross-project prefab accepted");
    Check(!SavePlayerPrefab(root.generic_u8string(),d.projectId,"Missing Arms",d.settings).succeeded,
        "unregistered arms accepted");
    Check(EnsureBasicPlayerPrefab(root.generic_u8string(),d.projectId,error),"basic preset create "+error);
    Check(EnsureBasicPlayerPrefab(root.generic_u8string(),d.projectId,error)&&
        ListPlayerPrefabs(root.generic_u8string(),d.projectId,error).size()==2,"basic preset duplicated");
    AssetRegistry indexed; Check(ReadAssetRegistry(root.generic_u8string(),d.projectId,indexed,error),"read catalogue index");
    AssetCatalogueMetadataDocument metadata;metadata.projectId=d.projectId;
    AssetCatalogue catalogue;
    Check(BuildAssetCatalogue(root.generic_u8string(),d.projectId,indexed,metadata,catalogue,error),"player catalogue "+error);
    bool namedBasic=false;
    for(const auto& e:catalogue.entries)
        if(e.name=="Basic Player Start"&&e.type==AssetType::Player&&e.state==AssetCatalogueState::Current)namedBasic=true;
    Check(namedBasic,"basic browser name/type missing");
    wi::scene::Scene emptyLevel;
    Check(ResolvePlayerStart(emptyLevel).resolution==PlayerStartResolution::Missing,"empty level acquired player");
    TransformState drop;drop.translation={12,3,-4};
    CommandService placement;
    auto place=std::make_unique<PlacePlayerPrefabCommand>(emptyLevel,drop,saved.document);
    auto* placed=place.get();
    Check(placement.Execute(std::move(place)),"browser placement");
    const auto placedId=placed->PlacedEntity();
    Check(CapturePlayerPrefabOrigin(emptyLevel,placedId)==saved.document.assetId&&
        ResolvePlayerStart(emptyLevel).start.transform.translation.x==12,"drop settings/position");
    PlacePlayerPrefabCommand duplicate(emptyLevel,drop,saved.document);
    Check(!duplicate.Execute(),"second player admitted");
    Check(placement.Undo()&&ResolvePlayerStart(emptyLevel).resolution==PlayerStartResolution::Missing,"placement undo");
    Check(placement.Redo()&&ResolvePlayerStart(emptyLevel).start.entity==placedId&&
        CapturePlayerPrefabOrigin(emptyLevel,placedId)==saved.document.assetId,"placement redo identity/settings");
    wi::Archive levelArchive;emptyLevel.Serialize(levelArchive);levelArchive.SetReadModeAndResetPos(true);
    wi::scene::Scene coldLevel;coldLevel.Serialize(levelArchive);
    const auto coldStart=ResolvePlayerStart(coldLevel);
    Check(coldStart.resolution==PlayerStartResolution::Success&&coldStart.start.transform.translation.x==12&&
        CapturePlayerPrefabOrigin(coldLevel,coldStart.start.entity)==saved.document.assetId,"drop cold reopen");
    fs::remove_all(root);
    std::cout<<"PLAYER PREFAB PASS // schema, undo/redo, overrides, reset, spawn, scene reopen, registry, rollback\n";
}
