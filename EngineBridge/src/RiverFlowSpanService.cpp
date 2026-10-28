#include "renegade/bridge/RiverFlowSpanService.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace renegade::bridge {
namespace {
void Check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
XMFLOAT3 Mix(XMFLOAT3 a,XMFLOAT3 b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
float Cross(XMFLOAT3 a,XMFLOAT3 b,XMFLOAT3 c){return (b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x);}
RiverFlowRibbonRow Row(const std::vector<RiverFlowRibbonRow>& rows,float t){
 auto it=std::lower_bound(rows.begin(),rows.end(),t,[](const auto& a,float b){return a.nativeParameter<b;});
 Check(it!=rows.end(),"Span parameter exceeds native ribbon");if(it->nativeParameter==t)return *it;
 Check(it!=rows.begin(),"Span parameter precedes native ribbon");const auto& a=*(it-1);const auto& b=*it;
 const XMFLOAT3 quad[]={a.rightBank,a.leftBank,b.leftBank,b.rightBank};float sign=0;
 for(int i=0;i<4;++i){float c=Cross(quad[i],quad[(i+1)%4],quad[(i+2)%4]);
  Check(std::abs(c)>1e-10f&&(!sign||c*sign>0),"Interior span cut requires convex native quad");sign=c;}
 const float f=(t-a.nativeParameter)/(b.nativeParameter-a.nativeParameter);
 return {Mix(a.centre,b.centre,f),Mix(a.bankOffset,b.bankOffset,f),Mix(a.rightBank,b.rightBank,f),Mix(a.leftBank,b.leftBank,f),t};
}
RiverFlowSample Sample(const std::vector<RiverFlowSample>& samples,float t){
 auto it=std::lower_bound(samples.begin(),samples.end(),t,[](const auto& a,float b){return a.nativeParameter<b;});
 Check(it!=samples.end(),"Span parameter exceeds flow samples");if(it->nativeParameter==t)return *it;
 Check(it!=samples.begin(),"Span parameter precedes flow samples");const auto& a=*(it-1);const auto& b=*it;
 const float f=(t-a.nativeParameter)/(b.nativeParameter-a.nativeParameter);auto s=a;
 s.nativeParameter=t;s.worldPosition=Mix(a.worldPosition,b.worldPosition,f);
 s.worldXZUnitTangent={a.worldXZUnitTangent.x+(b.worldXZUnitTangent.x-a.worldXZUnitTangent.x)*f,
  a.worldXZUnitTangent.y+(b.worldXZUnitTangent.y-a.worldXZUnitTangent.y)*f};
 const float length=std::hypot(s.worldXZUnitTangent.x,s.worldXZUnitTangent.y);Check(length>1e-5f,"Span flow tangent is undefined");
 s.worldXZUnitTangent.x/=length;s.worldXZUnitTangent.y/=length;
 s.worldXZUnitRight={-s.worldXZUnitTangent.y,s.worldXZUnitTangent.x};
 s.widthMeters=a.widthMeters+(b.widthMeters-a.widthMeters)*f;
 s.speedMetersPerSecond=a.speedMetersPerSecond+(b.speedMetersPerSecond-a.speedMetersPerSecond)*f;
 s.surfaceHeight=s.worldPosition.y;s.grade=a.grade+(b.grade-a.grade)*f;return s;
}
}
bool BuildRiverFlowFieldSpan(const RiverFlowField& source,float first,float last,
 std::shared_ptr<const RiverFlowField>& result,std::string& error){
 try{
  Check(std::isfinite(first)&&std::isfinite(last)&&first>=source.ribbon_.front().nativeParameter&&
   last<=source.ribbon_.back().nativeParameter&&last-first>=1e-6f,"Native span requires a finite increasing interval");
  auto span=std::shared_ptr<RiverFlowField>(new RiverFlowField);span->reversed_=source.reversed_;
  span->ribbon_.push_back(Row(source.ribbon_,first));
  for(const auto& row:source.ribbon_)if(row.nativeParameter>first&&row.nativeParameter<last)span->ribbon_.push_back(row);
  span->ribbon_.push_back(Row(source.ribbon_,last));
  span->samples_.push_back(Sample(source.samples_,first));
  for(const auto& sample:source.samples_)if(sample.nativeParameter>first&&sample.nativeParameter<last)span->samples_.push_back(sample);
  span->samples_.push_back(Sample(source.samples_,last));
  for(auto pair:{std::pair<size_t,size_t>{0,0},{span->samples_.size()-1,span->ribbon_.size()-1}}){
   auto& s=span->samples_[pair.first];const auto& row=span->ribbon_[pair.second];
   s.worldPosition=row.centre;s.surfaceHeight=row.centre.y;
   s.widthMeters=std::hypot(row.rightBank.x-row.leftBank.x,row.rightBank.z-row.leftBank.z);
  }
  double arc=0;for(size_t i=0;i<span->samples_.size();++i){auto& s=span->samples_[i];
   if(i){const auto& before=span->samples_[i-1];arc+=std::hypot(s.worldPosition.x-before.worldPosition.x,s.worldPosition.z-before.worldPosition.z);}
   s.arcLengthMeters=float(arc);if(i)Check(s.arcLengthMeters>span->samples_[i-1].arcLengthMeters,"Span has degenerate projected sample");
  }
  Check(arc>1e-5&&span->samples_.size()<=8192&&span->ribbon_.size()<=8194,"Span exceeds finite geometry budget");
  result=std::move(span);error.clear();return true;
 }catch(const std::exception& ex){error=ex.what();return false;}
}
}
