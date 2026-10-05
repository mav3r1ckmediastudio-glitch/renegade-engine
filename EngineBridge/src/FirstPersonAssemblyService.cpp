#include "renegade/bridge/FirstPersonAssemblyService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "json.hpp"
#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/PlayerViewAnimationMask.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <set>
#include <cmath>
#include <limits>
namespace renegade::bridge {
static bool PrepareIndependentHandAssembly(const std::string&,const StableId&,const FirstPersonAssemblySettings&,wi::scene::Scene&,wi::scene::Scene&,wi::scene::Scene&,std::string&);
std::vector<FirstPersonPartChoice> CollectFirstPersonPartChoices(const AssetRegistry& registry) {
 std::set<StableId> armsIds,weaponIds;
 for(const auto& product:registry.importedProducts) {
  if(product.importer!="renegade.first_person.assembly")continue;
  try {
   const auto recipe=nlohmann::json::parse(product.settingsJson);
   const auto& options=recipe.at("options");
   const auto arms=options.value("arms_asset_id",std::string{});
   const auto weapon=options.value("weapon_asset_id",std::string{});
   if(IsValidStableId(arms))armsIds.insert(arms);
   if(IsValidStableId(weapon))weaponIds.insert(weapon);
   if(options.contains("hand_layers")) {
    const auto off=options.at("hand_layers").value("off_hand_asset_id",std::string{});
    if(IsValidStableId(off))weaponIds.insert(off);
   }
  } catch(const nlohmann::json::exception&) { /* Invalid provenance cannot classify parts. */ }
 }
 std::vector<FirstPersonPartChoice> result;
 for(const auto& record:registry.records) {
  const auto product=std::find_if(registry.importedProducts.begin(),registry.importedProducts.end(),
   [&](const auto& p){return p.productAssetId==record.assetId;});
  if(!record.sourceAvailable||record.dependencyClass!=DependencyClass::ImportedContent||
     std::filesystem::u8path(record.projectRelativePath).extension()!=".rasset"||
     product==registry.importedProducts.end()||product->importer=="renegade.first_person.assembly")continue;
  const auto under=[&](const char* folder){const std::string prefix=folder;
   return record.projectRelativePath.compare(0,prefix.size(),prefix)==0;};
  const bool arms=under("Content/Player/Arms/")||armsIds.count(record.assetId)!=0;
  const bool weapon=under("Content/Player/Weapons/")||weaponIds.count(record.assetId)!=0;
  if(!arms&&!weapon)continue;
  result.push_back({record.assetId,record.projectRelativePath,arms,weapon});
 }
 std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){
  return a.label!=b.label?a.label<b.label:a.assetId<b.assetId;});
 return result;
}
namespace {
namespace fs = std::filesystem;
std::vector<std::uint8_t> Bytes(const std::string& s) { return {s.begin(),s.end()}; }
bool Read(const fs::path& p,std::vector<std::uint8_t>& b) {
 std::ifstream f(p,std::ios::binary); if(!f)return false;
 b.assign(std::istreambuf_iterator<char>(f),{}); return !f.bad();
}
std::string Hash(const std::vector<std::uint8_t>& b) {
 std::uint64_t h=1469598103934665603ull; for(auto v:b){h^=v;h*=1099511628211ull;}
 std::ostringstream s;s<<"fnv1a64:"<<std::hex<<std::setw(16)<<std::setfill('0')<<h;return s.str();
}
bool Within(const fs::path& p,const fs::path& r) {
 auto v=p.begin();for(auto a=r.begin();a!=r.end();++a,++v)if(v==p.end()||*a!=*v)return false;return true;
}
ProjectDocumentWrite Write(const fs::path& p,const std::vector<std::uint8_t>& b) {
 ProjectDocumentWrite w;w.destinationPath=p.generic_u8string();w.content=b;
 w.validator=[b](const std::string& p,std::string& e){std::vector<std::uint8_t> a;
 if(!Read(fs::u8path(p),a)||a!=b){e="Assembly staged bytes changed.";return false;}return true;};return w;
}
bool Valid(const FirstPersonAssemblySettings& s,std::string& e) {
 if(!ValidateFirearmSettings(s.firearm,e))return false;
 if(!IsValidStableId(s.armsAssetId)||!IsValidStableId(s.weaponAssetId)||s.parentBonePath.empty()||
 s.armsAssetId==s.weaponAssetId||s.pairs.empty()||s.pairs.size()>32) {
 e="Select distinct arms and weapon products, an explicit parent bone and at least one clip pair.";return false;}
 for(auto p:{s.weaponPosition,s.cameraPosition,s.offHandWeaponPosition})
 if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||
 std::abs(p.x)>10||std::abs(p.y)>10||std::abs(p.z)>10){e="Assembly positions must be finite and within +/-10 metres.";return false;}
 for(auto q:{s.weaponRotation,s.cameraRotation,s.offHandWeaponRotation}) {
 float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
 if(!std::isfinite(n)||std::abs(n-1)>0.002f){e="Assembly rotations must be unit quaternions.";return false;}
 }
 const auto path=nlohmann::json::parse(s.parentBonePath,nullptr,false);
 if(!path.is_array()||path.empty()||path.dump()!=s.parentBonePath||
 std::any_of(path.begin(),path.end(),[](const auto& n){return !n.is_string()||n.template get<std::string>().empty();})) {
 e="Assembly parent bone path is malformed.";return false;
 }
 if(s.IndependentHands()) {
  if(!IsValidStableId(s.offHandWeaponAssetId) || s.offHandWeaponAssetId==s.armsAssetId ||
     s.offHandWeaponAssetId==s.weaponAssetId) {e="Independent hands require three distinct governed products.";return false;}
  for(const auto& value:{s.offHandParentBonePath,s.primaryLayerRootPath,s.offHandLayerRootPath}) {
   const auto p=nlohmann::json::parse(value,nullptr,false);
   if(!p.is_array()||p.empty()||p.dump()!=value||
      std::any_of(p.begin(),p.end(),[](const auto& n){return !n.is_string()||n.template get<std::string>().empty();})) {
    e="Independent hand bone paths must be canonical explicit hierarchy paths.";return false;
   }
  }
 }
 if(s.IndependentHands()) {
  std::set<unsigned> bindings={s.blockStartClip,s.blockLoopClip,s.blockEndClip};
  if(bindings.size()!=3){e="Shield actions need distinct clips.";return false;}
  for(const auto& p:s.pairs)if(!bindings.insert(p.armsClip).second){e="Hand actions need distinct source clips.";return false;}
 }
 std::set<std::string> actions;
 for(const auto& p:s.pairs) {
 if(std::none_of(FirstPersonAssemblyActions.begin(),FirstPersonAssemblyActions.end(),
 [&](const char* action){return p.action==action;})) {
 e="Unknown assembly action.";return false;}
 if(!actions.insert(p.action).second && !(s.IndependentHands() && p.action=="Attack")){e="Each assembly action needs one explicit pair except independent Attack variants.";return false;}
 }
 e.clear();return true;
}
void Set(wi::scene::TransformComponent& t,const XMFLOAT3& p,const XMFLOAT4& q) {
 t.ClearTransform();t.translation_local=p;t.rotation_local=q;t.SetDirty();t.UpdateTransform();
}
std::vector<wi::ecs::Entity> Roots(const wi::scene::Scene& s) {
 std::vector<wi::ecs::Entity> r;
 for(size_t i=0;i<s.transforms.GetCount();++i) {
 auto e=s.transforms.GetEntity(i);if(!s.hierarchy.Contains(e))r.push_back(e);}
 return r;
}
}
#include "FirstPersonHandAssemblyPreparation.h"
bool SerializeFirstPersonAssemblySettings(const FirstPersonAssemblySettings& s,std::string& out,std::string& e) {
 if(!Valid(s,e))return false;
 nlohmann::json j={{"schema_version",1},{"arms_asset_id",s.armsAssetId},{"weapon_asset_id",s.weaponAssetId},
 {"parent_bone_path",s.parentBonePath},{"weapon_position",{s.weaponPosition.x,s.weaponPosition.y,s.weaponPosition.z}},
 {"weapon_rotation",{s.weaponRotation.x,s.weaponRotation.y,s.weaponRotation.z,s.weaponRotation.w}},
 {"camera_position",{s.cameraPosition.x,s.cameraPosition.y,s.cameraPosition.z}},
 {"camera_rotation",{s.cameraRotation.x,s.cameraRotation.y,s.cameraRotation.z,s.cameraRotation.w}},
 {"pairs",nlohmann::json::array()}};
 for(const auto& p:s.pairs)j["pairs"].push_back({{"action",p.action},{"arms_clip",p.armsClip},{"weapon_clip",p.weaponClip}});
 j["firearm"]={{"schema_version",1},{"capacity",s.firearm.capacity},
 {"minimum_shot_interval",s.firearm.minimumShotInterval},{"allow_partial_reload",s.firearm.allowPartialReload}};
 if(s.IndependentHands()) {
  j["schema_version"]=2;
  j["hand_layers"]={{"off_hand_asset_id",s.offHandWeaponAssetId},{"off_hand_parent",s.offHandParentBonePath},
   {"primary_root",s.primaryLayerRootPath},{"off_hand_root",s.offHandLayerRootPath},
   {"off_hand_position",{s.offHandWeaponPosition.x,s.offHandWeaponPosition.y,s.offHandWeaponPosition.z}},
   {"off_hand_rotation",{s.offHandWeaponRotation.x,s.offHandWeaponRotation.y,s.offHandWeaponRotation.z,s.offHandWeaponRotation.w}},
   {"block_start",s.blockStartClip},{"block_loop",s.blockLoopClip},{"block_end",s.blockEndClip}};
 }
 out=j.dump();return true;
}
bool ParseFirstPersonAssemblySettings(const std::string& text,FirstPersonAssemblySettings& s,std::string& e) {
 s={};try {
 auto j=nlohmann::json::parse(text);
 const bool independent=j.is_object() && j.value("schema_version",0)==2;
 if(!j.is_object() || (independent ? (j.size()!=11 || !j.contains("hand_layers") || !j.contains("firearm")) :
   ((j.size()!=9 && !(j.size()==10 && j.contains("firearm"))) || j.at("schema_version")!=1)))throw std::runtime_error("schema");
 if(j.contains("firearm")) {
 const auto& f=j.at("firearm");
 if(!f.is_object()||f.size()!=4||f.at("schema_version")!=1||
 !f.at("capacity").is_number_integer()||!f.at("minimum_shot_interval").is_number()||
 !f.at("allow_partial_reload").is_boolean())throw std::runtime_error("firearm");
 const auto capacity=f.at("capacity").get<long long>();
 if(capacity<1||capacity>1000)throw std::runtime_error("capacity");
 s.firearm.capacity=int(capacity);
 s.firearm.minimumShotInterval=f.at("minimum_shot_interval").get<float>();
 s.firearm.allowPartialReload=f.at("allow_partial_reload").get<bool>();
 }
 s.armsAssetId=j.at("arms_asset_id").get<std::string>();s.weaponAssetId=j.at("weapon_asset_id").get<std::string>();
 s.parentBonePath=j.at("parent_bone_path").get<std::string>();
 auto position=[&](const char* k){const auto& a=j.at(k);if(!a.is_array()||a.size()!=3)throw std::runtime_error("position");
 return XMFLOAT3(a[0].get<float>(),a[1].get<float>(),a[2].get<float>());};
 auto rotation=[&](const char* k){const auto& a=j.at(k);if(!a.is_array()||a.size()!=4)throw std::runtime_error("rotation");
 return XMFLOAT4(a[0].get<float>(),a[1].get<float>(),a[2].get<float>(),a[3].get<float>());};
 s.weaponPosition=position("weapon_position");s.cameraPosition=position("camera_position");
 s.weaponRotation=rotation("weapon_rotation");s.cameraRotation=rotation("camera_rotation");
 if(independent) {
  const auto& h=j.at("hand_layers");if(!h.is_object()||h.size()!=9)throw std::runtime_error("hand layers");
  s.offHandWeaponAssetId=h.at("off_hand_asset_id").get<std::string>();
  s.offHandParentBonePath=h.at("off_hand_parent").get<std::string>();
  s.primaryLayerRootPath=h.at("primary_root").get<std::string>();s.offHandLayerRootPath=h.at("off_hand_root").get<std::string>();
  const auto& p=h.at("off_hand_position");const auto& q=h.at("off_hand_rotation");
  if(!p.is_array()||p.size()!=3||!q.is_array()||q.size()!=4)throw std::runtime_error("hand transform");
  s.offHandWeaponPosition={p[0].get<float>(),p[1].get<float>(),p[2].get<float>()};
  s.offHandWeaponRotation={q[0].get<float>(),q[1].get<float>(),q[2].get<float>(),q[3].get<float>()};
  s.blockStartClip=h.at("block_start").get<unsigned>();s.blockLoopClip=h.at("block_loop").get<unsigned>();s.blockEndClip=h.at("block_end").get<unsigned>();
 }
 for(const auto& p:j.at("pairs"))s.pairs.push_back({p.at("action").get<std::string>(),p.at("arms_clip").get<unsigned>(),p.at("weapon_clip").get<unsigned>()});
 }catch(const std::exception&){e="Malformed first-person assembly recipe.";return false;}return Valid(s,e);
}
bool FirstPersonAssemblyService::Prepare(const std::string& root,const StableId& project,
 const FirstPersonAssemblySettings& s,wi::scene::Scene& result,std::string& e) const {
 if(!Valid(s,e))return false;
 auto arms=ReusableAssetService().PrepareModelAssetPlacement({root,project,s.armsAssetId});
 auto weapon=ReusableAssetService().PrepareModelAssetPlacement({root,project,s.weaponAssetId});
 if(!arms.IsReady()||!weapon.IsReady()){e=!arms.IsReady()?arms.Result().error:weapon.Result().error;return false;}
 auto& a=*arms.PeekMutableScene();auto& w=*weapon.PeekMutableScene();
 std::vector<PlayerViewBoneChoice> bones;if(!CollectPlayerViewBones(a,bones,e))return false;
 auto b=std::find_if(bones.begin(),bones.end(),[&](const auto& b){return b.path==s.parentBonePath;});
 if(b==bones.end()){e="Selected parent bone does not exist in the retained arms.";return false;}
 auto roots=Roots(w);if(roots.size()!=1){e="Weapon product must have one native transform root.";return false;}
 if(s.IndependentHands())return PrepareIndependentHandAssembly(root,project,s,a,w,result,e);
 // Copy selected pairs before merging; each track retains its own skeleton targets.
 auto tracks=[&](wi::scene::Scene& native,bool isArms,wi::scene::Scene& selected)->bool {
 wi::Archive copy;native.Serialize(copy);copy.SetReadModeAndResetPos(true);selected.Serialize(copy);
 // In-memory archives have no retained source directory. Preserve the already
 // resolved native resources on this private copy before composing.
 for(size_t i=0;i<native.materials.GetCount();++i) {
  for(size_t slot=0;slot<wi::scene::MaterialComponent::TEXTURESLOT_COUNT;++slot)
   selected.materials[i].textures[slot]=native.materials[i].textures[slot];
  selected.materials[i].SetDirty();
 }
 for(size_t i=0;i<selected.animations.GetCount();++i) {
 auto& clip=selected.animations[i];clip.Pause();clip.RootMotionOff();clip.amount=0;
 selected.metadatas.Create(selected.animations.GetEntity(i)).string_values.set(CreatorCharacterAnimationActionMetadataKey,"Unassigned");
 }
 for(size_t i=0;i<selected.metadatas.GetCount();++i) {
 selected.metadatas[i].bool_values.erase(CharacterAssetTemplateMetadataKey);
 selected.metadatas[i].int_values.erase(CharacterAssetTemplateVersionMetadataKey);
 }
 std::vector<wi::scene::AnimationComponent> clips;
 for(const auto& p:s.pairs){
 auto index=isArms?p.armsClip:p.weaponClip;
 if(index>=selected.animations.GetCount()){e="Paired clip index no longer exists in its product.";return false;}
 clips.push_back(selected.animations[index]);
 }
 selected.animations.Clear();
 for(size_t i=0;i<s.pairs.size();++i){
 auto entity=wi::ecs::CreateEntity();selected.animations.Create(entity)=clips[i];
 selected.names.Create(entity).name=std::string(isArms?"Arms / ":"Weapon / ")+s.pairs[i].action;
 auto& m=selected.metadatas.Create(entity);
 m.string_values.set(CreatorCharacterAnimationActionMetadataKey,s.pairs[i].action);
 m.string_values.set("renegade.first_person.assembly.track",isArms?"arms":"weapon");
 }
 return true;
 };
 wi::scene::Scene ac,wc;
 if(!tracks(a,true,ac)||!tracks(w,false,wc))return false;
 // Clone archives remap entity IDs: resolve the explicit hierarchy path again.
 if(!CollectPlayerViewBones(ac,bones,e))return false;
 b=std::find_if(bones.begin(),bones.end(),[&](const auto& b){return b.path==s.parentBonePath;});
 if(b==bones.end()){e="Parent bone changed while preparing native copy.";return false;}
 const auto anchor=b->entity;roots=Roots(wc);const auto weaponRoot=roots.front();
 auto armRoots=Roots(ac);ac.Merge(wc);
 ac.Component_Attach(weaponRoot,anchor,true);Set(*ac.transforms.GetComponent(weaponRoot),s.weaponPosition,s.weaponRotation);
 const auto viewRoot=ac.Entity_CreateTransform(CreatorAuthoredTransformRootName);
 Set(*ac.transforms.GetComponent(viewRoot),s.cameraPosition,s.cameraRotation);
 for(auto r:armRoots)ac.Component_Attach(r,viewRoot,true);
 ac.metadatas.Create(viewRoot).bool_values.set("renegade.first_person.assembly",true);
 ApplyFirearmSettings(*ac.metadatas.GetComponent(viewRoot),s.firearm);
 ac.Update(0);
 if(!Pose(ac,s.pairs.front().action,0,e))return false;
 result.Clear();result.Merge(ac);e.clear();return true;
}
ModelDerivedMetadata FirstPersonAssemblyService::Describe(const wi::scene::Scene& s) const {
 auto summary=ImportService::Summarize(s);auto evidence=ImportService::SummarizeModelEvidence(s);
 ModelDerivedMetadata m;m.known=true;m.skinned=evidence.skinnedMeshes!=0;m.animated=summary.animations!=0;
 m.meshCount=static_cast<unsigned>(summary.meshes);m.materialCount=static_cast<unsigned>(summary.materials);
 m.animationClipCount=static_cast<unsigned>(summary.animations);m.animationChannelCount=static_cast<unsigned>(evidence.animationChannels);
 m.armatureCount=static_cast<unsigned>(s.armatures.GetCount());m.boneCount=static_cast<unsigned>(evidence.armatureBones);return m;
}
bool FirstPersonAssemblyService::Pose(wi::scene::Scene& s,const std::string& action,float time,std::string& e) const {
 if(!std::isfinite(time)||time<0){e="Preview time must be finite and nonnegative.";return false;}
 bool handVariants=false;
 for(size_t i=0;i<s.metadatas.GetCount();++i)
  handVariants=handVariants || (s.metadatas[i].bool_values.has("renegade.first_person.independent_hands") && s.metadatas[i].bool_values.get("renegade.first_person.independent_hands"));
 unsigned selected=0;
 for(size_t i=0;i<s.animations.GetCount();++i) {
 auto& c=s.animations[i];auto* m=s.metadatas.GetComponent(s.animations.GetEntity(i));
 bool use=m&&m->string_values.has(CreatorCharacterAnimationActionMetadataKey)&&m->string_values.get(CreatorCharacterAnimationActionMetadataKey)==action;
 if(handVariants && action=="Attack" && selected!=0)use=false;
 c.Pause();c.RootMotionOff();c.amount=use?1.0f:0.0f;c.timer=std::clamp(c.start+time,c.start,c.end);
 c.last_update_time=-std::numeric_limits<float>::max();selected+=use;
 }
 bool independent=false;
 for(size_t i=0;i<s.metadatas.GetCount();++i)
  independent=independent || (s.metadatas[i].bool_values.has("renegade.first_person.independent_hands") &&
    s.metadatas[i].bool_values.get("renegade.first_person.independent_hands"));
 if(selected!=(independent?1u:2u)){e="Action has invalid native track coverage.";return false;}
 s.Update(1.0f/60);e.clear();return true;
}
SetFirstPersonAssemblySettingsCommand::SetFirstPersonAssemblySettingsCommand(
 FirstPersonAssemblySettings& target,FirstPersonAssemblySettings next)
 :target_(target),before_(target),after_(std::move(next)) {}
bool SetFirstPersonAssemblySettingsCommand::Execute() {
 auto equal3=[](const XMFLOAT3& a,const XMFLOAT3& b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
 auto equal4=[](const XMFLOAT4& a,const XMFLOAT4& b){return a.x==b.x&&a.y==b.y&&a.z==b.z&&a.w==b.w;};
 std::string beforeJson,afterJson,error;
 const bool semanticSame=SerializeFirstPersonAssemblySettings(before_,beforeJson,error)&&SerializeFirstPersonAssemblySettings(after_,afterJson,error)&&beforeJson==afterJson;
 bool same=semanticSame&&before_.firearm==after_.firearm&&before_.armsAssetId==after_.armsAssetId&&before_.weaponAssetId==after_.weaponAssetId&&
 before_.parentBonePath==after_.parentBonePath&&equal3(before_.weaponPosition,after_.weaponPosition)&&
 equal3(before_.cameraPosition,after_.cameraPosition)&&equal4(before_.weaponRotation,after_.weaponRotation)&&
 equal4(before_.cameraRotation,after_.cameraRotation)&&before_.pairs.size()==after_.pairs.size();
 for(size_t i=0;same&&i<before_.pairs.size();++i)
 same=before_.pairs[i].action==after_.pairs[i].action&&before_.pairs[i].armsClip==after_.pairs[i].armsClip&&
 before_.pairs[i].weaponClip==after_.pairs[i].weaponClip;
 if(same)return false;
 target_=after_;return true;
}
void SetFirstPersonAssemblySettingsCommand::Undo(){target_=before_;}
bool FirstPersonAssemblyService::ReadSettings(const std::string& root,const StableId& project,const StableId& id,
 FirstPersonAssemblySettings& settings,std::string& e) const {
 auto placement=ReusableAssetService().PrepareModelAssetPlacement({root,project,id});
 if(!placement.IsReady()){e=placement.Result().error;return false;}
 ReusableModelAssetDocument d;
 if(!ReadReusableModelAssetDocument((fs::u8path(root)/fs::u8path(placement.Result().assetProjectRelativePath)).generic_u8string(),d,e))return false;
 if(d.manifest.sourceFormat!="assembly"){e="Selected product is not a first-person assembly.";return false;}
 auto j=nlohmann::json::parse(d.manifest.settingsJson);
 return ParseFirstPersonAssemblySettings(j.at("options").dump(),settings,e);
}
bool FirstPersonAssemblyService::Save(const std::string& rootPath,const StableId& project,const std::string& name,
 const FirstPersonAssemblySettings& s,const std::vector<std::uint8_t>& thumbnail,StableId& asset,std::string& e) const {
 return SaveImpl(rootPath,project,name,s,thumbnail,asset,e,{}, {}, {});
}
bool FirstPersonAssemblyService::Update(const std::string& rootPath,const StableId& project,
 const StableId& existing,const std::string& expectedHash,const FirstPersonAssemblySettings& s,
 const std::vector<std::uint8_t>& thumbnail,std::string& e,ProjectDocumentTransactionHook hook) const {
 if(!IsValidStableId(existing)||expectedHash.empty()){e="Existing assembly identity and original hash are required.";return false;}
 StableId saved;
 return SaveImpl(rootPath,project,{},s,thumbnail,saved,e,existing,expectedHash,std::move(hook));
}
bool FirstPersonAssemblyService::SaveImpl(const std::string& rootPath,const StableId& project,const std::string& name,
 const FirstPersonAssemblySettings& s,const std::vector<std::uint8_t>& thumbnail,StableId& asset,std::string& e,
 const StableId& existing,const std::string& expectedHash,ProjectDocumentTransactionHook hook) const {
 const bool updating=!existing.empty();
 asset.clear();std::string settings;if(!SerializeFirstPersonAssemblySettings(s,settings,e))return false;
 if(!updating&&(name.empty()||name.size()>80||name=="."||name==".."||name.back()=='.'||name.back()==' '||
 name.find_first_of("<>:\"/\\|?*")!=std::string::npos||
 std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32;}))){e="Choose a valid new assembly name.";return false;}
 std::error_code ec;auto root=fs::weakly_canonical(fs::u8path(rootPath),ec);
 if(ec||!fs::is_directory(root)||!IsValidStableId(project)){e="Assembly project is unavailable.";return false;}
 wi::scene::Scene scene;if(!Prepare(rootPath,project,s,scene,e))return false;
 AssetRegistry registry;if(!ReadAssetRegistry(rootPath,project,registry,e))return false;
 StableId sourceId=GenerateStableId(),id=updating?existing:GenerateStableId();
 std::string relative="Content/Assemblies/"+name+".rasset";
 std::string sourceRelative="SourceAssets/Assemblies/"+name+".json";
 if(updating) {
 auto record=std::find_if(registry.records.begin(),registry.records.end(),[&](const auto& r){return r.assetId==existing;});
 auto product=std::find_if(registry.importedProducts.begin(),registry.importedProducts.end(),[&](const auto& p){return p.productAssetId==existing;});
 if(record==registry.records.end()||product==registry.importedProducts.end()||
 product->importer!="renegade.first_person.assembly"||record->contentHash!=expectedHash) {
 e="Assembly changed or is no longer available. Reopen it before saving changes.";return false;}
 relative=record->projectRelativePath;sourceId=product->sourceAssetId;
 auto sourceRecord=std::find_if(registry.records.begin(),registry.records.end(),[&](const auto& r){return r.assetId==sourceId;});
 if(sourceRecord==registry.records.end()){e="Assembly recipe identity is unavailable.";return false;}
 sourceRelative=sourceRecord->projectRelativePath;
 std::vector<std::uint8_t> original;
 if(!Read(root/fs::u8path(relative),original)||Hash(original)!=expectedHash) {
 e="Assembly product bytes changed. Reopen it before saving changes.";return false;}
 ReusableModelAssetDocument originalDocument;
 if(!ReadReusableModelAssetDocument((root/fs::u8path(relative)).generic_u8string(),originalDocument,e)||
 originalDocument.manifest.projectId!=project||originalDocument.manifest.assetId!=id||
 originalDocument.manifest.sourceAssetId!=sourceId||originalDocument.manifest.sourceFormat!="assembly") {
 e="Existing assembly identity is inconsistent.";return false;}
 }
 auto path=root/fs::u8path(relative),source=root/fs::u8path(sourceRelative);
 auto projection=root/fs::u8path(ResolveReusableModelManagedProjectionPath(relative));
 for(auto p:{path,source,projection}){
 if(!updating&&(fs::exists(p)||std::any_of(registry.records.begin(),registry.records.end(),[&](const auto& r){return r.projectRelativePath==p.lexically_relative(root).generic_u8string();})||
 std::any_of(registry.missingAssets.begin(),registry.missingAssets.end(),[&](const auto& r){return r.lastKnownPath==p.lexically_relative(root).generic_u8string();}))){
 e="Assembly destination exists or has a recovery identity; choose a new name.";return false;}
 fs::create_directories(p.parent_path(),ec);
 if(ec||!Within(fs::weakly_canonical(p.parent_path(),ec),root)){e="Assembly destination escapes the project.";return false;}
 }
 std::string recipe=nlohmann::json({{"source_format","assembly"},{"options",nlohmann::json::parse(settings)}}).dump();
 ReusableModelAssetDocument d;d.manifest.projectId=project;d.manifest.assetId=id;d.manifest.sourceAssetId=sourceId;
 d.manifest.sourceFormat="assembly";d.manifest.importer="renegade.first_person.assembly";d.manifest.settingsJson=recipe;
 // Resource bytes travel with the composed product, independent of both source bundles.
 struct Restore {wi::resourcemanager::Mode mode=wi::resourcemanager::GetMode();~Restore(){wi::resourcemanager::SetMode(mode);}} restore;
 wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
 const auto staged=root/"Intermediate/Imports"/fs::u8path(".assembly-"+id+".wiscene");
 fs::create_directories(staged.parent_path(),ec);
 struct Cleanup {fs::path path;~Cleanup(){std::error_code ec;fs::remove(path,ec);}} cleanup{staged};
 wi::Archive archive(staged.generic_u8string(),false,false);
 if(!archive.IsOpen()){e="Could not stage the native assembly payload.";return false;}
 archive.SetCompressionEnabled(true);
 scene.Serialize(archive);
 bool saved=archive.SaveFile(staged.generic_u8string());archive=wi::Archive();
 if(!saved||!Read(staged,d.payload)){e="Could not retain native assembly payload.";return false;}
 d.manifest.payloadHash=Hash(d.payload);
 std::vector<std::uint8_t> productBytes;if(!SerializeReusableModelAssetDocument(d,productBytes,e))return false;
 ReusableModelManagedProjection p;p.projectId=project;p.assetId=id;p.sourceAssetId=sourceId;
 p.sourceProjectRelativePath=sourceRelative;p.assetProjectRelativePath=relative;p.sourceFormat="assembly";
 p.importer=d.manifest.importer;p.settingsJson=recipe;p.payloadHash=d.manifest.payloadHash;p.modelMetadata=Describe(scene);
 if(updating&&thumbnail.empty()&&fs::is_regular_file(root/fs::u8path(ResolveReusableModelThumbnailPath(relative))))
 p.thumbnailProjectRelativePath=ResolveReusableModelThumbnailPath(relative);
 if(!thumbnail.empty()) {
 const auto image=wi::resourcemanager::Load("assembly-thumbnail-"+id+"-"+Hash(thumbnail)+".png",wi::resourcemanager::Flags::NONE,thumbnail.data(),thumbnail.size());
 if(thumbnail.size()>4*1024*1024||!image.IsValid()||!image.GetTexture().IsValid()||
 image.GetTexture().GetDesc().width!=512||image.GetTexture().GetDesc().height!=320){e="Assembly thumbnail must be a bounded 512x320 PNG.";return false;}
 p.thumbnailProjectRelativePath=ResolveReusableModelThumbnailPath(relative);
 }
 std::string projectionJson;if(!SerializeReusableModelManagedProjection(p,projectionJson,e))return false;
 auto add=[&](const StableId& id,const std::string& path,const std::string& hash,bool source){
 AssetRecord r;r.assetId=id;r.dependencyNodeId=std::string(source?"lp07.source:":"lp07.rasset:")+id;
 r.projectRelativePath=path;r.dependencyClass=DependencyClass::ImportedContent;
 r.requirement=source?DependencyRequirement::EditorOnly:DependencyRequirement::Required;
 r.provider=source?"lp07.source_asset":"lp07.rasset";r.contentHash=hash;
 if(updating) {
 auto current=std::find_if(registry.records.begin(),registry.records.end(),[&](const auto& item){return item.assetId==id;});
 current->contentHash=hash;
 } else registry.records.push_back(r);};
 add(sourceId,sourceRelative,Hash(Bytes(recipe)),true);add(id,relative,Hash(productBytes),false);
 ImportedProductRecord provenance;provenance.sourceAssetId=sourceId;provenance.productAssetId=id;
 provenance.importer=d.manifest.importer;provenance.settingsSchema=d.manifest.settingsSchema;provenance.settingsJson=recipe;
 provenance.sourceContentHashAtImport=Hash(Bytes(recipe));provenance.productContentHashAtImport=Hash(productBytes);
 auto products=registry.importedProducts;
 if(updating) {
 auto current=std::find_if(products.begin(),products.end(),[&](const auto& p){return p.productAssetId==id;});
 *current=provenance;
 } else products.push_back(provenance);
 // Preserve unrelated stale/missing provenance; only this assembly is rebuilt.
 registry.importedProducts=std::move(products);
 registry.schemaVersion=AssetRegistry::CurrentSchemaVersion;
 if(!ValidateAssetRegistry(registry,e))return false;
 AssetCatalogueMetadataDocument catalogue;
 if(!ReadAssetCatalogueMetadata(rootPath,project,catalogue,e)||!SetAssetModelDerivedMetadata(catalogue,id,p.modelMetadata,e))return false;
 std::string registryJson,catalogueJson;
 if(!SerializeAssetRegistry(registry,registryJson,e)||!SerializeAssetCatalogueMetadata(catalogue,catalogueJson,e))return false;
 ProjectDocumentTransactionOptions options;options.allowedRoot=root.generic_u8string();
 options.journalDirectory=(root/"Intermediate/Transactions").generic_u8string();
 options.operationHook=std::move(hook);
 std::vector<ProjectDocumentWrite> writes={Write(source,Bytes(recipe)),Write(path,productBytes),
 Write(projection,Bytes(projectionJson)),Write(root/AssetRegistryDocumentName,Bytes(registryJson)),
 Write(root/AssetCatalogueMetadataDocumentName,Bytes(catalogueJson))};
 if(!thumbnail.empty()) {
 auto thumb=root/fs::u8path(p.thumbnailProjectRelativePath);
 if(!updating&&fs::exists(thumb)){e="Assembly thumbnail destination already exists.";return false;}
 writes.push_back(Write(thumb,thumbnail));
 }
 auto transaction=ProjectDocumentTransaction().Execute(std::move(writes),options);
 if(!transaction.success){e=transaction.message;return false;}
 auto reopened=ReusableAssetService().PrepareModelAssetPlacement({rootPath,project,id});
 if(!reopened.IsReady()){e="Saved assembly failed governed reopen: "+reopened.Result().error;return false;}
 FirstPersonAssemblySettings verified;if(!ReadSettings(rootPath,project,id,verified,e))return false;
 std::string actual;if(!SerializeFirstPersonAssemblySettings(verified,actual,e)||actual!=settings){e="Saved assembly settings changed.";return false;}
 asset=id;e.clear();return true;
}
}
