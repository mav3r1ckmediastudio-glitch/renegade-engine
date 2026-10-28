#include "RiverWaterwaySurfaceBinding.h"
#include "RiverSurfaceMotion.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/RiverWaterwayService.h"
#include "renegade/bridge/RiverFlowRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <stdexcept>
#include <type_traits>
using namespace DirectX;
namespace renegade::bridge {
namespace {
void Require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct GeometryCache {
 uint32_t filterMaskDynamic=0,meshIndex=~0u,sortBits=0;
 XMFLOAT3 center{};float radius=0,fadeDistance=0;uint16_t lod=0;bool meshBlend=false;
 wi::primitive::AABB aabb;
 static GeometryCache Capture(const wi::scene::Scene& scene,wi::ecs::Entity e){
  const auto& o=*scene.objects.GetComponent(e);auto index=scene.objects.GetIndex(e);
  Require(index<scene.aabb_objects.size(),"Native object bounds are not settled");
  return {o.filterMaskDynamic,o.mesh_index,o.sort_bits,o.center,o.radius,o.fadeDistance,o.lod,o.mesh_blend_required,scene.aabb_objects[index]};
 }
 void Apply(wi::scene::Scene& scene,wi::ecs::Entity e)const noexcept{
  auto& o=*scene.objects.GetComponent(e);o.filterMaskDynamic=filterMaskDynamic;o.mesh_index=meshIndex;o.sort_bits=sortBits;
  o.center=center;o.radius=radius;o.fadeDistance=fadeDistance;o.lod=lod;o.mesh_blend_required=meshBlend;scene.aabb_objects[scene.objects.GetIndex(e)]=aabb;
 }
};
template<class T>void Append(std::string& text,const T& v){text.append(reinterpret_cast<const char*>(&v),sizeof(v));}
std::string Shape(const wi::scene::Scene& scene,const std::vector<wi::ecs::Entity>& members){
 std::string out;
 for(auto e:members){
  auto* s=scene.splines.GetComponent(e);const auto* material=scene.materials.GetComponent(e);const auto* object=scene.objects.GetComponent(e);
  Require(s&&material&&material->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER&&scene.meshes.Contains(e)&&object&&object->meshID==e,"Native water source components changed");
  Require(!scene.rigidbodies.Contains(e)&&!scene.softbodies.Contains(e),"Connected WATER source must not have a physics body");
  Require(!object->IsWetmapEnabled()&&object->vertex_ao.empty()&&object->lightmapTextureData.empty(),"Connected WATER does not support authored per-vertex AO or lightmap/wetmap data yet");
  auto id=PersistentEntityId(scene,e);Require(IsValidStableId(id),"Native river persistent identity missing");out+=id;
  Append(out,s->width);Append(out,s->rotation);Append(out,s->mesh_generation_subdivision);Append(out,s->mesh_generation_vertical_subdivision);
  const bool flags[]={s->IsLooped(),s->IsFilled(),s->IsDrawAligned()};Append(out,flags);
  const auto* metadata=scene.metadatas.GetComponent(e);for(const char* key:{RiverConnectionMetadataKey}){const bool present=metadata&&metadata->string_values.has(key);Append(out,present);if(present){const auto& text=metadata->string_values.get(key);Require(text.size()<=4096,"Native flow metadata exceeds binding guard budget");const auto length=uint64_t(text.size());Append(out,length);out+=text;}}
  RiverFlowProfile profile;std::string error;Require(ReadRiverFlowProfile(scene,e,profile,error),error.c_str());
  Append(out,profile.enabled);Append(out,profile.speed);Append(out,profile.reversed);
  // Animated native Z/W offsets are shader state, not a shape change.
  Append(out,material->texMulAdd.x);Append(out,material->texMulAdd.y);XMFLOAT4X4 world;XMStoreFloat4x4(&world,scene.ComputeEntityMatrixRecursive(e));Append(out,world);
  Require(s->prev_mesh_generation_nodes==int(s->spline_node_entities.size())&&s->prev_mesh_generation_subdivision==s->mesh_generation_subdivision&&s->prev_mesh_generation_vertical_subdivision==s->mesh_generation_vertical_subdivision&&s->prev_width==s->width&&s->prev_rotation==s->rotation&&s->prev_looped==s->IsLooped()&&s->prev_filled==s->IsFilled()&&s->prev_fill_normals_mode==s->fill_normals_mode,"Native spline geometry must settle before joined binding");
  for(auto node:s->spline_node_entities){const auto* t=scene.transforms.GetComponent(node);Require(t&&!t->IsDirty(),"Native spline node is unsettled");
   out+=PersistentEntityId(scene,node);Append(out,t->translation_local);Append(out,t->rotation_local);Append(out,t->scale_local);
   auto* parent=scene.hierarchy.GetComponent(node);Require(parent&&parent->parentID==e,"Native control hierarchy changed");
  }
 }
 return out;
}
}
struct RiverWaterwaySurfaceBinding::Impl {
 wi::scene::Scene* scene=nullptr;wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;std::vector<wi::ecs::Entity> members;
 wi::scene::MeshComponent mesh;std::vector<uint32_t> savedFlags;GeometryCache authored,derived;
 bool ready=false,bound=false,hasDerived=false;std::string shape,error;size_t vertices=0,indices=0;
};
RiverWaterwaySurfaceBinding::RiverWaterwaySurfaceBinding():impl_(std::make_unique<Impl>()){}
RiverWaterwaySurfaceBinding::~RiverWaterwaySurfaceBinding(){Restore();}
bool RiverWaterwaySurfaceBinding::Ready()const{return impl_->ready;}
bool RiverWaterwaySurfaceBinding::IsBound()const{return impl_->bound;}
const std::string& RiverWaterwaySurfaceBinding::Error()const{return impl_->error;}
void RiverWaterwaySurfaceBinding::Reset(){
 auto& state=*impl_;Require(!state.bound,"Cannot reset joined geometry inside a render scope");
 if(state.ready)wi::graphics::GetDevice()->WaitForGPU();impl_=std::make_unique<Impl>();
}
bool RiverWaterwaySurfaceBinding::Configure(wi::scene::Scene& scene,wi::ecs::Entity root,const std::vector<wi::ecs::Entity>& members,const RiverJunctionSurface& surface,float tile,std::string& error,bool surfaceMotion){
 try{
  Require(!impl_->bound,"Cannot configure joined geometry inside a render scope");
  Require(wi::graphics::GetDevice()!=nullptr,"Graphics device required for joined geometry");
  Require(members.size()>=2&&members.size()<=3&&std::set<wi::ecs::Entity>(members.begin(),members.end()).size()==members.size()&&std::find(members.begin(),members.end(),root)!=members.end(),"Joined geometry native source membership invalid");
  Require(std::isfinite(tile)&&tile>.01f&&tile<1000&&!surface.Positions().empty()&&!surface.Indices().empty(),"Joined surface or tiling invalid");
  auto next=std::make_unique<Impl>();next->scene=&scene;next->root=root;next->members=members;next->shape=Shape(scene,members);
  for(auto e:members)Require(scene.objects.GetComponent(e)->IsRenderable(),"Hidden native water cannot be implicitly joined");
  auto world=scene.ComputeEntityMatrixRecursive(root);XMVECTOR determinant;auto inverse=XMMatrixInverse(&determinant,world);
  Require(std::isfinite(XMVectorGetX(determinant))&&std::abs(XMVectorGetX(determinant))>1e-12f,"Joined root transform is singular");
  const auto& optics=*scene.materials.GetComponent(root);Require(std::isfinite(optics.texMulAdd.x)&&std::isfinite(optics.texMulAdd.y)&&std::abs(optics.texMulAdd.x)>1e-6f&&std::abs(optics.texMulAdd.y)>1e-6f,"Native WATER UV scale invalid");
  const auto* nativeMesh=scene.meshes.GetComponent(root);Require(nativeMesh->subsets.size()==1&&nativeMesh->subsets_per_lod==0,"Joined WATER requires one native material subset");
  auto& mesh=next->mesh;mesh.SetDoubleSided(true);mesh.geometryOffset=nativeMesh->geometryOffset;
  XMFLOAT3 normal;XMStoreFloat3(&normal,XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0,1,0,0),XMMatrixTranspose(world))));
  XMFLOAT4 tangent;XMStoreFloat4(&tangent,XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0,0,1,0),inverse)));tangent.w=1;
  for(auto p:surface.Positions()){
   XMFLOAT3 local;XMStoreFloat3(&local,XMVector3TransformCoord(XMLoadFloat3(&p),inverse));
   Require(std::isfinite(local.x)&&std::isfinite(local.y)&&std::isfinite(local.z),"Joined vertex transform invalid");
   mesh.vertex_positions.push_back(local);mesh.vertex_normals.push_back(normal);mesh.vertex_tangents.push_back(tangent);
   mesh.vertex_uvset_0.push_back({p.x/(tile*optics.texMulAdd.x),p.z/(tile*optics.texMulAdd.y)});
  }
  // Match the actual native WATER front-face convention. Wicked flips the
  // interpolated normal on backfaces, affecting Fresnel and transparency.
  auto orientation=[](XMFLOAT3 a,XMFLOAT3 b,XMFLOAT3 c){return (double(b.z)-a.z)*(c.x-a.x)-(double(b.x)-a.x)*(c.z-a.z);};
  double nativeWinding=0;for(size_t k=0;k+2<nativeMesh->indices.size()&&std::abs(nativeWinding)<1e-12;k+=3){XMFLOAT3 v[3];for(int j=0;j<3;++j)XMStoreFloat3(&v[j],XMVector3TransformCoord(XMLoadFloat3(&nativeMesh->vertex_positions[nativeMesh->indices[k+j]]),world));nativeWinding=orientation(v[0],v[1],v[2]);}
  Require(std::abs(nativeWinding)>1e-12,"Native WATER front face is degenerate");
  mesh.indices.assign(surface.Indices().begin(),surface.Indices().end());
  for(size_t k=0;k+2<mesh.indices.size();k+=3)if(orientation(surface.Positions()[mesh.indices[k]],surface.Positions()[mesh.indices[k+1]],surface.Positions()[mesh.indices[k+2]])*nativeWinding<0)std::swap(mesh.indices[k+1],mesh.indices[k+2]);auto& subset=mesh.subsets.emplace_back();subset.materialID=root;subset.indexCount=uint32_t(mesh.indices.size());
  if(surfaceMotion){
   const auto scale=std::max({XMVectorGetX(XMVector3Length(world.r[0])),XMVectorGetX(XMVector3Length(world.r[1])),XMVectorGetX(XMVector3Length(world.r[2])),.001f});
   auto refined=river_surface_motion::Refine(mesh,.35f/scale);
   mesh.vertex_positions.assign(refined.positions.begin(),refined.positions.end());
   mesh.vertex_uvset_0.assign(refined.uv.begin(),refined.uv.end());mesh.indices.assign(refined.indices.begin(),refined.indices.end());
   mesh.vertex_normals.assign(mesh.vertex_positions.size(),normal);mesh.vertex_tangents.assign(mesh.vertex_positions.size(),tangent);
   subset.indexCount=uint32_t(mesh.indices.size());
  }
  mesh.CreateRenderData();
  if(surfaceMotion){
   XMFLOAT3 expansion;XMStoreFloat3(&expansion,XMVectorAbs(XMVector3TransformNormal(XMVectorSet(0,.12f,0,0),inverse)));
   mesh.aabb._min.x-=expansion.x;mesh.aabb._max.x+=expansion.x;
   mesh.aabb._min.y-=expansion.y;mesh.aabb._max.y+=expansion.y;
   mesh.aabb._min.z-=expansion.z;mesh.aabb._max.z+=expansion.z;
  }

  next->derived=GeometryCache::Capture(scene,root);next->derived.aabb=mesh.aabb.transform(world);next->derived.center=next->derived.aabb.getCenter();next->derived.radius=next->derived.aabb.getRadius();next->derived.lod=0;next->hasDerived=true;
  next->vertices=mesh.vertex_positions.size();next->indices=mesh.indices.size();next->savedFlags.resize(members.size());next->ready=true;
  if(impl_->ready)wi::graphics::GetDevice()->WaitForGPU();impl_=std::move(next);error.clear();return true;
 }catch(const std::exception& ex){error=ex.what();impl_->error=error;return false;}
}
bool RiverWaterwaySurfaceBinding::Apply(){
 auto& state=*impl_;if(!state.ready||state.bound)return false;
 try{
  Require(Shape(*state.scene,state.members)==state.shape,"Native shape changed; rebuild joined geometry before binding");
  for(auto e:state.members)Require(state.scene->objects.GetComponent(e)->IsRenderable(),"Native water visibility changed; joined binding refused");
  state.authored=GeometryCache::Capture(*state.scene,state.root);
  for(size_t k=0;k<state.members.size();++k)state.savedFlags[k]=state.scene->objects.GetComponent(state.members[k])->_flags;
  // Constant-time ownership swap; no source copy, entity allocation or new GPU upload per frame.
  auto* nativeMesh=state.scene->meshes.GetComponent(state.root);Require(nativeMesh->subsets.size()==1&&nativeMesh->geometryOffset<state.scene->geometryArraySize,"Native geometry slot must be allocated before joined binding");
  // Wicked allocates geometry slots only at positive dt. Paused native updates
  // must retain the existing slot rather than overwrite another mesh at slot0.
  state.mesh.geometryOffset=nativeMesh->geometryOffset;
  using std::swap;swap(*nativeMesh,state.mesh);state.bound=true;
  if(state.hasDerived)state.derived.Apply(*state.scene,state.root);
  for(auto e:state.members)if(e!=state.root)state.scene->objects.GetComponent(e)->SetRenderable(false);
  state.error.clear();return true;
 }catch(const std::exception& ex){state.error=ex.what();Restore();return false;}
}
void RiverWaterwaySurfaceBinding::Restore()noexcept{
 auto& state=*impl_;if(!state.bound||!state.scene)return;
 auto* mesh=state.scene->meshes.GetComponent(state.root);
 if(mesh){
  try{state.derived=GeometryCache::Capture(*state.scene,state.root);state.hasDerived=true;}catch(...){state.ready=false;state.hasDerived=false;}
  if(mesh->vertex_positions.size()!=state.vertices||mesh->indices.size()!=state.indices){state.ready=false;state.error="Native geometry changed during joined render scope";}
  // Carry the frame allocator result back to the original native mesh as well.
  state.mesh.geometryOffset=mesh->geometryOffset;
  using std::swap;swap(*mesh,state.mesh);
 }
 auto* root=state.scene->objects.GetComponent(state.root);if(root&&state.scene->objects.GetIndex(state.root)<state.scene->aabb_objects.size())state.authored.Apply(*state.scene,state.root);
 for(size_t k=0;k<state.members.size();++k)if(auto* object=state.scene->objects.GetComponent(state.members[k]))object->_flags=state.savedFlags[k];
 state.bound=false;
}
RiverWaterwaySurfaceBinding::Scope::Scope(RiverWaterwaySurfaceBinding& owner):owner_(owner),applied_(owner_.Apply()){}
RiverWaterwaySurfaceBinding::Scope::~Scope(){if(applied_)owner_.Restore();}
}
