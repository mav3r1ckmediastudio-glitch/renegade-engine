#include "renegade/bridge/RiverWaterwayService.h"
#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/RiverFlowSpanService.h"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
using namespace DirectX;
namespace renegade::bridge {
namespace {
using Json=nlohmann::json;
void Check(bool ok,const std::string& reason){if(!ok)throw std::runtime_error(reason);}
bool Water(const wi::scene::Scene& s,wi::ecs::Entity e){const auto* m=s.materials.GetComponent(e);return s.splines.Contains(e)&&m&&m->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER;}
std::string Key(const wi::scene::Scene& s,wi::ecs::Entity e,const char* key){const auto* m=s.metadatas.GetComponent(e);return m&&m->string_values.has(key)?m->string_values.get(key):std::string{};}
bool Has(const wi::scene::Scene& s,wi::ecs::Entity e){const auto* m=s.metadatas.GetComponent(e);return m&&m->string_values.has(RiverConnectionMetadataKey);}
void Set(wi::scene::Scene& s,wi::ecs::Entity e,const std::string& text,bool present){
 auto* m=s.metadatas.GetComponent(e);Check(m!=nullptr,"Persistent river metadata is missing");
 if(present)m->string_values.set(RiverConnectionMetadataKey,text);else m->string_values.erase(RiverConnectionMetadataKey);
}
RiverFlowSettings Settings(const wi::scene::Scene& s,wi::ecs::Entity e){RiverFlowProfile p;std::string error;Check(ReadRiverFlowProfile(s,e,p,error),error);RiverFlowSettings f;f.baseSpeedMetersPerSecond=p.speed;f.downstreamReversed=p.reversed;return f;}
std::vector<float> Knots(const wi::scene::Scene& scene,wi::ecs::Entity river){
 const auto* spline=scene.splines.GetComponent(river);Check(spline&&spline->spline_node_entities.size()>=2&&spline->spline_node_entities.size()<=512,"Invalid receiving spline controls");
 wi::scene::SplineComponent native;
 for(auto e:spline->spline_node_entities){const auto* t=scene.transforms.GetComponent(e);const auto* parent=scene.hierarchy.GetComponent(e);
  Check(t&&parent&&parent->parentID==river,"Native receiving control hierarchy is invalid");auto copy=*t;copy.SetDirty();copy.UpdateTransform();copy.ApplyTransform();native.spline_node_transforms.push_back(copy);}
 native.PrecomputeSplineNodeDistances();Check(std::isfinite(native.precomputed_total_distance)&&native.precomputed_total_distance>0,"Native control distances are invalid");
 std::vector<float> result{0};float sum=0;for(size_t i=0;i+1<native.spline_node_transforms.size();++i){sum+=native.precomputed_node_distances[i];result.push_back(sum/native.precomputed_total_distance);}result.back()=1;return result;
}
XMFLOAT3 Anchor(const wi::scene::Scene& scene,wi::ecs::Entity river,wi::ecs::Entity control){
 Check(Water(scene,river),"Receiver must be a native WATER spline");const auto* spline=scene.splines.GetComponent(river);
 auto found=std::find(spline->spline_node_entities.begin(),spline->spline_node_entities.end(),control);
 Check(found!=spline->spline_node_entities.end(),"Receiving node does not belong to the selected spline");
 auto knots=Knots(scene,river);const float t=knots[size_t(found-spline->spline_node_entities.begin())];
 std::shared_ptr<const RiverFlowField> field;std::string error;Check(BuildRiverFlowField(scene,river,Settings(scene,river),field,error),error);
 if(t==0)return field->NativeRibbon().front().centre;if(t==1)return field->NativeRibbon().back().centre;
 std::shared_ptr<const RiverFlowField> span;Check(BuildRiverFlowFieldSpan(*field,0,t,span,error),error);return span->NativeRibbon().back().centre;
}
std::string ReachId(const wi::scene::Scene& scene,wi::ecs::Entity river,size_t a,size_t b){
 const auto* s=scene.splines.GetComponent(river);return PersistentEntityId(scene,river)+":"+PersistentEntityId(scene,s->spline_node_entities[a])+":"+PersistentEntityId(scene,s->spline_node_entities[b]);
}
// Transaction guard excludes all transient native caches and all unrelated
// material/metadata properties. It protects every WATER shape and connection.
std::string Signature(const wi::scene::Scene& scene){
 std::map<std::string,Json> values;size_t total=0;
 for(size_t i=0;i<scene.splines.GetCount();++i){auto e=scene.splines.GetEntity(i);if(!Water(scene,e))continue;
  const auto& s=scene.splines[i];Check(++total<=128&&s.spline_node_entities.size()<=512,"Waterway transaction source budget exceeded");
  Json j={{"connectionPresent",Has(scene,e)},{"connection",Key(scene,e,RiverConnectionMetadataKey)},{"profile",Key(scene,e,RiverFlowProfileMetadataKey)},
   {"width",s.width},{"rotation",s.rotation},{"subdivision",s.mesh_generation_subdivision},{"vertical",s.mesh_generation_vertical_subdivision},
   {"loop",s.IsLooped()},{"filled",s.IsFilled()},{"aligned",s.IsDrawAligned()}};
  XMFLOAT4X4 root;XMStoreFloat4x4(&root,scene.ComputeEntityMatrixRecursive(e));j["world"]=Json::array();for(int k=0;k<16;++k)j["world"].push_back((&root._11)[k]);
  j["nodes"]=Json::array();for(auto node:s.spline_node_entities){const auto* t=scene.transforms.GetComponent(node);Check(t!=nullptr,"Missing river control");const auto* h=scene.hierarchy.GetComponent(node);
   j["nodes"].push_back({PersistentEntityId(scene,node),h?PersistentEntityId(scene,h->parentID):"",t->translation_local.x,t->translation_local.y,t->translation_local.z,
    t->rotation_local.x,t->rotation_local.y,t->rotation_local.z,t->rotation_local.w,t->scale_local.x,t->scale_local.y,t->scale_local.z});}
  Check(values.emplace(PersistentEntityId(scene,e),j).second,"Duplicate river identity in transaction");
 }
 return Json(values).dump();
}
}
bool ReadRiverWaterwayControls(const wi::scene::Scene& scene,wi::ecs::Entity selected,RiverWaterwayControls& result,std::string& error){
 try{
  Check(Water(scene,selected),"Select a native WATER spline");
  RiverWaterwayControls out;out.owner=selected;size_t carriers=0;
  for(size_t k=0;k<scene.metadatas.GetCount();++k){
   const auto source=scene.metadatas.GetEntity(k);if(!Has(scene,source))continue;
   Check(++carriers<=15,"Connection limit is15");
   const auto text=Key(scene,source,RiverConnectionMetadataKey);Check(text.size()<=4096,"River connection metadata exceeds4096 bytes");
   const auto j=Json::parse(text);Check(j.size()==3&&j.at("version")==1,"Unsupported river connection schema");
   const auto receiverId=j.at("receiver").get<std::string>(),nodeId=j.at("node").get<std::string>();
   Check(Water(scene,source)&&IsValidStableId(PersistentEntityId(scene,source))&&IsValidStableId(receiverId)&&IsValidStableId(nodeId),"Malformed river connection identity");
   wi::ecs::Entity receiver=wi::ecs::INVALID_ENTITY;size_t matches=0;
   for(size_t i=0;i<scene.splines.GetCount();++i){auto e=scene.splines.GetEntity(i);if(PersistentEntityId(scene,e)==receiverId){receiver=e;++matches;}}
   Check(matches==1&&Water(scene,receiver)&&source!=receiver,"Missing, duplicate or self-referenced receiver");
   wi::ecs::Entity node=wi::ecs::INVALID_ENTITY;matches=0;
   for(auto e:scene.splines.GetComponent(receiver)->spline_node_entities)if(PersistentEntityId(scene,e)==nodeId){node=e;++matches;}
   const auto* hierarchy=scene.hierarchy.GetComponent(node);
   Check(matches==1&&hierarchy&&hierarchy->parentID==receiver,"Receiving node no longer belongs to its river");
   if(source==selected||receiver==selected){out={receiver,node,true};}
  }
  Check(!out.connected||carriers==1,"Creator controls support one flat confluence");
  result=out;error.clear();return true;
 }catch(const std::exception& ex){error=ex.what();return false;}
}
bool RiverConnectionAnchor(const wi::scene::Scene& s,wi::ecs::Entity river,wi::ecs::Entity control,XMFLOAT3& result,std::string& error){
 try{auto p=Anchor(s,river,control);result=p;error.clear();return true;}catch(const std::exception& ex){error=ex.what();return false;}
}
bool ReadRiverWaterway(const wi::scene::Scene& scene,RiverWaterway& result,std::string& error){
 try{
  RiverWaterway out;EntityIdentityIndex identities;std::set<wi::ecs::Entity> members;
  for(size_t i=0;i<scene.metadatas.GetCount();++i){const auto e=scene.metadatas.GetEntity(i);if(!Has(scene,e))continue;
   Check(Water(scene,e),"Connection metadata is carried by a non-river entity");auto text=Key(scene,e,RiverConnectionMetadataKey);Check(text.size()<=4096,"River connection metadata exceeds4096 bytes");
   auto j=Json::parse(text);Check(j.at("version")==1&&j.size()==3,"Unsupported river connection schema");
   RiverConnection c{PersistentEntityId(scene,e),j.at("receiver").get<std::string>(),j.at("node").get<std::string>()};
   Check(IsValidStableId(c.tributary)&&IsValidStableId(c.receiver)&&IsValidStableId(c.receiverNode),"Malformed connection UUID");
   Check(out.connections.size()<15,"Connection limit is15");out.connections.push_back(c);
  }
  if(out.connections.empty()){result=std::move(out);error.clear();return true;}
  Check(identities.Build(scene,error),error);
  std::sort(out.connections.begin(),out.connections.end(),[](const auto& a,const auto& b){return a.tributary<b.tributary;});
  std::map<wi::ecs::Entity,std::set<size_t>> cuts;
  for(const auto& c:out.connections){auto inlet=identities.Resolve(c.tributary),receiver=identities.Resolve(c.receiver),node=identities.Resolve(c.receiverNode);
   Check(Water(scene,inlet)&&Water(scene,receiver)&&inlet!=receiver,"Missing or self-referenced river connection");
   const auto* s=scene.splines.GetComponent(receiver);auto it=std::find(s->spline_node_entities.begin(),s->spline_node_entities.end(),node);
   Check(it!=s->spline_node_entities.end(),"Receiving node UUID no longer belongs to its river");
   members.insert(inlet);members.insert(receiver);cuts[receiver].insert(size_t(it-s->spline_node_entities.begin()));
  }
  std::map<wi::ecs::Entity,std::vector<size_t>> ordered;
  for(auto e:members){const auto* s=scene.splines.GetComponent(e);Check(s->spline_node_entities.size()>=2,"Waterway river requires at least two controls");cuts[e].insert(0);cuts[e].insert(s->spline_node_entities.size()-1);
   auto& indices=ordered[e];indices.assign(cuts[e].begin(),cuts[e].end());auto knots=Knots(scene,e);auto flow=Settings(scene,e);
   for(size_t k=1;k<indices.size();++k){Check(out.reaches.size()<16,"Logical native reach limit is16");out.reaches.push_back({ReachId(scene,e,indices[k-1],indices[k]),e,flow,knots[indices[k-1]],knots[indices[k]]});}
  }
  std::map<std::string,RiverFlowNetworkJunctionInput> joins;
  for(const auto& c:out.connections){auto source=identities.Resolve(c.tributary),receiver=identities.Resolve(c.receiver),node=identities.Resolve(c.receiverNode);
   const auto& sourceCuts=ordered.at(source);const auto& targetCuts=ordered.at(receiver);const auto* s=scene.splines.GetComponent(receiver);
   const size_t nodeIndex=size_t(std::find(s->spline_node_entities.begin(),s->spline_node_entities.end(),node)-s->spline_node_entities.begin());
   const size_t cut=size_t(std::find(targetCuts.begin(),targetCuts.end(),nodeIndex)-targetCuts.begin());const bool reverse=Settings(scene,receiver).downstreamReversed;
   Check(reverse?cut>0:cut+1<targetCuts.size(),"Cannot attach to the receiving river's downstream end");
   auto& join=joins[c.receiver+":"+c.receiverNode];
   if(join.id.empty()){
    join.id=c.receiver+":"+c.receiverNode;
    join.outgoing=reverse?ReachId(scene,receiver,targetCuts[cut-1],targetCuts[cut]):ReachId(scene,receiver,targetCuts[cut],targetCuts[cut+1]);
    if(reverse&&cut+1<targetCuts.size())join.incoming.push_back(ReachId(scene,receiver,targetCuts[cut],targetCuts[cut+1]));
    if(!reverse&&cut>0)join.incoming.push_back(ReachId(scene,receiver,targetCuts[cut-1],targetCuts[cut]));
   }
   const bool reversedSource=Settings(scene,source).downstreamReversed;const size_t k=reversedSource?0:sourceCuts.size()-2;
   join.incoming.push_back(ReachId(scene,source,sourceCuts[k],sourceCuts[k+1]));
  }
  for(auto& pair:joins)out.junctions.push_back(std::move(pair.second));
  Check(BuildRiverFlowNetwork(scene,out.reaches,out.junctions,out.network,error),error);
  result=std::move(out);error.clear();return true;
 }catch(const std::exception& ex){error=ex.what();return false;}
}
struct SetRiverConnectionCommand::Impl {
 wi::scene::Scene* scene;wi::ecs::Entity requestedSource,requestedReceiver,requestedControl;
 float maxSnap;bool disconnect=false,captured=false,hadKey=false;
 StableId sourceId,nodeId;std::string beforeText,afterText,beforeSignature,afterSignature,error;
 wi::scene::TransformComponent beforeNode,afterNode;bool changesNode=false;
 Impl(wi::scene::Scene& s,wi::ecs::Entity a,wi::ecs::Entity b,wi::ecs::Entity c,float d):scene(&s),requestedSource(a),requestedReceiver(b),requestedControl(c),maxSnap(d){}
 void ApplyNode(wi::ecs::Entity node,const wi::scene::TransformComponent& pose){
  auto* t=scene->transforms.GetComponent(node);Check(t!=nullptr,"Connection endpoint was deleted");
  // Existing publisher reconstructs world state and invalidates native spline mesh caches.
  const auto current=CaptureTransform(*t),target=CaptureTransform(pose);
  if(std::memcmp(&current.translation,&target.translation,sizeof(target.translation))==0&&
      std::memcmp(&current.rotation,&target.rotation,sizeof(target.rotation))==0&&
      std::memcmp(&current.scale,&target.scale,sizeof(target.scale))==0)return;
  // Undo's exact restoration path publishes even sub-threshold snap corrections.
  SetTransformCommand publish(*scene,node,target,current);publish.Undo();
 }
 void Apply(wi::ecs::Entity river,wi::ecs::Entity node,bool after){
  Set(*scene,river,after?afterText:beforeText,after?!disconnect:hadKey);
  if(changesNode)ApplyNode(node,after?afterNode:beforeNode);
  if(auto* spline=scene->splines.GetComponent(river))spline->SetDirty();
 }
 bool Execute(){
  error.clear();wi::ecs::Entity source=wi::ecs::INVALID_ENTITY,node=wi::ecs::INVALID_ENTITY;bool applied=false;bool priorDirty=false;
  try{
   EntityIdentityIndex index;Check(index.Build(*scene,error),error);
   if(captured){
    Check(Signature(*scene)==beforeSignature,"River shape or connection changed since Undo; stale Redo refused");
    source=index.Resolve(sourceId);node=changesNode?index.Resolve(nodeId):wi::ecs::INVALID_ENTITY;
    Check(Water(*scene,source),"River identity was deleted or replaced");priorDirty=scene->splines.GetComponent(source)->IsDirty();
    applied=true;Apply(source,node,true);RiverWaterway validate;Check(ReadRiverWaterway(*scene,validate,error),error);
    Check(Signature(*scene)==afterSignature,"Connection Redo no longer matches its captured result");return true;
   }
   source=requestedSource;Check(Water(*scene,source),"Choose a native WATER tributary");sourceId=PersistentEntityId(*scene,source);
   Check(IsValidStableId(sourceId),"Tributary persistent identity is missing");beforeSignature=Signature(*scene);
   hadKey=Has(*scene,source);beforeText=Key(*scene,source,RiverConnectionMetadataKey);
   priorDirty=scene->splines.GetComponent(source)->IsDirty();
   if(disconnect){Check(hadKey,"This tributary has no saved connection");afterText.clear();changesNode=false;}
   else{
    Check(!hadKey,"Tributary downstream endpoint is already connected; disconnect explicitly first");
    Check(std::isfinite(maxSnap)&&maxSnap>=0&&maxSnap<=5,"Connection snap limit must be finite0..5 metres");
    Check(source!=requestedReceiver&&Water(*scene,requestedReceiver),"Cannot connect a river to itself or a non-water receiver");
    RiverWaterway current;Check(ReadRiverWaterway(*scene,current,error),error);
    std::shared_ptr<const RiverFlowField> field;auto flow=Settings(*scene,source);Check(BuildRiverFlowField(*scene,source,flow,field,error),error);
    const auto anchor=Anchor(*scene,requestedReceiver,requestedControl);
    const auto endpoint=flow.downstreamReversed?field->NativeRibbon().front().centre:field->NativeRibbon().back().centre;
    Check(std::hypot(endpoint.x-anchor.x,endpoint.z-anchor.z)<=maxSnap+.000001f,"Tributary endpoint is outside explicit snap distance");
    Check(std::abs(endpoint.y-anchor.y)<=.005f,"Cannot snap different water levels; height transitions are unsupported");
    auto* spline=scene->splines.GetComponent(source);node=flow.downstreamReversed?spline->spline_node_entities.front():spline->spline_node_entities.back();
    nodeId=PersistentEntityId(*scene,node);Check(IsValidStableId(nodeId),"Tributary endpoint UUID is missing");beforeNode=*scene->transforms.GetComponent(node);afterNode=beforeNode;
    XMVECTOR determinant;auto inverse=XMMatrixInverse(&determinant,scene->ComputeEntityMatrixRecursive(source));
    Check(std::isfinite(XMVectorGetX(determinant))&&std::abs(XMVectorGetX(determinant))>1e-12f,"Tributary parent transform is singular");
    XMStoreFloat3(&afterNode.translation_local,XMVector3TransformCoord(XMLoadFloat3(&anchor),inverse));
    Check(std::isfinite(afterNode.translation_local.x)&&std::isfinite(afterNode.translation_local.y)&&std::isfinite(afterNode.translation_local.z),"Connection local transform is nonfinite");
    afterText=Json{{"version",1},{"receiver",PersistentEntityId(*scene,requestedReceiver)},{"node",PersistentEntityId(*scene,requestedControl)}}.dump();changesNode=true;
   }
   applied=true;Apply(source,node,true);RiverWaterway validate;Check(ReadRiverWaterway(*scene,validate,error),error);
   afterSignature=Signature(*scene);captured=true;return true;
  }catch(const std::exception& ex){
   error=ex.what();if(applied){try{Apply(source,node,false);scene->splines.GetComponent(source)->SetDirty(priorDirty);}catch(...){error+="; connection rollback failed";}}
   return false;
  }
 }
 void Undo(){
  error.clear();try{
   Check(captured,"Connection command has not executed");EntityIdentityIndex index;Check(index.Build(*scene,error),error);
   Check(Signature(*scene)==afterSignature,"River shape or connection changed since Execute; stale Undo refused");
   auto source=index.Resolve(sourceId),node=changesNode?index.Resolve(nodeId):wi::ecs::INVALID_ENTITY;
   Check(Water(*scene,source),"Connection source identity was replaced");Apply(source,node,false);
  }catch(const std::exception& ex){error=ex.what();}
 }
};
SetRiverConnectionCommand::SetRiverConnectionCommand(wi::scene::Scene& s,wi::ecs::Entity source,wi::ecs::Entity target,wi::ecs::Entity control,float distance):impl_(std::make_unique<Impl>(s,source,target,control,distance)){}
std::unique_ptr<SetRiverConnectionCommand> SetRiverConnectionCommand::Disconnect(wi::scene::Scene& s,wi::ecs::Entity river){
 auto result=std::make_unique<SetRiverConnectionCommand>(s,river,wi::ecs::INVALID_ENTITY,wi::ecs::INVALID_ENTITY,0);result->impl_->disconnect=true;return result;
}
SetRiverConnectionCommand::~SetRiverConnectionCommand()=default;
bool SetRiverConnectionCommand::Execute(){return impl_->Execute();}
void SetRiverConnectionCommand::Undo(){impl_->Undo();}
const std::string& SetRiverConnectionCommand::Error()const{return impl_->error;}
}
