#include "renegade/bridge/RiverWaterwayService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/RiverJunctionSurfaceService.h"
#include "json.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <functional>
#include <iostream>
#include <cmath>
#include <cstring>
using namespace DirectX;
namespace renegade::bridge {
namespace {
void Require(bool ok,const std::string& text){if(!ok)throw std::runtime_error(text);}
struct SavedPose{StableId id;TransformState pose;};
using Poses=std::vector<SavedPose>;
wi::ecs::Entity Endpoint(const wi::scene::Scene& scene,wi::ecs::Entity river){RiverFlowProfile p;std::string error;Require(ReadRiverFlowProfile(scene,river,p,error),error);const auto* s=scene.splines.GetComponent(river);Require(s&&!s->spline_node_entities.empty(),"Connected native controls missing");return p.reversed?s->spline_node_entities.front():s->spline_node_entities.back();}
XMFLOAT3 World(const wi::scene::Scene& scene,wi::ecs::Entity node){XMFLOAT3 p;XMStoreFloat3(&p,scene.ComputeEntityMatrixRecursive(node).r[3]);return p;}
float Distance(XMFLOAT3 a,XMFLOAT3 b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
bool SamePose(const TransformState& a,const TransformState& b){return std::memcmp(&a.translation,&b.translation,sizeof(a.translation))==0&&std::memcmp(&a.rotation,&b.rotation,sizeof(a.rotation))==0&&std::memcmp(&a.scale,&b.scale,sizeof(a.scale))==0;}
void Publish(wi::scene::Scene& scene,wi::ecs::Entity e,const TransformState& pose){
 auto* t=scene.transforms.GetComponent(e);Require(t!=nullptr,"Connected control was deleted");const auto current=CaptureTransform(*t);if(SamePose(current,pose))return;
 // Restore through the existing publisher without authoring no-op quantization.
 SetTransformCommand restore(scene,e,pose,current);restore.Undo();Require(SamePose(CaptureTransform(*t),pose),"Exact connected pose restoration failed");
}
TransformState AtWorld(const wi::scene::Scene& scene,wi::ecs::Entity node,XMFLOAT3 p){
 auto pose=CaptureTransform(*scene.transforms.GetComponent(node));auto* h=scene.hierarchy.GetComponent(node);
 if(h){XMVECTOR det;auto inverse=XMMatrixInverse(&det,scene.ComputeEntityMatrixRecursive(h->parentID));Require(std::isfinite(XMVectorGetX(det))&&std::abs(XMVectorGetX(det))>1e-12f,"Connected parent is singular");XMStoreFloat3(&pose.translation,XMVector3TransformCoord(XMLoadFloat3(&p),inverse));}
 else pose.translation=p;return pose;
}
Poses Capture(const wi::scene::Scene& scene,const std::set<StableId>& roots,const EntityIdentityIndex& index){
 Poses out;std::set<StableId> added;for(const auto& id:roots){auto root=index.Resolve(id);auto* s=scene.splines.GetComponent(root);Require(s&&s->spline_node_entities.size()<=512,"Connected curve budget exceeded");
  auto add=[&](wi::ecs::Entity e){auto uuid=PersistentEntityId(scene,e);Require(IsValidStableId(uuid)&&added.insert(uuid).second,"Missing or duplicate connected node identity");auto* transform=scene.transforms.GetComponent(e);Require(transform!=nullptr,"Connected transform missing");out.push_back({uuid,CaptureTransform(*transform)});};add(root);for(auto node:s->spline_node_entities)add(node);}
 return out;
}
void Restore(wi::scene::Scene& scene,const Poses& poses,const EntityIdentityIndex& index){
 // Resolve every target before publishing any pose so missing controls cannot cause a partial restore.
 for(const auto& saved:poses){const auto e=index.Resolve(saved.id);Require(e!=wi::ecs::INVALID_ENTITY&&scene.transforms.Contains(e),"Connected restore target missing");}
 for(bool roots:{true,false})for(const auto& saved:poses){auto e=index.Resolve(saved.id);Require(e!=wi::ecs::INVALID_ENTITY,"Connected identity was replaced");if(scene.splines.Contains(e)==roots)Publish(scene,e,saved.pose);}
}
bool MetadataMatches(const wi::scene::Scene& scene,const EntityIdentityIndex& index,const StableId& id,const std::string& expected){
 const auto* m=scene.metadatas.GetComponent(index.Resolve(id));return m&&m->string_values.has(RiverConnectionMetadataKey)&&m->string_values.get(RiverConnectionMetadataKey)==expected;
}
std::string TopologySignature(const wi::scene::Scene& scene){
 std::map<StableId,std::string> documents;
 for(size_t k=0;k<scene.metadatas.GetCount();++k){const auto& m=scene.metadatas[k];if(!m.string_values.has(RiverConnectionMetadataKey))continue;
  auto id=PersistentEntityId(scene,scene.metadatas.GetEntity(k));Require(IsValidStableId(id),"Connection carrier identity missing");
  const auto& text=m.string_values.get(RiverConnectionMetadataKey);Require(text.size()<=4096&&documents.size()<16,"Connection topology guard budget exceeded");
  Require(documents.emplace(id,text).second,"Duplicate connection carrier identity");
 }
 std::string out;for(const auto& entry:documents){out+=entry.first;const auto length=uint64_t(entry.second.size());out.append(reinterpret_cast<const char*>(&length),sizeof(length));out+=entry.second;}return out;
}
std::string PoseSignature(const wi::scene::Scene& scene,const Poses& poses,const EntityIdentityIndex& index){
 std::string out;for(const auto& saved:poses){auto e=index.Resolve(saved.id);Require(e!=wi::ecs::INVALID_ENTITY&&scene.transforms.Contains(e),"Connected transform was deleted");auto p=CaptureTransform(*scene.transforms.GetComponent(e));out+=saved.id;
 auto* parent=scene.hierarchy.GetComponent(e);out+=parent?PersistentEntityId(scene,parent->parentID):"no-parent";
 if(const auto* spline=scene.splines.GetComponent(e)){
  out.append(reinterpret_cast<const char*>(&spline->width),sizeof(spline->width));out.append(reinterpret_cast<const char*>(&spline->rotation),sizeof(spline->rotation));out.append(reinterpret_cast<const char*>(&spline->mesh_generation_subdivision),sizeof(spline->mesh_generation_subdivision));
  out.append(reinterpret_cast<const char*>(&spline->mesh_generation_vertical_subdivision),sizeof(spline->mesh_generation_vertical_subdivision));
  const bool flags[]={spline->IsLooped(),spline->IsFilled(),spline->IsDrawAligned()};out.append(reinterpret_cast<const char*>(flags),sizeof(flags));
  for(auto node:spline->spline_node_entities)out+=PersistentEntityId(scene,node);
  XMFLOAT4X4 world;XMStoreFloat4x4(&world,scene.ComputeEntityMatrixRecursive(e));out.append(reinterpret_cast<const char*>(&world),sizeof(world));
  auto* metadata=scene.metadatas.GetComponent(e);if(metadata&&metadata->string_values.has(RiverFlowProfileMetadataKey))out+=metadata->string_values.get(RiverFlowProfileMetadataKey);
 }
 out.append(reinterpret_cast<const char*>(&p.translation),sizeof(p.translation));out.append(reinterpret_cast<const char*>(&p.rotation),sizeof(p.rotation));out.append(reinterpret_cast<const char*>(&p.scale),sizeof(p.scale));}
 return out;
}
void ResolveGeometry(wi::scene::Scene& scene,const RiverWaterway& authored,const EntityIdentityIndex& index){
 std::map<StableId,const RiverConnection*> edges;for(const auto& c:authored.connections)edges.emplace(c.tributary,&c);
 std::map<StableId,int> visited;std::vector<StableId> order;
 std::function<void(const StableId&)> visit=[&](const StableId& id){Require(visited[id]!=1,"Connected edit contains a cycle");if(visited[id]==2)return;visited[id]=1;auto it=edges.find(id);if(it!=edges.end())visit(it->second->receiver);visited[id]=2;order.push_back(id);};
 for(const auto& c:authored.connections)visit(c.tributary);
 std::string error;for(const auto& id:order){auto found=edges.find(id);if(found==edges.end())continue;const auto& c=*found->second;
  auto receiver=index.Resolve(c.receiver),control=index.Resolve(c.receiverNode),source=index.Resolve(c.tributary);XMFLOAT3 target;
  Require(RiverConnectionAnchor(scene,receiver,control,target,error),error);auto endpoint=Endpoint(scene,source);
  if(Distance(World(scene,endpoint),target)>.00002f)Publish(scene,endpoint,AtWorld(scene,endpoint,target));
 }
 RiverWaterway current;Require(ReadRiverWaterway(scene,current,error),error);std::shared_ptr<const RiverJunctionSurface> surface;
 Require(current.network&&BuildRiverJunctionSurface(*current.network,surface,error),error);
}
class ConnectedEdit final:public ICommand {
 wi::scene::Scene* scene_;wi::ecs::Entity edited_;std::unique_ptr<ICommand> edit_;bool captured_=false;
 Poses before_,after_;std::string beforeSignature_,afterSignature_,topologySignature_;
 std::vector<std::pair<StableId,std::string>> metadata_;
public:
 ConnectedEdit(wi::scene::Scene& s,wi::ecs::Entity e,std::unique_ptr<ICommand> inner):scene_(&s),edited_(e),edit_(std::move(inner)){}
 bool Execute()override{
  bool touched=false;EntityIdentityIndex index;try{std::string error;Require(index.Build(*scene_,error),error);
   if(captured_){Require(TopologySignature(*scene_)==topologySignature_,"Connected topology changed since Undo");Require(PoseSignature(*scene_,before_,index)==beforeSignature_,"Connected edit Redo is stale");for(auto v:metadata_)Require(MetadataMatches(*scene_,index,v.first,v.second),"Connection changed since Undo");touched=true;Restore(*scene_,after_,index);RiverWaterway check;Require(ReadRiverWaterway(*scene_,check,error),error);return true;}
   RiverWaterway graph;Require(ReadRiverWaterway(*scene_,graph,error)&&graph.network,error.empty()?"Connected edit topology missing":error);
   std::set<StableId> roots;for(const auto& c:graph.connections){roots.insert(c.tributary);roots.insert(c.receiver);metadata_.push_back({c.tributary,scene_->metadatas.GetComponent(index.Resolve(c.tributary))->string_values.get(RiverConnectionMetadataKey)});}
   topologySignature_=TopologySignature(*scene_);before_=Capture(*scene_,roots,index);beforeSignature_=PoseSignature(*scene_,before_,index);
   XMFLOAT3 originalWorld{};const RiverConnection* dragged=nullptr;
   for(const auto& c:graph.connections)if(Endpoint(*scene_,index.Resolve(c.tributary))==edited_){dragged=&c;originalWorld=World(*scene_,edited_);break;}
   touched=true;Require(edit_&&edit_->Execute(),"Native connected edit failed");
   if(dragged){const auto desired=World(*scene_,edited_);XMFLOAT3 delta{desired.x-originalWorld.x,desired.y-originalWorld.y,desired.z-originalWorld.z};
    Require(std::abs(delta.y)<=.001f,"Connected flat junctions support horizontal edits only");auto leader=index.Resolve(dragged->receiverNode);auto position=World(*scene_,leader);
    position={position.x+delta.x,position.y+delta.y,position.z+delta.z};auto target=AtWorld(*scene_,leader,position);
    RiverNodeMovePreview shared(*scene_,leader,CaptureTransform(*scene_->transforms.GetComponent(leader)));Require(shared.Apply(target),"Receiving junction edit failed");
   }
   ResolveGeometry(*scene_,graph,index);after_=Capture(*scene_,roots,index);afterSignature_=PoseSignature(*scene_,after_,index);captured_=true;return true;
  }catch(const std::exception& ex){std::cerr<<"CONNECTED_EDIT_REFUSED="<<ex.what()<<"\n";if(touched){try{Restore(*scene_,before_,index);}catch(...){}}return false;}
 }
 void Undo()override{try{std::string error;EntityIdentityIndex index;Require(captured_&&index.Build(*scene_,error),"Connected Undo identity failure");Require(TopologySignature(*scene_)==topologySignature_,"Connected topology changed since Execute");Require(PoseSignature(*scene_,after_,index)==afterSignature_,"Connected edit Undo is stale");for(auto v:metadata_)Require(MetadataMatches(*scene_,index,v.first,v.second),"Connection changed since Execute");Restore(*scene_,before_,index);}catch(const std::exception& ex){std::cerr<<"CONNECTED_UNDO_REFUSED="<<ex.what()<<"\n";} }
};
}
namespace {
class RenderableConnection final:public ICommand {
 wi::scene::Scene& scene_;SetRiverConnectionCommand command_;
public:
 RenderableConnection(wi::scene::Scene& scene,wi::ecs::Entity source,wi::ecs::Entity receiver,wi::ecs::Entity control,float distance):scene_(scene),command_(scene,source,receiver,control,distance){}
 bool Execute()override{if(!command_.Execute())return false;RiverWaterway graph;std::string error;std::shared_ptr<const RiverJunctionSurface> surface;
  if(!ReadRiverWaterway(scene_,graph,error)||!graph.network||!BuildRiverJunctionSurface(*graph.network,surface,error)){command_.Undo();return false;}return true;}
 void Undo()override{command_.Undo();}
};
}
std::unique_ptr<ICommand> MakeSetRiverConnectionCommand(wi::scene::Scene& scene,wi::ecs::Entity tributary,wi::ecs::Entity receiver,wi::ecs::Entity control,float maxSnap){
 return std::make_unique<WatercourseSceneCommand>(scene,std::make_unique<RenderableConnection>(scene,tributary,receiver,control,maxSnap));
}
std::unique_ptr<ICommand> MakeDisconnectRiverCommand(wi::scene::Scene& scene,wi::ecs::Entity tributary){
 return std::make_unique<WatercourseSceneCommand>(scene,SetRiverConnectionCommand::Disconnect(scene,tributary));
}
std::unique_ptr<ICommand> MakeRiverWaterwayEditCommand(wi::scene::Scene& scene,wi::ecs::Entity entity,std::unique_ptr<ICommand> ordinary){
 wi::ecs::Entity root=scene.splines.Contains(entity)?entity:wi::ecs::INVALID_ENTITY;
 if(root==wi::ecs::INVALID_ENTITY)if(auto* parent=scene.hierarchy.GetComponent(entity))if(scene.splines.Contains(parent->parentID))root=parent->parentID;
 if(root==wi::ecs::INVALID_ENTITY)return ordinary;
 const auto* material=scene.materials.GetComponent(root);if(!material||material->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER)return ordinary;
 const auto id=PersistentEntityId(scene,root);bool relevant=false;
 for(size_t k=0;k<scene.metadatas.GetCount();++k){const auto& m=scene.metadatas[k];if(!m.string_values.has(RiverConnectionMetadataKey))continue;
  if(PersistentEntityId(scene,scene.metadatas.GetEntity(k))==id){relevant=true;break;}
  const auto& text=m.string_values.get(RiverConnectionMetadataKey);if(text.size()>4096)continue;
  try{auto j=nlohmann::json::parse(text);if(j.at("receiver").get<std::string>()==id){relevant=true;break;}}catch(...){}
 }
 if(relevant)return std::make_unique<ConnectedEdit>(scene,entity,std::move(ordinary));return ordinary;
}
struct CreateRiverTributaryCommand::Impl {
 wi::scene::Scene& scene;std::vector<XMFLOAT3> points;
 wi::ecs::Entity receiver,control,terrain,created=wi::ecs::INVALID_ENTITY;
 StableId receiverId,controlId,terrainId,sourceId;float width;bool captured=false,executed=false;
 wi::Archive branch;std::unique_ptr<SetRiverFlowProfileCommand> enable;
 std::unique_ptr<SetRiverConnectionCommand> connection;
 std::unique_ptr<SetRiverTerrainLayerCommand> bed;std::string error;
 Impl(wi::scene::Scene& s,std::vector<XMFLOAT3> p,wi::ecs::Entity r,wi::ecs::Entity c,float w,wi::ecs::Entity t):
  scene(s),points(std::move(p)),receiver(r),control(c),terrain(t),receiverId(PersistentEntityId(s,r)),controlId(PersistentEntityId(s,c)),terrainId(PersistentEntityId(s,t)),width(w){}
 bool Execute(){
  error.clear();bool enabled=false,made=false,linked=false,carved=false;
  try{
   Require(!executed,"Tributary command is already applied");
   Require(IsValidStableId(receiverId)&&IsValidStableId(controlId)&&
    PersistentEntityId(scene,receiver)==receiverId&&PersistentEntityId(scene,control)==controlId,"Receiving river or control identity changed");
   if(terrain!=wi::ecs::INVALID_ENTITY)Require(IsValidStableId(terrainId)&&PersistentEntityId(scene,terrain)==terrainId&&scene.terrains.Contains(terrain),"Receiving terrain identity changed");
   RiverFlowProfile profile;Require(ReadRiverFlowProfile(scene,receiver,profile,error),error);
   Require(profile.speed>0,"Receiving river must have positive flow");
   XMFLOAT3 anchor;Require(RiverConnectionAnchor(scene,receiver,control,anchor,error),error);
   const auto* spline=scene.splines.GetComponent(receiver);
   auto it=std::find(spline->spline_node_entities.begin(),spline->spline_node_entities.end(),control);
   Require(it!=spline->spline_node_entities.begin()&&it+1!=spline->spline_node_entities.end(),"Choose an interior receiving river control");
   Require(IsRiverSplineStrokeValid(points)&&std::isfinite(width)&&width>=.5f&&width<=50,"Tributary stroke or width is invalid");
   for(auto p:points)Require(std::abs(p.y-anchor.y)<=.005f,"Tributaries currently require the receiving water level");
   Require(Distance(points.back(),anchor)<=.01f,"Drawn tributary no longer ends at its receiving control");
   if(!enable){profile.enabled=true;enable=std::make_unique<SetRiverFlowProfileCommand>(scene,receiver,profile);}
   Require(enable->Execute(),"Receiving flow settings changed; redraw the tributary");enabled=true;
   if(captured){
    const auto entities=EnumeratePersistentSceneEntities(scene);
    Require(std::find(entities.begin(),entities.end(),created)==entities.end(),"Created river identity was reused");
    branch.SetReadModeAndResetPos(true);wi::ecs::EntitySerializer serializer;serializer.allow_remap=false;
    Require(scene.Entity_Serialize(branch,serializer)==created,"Tributary branch restore failed");made=true;
   }else{
    RiverSplineOptions options;options.widthMeters=width;options.surfaceOffsetMeters=0;
    CreateRiverSplineCommand create(scene,points,options);
    Require(create.Execute(),"Native WATER creation failed; normal map must be available");created=create.CreatedEntity();made=true;
    for(auto node:scene.splines.GetComponent(created)->spline_node_entities)Require(AssignNewPersistentEntityId(scene,node,error),error);
    sourceId=PersistentEntityId(scene,created);
    RiverFlowProfile own;own.enabled=true;SetRiverFlowProfileCommand setup(scene,created,own);Require(setup.Execute(),"New tributary flow initialization failed");
    RiverWaterMaterialState optics;Require(CaptureRiverWaterMaterialState(scene,receiver,optics),"Receiving WATER optics missing");
    SetRiverWaterMaterialCommand copyOptics(scene,created,optics);(void)copyOptics.Execute();
    // Native material commands intentionally return false for exact no-ops.
    // A new stock river already matches a stock receiver: verify the resulting
    // optics rather than interpreting no change as failed initialization.
    RiverWaterMaterialState applied;Require(CaptureRiverWaterMaterialState(scene,created,applied)&&
     applied.tintOpacity.x==optics.tintOpacity.x&&applied.tintOpacity.y==optics.tintOpacity.y&&
     applied.tintOpacity.z==optics.tintOpacity.z&&applied.tintOpacity.w==optics.tintOpacity.w&&
     applied.flowUvU==optics.flowUvU&&applied.flowUvV==optics.flowUvV&&applied.normalStrength==optics.normalStrength,
     "Tributary optics initialization failed");
    branch.SetReadModeAndResetPos(false);wi::ecs::EntitySerializer serializer;scene.Entity_Serialize(branch,serializer,created);
    connection=std::make_unique<SetRiverConnectionCommand>(scene,created,receiver,control,.01f);
   }
   Require(PersistentEntityId(scene,created)==sourceId,"Tributary persistent identity did not restore");
   Require(connection->Execute(),connection->Error());linked=true;
   RiverWaterway graph;std::shared_ptr<const RiverJunctionSurface> surface;
   Require(ReadRiverWaterway(scene,graph,error)&&graph.network&&BuildRiverJunctionSurface(*graph.network,surface,error),error);
   if(terrain!=wi::ecs::INVALID_ENTITY){
    if(!bed)bed=std::make_unique<SetRiverTerrainLayerCommand>(scene,terrain,created,DefaultRiverBedDepthMeters,DefaultRiverBankWidthMeters,true);
    Require(bed->Execute(),bed->Error());carved=true;
   }
   captured=executed=true;return true;
  }catch(const std::exception& ex){
   error=ex.what();if(carved)bed->Undo();
   if(linked){connection->Undo();if(!connection->Error().empty()){error+="; connection rollback refused";return false;}}
   if(made)scene.Entity_Remove(created,true);if(enabled)enable->Undo();return false;
  }
 }
 void Undo(){
  error.clear();if(!executed)return;
  connection->Undo();if(!connection->Error().empty()){error=connection->Error();return;}
  if(bed)bed->Undo();
  scene.Entity_Remove(created,true);enable->Undo();executed=false;
 }
};
CreateRiverTributaryCommand::CreateRiverTributaryCommand(wi::scene::Scene& s,std::vector<XMFLOAT3> points,wi::ecs::Entity receiver,wi::ecs::Entity control,float width,wi::ecs::Entity terrain):
 impl_(std::make_unique<Impl>(s,std::move(points),receiver,control,width,terrain)){}
CreateRiverTributaryCommand::~CreateRiverTributaryCommand()=default;
bool CreateRiverTributaryCommand::Execute(){return impl_->Execute();}
void CreateRiverTributaryCommand::Undo(){impl_->Undo();}
wi::ecs::Entity CreateRiverTributaryCommand::CreatedEntity()const{return impl_->created;}
const std::string& CreateRiverTributaryCommand::Error()const{return impl_->error;}
}
