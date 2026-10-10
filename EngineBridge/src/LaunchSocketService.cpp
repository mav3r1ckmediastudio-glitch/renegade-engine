#include "renegade/bridge/LaunchSocketService.h"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
namespace renegade::bridge {
namespace {
using json=nlohmann::json;
bool Finite(const XMFLOAT3& v,float limit) {
 return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&
  std::abs(v.x)<=limit&&std::abs(v.y)<=limit&&std::abs(v.z)<=limit;
}
std::string Path(const wi::scene::Scene& scene,wi::ecs::Entity e) {
 std::vector<std::string> names;std::set<wi::ecs::Entity> seen;
 while(e!=wi::ecs::INVALID_ENTITY && seen.insert(e).second) {
  const auto* n=scene.names.GetComponent(e);if(!n||n->name.empty())return {};
  if(n->name!="__renegade_creator_authored_transform")names.push_back(n->name);
  const auto* h=scene.hierarchy.GetComponent(e);e=h?h->parentID:wi::ecs::INVALID_ENTITY;
 }
 if(e!=wi::ecs::INVALID_ENTITY || names.empty())return {};
 std::reverse(names.begin(),names.end());return json(names).dump();
}
bool Descendant(const wi::scene::Scene& scene,wi::ecs::Entity e,wi::ecs::Entity root) {
 std::set<wi::ecs::Entity> seen;
 while(e!=wi::ecs::INVALID_ENTITY && seen.insert(e).second) {
  if(e==root)return true;
  const auto* h=scene.hierarchy.GetComponent(e);e=h?h->parentID:wi::ecs::INVALID_ENTITY;
 }
 return false;
}
XMMATRIX Local(const LaunchSocketDefinition& s) {
 return XMMatrixRotationRollPitchYaw(XMConvertToRadians(s.rotationDegrees.x),
  XMConvertToRadians(s.rotationDegrees.y),XMConvertToRadians(s.rotationDegrees.z)) *
  XMMatrixTranslation(s.position.x,s.position.y,s.position.z);
}
bool Pose(FXMMATRIX matrix,XMFLOAT3& p,XMFLOAT3& d,std::string& error) {
 XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorZero(),matrix));
 const auto forward=XMVector3TransformNormal(XMVectorSet(0,0,1,0),matrix);
 const float length=XMVectorGetX(XMVector3Length(forward));
 if(!Finite(p,1000000)||!std::isfinite(length)||length<0.000001f) {
  error="Launch socket has an invalid world transform.";return false;
 }
 XMStoreFloat3(&d,forward/length);error.clear();return true;
}
}
bool ValidateLaunchSockets(const std::vector<LaunchSocketDefinition>& sockets,std::string& error) {
 if(sockets.size()>16){error="An assembly supports at most 16 launch sockets.";return false;}
 std::set<std::string> names;
 for(const auto& s:sockets) {
  if(s.name.empty()||s.name.size()>64||s.name.find_first_of("\r\n\t")!=std::string::npos||
     !names.insert(s.name).second||unsigned(s.part)>unsigned(LaunchSocketPart::Arms)||
     !Finite(s.position,10)||!Finite(s.rotationDegrees,360)) {
   error="Socket names must be unique; positions must be within 10m and rotations within 360 degrees.";return false;
  }
  if(!s.parentPath.empty()) {
   const auto p=json::parse(s.parentPath,nullptr,false);
   if(s.parentPath.size()>8192||!p.is_array()||p.empty()||p.dump()!=s.parentPath||
    std::any_of(p.begin(),p.end(),[](const auto& n){return !n.is_string()||n.template get<std::string>().empty();})) {
    error="Socket parent path is malformed.";return false;
   }
  }
 }
 error.clear();return true;
}
bool SerializeLaunchSockets(const std::vector<LaunchSocketDefinition>& sockets,std::string& out,std::string& error) {
 if(!ValidateLaunchSockets(sockets,error))return false;
 auto values=json::array();
 for(const auto& s:sockets)values.push_back({{"name",s.name},{"part",unsigned(s.part)},
  {"parent_path",s.parentPath},{"position",{s.position.x,s.position.y,s.position.z}},
  {"rotation_degrees",{s.rotationDegrees.x,s.rotationDegrees.y,s.rotationDegrees.z}}});
 out=json{{"schema_version",1},{"sockets",values}}.dump();return true;
}
bool ParseLaunchSockets(const std::string& text,std::vector<LaunchSocketDefinition>& out,std::string& error) {
 try {
  const auto j=json::parse(text);
  if(!j.is_object()||j.size()!=2||j.at("schema_version")!=1||!j.at("sockets").is_array()||
     j.at("sockets").size()>16)throw std::runtime_error("schema");
  std::vector<LaunchSocketDefinition> result;
  for(const auto& v:j.at("sockets")) {
   if(!v.is_object()||v.size()!=5||!v.at("part").is_number_unsigned())throw std::runtime_error("socket");
   const auto part=v.at("part").get<unsigned>();if(part>2)throw std::runtime_error("part");
   LaunchSocketDefinition s;s.name=v.at("name").get<std::string>();s.part=LaunchSocketPart(part);
   s.parentPath=v.at("parent_path").get<std::string>();
   const auto vector=[&](const char* key) {
    const auto& a=v.at(key);if(!a.is_array()||a.size()!=3)throw std::runtime_error("vector");
    return XMFLOAT3(a[0].get<float>(),a[1].get<float>(),a[2].get<float>());
   };
   s.position=vector("position");s.rotationDegrees=vector("rotation_degrees");result.push_back(s);
  }
  if(!ValidateLaunchSockets(result,error))return false;
  out=std::move(result);return true;
 }catch(const std::exception&){error="Malformed launch socket settings.";return false;}
}
std::vector<PlayerViewBoneChoice> CollectLaunchSocketParents(const wi::scene::Scene& scene) {
 std::map<std::string,std::vector<wi::ecs::Entity>> paths;
 for(size_t i=0;i<scene.transforms.GetCount();++i) {
  const auto e=scene.transforms.GetEntity(i);
  if(scene.lights.Contains(e)||scene.cameras.Contains(e))continue;
  const auto* m=scene.metadatas.GetComponent(e);
  if(m&&m->string_values.has(LaunchSocketNameKey))continue;
  const auto path=Path(scene,e);if(!path.empty())paths[path].push_back(e);
 }
 std::vector<PlayerViewBoneChoice> result;
 for(const auto& [path,entities]:paths)if(entities.size()==1) {
  const auto names=json::parse(path);std::string label;
  for(const auto& name:names){if(!label.empty())label+=" > ";label+=name.get<std::string>();}
  result.push_back({path,label,entities.front()});
 }
 return result;
}
wi::ecs::Entity ResolveLaunchSocketParent(const wi::scene::Scene& scene,const std::string& path) {
 if(path.empty()) {
  wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;
  for(size_t i=0;i<scene.transforms.GetCount();++i) {
   const auto e=scene.transforms.GetEntity(i);if(scene.lights.Contains(e)||scene.cameras.Contains(e))continue;
   const auto* h=scene.hierarchy.GetComponent(e);
   if(h&&h->parentID!=wi::ecs::INVALID_ENTITY)continue;
   if(root!=wi::ecs::INVALID_ENTITY)return wi::ecs::INVALID_ENTITY;root=e;
  }
  return root;
 }
 for(const auto& p:CollectLaunchSocketParents(scene))if(p.path==path)return p.entity;
 return wi::ecs::INVALID_ENTITY;
}
bool AttachLaunchSockets(wi::scene::Scene& scene,const std::vector<LaunchSocketDefinition>& sockets,
 LaunchSocketPart part,std::string& error) {
 if(!ValidateLaunchSockets(sockets,error))return false;
 std::vector<std::pair<LaunchSocketDefinition,wi::ecs::Entity>> resolved;
 for(const auto& s:sockets)if(s.part==part) {
  const auto parent=ResolveLaunchSocketParent(scene,s.parentPath);
  if(parent==wi::ecs::INVALID_ENTITY){error="Socket '"+s.name+"' has a missing or ambiguous parent.";return false;}
  resolved.push_back({s,parent});
 }
 for(const auto& [s,parent]:resolved) {
  const auto e=scene.Entity_CreateTransform("Launch / "+s.name);
  scene.Component_Attach(e,parent,true);
  auto& t=*scene.transforms.GetComponent(e);t.ClearTransform();t.MatrixTransform(Local(s));t.UpdateTransform();
  scene.metadatas.Create(e).string_values.set(LaunchSocketNameKey,s.name);
 }
 error.clear();return true;
}
bool ReadLaunchSocketPose(const wi::scene::Scene& scene,wi::ecs::Entity root,
 const std::string& name,XMFLOAT3& p,XMFLOAT3& d,std::string& error) {
 wi::ecs::Entity match=wi::ecs::INVALID_ENTITY;
 for(size_t i=0;i<scene.metadatas.GetCount();++i) {
  const auto& m=scene.metadatas[i];const auto e=scene.metadatas.GetEntity(i);
  if(!m.string_values.has(LaunchSocketNameKey)||m.string_values.get(LaunchSocketNameKey)!=name||
     !Descendant(scene,e,root))continue;
  if(match!=wi::ecs::INVALID_ENTITY){error="Launch socket name is ambiguous.";return false;}match=e;
 }
 const auto* t=scene.transforms.GetComponent(match);
 if(!t){error="Assigned launch socket is unavailable.";return false;}
 return Pose(XMLoadFloat4x4(&t->world),p,d,error);
}
bool LaunchSocketInspectionPose(const wi::scene::Scene& scene,const LaunchSocketDefinition& s,
 XMFLOAT3& p,XMFLOAT3& d,std::string& error) {
 const auto parent=ResolveLaunchSocketParent(scene,s.parentPath);
 const auto* t=scene.transforms.GetComponent(parent);
 if(!t){error="Choose an available socket parent.";return false;}
 return Pose(Local(s)*XMLoadFloat4x4(&t->world),p,d,error);
}
bool LaunchSocketSurfacePoint(const wi::scene::Scene& scene,const wi::primitive::Ray& ray,
 const std::string& parentPath,XMFLOAT3& out,std::string& error) {
 const auto* parent=scene.transforms.GetComponent(ResolveLaunchSocketParent(scene,parentPath));
 if(!parent){error="Choose a valid socket parent first.";return false;}
 wi::vector<wi::scene::Scene::RayIntersectionResult> hits;
 scene.IntersectsAll(hits,ray,wi::enums::FILTER_OPAQUE|wi::enums::FILTER_TRANSPARENT);
 const wi::scene::Scene::RayIntersectionResult* closest=nullptr;
 for(const auto& candidate:hits) {
  const auto* m=scene.metadatas.GetComponent(candidate.entity);
  if(m&&m->string_values.has(LaunchSocketNameKey))continue;
  if(!closest||candidate.distance<closest->distance)closest=&candidate;
 }
 if(!closest){error="Click a visible surface of the model.";return false;}
 const auto& hit=*closest;
 XMVECTOR determinant;
 const auto inverse=XMMatrixInverse(&determinant,XMLoadFloat4x4(&parent->world));
 if(std::abs(XMVectorGetX(determinant))<0.0000001f){error="Socket parent transform is singular.";return false;}
 XMStoreFloat3(&out,XMVector3TransformCoord(XMLoadFloat3(&hit.position),inverse));
 if(!Finite(out,10)){error="Clicked position is outside the socket bounds.";return false;}
 error.clear();return true;
}
}
