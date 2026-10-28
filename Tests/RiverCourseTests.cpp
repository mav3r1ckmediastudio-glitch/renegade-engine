#include "renegade/bridge/RiverCourseService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "../EngineBridge/src/RiverCourseField.h"
#include <iostream>
#include <chrono>
using namespace renegade::bridge;using namespace DirectX;
static void Check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
static wi::ecs::Entity Make(wi::scene::Scene& scene,float angle=0,float grade=0){
 auto root=scene.Entity_CreateTransform("Creator river");auto* rt=scene.transforms.GetComponent(root);XMStoreFloat4(&rt->rotation_local,XMQuaternionRotationAxis(XMVectorSet(0,1,0,0),angle));rt->translation_local={23,5,-18};rt->UpdateTransform();
 auto& s=scene.splines.Create(root);s.width=2;s.mesh_generation_subdivision=24;s.SetDrawAligned(true);
 scene.materials.Create(root).shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
 const XMFLOAT3 points[]={{0,0,0},{6,0,0},{10,0,0},{10.04f,-1.1f,0},{14,-1.1f,0},{20,-1.1f,-.2f},{28,-1.2f,-.6f},{40,-1.2f,-.5f}};
 for(auto p:points){p.y+=p.x*grade;auto e=scene.Entity_CreateTransform("Creator control");auto* t=scene.transforms.GetComponent(e);scene.Component_Attach(e,root);t->translation_local=p;t->SetDirty();t->UpdateTransform();scene.splines.GetComponent(root)->spline_node_entities.push_back(e);}
 return root;
}
int main(){try{
 for(float angle:{0.f,XM_PIDIV2,.47f}){
  wi::scene::Scene scene;auto root=Make(scene,angle);RiverCourseSurface surface;std::string error;
  auto begin=std::chrono::steady_clock::now();bool built=BuildRiverCourseSurface(scene,root,1,surface,error);Check(built,error.c_str());
  Check(std::abs(surface.flow->NativeRibbon().front().centre.x-23)<.001f,"World-space root transform applied");
  Check(surface.hasCascade&&surface.positions.size()==surface.travelUV.size()&&surface.path.size()==2048,"Derived surface streams");
  const auto& native=surface.flow->NativeRibbon();
  Check(surface.positions.size()==(native.size()+2)*17,"One connected ribbon with two buried extension rows");
  for(size_t k=0;k<native.size();++k){
   const auto p=surface.positions[(k+1)*17+8];
   Check(std::hypot(p.x-native[k].centre.x,p.z-native[k].centre.z)<1e-4f&&std::abs(p.y-native[k].centre.y)<1e-4f,"Extension changed existing native course");
  }
  Check(std::abs(surface.uv[17].y)<1e-5f&&std::abs(surface.travelUV[17].y)<1e-5f,"Original inlet UV phase changed");
  Check(surface.uv.front().y<0&&surface.travelUV.front().y<0,"Buried extension UV continuity");
  const auto extension=surface.positions[8];
  Check(std::abs(std::hypot(extension.x-native.front().centre.x,extension.z-native.front().centre.z)-(surface.nativeHalfWidth+.5f))<1e-4f,"Original river was not extended into terrain");
  river_graded::Settings settings;settings.maximumGrade=8;settings.maximumHeightRange=2.5f;
  river_course::Field fast(*surface.flow,settings);
  float errorMax=0;
  for(size_t k=1;k+1<surface.flow->NativeRibbon().size();k+=11){
   auto p=surface.flow->NativeRibbon()[k].centre;auto a=surface.flow->Evaluate({p.x,p.z}),q=fast.Evaluate({p.x,p.z});Check(a.valid&&q.valid,"Actual native field validity");
   errorMax=std::max(errorMax,std::abs(a.surfaceHeight-q.surfaceHeight));
   Check(std::abs(fast.Height({p.x,p.z})-p.y)<1e-4,"Indexed height must match native geometry");
  }
  Check(errorMax<1e-4,"Indexed classifier native height parity");
  std::cout<<"COURSE_NATIVE angle="<<angle<<" rows="<<surface.flow->NativeRibbon().size()<<" heightError="<<errorMax<<" totalTravel="<<surface.totalTravel<<" buildMS="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()<<"\n";
 }
 wi::scene::Scene scene;auto root=Make(scene);auto nodes=scene.splines.GetComponent(root)->spline_node_entities;
 auto rock=scene.Entity_CreateTransform("Ordinary obstacle");
 Check(!RiverTransformNeedsTerrainTransaction(scene,rock),"Rock movement must not rebuild linked terrain");
 Check(RiverTransformNeedsTerrainTransaction(scene,root)&&RiverTransformNeedsTerrainTransaction(scene,nodes[3]),"River geometry lost linked-bed transaction");
 auto group=scene.Entity_CreateTransform("River parent");scene.Component_Attach(root,group);
 Check(RiverTransformNeedsTerrainTransaction(scene,group),"River parent transform lost linked-bed transaction");
 scene.Component_Detach(root);
 // The ordinary editor transaction, not a bespoke proof-only drop command.
 auto before=CaptureTransform(*scene.transforms.GetComponent(nodes[4]));auto after=before;after.translation.y-=.3f;
 RiverNodeMovePreview preview(scene,nodes[4],before);Check(preview.Apply(after),"Ordinary node preview");
 for(size_t k=0;k<nodes.size();++k){float expected=k>=4?-.3f:0.f;auto value=scene.transforms.GetComponent(nodes[k])->translation_local.y;const float original=k<3?0:(k<6?-1.1f:-1.2f);Check(std::abs(value-original-expected)<1e-5,"Downstream level propagation");}
 preview.Restore();Check(std::abs(scene.transforms.GetComponent(nodes.back())->translation_local.y+1.2f)<1e-5,"Preview cancellation restores downstream");
 std::cout<<"COURSE_CPU_PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"COURSE_CPU_FAIL "<<e.what()<<"\n";return 1;}}
