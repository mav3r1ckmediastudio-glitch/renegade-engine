#pragma once
#include "RiverJunctionFixture.h"
#include "renegade/bridge/RiverWaterwayService.h"
#include "renegade/bridge/RiverFlowRenderer.h"
namespace waterway_proof {
using namespace renegade::bridge;
struct Fixture {
 wi::scene::Scene scene;wi::ecs::Entity receiver=wi::ecs::INVALID_ENTITY,tributary=wi::ecs::INVALID_ENTITY,node=wi::ecs::INVALID_ENTITY;
 Fixture(bool reverse=false,bool transformed=false){
  std::vector<XMFLOAT3> receiving={{-20,0,0},{-10,0,0},{0,0,2},{10,0,3},{20,0,2}};
  std::vector<XMFLOAT3> incoming={{-14,0,-10},{-8,0,-6},{-3,0,-1},{-.2f,0,1.9f}};
  if(reverse){std::reverse(receiving.begin(),receiving.end());std::reverse(incoming.begin(),incoming.end());}
  receiver=junction_proof::Reach(scene,"Receiving native river",receiving,.35f);
  tributary=junction_proof::Reach(scene,"Native tributary",incoming,.35f);
  node=scene.splines.GetComponent(receiver)->spline_node_entities[2];
  if(transformed){auto parent=scene.Entity_CreateTransform("Transformed waterway parent");scene.Component_Attach(receiver,parent);scene.Component_Attach(tributary,parent);
   auto* t=scene.transforms.GetComponent(parent);t->translation_local={40,3,-25};t->scale_local={1.3f,1,.8f};
   XMStoreFloat4(&t->rotation_local,XMQuaternionRotationRollPitchYaw(0,.6f,0));t->SetDirty();t->UpdateTransform();}
  std::string error;junction_proof::Check(EnsurePersistentEntityIdentities(scene,error),error);
  RiverFlowProfile p;p.enabled=true;p.reversed=reverse;
  SetRiverFlowProfileCommand a(scene,receiver,p),b(scene,tributary,p);junction_proof::Check(a.Execute()&&b.Execute(),"Fixture flow profile");
 }
};
inline XMFLOAT3 Endpoint(const wi::scene::Scene& scene,wi::ecs::Entity river){
 RiverFlowProfile p;std::string error;junction_proof::Check(ReadRiverFlowProfile(scene,river,p,error),error);
 const auto* s=scene.splines.GetComponent(river);auto id=p.reversed?s->spline_node_entities.front():s->spline_node_entities.back();
 XMFLOAT3 out;XMStoreFloat3(&out,scene.ComputeEntityMatrixRecursive(id).r[3]);return out;
}
inline float Distance(XMFLOAT3 a,XMFLOAT3 b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
}
