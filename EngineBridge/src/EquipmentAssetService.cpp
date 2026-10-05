#include "renegade/bridge/EquipmentAssetService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "json.hpp"
#include "renegade/bridge/FirstPersonAssemblyService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iomanip>
#include <sstream>
namespace renegade::bridge {
namespace {
using json=nlohmann::json;
namespace fs=std::filesystem;
constexpr const char* HandNames[]={"PrimaryOnly","OffHandOnly","EitherHand","TwoHanded","PrimaryWithSupport"};
constexpr const char* ActionNames[]={"Equip","Unequip","PrimaryUse","AlternateUse","Reload","Charge","Release","Block","Parry","Cast","Use","Inspect"};
template<size_t N> unsigned Index(const std::string& value,const char* const (&names)[N]) {
    for(unsigned i=0;i<N;++i)if(value==names[i])return i;
    throw std::runtime_error("Unknown equipment semantic.");
}
const AssetRecord* Find(const AssetRegistry& registry,const StableId& id) {
    auto it=std::find_if(registry.records.begin(),registry.records.end(),[&](const auto& r){return r.assetId==id;});
    return it==registry.records.end()?nullptr:&*it;
}
bool Valid(const EquipmentAssetDocument& d,std::string& error) {
    if(!IsValidStableId(d.projectId)||!IsValidStableId(d.equipment.assetId)||
       (!d.equipment.presentationAssetId.empty()&&!IsValidStableId(d.equipment.presentationAssetId))||
       d.equipment.name.size()>96||!ValidateEquipmentDefinition(d.equipment)) {
        error="Invalid equipment identity, name or action definition.";return false;
    }
    for(const auto& a:d.equipment.actions)if(a.animationAction.size()>96) {
        error="Equipment semantic animation action is too long.";return false;
    }
    error.clear();return true;
}
bool PresentationAvailable(const std::string& root,const AssetRegistry& registry,
    const StableId& id,std::string& error) {
    if(id.empty())return true;
    const auto* record=Find(registry,id);
    if(!record||!record->sourceAvailable||record->dependencyClass!=DependencyClass::ImportedContent||
       fs::u8path(record->projectRelativePath).extension()!=ReusableAssetExtension) {
        error="Equipment presentation must resolve to an available project .rasset.";return false;
    }
    const auto path=ResolveDependencyPath(root,record->projectRelativePath);
    if(!path.accepted||!path.exists){error="Equipment presentation file is missing or outside the project.";return false;}
    return true;
}
std::string Hash(const std::string& text) {
    uint64_t value=1469598103934665603ull;
    for(unsigned char c:text){value^=c;value*=1099511628211ull;}
    std::ostringstream out;out<<"fnv1a64:"<<std::hex<<std::setfill('0')<<std::setw(16)<<value;return out.str();
}
}
bool SerializeEquipmentAsset(const EquipmentAssetDocument& d,std::string& text,std::string& error) {
    if(!Valid(d,error))return false;
    json actions=json::array();
    for(const auto& a:d.equipment.actions)actions.push_back({
        {"action",ActionNames[unsigned(a.action)]},{"animation_action",a.animationAction},
        {"prepare_seconds",a.prepareSeconds},{"windup_seconds",a.windupSeconds},
        {"active_seconds",a.activeSeconds},{"recovery_seconds",a.recoverySeconds},
        {"hold_until_release",a.holdUntilRelease},{"cancellable_before_active",a.cancellableBeforeActive}});
    text=json{{"format","renegade-equipment"},{"schema_version",1},{"project_id",d.projectId},
        {"asset_id",d.equipment.assetId},{"name",d.equipment.name},
        {"presentation_asset_id",d.equipment.presentationAssetId},
        {"hand_use",HandNames[unsigned(d.equipment.handUse)]},{"actions",actions}}.dump(2);
    return true;
}
bool DeserializeEquipmentAsset(const std::string& text,EquipmentAssetDocument& out,std::string& error) {
    try {
        const auto j=json::parse(text);
        if(!j.is_object()||j.size()!=8||j.at("format")!="renegade-equipment"||
           !j.at("schema_version").is_number_integer()||j.at("schema_version")!=1)
            throw std::runtime_error("Unsupported equipment schema.");
        EquipmentAssetDocument d;
        d.projectId=j.at("project_id").get<std::string>();
        auto& item=d.equipment;
        item.assetId=j.at("asset_id").get<std::string>();item.name=j.at("name").get<std::string>();
        item.presentationAssetId=j.at("presentation_asset_id").get<std::string>();
        item.handUse=static_cast<EquipmentHandUse>(Index(j.at("hand_use").get<std::string>(),HandNames));
        const auto& actions=j.at("actions");
        if(!actions.is_array()||actions.empty()||actions.size()>32)throw std::runtime_error("Invalid equipment action list.");
        for(const auto& v:actions) {
            if(!v.is_object()||v.size()!=8)throw std::runtime_error("Incomplete equipment action.");
            EquipmentActionDefinition a;
            a.action=static_cast<EquipmentAction>(Index(v.at("action").get<std::string>(),ActionNames));
            a.animationAction=v.at("animation_action").get<std::string>();
            for(const char* key:{"prepare_seconds","windup_seconds","active_seconds","recovery_seconds"})
                if(!v.at(key).is_number())throw std::runtime_error("Action duration must be numeric.");
            a.prepareSeconds=v.at("prepare_seconds").get<float>();a.windupSeconds=v.at("windup_seconds").get<float>();
            a.activeSeconds=v.at("active_seconds").get<float>();a.recoverySeconds=v.at("recovery_seconds").get<float>();
            a.holdUntilRelease=v.at("hold_until_release").get<bool>();
            a.cancellableBeforeActive=v.at("cancellable_before_active").get<bool>();item.actions.push_back(a);
        }
        if(!Valid(d,error))return false;
        out=std::move(d);error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool ReadEquipmentAssetFile(const std::string& path,EquipmentAssetDocument& d,std::string& error) {
    std::ifstream in(fs::u8path(path),std::ios::binary);
    if(!in){error="Cannot read equipment asset.";return false;}
    const std::string text((std::istreambuf_iterator<char>(in)),{});
    if(in.bad()){error="Cannot read complete equipment asset.";return false;}
    return DeserializeEquipmentAsset(text,d,error);
}
bool LoadEquipmentAsset(const std::string& root,const StableId& project,const StableId& id,
    EquipmentAssetDocument& out,std::string& error) {
    AssetRegistry registry;if(!ReadAssetRegistry(root,project,registry,error))return false;
    const auto* record=Find(registry,id);
    if(!record||!record->sourceAvailable||record->dependencyClass!=DependencyClass::Data||
       record->provider!="renegade.equipment"||fs::u8path(record->projectRelativePath).extension()!=EquipmentAssetExtension) {
        error="Equipment asset is missing from the project registry.";return false;
    }
    const auto path=ResolveDependencyPath(root,record->projectRelativePath);
    EquipmentAssetDocument d;
    if(!path.accepted||!path.exists){error="Equipment asset file is missing or outside the project.";return false;}
    if(!ReadEquipmentAssetFile(path.absolutePath,d,error))return false;
    if(d.projectId!=project||d.equipment.assetId!=id){error="Equipment identity does not match its project registry.";return false;}
    const std::vector<StableId> expected=d.equipment.presentationAssetId.empty()?std::vector<StableId>{}:
        std::vector<StableId>{d.equipment.presentationAssetId};
    if(record->dependencyAssetIds!=expected){error="Equipment presentation dependencies do not match its registry.";return false;}
    if(!PresentationAvailable(root,registry,d.equipment.presentationAssetId,error))return false;
    out=std::move(d);error.clear();return true;
}
EquipmentAssetSaveResult SaveEquipmentAsset(const std::string& root,const StableId& project,
    EquipmentDefinition item,ProjectDocumentTransactionHook hook) {
    EquipmentAssetSaveResult r;AssetRegistry registry;
    if(!ReadAssetRegistry(root,project,registry,r.error)||
       !PresentationAvailable(root,registry,item.presentationAssetId,r.error))return r;
    item.assetId=GenerateStableId();r.document={project,std::move(item)};
    std::string text;if(!SerializeEquipmentAsset(r.document,text,r.error))return r;
    r.projectRelativePath="Content/Player/Equipment/"+r.document.equipment.assetId+EquipmentAssetExtension;
    std::error_code ec;const auto canonical=fs::weakly_canonical(fs::u8path(root),ec);
    if(ec||!fs::is_directory(canonical)){r.error="Equipment project root is unavailable.";return r;}
    const auto destination=canonical/fs::u8path(r.projectRelativePath);
    const auto admitted=ResolveDependencyPath(canonical.generic_u8string(),r.projectRelativePath);
    if(!admitted.accepted){r.error=admitted.error;return r;}
    fs::create_directories(destination.parent_path(),ec);
    if(ec){r.error="Cannot create equipment directory.";return r;}
    AssetRecord record;record.assetId=r.document.equipment.assetId;
    record.dependencyNodeId="equipment:"+record.assetId;record.projectRelativePath=r.projectRelativePath;
    record.provider="renegade.equipment";record.contentHash=Hash(text);
    if(!r.document.equipment.presentationAssetId.empty())record.dependencyAssetIds={r.document.equipment.presentationAssetId};
    registry.records.push_back(record);
    std::string registryText,registryPath;
    if(!SerializeAssetRegistry(registry,registryText,r.error)||
       !ResolveAssetRegistryDocumentPath(canonical.generic_u8string(),registryPath,r.error))return r;
    ProjectDocumentWrite asset{destination.generic_u8string(),{text.begin(),text.end()},
        [text](const std::string& path,std::string& error) {
            EquipmentAssetDocument d;std::string serialized;
            return ReadEquipmentAssetFile(path,d,error)&&SerializeEquipmentAsset(d,serialized,error)&&serialized==text;
        }};
    ProjectDocumentWrite index{registryPath,{registryText.begin(),registryText.end()},
        [project,id=record.assetId](const std::string& path,std::string& error) {
            std::ifstream in(fs::u8path(path),std::ios::binary);
            std::string text((std::istreambuf_iterator<char>(in)),{});AssetRegistry parsed;
            return DeserializeAssetRegistry(text,parsed,error)&&parsed.projectId==project&&Find(parsed,id);
        }};
    ProjectDocumentTransactionOptions options;options.allowedRoot=canonical.generic_u8string();
    options.journalDirectory=(canonical/"Intermediate/Transactions").generic_u8string();options.operationHook=std::move(hook);
    r.transaction=ProjectDocumentTransaction().Execute({std::move(asset),std::move(index)},options);
    r.succeeded=r.transaction.success;if(!r.succeeded)r.error=r.transaction.message;return r;
}
std::vector<EquipmentAssetDocument> ListEquipmentAssets(const std::string& root,const StableId& project,std::string& error) {
    AssetRegistry registry;std::vector<EquipmentAssetDocument> result;
    if(!ReadAssetRegistry(root,project,registry,error))return result;
    for(const auto& record:registry.records)if(fs::u8path(record.projectRelativePath).extension()==EquipmentAssetExtension) {
        EquipmentAssetDocument d;if(!LoadEquipmentAsset(root,project,record.assetId,d,error))return {};
        result.push_back(std::move(d));
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){
        return a.equipment.name==b.equipment.name?a.equipment.assetId<b.equipment.assetId:a.equipment.name<b.equipment.name;
    });error.clear();return result;
}
bool PrepareEquipmentFromAssembly(const std::string& root,const StableId& project,
    const StableId& presentation,const std::string& name,EquipmentHandUse handUse,
    EquipmentDefinition& out,std::string& error) {
    FirstPersonAssemblySettings settings;
    if(!FirstPersonAssemblyService().ReadSettings(root,project,presentation,settings,error))return false;
    auto prepared=ReusableAssetService().PrepareModelAssetPlacement({root,project,presentation});
    if(!prepared.IsReady()){error=prepared.Result().error;return false;}
    const auto& scene=*prepared.PeekScene();
    EquipmentDefinition item;item.assetId=GenerateStableId();item.name=name;
    item.presentationAssetId=presentation;item.handUse=handUse;
    struct Binding {EquipmentAction action;const char* semantic;};
    constexpr Binding bindings[]={
        {EquipmentAction::Equip,"Equip"},{EquipmentAction::Unequip,"Unequip"},
        {EquipmentAction::PrimaryUse,"Attack"},{EquipmentAction::AlternateUse,"AimIn"},
        {EquipmentAction::Reload,"Reload"}};
    for(const auto& binding:bindings) {
        float duration=0;unsigned count=0;
        for(size_t i=0;i<scene.animations.GetCount();++i) {
            const auto* m=scene.metadatas.GetComponent(scene.animations.GetEntity(i));
            if(!m||!m->string_values.has(CreatorCharacterAnimationActionMetadataKey)||
               m->string_values.get(CreatorCharacterAnimationActionMetadataKey)!=binding.semantic)continue;
            const float clipDuration=scene.animations[i].end-scene.animations[i].start;
            if(!std::isfinite(clipDuration)||clipDuration<0) {
                error="Assembly action has invalid native timing.";return false;
            }
            duration=std::max(duration,clipDuration);++count;
        }
        if(count==2)item.actions.push_back({binding.action,binding.semantic,0,0,duration,0,false,true});
        else if(count!=0){error="Assembly action does not have exactly two presentation tracks.";return false;}
    }
    EquipmentAssetDocument d{project,item};
    if(!Valid(d,error))return false;
    out=std::move(item);error.clear();return true;
}
bool ValidateStartingEquipment(const std::string& root,const StableId& project,const StableId& primary,
    const StableId& offHand,std::string& error) {
    EquipmentAssetDocument p,o;
    if(!primary.empty()&&!LoadEquipmentAsset(root,project,primary,p,error))return false;
    if(!offHand.empty()&&!LoadEquipmentAsset(root,project,offHand,o,error))return false;
    if(!primary.empty()&&p.equipment.handUse==EquipmentHandUse::OffHandOnly) {
        error="Primary slot cannot use an off-hand-only item.";return false;
    }
    if(!offHand.empty()&&o.equipment.handUse!=EquipmentHandUse::OffHandOnly&&
       o.equipment.handUse!=EquipmentHandUse::EitherHand) {
        error="Off-hand slot requires OffHandOnly or EitherHand.";return false;
    }
    if(!primary.empty()&&!offHand.empty()&&(p.equipment.handUse==EquipmentHandUse::TwoHanded||
       p.equipment.handUse==EquipmentHandUse::PrimaryWithSupport)) {
        error="Two-handed/support equipment reserves both hands.";return false;
    }
    error.clear();return true;
}
}
