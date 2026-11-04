#include "../Studio/src/CreatorAssetFrameBoundary.h"
#include <Windows.h>
#include <iostream>
#include <stdexcept>
#include <memory>
static void Require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main()
{
 auto window=CreateWindowExW(0,L"STATIC",L"Creator asset frame publication proof",WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 auto* gpu=wi::graphics::GetDevice();int result=0;
 try{
  wi::scene::Scene scene;wi::physics::SetSimulationEnabled(false);
  scene.Update(1.f/60.f);
  // Exercise the production gate through the exact cancelled-release/fallback
  // ordering that created an unwanted fifth model in actual packaged Studio.
  renegade::studio::detail::CreatorAssetDragCancellation gate;
  for (int gesture=0; gesture<3; ++gesture) {
   const auto before=scene.objects.GetCount();
   gate.Cancel(true); // Escape, RMB or leaving the viewport while still held.
   Require(gate.BlocksDrop(),"Cancelled gesture admitted fallback release");
   Require(gate.SuppressFrame(false,false,true),"Cancelled pending preparation was not suppressed");
   Require(gate.BlocksDrop(),"Held cancellation latch cleared before mouse-up");
   // Intake sees the latch before boundary processing consumes the release.
   bool queued=false;
   if(!gate.BlocksDrop()) queued=true;
   Require(!queued,"Cancelled mouse-up queued a fresh placement");
   Require(gate.SuppressFrame(false,true,false),"Cancelled release reached commit branch");
   Require(!gate.BlocksDrop(),"Cancellation survived its consumed release");
   Require(scene.objects.GetCount()==before,"Cancelled gesture mutated scene");
   Require(!gate.SuppressFrame(true,false,true),"Fresh gesture did not rearm");
   if(!gate.BlocksDrop()) {
    auto ticket=renegade::studio::detail::QueueCreatorAssetFrameMutation(
     [&scene](std::uint64_t){scene.Entity_CreateCube("fresh-after-cancel");});
    wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
    Require(!ticket.IsPending(),"Fresh gesture publication remained pending");
   }
   Require(scene.objects.GetCount()==before+1,"Fresh gesture did not place exactly once");
   scene.Update(1.f/60.f);
  }
  gate.Cancel(true);
  Require(!gate.SuppressFrame(true,false,true)&&!gate.BlocksDrop(),"Fresh explicit press did not supersede cancellation");
  gate.Cancel(false);
  Require(!gate.BlocksDrop(),"Cancellation without held button latched a later gesture");
  // Negative control: post-update insertion has an unpublished frame index.
  wi::scene::Scene unprepared;auto entity=unprepared.Entity_CreateCube("unpublished");
  Require(unprepared.objects.GetComponent(entity)->mesh_index==~0u,"Negative control no longer reproduces unpublished index");
  for(int round=0;round<3;++round){
   const auto before=scene.objects.GetCount();
   auto prefab=std::make_shared<wi::scene::Scene>();prefab->Entity_CreateCube("frame-published");
   auto cancelled=renegade::studio::detail::QueueCreatorAssetFrameMutation(
    [&scene,prefab](std::uint64_t){scene.Merge(*prefab);});
   Require(scene.objects.GetCount()==before,"Publication ran before frame boundary");
   cancelled.Cancel();wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
   Require(scene.objects.GetCount()==before,"Cancelled publication mutated scene");
   auto ticket=renegade::studio::detail::QueueCreatorAssetFrameMutation(
    [&scene,prefab](std::uint64_t){scene.Merge(*prefab);});
   Require(ticket.IsPending()&&scene.objects.GetCount()==before,"Publication was not deferred");
   wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
   Require(!ticket.IsPending()&&scene.objects.GetCount()==before+1,"Boundary did not publish exactly one object");
   scene.Update(1.f/60.f);
   for(std::size_t i=0;i<scene.objects.GetCount();++i){
    const auto& object=scene.objects[i];
    Require(object.mesh_index<scene.meshes.GetCount(),"Uninitialized frame mesh index");
    Require(scene.meshes.GetEntity(object.mesh_index)==object.meshID,"Frame index resolves wrong mesh");
    const auto& mesh=scene.meshes[object.mesh_index];
    const auto lod=scene.ComputeObjectLODForView(object,scene.aabb_objects[i],mesh,DirectX::XMMatrixIdentity());
    Require(lod<mesh.GetLODCount(),"Native shadow-view LOD lookup invalid");
   }
   gpu->SubmitCommandLists();
  }
  gpu->WaitForGPU();std::cout<<"PASS deferred/repeated/cancelled publication; native mesh indices and shadow-view LOD valid\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=1;}
 while(wi::renderer::IsPipelineCreationActive())Sleep(10);gpu->WaitForGPU();DestroyWindow(window);return result;
}
