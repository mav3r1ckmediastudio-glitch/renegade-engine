#pragma once
#include "../EngineBridge/src/RiverGradedTransport.h"
#include "../EngineBridge/src/RiverSurfaceMotionProof.h"
#include "renegade/bridge/RiverSplineService.h"
#include <iostream>
namespace graded_proof {
using namespace renegade::bridge;using namespace DirectX;using namespace river_v2_classification;
inline std::vector<Surface> Gather(const wi::scene::Scene& scene){
 std::vector<Surface> out;
 for(size_t n=0;n<scene.objects.GetCount();++n){
  auto e=scene.objects.GetEntity(n);const auto* name=scene.names.GetComponent(e);
  if(!name||(name->name!="Graded rocky channel bed"&&name->name.rfind("Graded boulder",0)!=0))continue;
  const auto* mesh=scene.meshes.GetComponent(scene.objects[n].meshID);river_graded::Check(mesh,"Graded fixture mesh");
  const auto world=scene.ComputeEntityMatrixRecursive(e);Surface s;s.terrain=name->name=="Graded rocky channel bed";
  for(size_t k=0;k<mesh->indices.size();k+=3){Triangle t;XMFLOAT3* v[]={&t.a,&t.b,&t.c};
   for(int j=0;j<3;++j)XMStoreFloat3(v[j],XMVector3TransformCoord(XMLoadFloat3(&mesh->vertex_positions[mesh->indices[k+j]]),world));
   s.triangles.push_back(t);
  }out.push_back(std::move(s));
 }return out;
}
inline wi::ecs::Entity Create(wi::scene::Scene& scene){
 const auto sourceWater=scene.Entity_FindByName("G7B continuous native WATER");const auto* material=scene.materials.GetComponent(sourceWater);
 river_graded::Check(material,"Reference original WATER material");const auto optics=*material;
 wi::scene::MaterialComponent ground;bool gotGround=false;wi::ecs::Entity rockMesh=wi::ecs::INVALID_ENTITY;
 for(size_t n=0;n<scene.objects.GetCount();++n){const auto e=scene.objects.GetEntity(n);const auto* name=scene.names.GetComponent(e);auto* mesh=scene.meshes.GetComponent(scene.objects[n].meshID);
  if(name&&name->name=="Shallow rocky confluence bed"&&mesh&&!mesh->subsets.empty()){ground=*scene.materials.GetComponent(mesh->subsets[0].materialID);gotGround=true;}
  if(name&&name->name=="Rapid boulder 1")rockMesh=scene.objects[n].meshID;
  scene.objects[n].SetRenderable(false);
 }
 river_graded::Check(gotGround&&rockMesh!=wi::ecs::INVALID_ENTITY,"Licensed native bed/rounded boulder reference");
 const std::vector<XMFLOAT3> points={{0,0,0},{5,0,0},{10,0,-.2f},{15,-.405f,-1},{20,-.9f,-1.6f},{25,-.9f,-1.5f},{32,-.9f,-.5f},{40,-.9f,0}};
 RiverSplineOptions options;options.widthMeters=4;options.surfaceOffsetMeters=0;options.subdivisionsPerSegment=32;
 CreateRiverSplineCommand create(scene,points,options);river_graded::Check(create.Execute(),"Create true native graded spline");
 scene.Update(0);const auto river=create.CreatedEntity();scene.names.GetComponent(river)->name="G10A graded native WATER";
 *scene.materials.GetComponent(river)=optics;scene.materials.GetComponent(river)->DisableCustomShader();scene.materials.GetComponent(river)->userdata={};
 scene.objects.GetComponent(river)->SetRequestPlanarReflection(true);
 std::shared_ptr<const RiverFlowField> native;std::string error;river_graded::Check(BuildRiverFlowField(scene,river,{},native,error),error.c_str());river_graded::Field field(*native);
 const auto bed=scene.Entity_CreateObject("Graded rocky channel bed");auto& mesh=scene.meshes.Create(bed);mesh.SetDoubleSided(true);scene.objects.GetComponent(bed)->meshID=bed;
 const int nx=176,nz=48;
 for(int z=0;z<=nz;++z)for(int x=0;x<=nx;++x){float px=-2+x*.25f,pz=-7+z*.25f;
  mesh.vertex_positions.push_back({px,0,pz});mesh.vertex_uvset_0.push_back({px,pz});}
 for(int z=0;z<nz;++z)for(int x=0;x<nx;++x){uint32_t a=z*(nx+1)+x,b=a+1,c=a+nx+1,d=c+1;for(auto i:{a,c,b,b,c,d})mesh.indices.push_back(i);}
 auto& subset=mesh.subsets.emplace_back();subset.materialID=bed;subset.indexCount=uint32_t(mesh.indices.size());scene.materials.Create(bed)=ground;
 scene.materials.GetComponent(bed)->texMulAdd={.5f,.5f,0,0};
 const auto* rock=scene.meshes.GetComponent(rockMesh);XMFLOAT3 lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
 for(auto p:rock->vertex_positions){lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);}
 const XMFLOAT2 positions[]={{12,.8f},{15,-1.9f},{18,-.6f},{22,-2.5f},{28,.3f}};
 for(int k=0;k<5;++k){auto e=scene.Entity_CreateObject("Graded boulder "+std::to_string(k));scene.objects.GetComponent(e)->meshID=rockMesh;
  auto* t=scene.transforms.GetComponent(e);float scale=(.75f+.1f*(k%3))/std::max(hi.x-lo.x,hi.z-lo.z);t->scale_local={scale,scale*.8f,scale};
  const auto p=positions[k];t->translation_local={p.x-(lo.x+hi.x)*.5f*scale,field.Height(p)+.28f-hi.y*scale*.8f,p.y-(lo.z+hi.z)*.5f*scale};t->SetDirty();t->UpdateTransform();}
 return river;
}
struct Built {
 std::shared_ptr<const RiverFlowField> native;std::unique_ptr<river_graded::Field> field;
 river_rapids_transport::Domain domain;
};
inline Built Refresh(wi::scene::Scene& scene,wi::ecs::Entity river){
 Built b;std::string error;for(auto e:scene.splines.GetComponent(river)->spline_node_entities)scene.transforms.GetComponent(e)->SetDirty();scene.Update(0);
 river_graded::Check(BuildRiverFlowField(scene,river,{},b.native,error),error.c_str());b.field=std::make_unique<river_graded::Field>(*b.native);
 for(size_t k=0;k<scene.objects.GetCount();++k){const auto e=scene.objects.GetEntity(k);const auto* n=scene.names.GetComponent(e);
  if(e!=river&&(!n||(n->name!="Graded rocky channel bed"&&n->name.rfind("Graded boulder",0)!=0)))scene.objects[k].SetRenderable(false);}
 auto* water=scene.meshes.GetComponent(scene.objects.GetComponent(river)->meshID);river_graded::Check(water,"Actual Wicked generated graded ribbon");
 float heightError=0;
 for(auto p:water->vertex_positions){auto q=b.native->Evaluate({p.x,p.z});if(q.valid)heightError=std::max(heightError,std::abs(q.surfaceHeight-p.y));}
 river_graded::Check(heightError<.003f,"Graded CPU height disagrees with actual Wicked mesh");
 auto bed=scene.Entity_FindByName("Graded rocky channel bed");auto* mesh=scene.meshes.GetComponent(bed);
 for(auto& p:mesh->vertex_positions){const auto section=b.field->Nearest({p.x,p.z});auto q=b.native->Evaluate({p.x,p.z});
  float distance=INFINITY;for(const auto& row:b.native->NativeRibbon())distance=std::min(distance,std::hypot(p.x-row.centre.x,p.z-row.centre.z));
  float bank=std::max(0.f,distance-1.65f);p.y=section.height-section.depth+std::min(.65f,bank*bank*.9f)+.004f*std::sin(p.x*1.7f+p.z*.9f);
 }
 mesh->ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);mesh->CreateRenderData();
 b.domain=river_graded_transport::Build(*b.field,Gather(scene),512,256);
 std::cout<<"ACTUAL_GRADED_NATIVE heightError="<<heightError<<" Q="<<b.domain.inflow<<" divergence="<<b.domain.maximumDivergence<<" speedMax="<<b.domain.maximumSpeed<<" FrMax="<<b.domain.maximumFroude<<" emission="<<b.domain.rapidEmission<<"\n";
 return b;
}
}
