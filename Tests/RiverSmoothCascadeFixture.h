#pragma once
#include "RiverCascadeFixture.h"
#include "../EngineBridge/src/RiverSmoothDrop.h"
namespace smooth_cascade_proof {
using namespace cascade_proof;
inline constexpr const char* connectedName="G10C smooth connected native WATER";
inline wi::ecs::Entity Create(wi::scene::Scene& scene){return cascade_proof::Create(scene);}
inline XMFLOAT4 Shape(wi::scene::Scene& scene,wi::ecs::Entity river){
 const auto upper=scene.Entity_FindByName("Cascade upstream native WATER");
 const auto top=scene.transforms.GetComponent(scene.splines.GetComponent(upper)->spline_node_entities.back())->translation_local;
 const auto bottom=scene.transforms.GetComponent(scene.splines.GetComponent(river)->spline_node_entities.front())->translation_local;
 const float radius=std::clamp((top.y-bottom.y)*.5f,.2f,.8f);
 return {top.x-radius,bottom.x+radius,top.y,bottom.y};
}
inline Built Base(wi::scene::Scene& scene,wi::ecs::Entity river){
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
 b.domain.inflow=b.field->Discharge();return b;
}
inline Built Refresh(wi::scene::Scene& scene,wi::ecs::Entity river){
 auto b=Base(scene,river);
 const auto upper=scene.Entity_FindByName("Cascade upstream native WATER");
 const auto& un=scene.splines.GetComponent(upper)->spline_node_entities;
 const auto& ln=scene.splines.GetComponent(river)->spline_node_entities;
 auto position=[&](wi::ecs::Entity e){return scene.transforms.GetComponent(e)->translation_local;};
 const auto top=position(un.back()),bottom=position(ln.front());
 auto curve=river_smooth_drop::Controls(top,bottom,{1,0,0});
 std::vector<XMFLOAT3> controls;
 for(size_t k=0;k+1<un.size();++k)if(position(un[k]).x<curve.front().x-.01f)controls.push_back(position(un[k]));
 controls.insert(controls.end(),curve.begin(),curve.end());
 for(size_t k=1;k<ln.size();++k)if(position(ln[k]).x>controls.back().x+.01f)controls.push_back(position(ln[k]));
 // The 2D receiving solver starts at the actual rounded toe, never beneath
 // the elevated sheet. Derived domain controls are rebuilt from creator nodes.
 const auto shape=Shape(scene,river);
 std::vector<XMFLOAT3> receiving{{shape.y,bottom.y,bottom.z}};
 for(size_t k=1;k<ln.size();++k)receiving.push_back(position(ln[k]));
 auto domainRoot=scene.Entity_FindByName("G10C receiving domain native WATER");
 if(domainRoot==wi::ecs::INVALID_ENTITY)domainRoot=cascade_proof::Reach(scene,"G10C receiving domain native WATER",receiving,*scene.materials.GetComponent(river));
 auto dn=scene.splines.GetComponent(domainRoot)->spline_node_entities;
 river_cascade::Require(dn.size()==receiving.size(),"Receiving derived support count");
 for(size_t k=0;k<dn.size();++k){auto* t=scene.transforms.GetComponent(dn[k]);t->translation_local=receiving[k];t->SetDirty();}
 scene.Update(0);std::string error;
 river_cascade::Require(BuildRiverFlowField(scene,domainRoot,{},b.native,error),error.c_str());
 b.field=std::make_unique<river_graded::Field>(*b.native);
 auto root=scene.Entity_FindByName(connectedName);
 if(root==wi::ecs::INVALID_ENTITY){
  root=scene.Entity_CreateTransform(connectedName);scene.splines.Create(root);
  scene.materials.Create(root)=*scene.materials.GetComponent(river);
  scene.materials.GetComponent(root)->DisableCustomShader();scene.materials.GetComponent(root)->userdata={};
  for(size_t k=0;k<controls.size();++k){
   auto node=scene.Entity_CreateTransform("Derived smooth river support");scene.Component_Attach(node,root);
   scene.splines.GetComponent(root)->spline_node_entities.push_back(node);
  }
 }
 auto* spline=scene.splines.GetComponent(root);
 river_cascade::Require(spline->spline_node_entities.size()==controls.size(),"Derived native support count changed unexpectedly");
 spline->width=2;spline->mesh_generation_subdivision=uint32_t(std::clamp(std::ceil(72.f*1.1f/(top.y-bottom.y)),72.f,180.f));spline->SetDrawAligned(true);spline->SetFilled(false);spline->SetLooped(false);
 for(size_t k=0;k<controls.size();++k){
  auto* t=scene.transforms.GetComponent(spline->spline_node_entities[k]);t->translation_local=controls[k];
  t->rotation_local=river_smooth_drop::Frame(controls[k?k-1:k],controls[std::min(k+1,controls.size()-1)]);t->SetDirty();
 }
 scene.Update(0);spline=scene.splines.GetComponent(root);
 auto* mesh=scene.meshes.GetComponent(scene.objects.GetComponent(root)->meshID);
 river_cascade::Require(mesh&&mesh->vertex_positions.size()%2==0,"Actual continuous native mesh absent");
 auto field=river_smooth_drop::Build(*mesh,scene.ComputeEntityMatrixRecursive(root),1,float(b.domain.inflow),false);
 float parity=0,previousX=-INFINITY,maxTurn=0;
 for(size_t k=0;k<field.rows.size();++k){
  auto m=spline->EvaluateSplineAt(float(k)/float(field.rows.size()-1));XMFLOAT3 p;XMStoreFloat3(&p,m.r[3]);
  const auto actual=field.rows[k].centre;
  parity=std::max(parity,std::hypot(std::hypot(p.x-actual.x,p.z-actual.z),p.y-actual.y));
  river_cascade::Require(actual.x>previousX,"Actual native smooth surface folded backwards");previousX=actual.x;
  if(k>1){auto a=XMLoadFloat3(&field.rows[k-2].centre),c=XMLoadFloat3(&field.rows[k-1].centre),d=XMLoadFloat3(&actual);
   const float cosine=XMVectorGetX(XMVector3Dot(XMVector3Normalize(c-a),XMVector3Normalize(d-c)));
   maxTurn=std::max(maxTurn,std::acos(std::clamp(cosine,-1.f,1.f)));}
 }
 river_cascade::Require(parity<1e-5f,"Actual generated smooth mesh/native evaluator mismatch");
 std::cout<<"SMOOTH_NATIVE_RESOLUTION maxTurn="<<maxTurn<<" rows="<<field.rows.size()<<"\n";
 river_cascade::Require(maxTurn<.08f,"Actual native smooth ribbon has a hard crease");
 float travel=0;
 for(size_t k=0;k<field.rows.size();++k){auto& row=field.rows[k];
  // Smoothly return to the accepted receiving-channel speed, without phase reset.
  const float t=std::clamp((row.centre.x-shape.y)/1.5f,0.f,1.f),w=t*t*(3-2*t);
  row.speed+=w*(b.field->Nearest({row.centre.x,row.centre.z}).speed-row.speed);
  row.thickness=field.discharge/(4*row.speed);
  const float pool=std::clamp((row.centre.x-(shape.y-.5f))/.5f,0.f,1.f),poolBlend=pool*pool*(3-2*pool);
  row.thickness+=poolBlend*(b.field->Nearest({row.centre.x,row.centre.z}).depth-row.thickness);
  if(k){auto& old=field.rows[k-1];travel+=2*(row.distance-old.distance)/(row.speed+old.speed);}
  row.travel=travel;
 }
 river_cascade::SetTravelUV(*mesh,field);river_smooth_drop::Refine(*mesh,field);
 mesh->aabb._min.y-=.12f;mesh->aabb._max.y+=.12f;
 scene.objects.GetComponent(root)->SetRequestPlanarReflection(true);scene.objects.GetComponent(root)->SetRenderable(true);
 for(auto e:{river,upper,domainRoot,scene.Entity_FindByName("Cascade falling native WATER")})scene.objects.GetComponent(e)->SetRenderable(false);
 // Disposable bed only: bury the water edges and follow the same native profile.
 auto* bed=scene.meshes.GetComponent(scene.Entity_FindByName("Graded rocky channel bed"));
 for(auto& p:bed->vertex_positions){
  auto it=std::lower_bound(field.rows.begin(),field.rows.end(),p.x,[](const auto& r,float x){return r.centre.x<x;});
  if(it==field.rows.end())--it;
  float height=it->centre.y,depth=it->thickness,centreZ=it->centre.z;
  if(it!=field.rows.begin()){auto& a=*(it-1);float t=std::clamp((p.x-a.centre.x)/(it->centre.x-a.centre.x),0.f,1.f);
   height=a.centre.y+t*(height-a.centre.y);depth=a.thickness+t*(depth-a.thickness);centreZ=a.centre.z+t*(centreZ-a.centre.z);}
  const float bank=std::max(0.f,std::abs(p.z-centreZ)-1.35f);
  p.y=height-depth+std::min(depth+.35f,bank*bank*4);
 }
 bed->ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);bed->CreateRenderData();
 b.domain=river_graded_transport::Build(*b.field,graded_proof::Gather(scene),512,256);
 field.drop=top.y-bottom.y;field.discharge=float(b.domain.inflow);
 field.impactPower=1000*field.discharge*9.80665f*field.drop;
 const auto& fall=field;
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
 std::cout<<"SMOOTH_ACTUAL_NATIVE rows="<<field.rows.size()<<" parity="<<parity<<" maxTurnRadians="<<maxTurn<<" vertices="<<mesh->vertex_positions.size()<<" sharedTravel="<<travel<<"\n";
 return b;
}
struct ArchiveMask {
 cascade_proof::ArchiveMask base;wi::scene::Scene& scene;wi::ecs::Entity root;wi::scene::MaterialComponent original;
 explicit ArchiveMask(wi::scene::Scene& s):base(s),scene(s),root(s.Entity_FindByName(connectedName)),original(*s.materials.GetComponent(root)){
  auto* m=scene.materials.GetComponent(root);m->DisableCustomShader();m->userdata={};m->SetDirty();
 }
 ~ArchiveMask(){*scene.materials.GetComponent(root)=original;scene.materials.GetComponent(root)->SetDirty();}
};
}
