#pragma once
// Bounded G10A engineering model. Authored native surface, prescribed inlet
// discharge, gravity-driven EXCESS kinetic head and explicit roughness damping.
// Not a free-surface solver. Bed targets are derived, and must be measured back
// from actual generated geometry before claiming a valid transport domain.
#include "renegade/bridge/RiverFlowFieldService.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace renegade::bridge::river_graded {
inline void Check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Section {float along=0,height=0,width=0,speed=0,depth=0,travel=0,grade=0;};
struct Settings {float inletSpeed=1,inletDepth=.65f,roughness=.045f,maximumGrade=.25f,maximumHeightRange=1.25f;};
class Field {
 const RiverFlowField* native_;
 std::vector<Section> sections_;
 float discharge_=0;
public:
 Field(const RiverFlowField& native,Settings settings={}):native_(&native){
  Check(!native.Samples().empty()&&settings.inletSpeed>.05f&&settings.inletSpeed<=3&&settings.inletDepth>=.15f&&settings.inletDepth<=2&&settings.roughness>=0&&settings.roughness<=.1f,"Invalid graded settings");
  const auto& samples=native.Samples();Check(samples.back().surfaceHeight<=samples.front().surfaceHeight+.01f,"Graded proof requires a net downstream descent");Check(samples.front().worldXZUnitTangent.x*(samples[1].worldPosition.x-samples[0].worldPosition.x)+samples.front().worldXZUnitTangent.y*(samples[1].worldPosition.z-samples[0].worldPosition.z)>0,"Graded proof requires authored downstream order");discharge_=samples.front().widthMeters*settings.inletSpeed*settings.inletDepth;
  const double baseK=settings.inletSpeed*settings.inletSpeed/(2*9.80665),gravity=9.80665;
  double previousK=baseK;float lowest=samples.front().surfaceHeight,highest=lowest;
  for(size_t k=0;k<samples.size();++k){
   const auto& s=samples[k];lowest=std::min(lowest,s.surfaceHeight);highest=std::max(highest,s.surfaceHeight);
   Check(highest-lowest<=settings.maximumHeightRange&&std::abs(s.grade)<=settings.maximumGrade,"Graded proof supports at most1.25m height range and25percent grade");
   Check(s.widthMeters>=.5f&&s.widthMeters<=12,"Graded channel width limit");
   double kinetic=baseK;
   if(k){
    const auto& prev=sections_.back();const double ds=s.arcLengthMeters-prev.along;Check(ds>0,"Graded arc order");
    const double drop=prev.height-s.surfaceHeight;
    auto friction=[&](double value){
     const double velocity=std::sqrt(2*gravity*value),depth=discharge_/(s.widthMeters*velocity);
     const double radius=s.widthMeters*depth/(s.widthMeters+2*depth);
     return settings.roughness*settings.roughness*velocity*velocity/std::pow(radius,4./3.);
    };
    // Baseline flow remains authored, as in flat River V2. Drag dissipates
    // only the excess head; no unsupported claim of solved natural discharge.
    double lo=baseK,hi=std::max(baseK,previousK+std::max(0.,drop));
    const double floorFriction=friction(baseK);
    for(int iteration=0;iteration<48;++iteration){
     const double mid=(lo+hi)*.5;
     const double residual=mid-previousK-drop+ds*std::max(0.,friction(mid)-floorFriction);
     if(residual>0)hi=mid;else lo=mid;
    }kinetic=std::max(baseK,(lo+hi)*.5);
   }
   const float speed=float(std::sqrt(2*gravity*kinetic)),depth=discharge_/(s.widthMeters*speed);
   Check(std::isfinite(speed)&&speed<=8&&depth>=.075f&&depth<=2,"Graded speed/depth outside bounded envelope");
   const float travel=k?sections_.back().travel+(s.arcLengthMeters-sections_.back().along)*.5f*(1/sections_.back().speed+1/speed):0;
   sections_.push_back({s.arcLengthMeters,s.surfaceHeight,s.widthMeters,speed,depth,travel,s.grade});previousK=kinetic;
  }
 }
 const auto& Sections()const{return sections_;}
 const auto& NativeRibbon()const{return native_->NativeRibbon();}
 const RiverFlowField& Native()const{return *native_;}
 float Discharge()const{return discharge_;}
 Section At(float along)const{
  auto it=std::lower_bound(sections_.begin(),sections_.end(),along,[](const Section& s,float x){return s.along<x;});
  if(it==sections_.begin())return *it;if(it==sections_.end())return sections_.back();
  const auto& a=*(it-1);const auto& b=*it;float t=std::clamp((along-a.along)/(b.along-a.along),0.f,1.f);
  auto mix=[&](float x,float y){return x+(y-x)*t;};
  Section q{along,mix(a.height,b.height),mix(a.width,b.width),mix(a.speed,b.speed),0,mix(a.travel,b.travel),mix(a.grade,b.grade)};
  q.depth=discharge_/(q.width*q.speed);return q;
 }
 RiverFlowQuery Evaluate(XMFLOAT2 p)const{
  auto q=native_->Evaluate(p);if(q.valid)q.speedMetersPerSecond=At(q.alongMeters).speed;return q;
 }
 Section Nearest(XMFLOAT2 p)const{
  auto q=native_->Evaluate(p);if(q.valid){auto s=At(q.alongMeters);s.height=q.surfaceHeight;return s;}
  const auto& samples=native_->Samples();float best=INFINITY,along=0;
  for(size_t k=1;k<samples.size();++k){
   const auto a=samples[k-1].worldPosition,b=samples[k].worldPosition;const float dx=b.x-a.x,dz=b.z-a.z;
   const float t=std::clamp(((p.x-a.x)*dx+(p.y-a.z)*dz)/(dx*dx+dz*dz),0.f,1.f);
   const float d=std::hypot(p.x-a.x-t*dx,p.y-a.z-t*dz);
   if(d<best){best=d;along=samples[k-1].arcLengthMeters+t*(samples[k].arcLengthMeters-samples[k-1].arcLengthMeters);}
  }return At(along);
 }
 float Height(XMFLOAT2 p)const{return Nearest(p).height;}
};
}
