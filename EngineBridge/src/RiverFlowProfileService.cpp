#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/RiverCourseService.h"
#include "renegade/bridge/RiverWaterwayService.h"
#include "json.hpp"
#include "renegade/bridge/RiverJunctionSurfaceService.h"
#include <cmath>
namespace renegade::bridge {
namespace {
bool Water(const wi::scene::Scene& s,wi::ecs::Entity e){const auto* m=s.materials.GetComponent(e);return s.splines.Contains(e)&&m&&m->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER;}
bool Range(float v,float a,float b){return std::isfinite(v)&&v>=a&&v<=b;}
std::string Encode(const RiverFlowProfile& p){
 return nlohmann::json{{"version",5},{"enabled",p.enabled},{"foam",p.foam},{"reversed",p.reversed},
 {"rapids",p.rapids},{"rapidIntensity",p.rapidIntensity},
 {"startWhitewater",p.startWhitewater},{"startWhitewaterAmount",p.startWhitewaterAmount},
 {"startWhitewaterNodeIndex",p.startWhitewaterNodeIndex},
 {"startWhitewaterNodeId",p.startWhitewaterNodeId},
 {"speed",p.speed},{"intensity",p.intensity},{"lifetime",p.lifetime},{"turbulence",p.turbulence},{"sourceWidth",p.sourceWidth},{"threshold",p.threshold}}.dump();
}
}
bool ValidateRiverFlowProfile(const RiverFlowProfile& p,std::string& error){
 error.clear();
 if(p.startWhitewaterNodeId.size()>64 ||
    (!p.startWhitewaterNodeId.empty()&&!IsValidStableId(p.startWhitewaterNodeId))||
    p.startWhitewaterNodeIndex < -1 || p.startWhitewaterNodeIndex >= 512 ||
    !Range(p.startWhitewaterAmount,0,2)||!Range(p.rapidIntensity,0,2)||!Range(p.speed,0,3)||!Range(p.intensity,0,2)||!Range(p.lifetime,.5f,6)||!Range(p.turbulence,0,1.5f)||!Range(p.sourceWidth,.01f,.08f)||!Range(p.threshold,-.15f,.15f)){
 error="River flow settings are nonfinite or outside supported bounds";return false;}return true;
}
bool ReadRiverFlowProfile(const wi::scene::Scene& s,wi::ecs::Entity e,RiverFlowProfile& p,std::string& error){
 p={};error.clear();if(!Water(s,e)){error="Select a native WATER spline";return false;}
 const auto* m=s.metadatas.GetComponent(e);if(!m||!m->string_values.has(RiverFlowProfileMetadataKey))return true;
 const auto& text=m->string_values.get(RiverFlowProfileMetadataKey);if(text.size()>4096){error="River flow metadata exceeds limit";return false;}
 try{auto j=nlohmann::json::parse(text);
 const int version=j.at("version").get<int>();
 if(version!=1&&version!=2&&version!=3&&version!=4&&version!=5)throw std::runtime_error("Unsupported river flow schema or identity");
 // Profile identity is the carrying entity's persistent UUID metadata.
 RiverFlowProfile q;
 if(version>=2){q.rapids=j.at("rapids").get<bool>();q.rapidIntensity=j.at("rapidIntensity").get<float>();}
 if(version>=3){q.startWhitewater=j.at("startWhitewater").get<bool>();q.startWhitewaterAmount=j.at("startWhitewaterAmount").get<float>();}
 if(version>=4)q.startWhitewaterNodeIndex=j.at("startWhitewaterNodeIndex").get<int>();
 if(version>=5)q.startWhitewaterNodeId=j.at("startWhitewaterNodeId").get<std::string>();
 q.enabled=j.at("enabled").get<bool>();q.foam=j.at("foam").get<bool>();q.reversed=j.at("reversed").get<bool>();
 q.speed=j.at("speed").get<float>();q.intensity=j.at("intensity").get<float>();q.lifetime=j.at("lifetime").get<float>();
 q.turbulence=j.at("turbulence").get<float>();q.sourceWidth=j.at("sourceWidth").get<float>();q.threshold=j.at("threshold").get<float>();
 if(!ValidateRiverFlowProfile(q,error))return false;p=q;return true;
 }catch(const std::exception& x){error=x.what();return false;}
}
wi::ecs::Entity ResolveRiverWhitewaterSourceNode(
 const wi::scene::Scene& scene,wi::ecs::Entity river,const RiverFlowProfile& profile){
 const auto* spline=scene.splines.GetComponent(river);
 if(!spline)return wi::ecs::INVALID_ENTITY;
 if(!profile.startWhitewaterNodeId.empty()){
  for(const auto node:spline->spline_node_entities)
   if(PersistentEntityId(scene,node)==profile.startWhitewaterNodeId &&
      scene.transforms.Contains(node))return node;
  return wi::ecs::INVALID_ENTITY; // v5 deleted anchor: NEVER reselect by index
 }
 const int index=profile.startWhitewaterNodeIndex;
 return index>=0 && size_t(index)<spline->spline_node_entities.size() &&
        scene.transforms.Contains(spline->spline_node_entities[size_t(index)])
  ?spline->spline_node_entities[size_t(index)] : wi::ecs::INVALID_ENTITY;
}
SetRiverFlowProfileCommand::SetRiverFlowProfileCommand(wi::scene::Scene& s,wi::ecs::Entity e,RiverFlowProfile p):scene_(&s),river_(e),profile_(p){}
bool SetRiverFlowProfileCommand::Execute(){
 std::string error;if(!Water(*scene_,river_)||!ValidateRiverFlowProfile(profile_,error))return false;
 RiverWaterwayControls controls;RiverFlowProfile current;
 if(!ReadRiverWaterwayControls(*scene_,river_,controls,error)||!ReadRiverFlowProfile(*scene_,river_,current,error))return false;
 const auto* authorSpline=scene_->splines.GetComponent(river_);
 // A deleted source must not shift to a different node. But creators
 // must still be able to switch whitewater OFF without restoring the node.
 if(profile_.startWhitewater && !profile_.startWhitewaterNodeId.empty() &&
    ResolveRiverWhitewaterSourceNode(*scene_,river_,profile_)==wi::ecs::INVALID_ENTITY)
    return false;
 if(profile_.startWhitewaterNodeId.empty() && profile_.startWhitewaterNodeIndex>=0 &&
    (!authorSpline || size_t(profile_.startWhitewaterNodeIndex)>=authorSpline->spline_node_entities.size()))
    return false;
 const bool elevated=RiverCourseHasElevation(*scene_,river_);
 if(elevated && (profile_.startWhitewaterNodeIndex>=0||
     !profile_.startWhitewaterNodeId.empty()))return false;
 if(profile_.rapids&&!controls.connected&&!elevated)return false;
 if(elevated){RiverCourseSurface surface;if(profile_.reversed||!BuildRiverCourseSurface(*scene_,river_,profile_.speed,surface,error))return false;}
 if(controls.connected&&controls.owner!=river_&&(profile_.rapids!=current.rapids||profile_.rapidIntensity!=current.rapidIntensity))return false;
 // Until coordinated flow/shape edits are exposed, refuse operations which can
 // detach a saved downstream endpoint or pretend unsupported source knobs work.
 if(controls.connected&&(profile_.speed<=0||profile_.reversed!=current.reversed||
    profile_.turbulence!=current.turbulence||
    profile_.startWhitewater!=current.startWhitewater||
    profile_.startWhitewaterAmount!=current.startWhitewaterAmount||
    profile_.startWhitewaterNodeId!=current.startWhitewaterNodeId||
    profile_.startWhitewaterNodeIndex!=current.startWhitewaterNodeIndex))return false;
 auto* m=scene_->metadatas.GetComponent(river_);
 if(!captured_){
 // Upgrade a selected-node v4 source when first edited. Assign the authored
 // spline node a standard persistent ID before serializing the new profile.
 // New river nodes get normal scene IDs during scene save; no private ID type.
 if(profile_.startWhitewaterNodeId.empty()&&profile_.startWhitewaterNodeIndex>=0){
  const auto node=authorSpline->spline_node_entities[
      size_t(profile_.startWhitewaterNodeIndex)];
  auto stable=PersistentEntityId(*scene_,node);
  if(!IsValidStableId(stable)){
   if(!AssignNewPersistentEntityId(*scene_,node,error))return false;
   stable=PersistentEntityId(*scene_,node);
  }
  if(!IsValidStableId(stable))return false;
  profile_.startWhitewaterNodeId=stable;
 }
 // Creating an ID for a previously unregistered node may grow the
 // metadata component storage. Refresh the river metadata pointer before
 // reading or writing its profile; the old pointer could be invalidated.
 m=scene_->metadatas.GetComponent(river_);
 RiverFlowProfile old;if(!ReadRiverFlowProfile(*scene_,river_,old,error))return false;
 id_=PersistentEntityId(*scene_,river_);if(!IsValidStableId(id_))return false;
 hadMetadata_=m!=nullptr;hadKey_=m&&m->string_values.has(RiverFlowProfileMetadataKey);
 if(hadKey_)before_=m->string_values.get(RiverFlowProfileMetadataKey);
 after_=Encode(profile_);captured_=true;
 }else if(PersistentEntityId(*scene_,river_)!=id_||(m&&m->string_values.has(RiverFlowProfileMetadataKey))!=hadKey_||(hadKey_&&m->string_values.get(RiverFlowProfileMetadataKey)!=before_))return false;
 if(!m)m=&scene_->metadatas.Create(river_);m->string_values.set(RiverFlowProfileMetadataKey,after_);
 if(controls.connected&&profile_.speed!=current.speed){
  RiverWaterway graph;std::shared_ptr<const RiverJunctionSurface> surface;
  if(!ReadRiverWaterway(*scene_,graph,error)||!graph.network||!BuildRiverJunctionSurface(*graph.network,surface,error)){
   if(hadKey_)m->string_values.set(RiverFlowProfileMetadataKey,before_);else m->string_values.erase(RiverFlowProfileMetadataKey);
   return false;
  }
 }
 return true;
}
void SetRiverFlowProfileCommand::Undo(){
 auto* m=scene_->metadatas.GetComponent(river_);
 if(!captured_||!m||!m->string_values.has(RiverFlowProfileMetadataKey)||m->string_values.get(RiverFlowProfileMetadataKey)!=after_)return;
 if(hadKey_)m->string_values.set(RiverFlowProfileMetadataKey,before_);else m->string_values.erase(RiverFlowProfileMetadataKey);
 if(!hadMetadata_&&m->string_values.size()==0&&m->int_values.size()==0&&m->float_values.size()==0&&m->bool_values.size()==0)scene_->metadatas.Remove(river_);
}
}

#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/RiverFlowFieldService.h"
namespace renegade::bridge {
namespace {
class WidthCommand final:public ICommand {
 wi::scene::Scene* scene_;wi::ecs::Entity river_;float before_=0,after_=0;bool captured_=false;
public:
 WidthCommand(wi::scene::Scene& s,wi::ecs::Entity e,float width):scene_(&s),river_(e),after_(width*.5f){}
 bool Execute()override {
  auto* spline=scene_->splines.GetComponent(river_);
  if(!spline||!std::isfinite(after_)||after_<.25f||after_>25)return false;
  RiverWaterwayControls controls;std::string controlsError;
  if(!ReadRiverWaterwayControls(*scene_,river_,controls,controlsError)||controls.connected)return false;
  if(!captured_){before_=spline->width;captured_=true;}else if(spline->width!=before_)return false;
  spline->width=after_;
  std::shared_ptr<const RiverFlowField> field;std::string error;
  RiverCourseSurface course;bool valid=RiverCourseHasElevation(*scene_,river_)?BuildRiverCourseSurface(*scene_,river_,1,course,error):BuildRiverFlowField(*scene_,river_,RiverFlowSettings{},field,error);
  if(!valid){spline->width=before_;return false;}
  return true;
 }
 void Undo()override {auto* s=scene_->splines.GetComponent(river_);if(captured_&&s&&s->width==after_)s->width=before_;}
};
}
std::unique_ptr<ICommand> MakeSetRiverWidthCommand(wi::scene::Scene& s,wi::ecs::Entity e,float width){
 return std::make_unique<WatercourseSceneCommand>(s,std::make_unique<WidthCommand>(s,e,width));
}
}
