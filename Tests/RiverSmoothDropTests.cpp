#include "../EngineBridge/src/RiverSmoothDrop.h"
#include <iostream>
using namespace renegade::bridge;
using namespace DirectX;
int main(){try{
 for(float run:{0.f,.04f,.5f})for(float drop:{.5f,1.1f,1.6f}){
  auto points=river_smooth_drop::Controls({10,0,0},{10+run,-drop,0},{1,0,0});
  wi::scene::SplineComponent spline;spline.width=2;
  for(size_t k=0;k<points.size();++k){wi::scene::TransformComponent t;t.translation_local=points[k];
   t.rotation_local=river_smooth_drop::Frame(points[k?k-1:k],points[std::min(k+1,points.size()-1)]);t.UpdateTransform();spline.spline_node_transforms.push_back(t);}
  spline.PrecomputeSplineNodeDistances();float last=-INFINITY,maxTurn=0;XMVECTOR previous{},previousT{};
  for(int k=0;k<=600;++k){auto m=spline.EvaluateSplineAt(k/600.f);XMFLOAT3 p;XMStoreFloat3(&p,m.r[3]);
   river_cascade::Require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&p.x>last,"Native curve folds or nonfinite position");
   river_cascade::Require(p.y<.003f&&p.y>-drop-.003f,"Native curve vertical overshoot");
   river_cascade::Require(std::abs(XMVectorGetX(XMVector3Length(m.r[0]))-1)<1e-5f,"Native bank frame degenerates");
   if(k){auto tangent=XMVector3Normalize(m.r[3]-previous);if(k>1)maxTurn=std::max(maxTurn,std::acos(std::clamp(XMVectorGetX(XMVector3Dot(tangent,previousT)),-1.f,1.f)));previousT=tangent;}
   previous=m.r[3];last=p.x;
  }
  river_cascade::Require(maxTurn<.08f,"Native interpolation hard bend");
  std::cout<<"SMOOTH_NATIVE_EVALUATOR run="<<run<<" drop="<<drop<<" maxTurn="<<maxTurn<<"\n";
 }
 bool refused=false;try{river_smooth_drop::Controls({0,0,0},{0,-1,0},{0,0,0});}catch(...){refused=true;}
 river_cascade::Require(refused,"Degenerate heading refusal");
 std::cout<<"SMOOTH_DROP_CPU_PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"SMOOTH_DROP_CPU_FAIL "<<e.what()<<"\n";return 1;}}
