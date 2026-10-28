#pragma once
#include "RiverGradedFixture.h"
#include "../EngineBridge/src/RiverCascadeFlow.h"
namespace cascade_proof {
using namespace graded_proof;
inline wi::ecs::Entity Reach(wi::scene::Scene& scene,const char* name,const std::vector<XMFLOAT3>& points,const wi::scene::MaterialComponent& optics){
 if(points.size()==2){
  auto e=scene.Entity_CreateTransform(name);auto& spline=scene.splines.Create(e);spline.width=2;spline.mesh_generation_subdivision=24;spline.mesh_generation_vertical_subdivision=0;spline.SetDrawAligned(true);spline.SetFilled(false);spline.SetLooped(false);
  for(const auto p:points){auto n=scene.Entity_CreateTransform(std::string(name)+" control");auto* t=scene.transforms.GetComponent(n);t->translation_local=p;XMStoreFloat4(&t->rotation_local,XMQuaternionRotationAxis(XMVectorSet(0,0,1,0),-XM_PIDIV4));t->SetDirty();scene.Component_Attach(n,e);scene.splines.GetComponent(e)->spline_node_entities.push_back(n);}
  auto& m=scene.materials.Create(e);m=optics;m.DisableCustomShader();m.userdata={};scene.Update(0);return e;
 }
 RiverSplineOptions o;o.widthMeters=4;o.surfaceOffsetMeters=0;o.subdivisionsPerSegment=24;
 CreateRiverSplineCommand c(scene,points,o);river_cascade::Require(c.Execute(),"Native cascade reach create");scene.Update(0);auto e=c.CreatedEntity();scene.names.GetComponent(e)->name=name;*scene.materials.GetComponent(e)=optics;scene.materials.GetComponent(e)->DisableCustomShader();scene.materials.GetComponent(e)->userdata={};return e;
}
inline wi::ecs::Entity Create(wi::scene::Scene& scene){
 auto river=graded_proof::Create(scene);scene.names.GetComponent(river)->name="G10B receiving native WATER";
 std::shared_ptr<const RiverFlowField> previous;std::string error;river_cascade::Require(BuildRiverFlowField(scene,river,{},previous,error),error.c_str());river_graded::Field old(*previous);
 auto nodes=scene.splines.GetComponent(river)->spline_node_entities;
 const float x[]={10.04f,14,18,23,28,33,38,40},y[]={-1.1f,-1.1f,-1.1f,-1.2f,-1.3f,-1.3f,-1.3f,-1.3f};
 for(size_t k=0;k<nodes.size();++k){auto* t=scene.transforms.GetComponent(nodes[k]);t->translation_local={x[k],y[k],k<2?0.f:(k<5?-.2f:0.f)};t->SetDirty();}
 scene.Update(0);std::shared_ptr<const RiverFlowField> next;river_cascade::Require(BuildRiverFlowField(scene,river,{},next,error),error.c_str());river_graded::Field now(*next);
 for(size_t k=0;k<scene.objects.GetCount();++k){auto e=scene.objects.GetEntity(k);auto* n=scene.names.GetComponent(e);if(n&&n->name.rfind("Graded boulder",0)==0){auto* t=scene.transforms.GetComponent(e);auto p=t->translation_local;t->translation_local.y+=now.Height({p.x,p.z})-old.Height({p.x,p.z});t->SetDirty();}}
 const auto optics=*scene.materials.GetComponent(river);
 Reach(scene,"Cascade upstream native WATER",{{0,.2f,0},{4,.15f,0},{8,0,0},{10,0,0}},optics);
 Reach(scene,"Cascade falling native WATER",{{10,0,0},{10.04f,-1.1f,0}},optics);
 return river;
}
inline river_cascade::Field Fall(wi::scene::Scene& scene,wi::ecs::Entity river,float discharge){
 auto upper=scene.Entity_FindByName("Cascade upstream native WATER"),fall=scene.Entity_FindByName("Cascade falling native WATER");
 river_cascade::Require(upper!=wi::ecs::INVALID_ENTITY&&fall!=wi::ecs::INVALID_ENTITY,"Cascade native reaches absent");
 const auto& upperNodes=scene.splines.GetComponent(upper)->spline_node_entities;
 const auto& lowerNodes=scene.splines.GetComponent(river)->spline_node_entities;
 const auto top=scene.transforms.GetComponent(upperNodes.back())->translation_local,bottom=scene.transforms.GetComponent(lowerNodes.front())->translation_local;
 auto* spline=scene.splines.GetComponent(fall);const auto fallNodes=spline->spline_node_entities;
 for(int k=0;k<2;++k){auto* t=scene.transforms.GetComponent(fallNodes[k]);t->translation_local=k?bottom:top;
  // N=(sqrt(.5),sqrt(.5),0) cannot be parallel to +X or -Y flow.
  XMStoreFloat4(&t->rotation_local,XMQuaternionRotationAxis(XMVectorSet(0,0,1,0),-XM_PIDIV4));t->SetDirty();}
 scene.Update(0);spline=scene.splines.GetComponent(fall);auto* mesh=scene.meshes.GetComponent(scene.objects.GetComponent(fall)->meshID);
 auto* upperMesh=scene.meshes.GetComponent(scene.objects.GetComponent(upper)->meshID);
 auto upstream=river_cascade::Build(*upperMesh,scene.ComputeEntityMatrixRecursive(upper),1.f,discharge,false);
 river_cascade::SetTravelUV(*upperMesh,upstream);upperMesh->CreateRenderData();
 auto field=river_cascade::Build(*mesh,scene.ComputeEntityMatrixRecursive(fall),upstream.rows.back().speed,discharge);
 float parity=0;for(size_t k=0;k<field.rows.size();++k){auto m=spline->EvaluateSplineAt(float(k)/float(field.rows.size()-1));XMFLOAT3 p;XMStoreFloat3(&p,m.r[3]);auto actual=field.rows[k].centre;parity=std::max(parity,std::hypot(std::hypot(p.x-actual.x,p.z-actual.z),p.y-actual.y));}
 river_cascade::Require(parity<1e-5f,"Actual native falling mesh/evaluator parity");
 river_cascade::SetTravelUV(*mesh,field);river_cascade::RefineAcross(*mesh,field);
 scene.objects.GetComponent(fall)->SetRenderable(true);scene.objects.GetComponent(upper)->SetRenderable(true);
 std::cout<<"CASCADE_NATIVE rows="<<field.rows.size()<<" height="<<field.drop<<" impactSpeed="<<field.rows.back().speed<<" Q="<<discharge<<" powerW="<<field.impactPower<<" parity="<<parity<<"\n";
 return field;
}
inline Built Refresh(wi::scene::Scene& scene,wi::ecs::Entity river){
 auto b=graded_proof::Refresh(scene,river);auto fall=Fall(scene,river,float(b.domain.inflow));
 // Fixture-owned solid bed is raised under the upstream reach, never owner terrain.
 auto* bed=scene.meshes.GetComponent(scene.Entity_FindByName("Graded rocky channel bed"));
 auto up=scene.Entity_FindByName("Cascade upstream native WATER");auto* upMesh=scene.meshes.GetComponent(scene.objects.GetComponent(up)->meshID);
 auto upstream=river_cascade::Build(*upMesh,scene.ComputeEntityMatrixRecursive(up),1.f,float(b.domain.inflow),false);
 for(auto& p:bed->vertex_positions)if(p.x<10){
  auto it=std::lower_bound(upstream.rows.begin(),upstream.rows.end(),p.x,[](const auto& row,float x){return row.centre.x<x;});if(it==upstream.rows.end())--it;
  float height=it->centre.y,depth=it->thickness;
  if(it!=upstream.rows.begin()){const auto& a=*(it-1);float t=std::clamp((p.x-a.centre.x)/(it->centre.x-a.centre.x),0.f,1.f);height=a.centre.y+t*(height-a.centre.y);depth=a.thickness+t*(depth-a.thickness);}
  float bank=std::max(0.f,std::abs(p.z)-1.65f);p.y=height-depth+std::min(depth+.35f,bank*bank*.9f);
 }
 bed->ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);bed->CreateRenderData();
 double emission=0,airFeed=0,fedWater=0;const auto& first=b.native->Samples().front();const float dx=b.domain.mask.bounds.z/b.domain.mask.width,dz=b.domain.mask.bounds.w/b.domain.mask.height;
 for(int k:b.domain.active){const float x=b.domain.mask.bounds.x+(k%b.domain.mask.width+.5f)*dx,z=b.domain.mask.bounds.y+(k/b.domain.mask.width+.5f)*dz;
  const float along=(x-first.worldPosition.x)*first.worldXZUnitTangent.x+(z-first.worldPosition.z)*first.worldXZUnitTangent.y;
  if(along<0||along>4)continue;
  const float transverse=std::abs((x-first.worldPosition.x)*first.worldXZUnitRight.x+(z-first.worldPosition.z)*first.worldXZUnitRight.y);
  const float shore=std::clamp((first.widthMeters*.5f-transverse)/.35f,0.f,1.f);
  const float rate=std::min(5.f,fall.impactPower/20000.f)*std::exp(-along/1.1f)*shore;
  b.domain.source[k].w+=rate;emission+=rate*dx*dz;
  // Incoming air carried by the falling sheet enters only through actual open
  // inflow faces. This is a foam boundary flux, not a water-volume source.
  float incoming=0;const int ix=k%b.domain.mask.width,iz=k/b.domain.mask.width;
  const int neighbour[]={ix+1<int(b.domain.mask.width)?k+1:-1,iz+1<int(b.domain.mask.height)?k+int(b.domain.mask.width):-1,ix?k-1:-1,iz?k-int(b.domain.mask.width):-1};
  for(int side=0;side<4;++side)if(neighbour[side]<0||b.domain.index[neighbour[side]]<0){incoming+=std::max(0.f,-(&b.domain.faces[k].x)[side]);fedWater+=std::max(0.f,-(&b.domain.volumeFlux[k].x)[side]);}
  const float carried=incoming*8.f*(1-std::exp(-.8f*fall.drop));
  b.domain.source[k].w+=carried/(dx*dz);emission+=carried;airFeed+=carried;
 }
 river_cascade::Require(std::abs(fedWater-b.domain.inflow)<1e-5,"Carried air must enter through the actual measured water inlet");
 b.domain.rapidEmission+=emission;std::cout<<"CASCADE_IMPACT emission="<<emission<<" fedWater="<<fedWater<<" incomingAir="<<airFeed<<" conservedQ="<<b.domain.inflow<<" divergence="<<b.domain.maximumDivergence<<"\n";
 return b;
}
}

namespace cascade_proof {
struct ArchiveMask {
 wi::scene::Scene& scene;std::vector<std::pair<wi::ecs::Entity,wi::scene::MaterialComponent>> originals;
 explicit ArchiveMask(wi::scene::Scene& s):scene(s){for(const char* name:{"Cascade upstream native WATER","Cascade falling native WATER"}){
  auto e=s.Entity_FindByName(name);auto* m=s.materials.GetComponent(e);river_cascade::Require(m,"Cascade archive material missing");originals.emplace_back(e,*m);}
  for(auto& entry:originals){auto* m=s.materials.GetComponent(entry.first);m->DisableCustomShader();m->userdata={};m->SetDirty();}
 }
 ~ArchiveMask(){for(auto& entry:originals){*scene.materials.GetComponent(entry.first)=entry.second;scene.materials.GetComponent(entry.first)->SetDirty();}}
};
}
