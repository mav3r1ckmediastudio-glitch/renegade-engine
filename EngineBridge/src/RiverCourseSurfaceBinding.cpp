#include "RiverCourseSurfaceBinding.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/RiverFlowRenderer.h"
#include <cstring>
#include <stdexcept>
using namespace DirectX;
namespace renegade::bridge {
namespace {
void Require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Cache {
 uint32_t filter=0,mesh=~0u,sort=0;XMFLOAT3 centre{};float radius=0,fade=0;uint16_t lod=0;bool blend=false;wi::primitive::AABB bounds;
 static Cache Read(const wi::scene::Scene& s,wi::ecs::Entity e){
  const auto* o=s.objects.GetComponent(e);auto k=s.objects.GetIndex(e);Require(o&&k<s.aabb_objects.size(),"Native course object bounds are unsettled");
  return {o->filterMaskDynamic,o->mesh_index,o->sort_bits,o->center,o->radius,o->fadeDistance,o->lod,o->mesh_blend_required,s.aabb_objects[k]};
 }
 void Write(wi::scene::Scene& s,wi::ecs::Entity e)const{
  auto* o=s.objects.GetComponent(e);o->filterMaskDynamic=filter;o->mesh_index=mesh;o->sort_bits=sort;o->center=centre;o->radius=radius;o->fadeDistance=fade;o->lod=lod;o->mesh_blend_required=blend;s.aabb_objects[s.objects.GetIndex(e)]=bounds;
 }
};
template<class T>void Add(std::string& s,const T& v){s.append(reinterpret_cast<const char*>(&v),sizeof(v));}
std::string Shape(const wi::scene::Scene& scene,wi::ecs::Entity root){
 const auto* s=scene.splines.GetComponent(root);const auto* o=scene.objects.GetComponent(root);
 Require(s&&o&&o->meshID==root&&scene.meshes.Contains(root),"Native course components changed");
 Require(!scene.rigidbodies.Contains(root)&&!scene.softbodies.Contains(root)&&o->vertex_ao.empty()&&o->lightmapTextureData.empty()&&!o->IsWetmapEnabled(),"Course source has unsupported per-vertex or physics state");
 std::string result=PersistentEntityId(scene,root);Add(result,s->width);Add(result,s->rotation);Add(result,s->mesh_generation_subdivision);Add(result,s->mesh_generation_vertical_subdivision);
 XMFLOAT4X4 world;XMStoreFloat4x4(&world,scene.ComputeEntityMatrixRecursive(root));Add(result,world);
 for(auto e:s->spline_node_entities){const auto* t=scene.transforms.GetComponent(e);Require(t,"Native course control missing");Add(result,e);Add(result,t->translation_local);Add(result,t->rotation_local);Add(result,t->scale_local);}

 return result;
}
}
struct RiverCourseSurfaceBinding::Impl {
 wi::scene::Scene* scene=nullptr;wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;
 wi::scene::MeshComponent derived;Cache authored,rendered;std::string shape,error;
 bool ready=false,bound=false;size_t vertices=0,indices=0;
};
RiverCourseSurfaceBinding::RiverCourseSurfaceBinding():impl_(std::make_unique<Impl>()){}
RiverCourseSurfaceBinding::~RiverCourseSurfaceBinding(){Restore();}
bool RiverCourseSurfaceBinding::Ready()const{return impl_->ready;}
const std::string& RiverCourseSurfaceBinding::Error()const{return impl_->error;}
bool RiverCourseSurfaceBinding::Configure(wi::scene::Scene& scene,wi::ecs::Entity root,const RiverCourseSurface& source,std::string& error){
 try{
  Require(!impl_->bound,"Course configuration inside render binding");
  auto next=std::make_unique<Impl>();next->scene=&scene;next->root=root;next->shape=Shape(scene,root);
  const auto* native=scene.meshes.GetComponent(root);Require(native->subsets.size()==1&&native->subsets_per_lod==0,"Course requires one native WATER subset");
  auto world=scene.ComputeEntityMatrixRecursive(root);XMVECTOR determinant;auto inverse=XMMatrixInverse(&determinant,world);Require(std::abs(XMVectorGetX(determinant))>1e-10,"Singular course root");
  auto& mesh=next->derived;mesh.SetDoubleSided(true);mesh.geometryOffset=native->geometryOffset;
  for(auto p:source.positions){XMFLOAT3 local;XMStoreFloat3(&local,XMVector3TransformCoord(XMLoadFloat3(&p),inverse));mesh.vertex_positions.push_back(local);}
  mesh.vertex_uvset_0.assign(source.uv.begin(),source.uv.end());mesh.vertex_uvset_1.assign(source.travelUV.begin(),source.travelUV.end());mesh.indices.assign(source.indices.begin(),source.indices.end());
  auto& subset=mesh.subsets.emplace_back();subset.materialID=root;subset.indexCount=uint32_t(mesh.indices.size());
  mesh.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);mesh.CreateRenderData();
  XMFLOAT3 expansion;XMStoreFloat3(&expansion,XMVectorAbs(XMVector3TransformNormal(XMVectorSet(.12f,.12f,.12f,0),inverse)));
  mesh.aabb._min.x-=expansion.x;mesh.aabb._max.x+=expansion.x;mesh.aabb._min.y-=expansion.y;mesh.aabb._max.y+=expansion.y;mesh.aabb._min.z-=expansion.z;mesh.aabb._max.z+=expansion.z;
  next->rendered=Cache::Read(scene,root);next->rendered.bounds=mesh.aabb.transform(world);next->rendered.centre=next->rendered.bounds.getCenter();next->rendered.radius=next->rendered.bounds.getRadius();next->rendered.lod=0;
  next->vertices=mesh.vertex_positions.size();next->indices=mesh.indices.size();next->ready=true;
  if(impl_->ready)wi::graphics::GetDevice()->WaitForGPU();impl_=std::move(next);error.clear();return true;
 }catch(const std::exception& ex){error=ex.what();impl_->error=error;return false;}
}
bool RiverCourseSurfaceBinding::Apply(){
 auto& s=*impl_;if(!s.ready||s.bound)return false;
 try{Require(Shape(*s.scene,s.root)==s.shape,"Course changed after preparation; preview waits for rebuild");
  auto* native=s.scene->meshes.GetComponent(s.root);Require(native->geometryOffset<s.scene->geometryArraySize,"Native course geometry slot not allocated");
  s.authored=Cache::Read(*s.scene,s.root);s.derived.geometryOffset=native->geometryOffset;
  using std::swap;swap(*native,s.derived);s.bound=true;s.rendered.Write(*s.scene,s.root);s.error.clear();return true;
 }catch(const std::exception& ex){s.error=ex.what();Restore();return false;}
}
void RiverCourseSurfaceBinding::Restore()noexcept{
 auto& s=*impl_;if(!s.bound)return;
 try{auto* mesh=s.scene->meshes.GetComponent(s.root);if(!mesh){s.bound=false;s.ready=false;return;}
  s.rendered=Cache::Read(*s.scene,s.root);bool changed=mesh->vertex_positions.size()!=s.vertices||mesh->indices.size()!=s.indices;
  s.derived.geometryOffset=mesh->geometryOffset;using std::swap;swap(*mesh,s.derived);s.authored.Write(*s.scene,s.root);
  if(changed){s.ready=false;s.error="Native geometry changed inside course render scope";for(auto e:s.scene->splines.GetComponent(s.root)->spline_node_entities)s.scene->transforms.GetComponent(e)->SetDirty();}
 }catch(...){s.ready=false;}s.bound=false;
}
}
