#include "../EngineBridge/src/RiverCascadeFlow.h"
#include <iostream>
using namespace renegade::bridge::river_cascade;using namespace DirectX;
int main(){try{
 for(float run:{0.f,.04f,.5f})for(float drop:{.5f,1.1f,1.6f}){
  wi::scene::SplineComponent s;s.width=2;s.spline_node_transforms.resize(2);
  for(int k=0;k<2;++k){auto& t=s.spline_node_transforms[k];t.translation_local={k?run:0,k?-drop:0,0};XMStoreFloat4(&t.rotation_local,XMQuaternionRotationAxis(XMVectorSet(0,0,1,0),-XM_PIDIV4));t.UpdateTransform();}
  s.PrecomputeSplineNodeDistances();wi::scene::MeshComponent mesh;
  for(int k=0;k<24;++k){auto m=s.EvaluateSplineAt(k/23.f);XMFLOAT3 a,b;XMStoreFloat3(&a,m.r[3]+m.r[0]*2);XMStoreFloat3(&b,m.r[3]-m.r[0]*2);mesh.vertex_positions.push_back(a);mesh.vertex_positions.push_back(b);}
  auto f=Build(mesh,XMMatrixTranslation(30,4,-5),1.8f,2.6f);
  Require(std::abs(f.drop-drop)<1e-5,"World height parity");Require(f.rows.back().speed>f.rows.front().speed,"Gravity acceleration");
  for(auto row:f.rows){Require(std::abs(row.thickness*4*row.speed-f.discharge)<1e-5,"Falling discharge");Require(row.velocity.y<0&&std::isfinite(row.travel),"Finite actual native vertical frame");}
  SetTravelUV(mesh,f);Require(mesh.vertex_uvset_1.size()==mesh.vertex_positions.size(),"Travel UV count");
  std::cout<<"CASCADE_CASE run="<<run<<" drop="<<drop<<" speed="<<f.rows.back().speed<<" travel="<<f.rows.back().travel<<" power="<<f.impactPower<<"\n";
 }
 wi::scene::MeshComponent bad;bool refused=false;try{Build(bad,XMMatrixIdentity());}catch(...){refused=true;}Require(refused,"Malformed mesh refusal");
 std::cout<<"CASCADE_CPU_PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"CASCADE_CPU_FAIL "<<e.what()<<"\n";return 1;}}
