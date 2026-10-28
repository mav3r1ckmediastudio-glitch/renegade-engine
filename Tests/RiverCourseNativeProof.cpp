#include "renegade/bridge/RiverCourseService.h"
#include <Windows.h>
#include <iostream>
#include <cstring>
using namespace renegade::bridge;using namespace DirectX;
static void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main(){
 HWND window=CreateWindowExW(0,L"STATIC",L"Native course parity",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 struct App:wi::Application{void Initialize()override{wi::Application::Initialize();alwaysactive=true;}}app;
 app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 struct Drain{~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}}drain;
 try{
 for(float angle:{0.f,XM_PIDIV2,.47f}){
  wi::scene::Scene source;auto root=source.Entity_CreateTransform("Authored course");
  auto* rt=source.transforms.GetComponent(root);rt->translation_local={23,5,-18};XMStoreFloat4(&rt->rotation_local,XMQuaternionRotationAxis(XMVectorSet(0,1,0,0),angle));rt->SetDirty();rt->UpdateTransform();
  auto& spline=source.splines.Create(root);spline.width=2;spline.mesh_generation_subdivision=24;spline.SetDrawAligned(true);
  source.materials.Create(root).shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
  for(auto p:{XMFLOAT3{0,0,0},XMFLOAT3{6,0,0},XMFLOAT3{10,0,0},XMFLOAT3{10.04f,-1.1f,0},XMFLOAT3{14,-1.1f,0},XMFLOAT3{20,-1.1f,-.2f},XMFLOAT3{28,-1.2f,-.6f},XMFLOAT3{40,-1.2f,-.5f}}){
   auto n=source.Entity_CreateTransform("Authored node");source.Component_Attach(n,root);auto* t=source.transforms.GetComponent(n);t->translation_local=p;t->SetDirty();source.splines.GetComponent(root)->spline_node_entities.push_back(n);
  }
  RiverCourseSurface surface;std::string error;bool built=BuildRiverCourseSurface(source,root,1,surface,error);Check(built,error.c_str());
  wi::scene::Scene actual;auto derived=actual.Entity_CreateTransform("Native derived oracle");
  *actual.transforms.GetComponent(derived)=*source.transforms.GetComponent(root);actual.transforms.GetComponent(derived)->SetDirty();
  auto& native=actual.splines.Create(derived);native.width=source.splines.GetComponent(root)->width;native.mesh_generation_subdivision=surface.nativeSubdivisions;native.SetDrawAligned(true);
  actual.materials.Create(derived).shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
  for(const auto& control:surface.nativeControls){
   auto n=actual.Entity_CreateTransform("Derived oracle control");actual.Component_Attach(n,derived);*actual.transforms.GetComponent(n)=control;actual.transforms.GetComponent(n)->SetDirty();actual.splines.GetComponent(derived)->spline_node_entities.push_back(n);
  }
  actual.Update(0);wi::graphics::GetDevice()->WaitForGPU();
  const auto* mesh=actual.meshes.GetComponent(derived);Check(mesh,"Actual Scene::Update mesh missing");
  const auto& rows=surface.flow->NativeRibbon();Check(mesh->vertex_positions.size()==rows.size()*2,"Actual generated native row count");
  auto world=actual.ComputeEntityMatrixRecursive(derived);double maximum=0;
  for(size_t k=0;k<rows.size();++k)for(int bank=0;bank<2;++bank){
   XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMLoadFloat3(&mesh->vertex_positions[k*2+bank]),world));
   const auto expected=bank?rows[k].leftBank:rows[k].rightBank;
   maximum=std::max(maximum,double(XMVectorGetX(XMVector3Length(XMLoadFloat3(&p)-XMLoadFloat3(&expected)))));
  }
  Check(maximum<.0001,"Actual native generated bank geometry differs from flow");
  size_t negative=0,positive=0;float maximumEntrainment=0;
  for(auto p:surface.path){negative+=p.x<0;positive+=p.x>=0&&p.x<6;maximumEntrainment=std::max(maximumEntrainment,p.y);}
  Check(negative>0&&positive>0&&maximumEntrainment>.05,"Rotated toe recovery/entrainment mapping missing");
  std::cout<<"ACTUAL_NATIVE_COURSE angle="<<angle<<" rows="<<rows.size()<<" maximumBankError="<<maximum<<" upstream="<<negative<<" receiving="<<positive<<" entrainment="<<maximumEntrainment<<std::endl;
 }
 std::cout<<"COURSE_ACTUAL_NATIVE_PASS"<<std::endl;return 0;
 }catch(const std::exception& e){std::cerr<<"COURSE_ACTUAL_NATIVE_FAIL "<<e.what()<<std::endl;return 1;}
}
