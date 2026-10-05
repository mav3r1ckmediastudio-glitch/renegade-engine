#include "renegade/bridge/FirstPersonAssemblyService.h"
#include <iostream>
#include <limits>
using namespace renegade::bridge;
static bool PartPickerRegression() {
 AssetRegistry registry;
 auto add=[&](const std::string& path,bool available=true,const std::string& importer="wicked.fbx") {
  AssetRecord record;record.assetId=GenerateStableId();record.projectRelativePath=path;
  record.dependencyClass=DependencyClass::ImportedContent;record.sourceAvailable=available;
  registry.records.push_back(record);ImportedProductRecord product;
  product.productAssetId=record.assetId;product.importer=importer;
  registry.importedProducts.push_back(product);return record.assetId;
 };
 const auto arms=add("Content/Player/Arms/Pack/Hands.rasset");
 const auto weapon=add("Content/Player/Weapons/Shotgun.rasset");
 const auto sharedArms=add("Content/Packs/Combined/Hands.rasset");
 const auto sharedWeapon=add("Content/Packs/Combined/Gun.rasset");
 for(int i=0;i<100;++i)add("Content/Models/Unrelated"+std::to_string(i)+".rasset");
 add("Content/Player/ArmsBackup/Excluded.rasset");
 add("Content/Player/Arms/Missing.rasset",false);
 add("Content/Player/Weapons/Assembly.rasset",true,"renegade.first_person.assembly");
 ImportedProductRecord recipe;recipe.importer="renegade.first_person.assembly";
 recipe.settingsJson="{\"options\":{\"arms_asset_id\":\""+sharedArms+"\",\"weapon_asset_id\":\""+sharedWeapon+"\"}}";
 registry.importedProducts.push_back(recipe);
 const auto choices=CollectFirstPersonPartChoices(registry);
 if(choices.size()!=4)return false;
 for(const auto& c:choices) {
  if(c.assetId==arms||c.assetId==sharedArms){if(!c.arms||c.weapon)return false;}
  else if(c.assetId==weapon||c.assetId==sharedWeapon){if(!c.weapon||c.arms)return false;}
  else return false;
 }
 registry.importedProducts.back().settingsJson="malformed";
 return CollectFirstPersonPartChoices(registry).size()==2;
}
int main()
{
    if(!PartPickerRegression()){std::cerr<<"Part picker filtering regression";return 20;}
    FirstPersonAssemblySettings settings;
    settings.armsAssetId = GenerateStableId();
    settings.weaponAssetId = GenerateStableId();
    settings.parentBonePath = "[\"root\",\"grip\"]";
    settings.weaponPosition = {-0.03466970f, 0.27336276f, -0.04505738f};
    settings.weaponRotation = {-0.059182247f, 0.087738050f, 0.994176416f, -0.020316230f};
    settings.cameraPosition = {0, -1.65f, 0.08f};
    settings.cameraRotation = {0, 0.707106781f, -0.707106781f, 0};
    settings.pairs = {{"Idle", 0, 1}, {"Reload", 2, 3}};
    std::string json, reopened, error;
    FirstPersonAssemblySettings parsed;
    if (!SerializeFirstPersonAssemblySettings(settings, json, error) ||
        !ParseFirstPersonAssemblySettings(json, parsed, error) ||
        !SerializeFirstPersonAssemblySettings(parsed, reopened, error) || reopened != json)
    { std::cerr << "Assembly attachment or paired indices lost on reopen: " << error; return 1; }
    auto tuned=settings;tuned.firearm={6,1.25f,false};
    if(!SerializeFirstPersonAssemblySettings(tuned,reopened,error) ||
        !ParseFirstPersonAssemblySettings(reopened,parsed,error) || !(parsed.firearm==tuned.firearm))
        return 9;
    // Old recipes lack the firearm member and retain the accepted shotgun default.
    auto legacy=json;const auto field=legacy.find("\"firearm\":");
    if(field==std::string::npos)return 10;
    const auto end=legacy.find('}',field);
    legacy.erase(field,end-field+2);
    if(!ParseFirstPersonAssemblySettings(legacy,parsed,error) || !(parsed.firearm==FirearmSettings{}))return 11;
    for(int i=0;i<4;++i) {
        auto bad=settings;
        if(i==0)bad.firearm.capacity=0;
        if(i==1)bad.firearm.capacity=1001;
        if(i==2)bad.firearm.minimumShotInterval=-1;
        if(i==3)bad.firearm.minimumShotInterval=std::numeric_limits<float>::quiet_NaN();
        if(SerializeFirstPersonAssemblySettings(bad,reopened,error))return 12;
    }
    auto hands=settings;
    hands.offHandWeaponAssetId=GenerateStableId();
    hands.offHandParentBonePath="[\"root\",\"left\"]";
    hands.primaryLayerRootPath="[\"root\",\"right\"]";
    hands.offHandLayerRootPath="[\"root\",\"left\"]";
    hands.blockStartClip=17;hands.blockLoopClip=22;hands.blockEndClip=24;
    std::string handsJson;
    if(!SerializeFirstPersonAssemblySettings(hands,handsJson,error)||
       !ParseFirstPersonAssemblySettings(handsJson,parsed,error)||
       !SerializeFirstPersonAssemblySettings(parsed,reopened,error)||reopened!=handsJson||
       !parsed.IndependentHands())return 21;
    for(int i=0;i<3;++i) {
        auto bad=hands;
        if(i==0)bad.offHandWeaponAssetId=bad.weaponAssetId;
        if(i==1)bad.offHandLayerRootPath="[1]";
        if(i==2)bad.offHandWeaponRotation.w=5;
        if(SerializeFirstPersonAssemblySettings(bad,reopened,error))return 22;
    }
    CommandService handHistory;auto handDraft=settings;
    if(!handHistory.Execute(std::make_unique<SetFirstPersonAssemblySettingsCommand>(handDraft,hands))||
       !handDraft.IndependentHands()||!handHistory.Undo()||handDraft.IndependentHands()||
       !handHistory.Redo()||handDraft.blockLoopClip!=22)return 23;
    CommandService history;
    auto draft=settings;
    auto next=draft;next.firearm=tuned.firearm;next.weaponPosition.x+=0.01f;next.cameraPosition.z+=0.02f;
    next.cameraRotation={0,0,0,1};next.pairs.pop_back();
    if(!history.Execute(std::make_unique<SetFirstPersonAssemblySettingsCommand>(draft,next))||
        !history.IsDirty()||draft.pairs.size()!=1||!history.Undo()||
        !SerializeFirstPersonAssemblySettings(draft,reopened,error)||reopened!=json||
        history.IsDirty()||!history.Redo()||draft.cameraPosition.z!=next.cameraPosition.z)
        return 4;
    history.MarkSaved();
    if(history.Execute(std::make_unique<SetFirstPersonAssemblySettingsCommand>(draft,draft))||
        history.IsDirty()||!history.Undo()||!history.IsDirty()||!history.Redo()||history.IsDirty())
        return 5;
    auto incomplete=draft;incomplete.pairs[0].weaponClip=~0u;
    if(!history.Execute(std::make_unique<SetFirstPersonAssemblySettingsCommand>(draft,incomplete))||
        !history.Undo()||draft.pairs[0].weaponClip!=next.pairs[0].weaponClip)
        return 6;
    auto branch=draft;branch.weaponPosition.y+=0.01f;
    if(!history.Execute(std::make_unique<SetFirstPersonAssemblySettingsCommand>(draft,branch))||
        history.CanRedo())
        return 7;
    auto full=settings;full.pairs.clear();
    for(unsigned i=0;i<FirstPersonAssemblyActions.size();++i)full.pairs.push_back({FirstPersonAssemblyActions[i],i,0});
    if(!SerializeFirstPersonAssemblySettings(full,reopened,error)||
        !ParseFirstPersonAssemblySettings(reopened,parsed,error)||parsed.pairs.size()!=FirstPersonAssemblyActions.size())return 8;
    for (int failure = 0; failure < 7; ++failure)
    {
        auto bad = settings;
        if (failure == 0) bad.parentBonePath = "[1]";
        if (failure == 1) bad.weaponAssetId = bad.armsAssetId;
        if (failure == 2) bad.weaponRotation.w = 5;
        if (failure == 3) bad.cameraPosition.x = std::numeric_limits<float>::quiet_NaN();
        if (failure == 4) bad.pairs.push_back(bad.pairs.front());
        if (failure == 5) bad.pairs[0].action = "GuessFromFilename";
        if (failure == 6) bad.pairs.clear();
        if (SerializeFirstPersonAssemblySettings(bad, reopened, error))
        { std::cerr << "Invalid assembly accepted: " << failure; return 2; }
    }
    if (ParseFirstPersonAssemblySettings("{}", parsed, error) ||
        ParseFirstPersonAssemblySettings("{\"schema_version\":2}", parsed, error))
    { std::cerr << "Unsupported assembly recipe accepted."; return 3; }
    std::cout << "Assembly retained attachment, paired indices and invalid-input checks pass.\n";
    return 0;
}
