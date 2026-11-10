#include "renegade/bridge/LightPresetService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "LightBehaviourScript.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace renegade::bridge {
namespace {
namespace fs=std::filesystem;
bool Atomic(const std::string& root,const fs::path& path,const std::string& text,std::string& error) {
 ProjectDocumentWrite w;w.destinationPath=path.generic_u8string();
 w.content.assign(text.begin(),text.end());
 ProjectDocumentTransactionOptions o;o.allowedRoot=root;
 o.journalDirectory=(fs::u8path(root)/"Saved"/"Transactions").generic_u8string();
 auto result=ProjectDocumentTransaction().Execute({std::move(w)},o);
 if(!result.success){error=result.message;return false;}error.clear();return true;
}
float Finite(float v,float fallback,float lo,float hi){return std::clamp(std::isfinite(v)?v:fallback,lo,hi);}
bool Parse(const fs::path& path,const StableId& project,LightPreset& p) {
 std::ifstream in(path);std::string magic,owner;int behaviour,type;unsigned version;
 if(!(in>>magic>>version>>owner>>p.id>>std::quoted(p.name)) ||
    magic!="renegade-light"||version!=1||owner!=project||!IsValidStableId(p.id)||p.name.empty())return false;
 auto& l=p.light;
 if(!(in>>type>>l.color.x>>l.color.y>>l.color.z>>l.intensity>>l.range>>
 l.outerConeDegrees>>l.innerConeDegrees>>l.radius>>l.length>>l.height>>
 l.castShadow>>l.volumetrics>>l.volumetricBoost>>behaviour>>p.strength>>p.speed>>p.movement>>p.randomise))return false;
 if(type<0||type>3||behaviour<0||behaviour>5)return false;
 l.type=static_cast<wi::scene::LightComponent::LightType>(type);
 p.behaviour=static_cast<LightBehaviour>(behaviour);p=SanitizeLightPreset(p);return true;
}
bool IsStock(const ScriptAttachment& a){return a.sourcePath==LightBehaviourSourcePath;}
}
const char* LightBehaviourName(LightBehaviour v) noexcept {
 switch(v){case LightBehaviour::Flame:return "Flame";case LightBehaviour::ElectricalFault:return "Electrical Fault";
 case LightBehaviour::Rotating:return "Rotating";case LightBehaviour::Pulsing:return "Pulsing";
 case LightBehaviour::Strobe:return "Strobe";default:return "Constant";}
}
LightPreset SanitizeLightPreset(LightPreset p) {
 p.light=SanitizeLightState(p.light);
 p.strength=Finite(p.strength,.35f,0,1);p.speed=Finite(p.speed,1.2f,.05f,20);
 p.movement=Finite(p.movement,.03f,0,.5f);
 if(static_cast<unsigned>(p.behaviour)>5)p.behaviour=LightBehaviour::Constant;
 p.name.erase(std::remove_if(p.name.begin(),p.name.end(),[](unsigned char c){return c<32;}),p.name.end());
 const auto first=p.name.find_first_not_of(' '),last=p.name.find_last_not_of(' ');
 p.name=first==std::string::npos?"New Light":p.name.substr(first,last-first+1);
 if(p.name.size()>120)p.name.resize(120);
 return p;
}
std::vector<LightPreset> DefaultLightPresets() {
 std::vector<LightPreset> out;
 LightPreset p;p.light=MakeNewLightState(wi::scene::LightComponent::POINT);
 p.name="Firelight";p.light.color={1,.714f,.361f};p.light.intensity=24;p.light.range=8;
 p.behaviour=LightBehaviour::Flame;out.push_back(p);
 p.name="Warm Lantern";p.behaviour=LightBehaviour::Constant;p.light.intensity=12;p.light.range=6;out.push_back(p);
 p.name="Cold Moonlight";p.light=MakeNewLightState(wi::scene::LightComponent::DIRECTIONAL);
 p.light.color={.56f,.69f,1};p.light.intensity=.5f;out.push_back(p);
 p.name="Faulty Lamp";p.light=MakeNewLightState(wi::scene::LightComponent::POINT);
 p.light.intensity=18;p.behaviour=LightBehaviour::ElectricalFault;p.strength=1;out.push_back(p);
 p.name="Rotating Beacon";p.light=MakeNewLightState(wi::scene::LightComponent::SPOT);
 p.light.color={1,.16f,.08f};p.light.intensity=35;p.light.outerConeDegrees=25;
 p.behaviour=LightBehaviour::Rotating;p.speed=1;out.push_back(p);
 p.name="Soft Fill";p.light=MakeNewLightState(wi::scene::LightComponent::POINT);
 p.light.intensity=4;p.light.range=12;p.behaviour=LightBehaviour::Constant;out.push_back(p);return out;
}
bool SaveLightPreset(const std::string& root,const StableId& project,LightPreset& p,bool asNew,std::string& error) {
 if(!IsValidStableId(project)){error="Open a project before saving a preset.";return false;}
 p=SanitizeLightPreset(p);
 if(asNew||p.id.empty())p.id=GenerateStableId();
 if(!IsValidStableId(p.id)){error="Invalid light preset identity.";return false;}
 const auto path=fs::u8path(root)/"Content"/"Lights"/(p.id+".rlight");
 if(!asNew && !fs::exists(path)){error="Select a saved project preset before updating it.";return false;}
 auto& l=p.light;std::ostringstream out;out<<std::setprecision(9);
 out<<"renegade-light 1\n"<<project<<'\n'<<p.id<<'\n'<<std::quoted(p.name)<<'\n'<<int(l.type)<<' '<<l.color.x<<' '<<l.color.y<<' '<<l.color.z<<' '<<l.intensity<<' '<<l.range<<' '<<l.outerConeDegrees<<' '<<l.innerConeDegrees<<' '<<l.radius<<' '<<l.length<<' '<<l.height<<' '<<l.castShadow<<' '<<l.volumetrics<<' '<<l.volumetricBoost<<'\n'<<int(p.behaviour)<<' '<<p.strength<<' '<<p.speed<<' '<<p.movement<<' '<<p.randomise<<'\n';
 return Atomic(root,path,out.str(),error);
}
std::vector<LightPreset> ListLightPresets(const std::string& root,const StableId& project,std::string& error) {
 std::vector<LightPreset> result;std::error_code ec;
 auto dir=fs::u8path(root)/"Content"/"Lights";if(!fs::exists(dir,ec))return result;
 for(fs::directory_iterator it(dir,ec),end;!ec&&it!=end;it.increment(ec)) {
  if(it->path().extension()!=".rlight")continue;
  LightPreset p;if(Parse(it->path(),project,p))result.push_back(p);
  else error="A light preset could not be read; valid presets remain available.";
 }
 if(ec)error=ec.message();
 std::sort(result.begin(),result.end(),[](const auto&a,const auto&b){return a.name<b.name;});return result;
}
bool SaveLightColours(const std::string& root,const std::vector<XMFLOAT3>& colours,std::string& error) {
 std::ostringstream out;out<<"renegade-light-colours 1\n"<<std::setprecision(9);
 for(size_t i=0;i<std::min<size_t>(24,colours.size());++i)
  out<<Finite(colours[i].x,1,0,1)<<' '<<Finite(colours[i].y,1,0,1)<<' '<<Finite(colours[i].z,1,0,1)<<'\n';
 return Atomic(root,fs::u8path(root)/"Content"/"Lights"/"SavedColours.rcolours",out.str(),error);
}
std::vector<XMFLOAT3> LoadLightColours(const std::string& root) {
 std::vector<XMFLOAT3> result;std::ifstream in(fs::u8path(root)/"Content"/"Lights"/"SavedColours.rcolours");
 std::string magic;int version;if(!(in>>magic>>version)||magic!="renegade-light-colours"||version!=1)return result;
 XMFLOAT3 c;while(result.size()<24 && in>>c.x>>c.y>>c.z)result.push_back({Finite(c.x,1,0,1),Finite(c.y,1,0,1),Finite(c.z,1,0,1)});
 return result;
}
bool EnsureLightBehaviourSource(const std::string& root,std::string& error) {
 auto path=fs::u8path(root)/LightBehaviourSourcePath;
 if(fs::exists(path))return ValidateProjectScriptSource(root,LightBehaviourSourcePath,error);
 return Atomic(root,path,LightBehaviourLua,error);
}
ScriptAttachment MakeLightBehaviourAttachment(const StableId& owner,const LightPreset& p,const ScriptDocument& doc) {
 ScriptSourceBinding source;source.sourceId=GenerateStableId();source.sourcePath=LightBehaviourSourcePath;
 for(const auto& a:doc.attachments)if(IsStock(a)){source=CaptureScriptSourceBinding(a);break;}
 auto a=CreateScriptAttachment(ScriptScope::Entity,owner,source);
 ScriptPropertyValue v;v.name="mode";v.type=ScriptPropertyType::Enum;v.textValue=LightBehaviourName(p.behaviour);a.properties.push_back(v);
 for(auto item:{std::pair<const char*,float>{"strength",p.strength},{"speed",p.speed},{"movement",p.movement}}) {
  v={};v.name=item.first;v.type=ScriptPropertyType::Float;v.numberValue=item.second;a.properties.push_back(v);
 }
 v={};v.name="randomise";v.type=ScriptPropertyType::Boolean;v.booleanValue=p.randomise;a.properties.push_back(v);return a;
}
LightPreset CaptureLightPreset(const wi::scene::Scene& scene,wi::ecs::Entity e,const ScriptDocument* doc) {
 LightPreset p;if(auto* l=scene.lights.GetComponent(e))p.light=CaptureLight(*l);
 if(auto* n=scene.names.GetComponent(e))p.name=n->name;
 if(auto* m=scene.metadatas.GetComponent(e);m&&m->string_values.has("renegade.light.preset_id"))
  p.id=m->string_values.get("renegade.light.preset_id");
 if(doc)for(const auto& a:doc->attachments)if(a.ownerEntityId==PersistentEntityId(scene,e)&&IsStock(a)&&a.enabled) {
  for(const auto& v:a.properties) {
   if(v.name=="mode")for(int i=1;i<=5;++i)if(v.textValue==LightBehaviourName(static_cast<LightBehaviour>(i)))p.behaviour=static_cast<LightBehaviour>(i);
   if(v.name=="strength")p.strength=v.numberValue;if(v.name=="speed")p.speed=v.numberValue;
   if(v.name=="movement")p.movement=v.numberValue;if(v.name=="randomise")p.randomise=v.booleanValue;
  }
 }return SanitizeLightPreset(p);
}
namespace {
class DeleteLightSetupCommand final : public ICommand {
 wi::scene::Scene& scene_;ScriptDocument* doc_;DeleteEntityCommand native_;
 ScriptDocument before_,after_;bool captured_=false;wi::ecs::Entity entity_;
public:
 DeleteLightSetupCommand(wi::scene::Scene& s,ScriptDocument* d,wi::ecs::Entity e):scene_(s),doc_(d),native_(s,e),entity_(e){}
 bool Execute() override {
  if(!captured_&&doc_) {
   before_=*doc_;after_=before_;std::vector<StableId> removed;
   for(size_t i=0;i<scene_.metadatas.GetCount();++i) {
    auto e=scene_.metadatas.GetEntity(i);
    if(e==entity_||scene_.Entity_IsDescendant(e,entity_))removed.push_back(PersistentEntityId(scene_,e));
   }
   auto& a=after_.attachments;
   a.erase(std::remove_if(a.begin(),a.end(),[&](const auto& row){return row.scope==ScriptScope::Entity&&std::find(removed.begin(),removed.end(),row.ownerEntityId)!=removed.end();}),a.end());
   NormalizeScriptAttachmentOrder(after_);captured_=true;
  }
  if(!native_.Execute())return false;if(doc_)*doc_=after_;return true;
 }
 void Undo() override {native_.Undo();if(doc_)*doc_=before_;}
};
}
std::unique_ptr<ICommand> MakeDeleteLightSetupCommand(wi::scene::Scene& s,ScriptDocument* d,wi::ecs::Entity e) {
 return std::make_unique<DeleteLightSetupCommand>(s,d,e);
}
LightSetupCommand::LightSetupCommand(wi::scene::Scene& scene,ScriptDocument* scripts,LightPreset p,
 wi::ecs::Entity target,XMFLOAT3 pos,XMFLOAT4 rot):scene_(&scene),scripts_(scripts),setup_(SanitizeLightPreset(p)),entity_(target) {
 if(target==wi::ecs::INVALID_ENTITY)create_=std::make_unique<CreateLightCommand>(scene,setup_.light.type,pos,rot);
}
bool LightSetupCommand::Execute() {
 if(create_){if(!create_->Execute())return false;entity_=create_->CreatedEntity();}
 if(!scene_->lights.Contains(entity_) || (setup_.behaviour!=LightBehaviour::Constant&&!scripts_)) {
  if(create_)create_->Undo();return false;
 }
 std::string error;
 if(!captured_) {
  hadName_=scene_->names.Contains(entity_);
  before_=CaptureLightPreset(*scene_,entity_,scripts_);ownerId_=PersistentEntityId(*scene_,entity_);
  hadIdentity_=!ownerId_.empty();if(!hadIdentity_)ownerId_=GenerateStableId();
  if(!IsValidStableId(ownerId_)){if(create_)create_->Undo();return false;}
  if(scripts_) {
   scriptsBefore_=*scripts_;scriptsAfter_=scriptsBefore_;
   auto& rows=scriptsAfter_.attachments;
   rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto&a){return a.ownerEntityId==ownerId_&&IsStock(a);}),rows.end());
   if(setup_.behaviour!=LightBehaviour::Constant &&
      !AddScriptAttachment(scriptsAfter_,MakeLightBehaviourAttachment(ownerId_,setup_,scriptsBefore_),error)) {
    if(create_)create_->Undo();return false;
   }
   NormalizeScriptAttachmentOrder(scriptsAfter_);
  }
  if(create_) {
   const std::string base=setup_.name;int suffix=2;bool clash;
   do {clash=false;for(size_t i=0;i<scene_->names.GetCount();++i)
    if(scene_->names.GetEntity(i)!=entity_&&scene_->names[i].name==setup_.name){clash=true;break;}
    if(clash)setup_.name=base+" "+std::to_string(suffix++);
   }while(clash);
  }
  captured_=true;
 }
 if(!AssignPersistentEntityId(*scene_,entity_,ownerId_,error)){if(create_)create_->Undo();return false;}
 auto* metadata=scene_->metadatas.GetComponent(entity_);
 if(setup_.id.empty())metadata->string_values.erase("renegade.light.preset_id");
 else metadata->string_values.set("renegade.light.preset_id",setup_.id);
 ApplyLight(*scene_->lights.GetComponent(entity_),setup_.light);
 auto* name=scene_->names.GetComponent(entity_);if(!name)name=&scene_->names.Create(entity_);name->name=setup_.name;
 if(scripts_)*scripts_=scriptsAfter_;
 return true;
}
void LightSetupCommand::Undo() {
 if(scripts_)*scripts_=scriptsBefore_;
 if(create_)create_->Undo();
 else if(auto* l=scene_->lights.GetComponent(entity_)) {
  ApplyLight(*l,before_.light);
  if(hadName_)scene_->names.GetComponent(entity_)->name=before_.name;else scene_->names.Remove(entity_);
  if(auto* m=scene_->metadatas.GetComponent(entity_)) {
   if(before_.id.empty())m->string_values.erase("renegade.light.preset_id");
   else m->string_values.set("renegade.light.preset_id",before_.id);
  }
  if(!hadIdentity_)if(auto* m=scene_->metadatas.GetComponent(entity_))m->string_values.erase(PersistentEntityIdMetadataKey);
 }
}
}
