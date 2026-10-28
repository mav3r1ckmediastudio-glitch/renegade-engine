#pragma once
#include "RiverGradedFlow.h"
#include <numeric>
#include <array>
#include <queue>
namespace renegade::bridge::river_course {
using namespace DirectX;
class Field {
 struct Box{float x0=INFINITY,z0=INFINITY,x1=-INFINITY,z1=-INFINITY;void Add(XMFLOAT3 p){x0=std::min(x0,p.x);x1=std::max(x1,p.x);z0=std::min(z0,p.z);z1=std::max(z1,p.z);}float Distance(XMFLOAT2 p)const{float x=std::max({x0-p.x,0.f,p.x-x1}),z=std::max({z0-p.y,0.f,p.y-z1});return x*x+z*z;}bool Contains(XMFLOAT2 p)const{return p.x>=x0-1e-5f&&p.x<=x1+1e-5f&&p.y>=z0-1e-5f&&p.y<=z1+1e-5f;}};
 struct Node{Box water,centre;int left=-1,right=-1;size_t first=0,last=0;};
 const RiverFlowField& native_;river_graded::Field model_;
 std::vector<RiverFlowRibbonRow> ribbon_;size_t inletRow_=0,outletRow_=0;float inletAlong_=0,outletAlong_=INFINITY;
 std::vector<Node> nodes_;std::vector<size_t> order_;std::vector<float> along_;
 static float Project(XMFLOAT2 p,XMFLOAT3 a,XMFLOAT3 b){const float x=b.x-a.x,z=b.z-a.z;return std::clamp(((p.x-a.x)*x+(p.y-a.z)*z)/std::max(1e-12f,x*x+z*z),0.f,1.f);}
 static float Distance(XMFLOAT2 p,XMFLOAT3 a,XMFLOAT3 b,float& t){t=Project(p,a,b);float x=p.x-a.x-t*(b.x-a.x),z=p.y-a.z-t*(b.z-a.z);return x*x+z*z;}
 int Build(size_t first,size_t last){
  int id=int(nodes_.size());nodes_.push_back({});Node n;n.first=first;n.last=last;const auto& rows=native_.NativeRibbon();
  for(size_t k=first;k<last;++k)for(auto r:{order_[k],order_[k]+1}){n.centre.Add(rows[r].centre);n.water.Add(rows[r].leftBank);n.water.Add(rows[r].rightBank);}
  if(last-first>8){bool x=n.centre.x1-n.centre.x0>n.centre.z1-n.centre.z0;auto mid=first+(last-first)/2;
   std::nth_element(order_.begin()+first,order_.begin()+mid,order_.begin()+last,[&](size_t a,size_t b){return x?rows[a].centre.x<rows[b].centre.x:rows[a].centre.z<rows[b].centre.z;});
   n.left=Build(first,mid);n.right=Build(mid,last);
  }nodes_[id]=n;return id;
 }
 struct Hit{size_t row=~size_t(0);float height=0,lateral=0;bool ambiguous=false;};
 static bool Triangle(XMFLOAT2 p,XMFLOAT3 a,XMFLOAT3 b,XMFLOAT3 c,double& u,double& v,double& w){
  auto cross=[](double ax,double az,double bx,double bz){return ax*bz-az*bx;};
  double det=cross(b.x-a.x,b.z-a.z,c.x-a.x,c.z-a.z);if(std::abs(det)<1e-12)return false;
  v=cross(p.x-a.x,p.y-a.z,c.x-a.x,c.z-a.z)/det;w=cross(b.x-a.x,b.z-a.z,p.x-a.x,p.y-a.z)/det;u=1-v-w;return u>=-1e-6&&v>=-1e-6&&w>=-1e-6;
 }
 Hit Find(XMFLOAT2 p)const{
  Hit hit;std::vector<int> stack{0};const auto& rows=native_.NativeRibbon();
  while(!stack.empty()){auto id=stack.back();stack.pop_back();const auto& n=nodes_[id];if(!n.water.Contains(p))continue;
   if(n.left>=0){stack.push_back(n.left);stack.push_back(n.right);continue;}
   for(size_t k=n.first;k<n.last;++k){const auto row=order_[k];XMFLOAT3 q[]={rows[row].rightBank,rows[row].leftBank,rows[row+1].rightBank,rows[row+1].leftBank};
    for(auto tri:{std::array<int,3>{0,1,2},std::array<int,3>{2,1,3}}){double u,v,w;if(!Triangle(p,q[tri[0]],q[tri[1]],q[tri[2]],u,v,w))continue;
     if(hit.row!=~size_t(0)&&std::max(row,hit.row)-std::min(row,hit.row)>1)hit.ambiguous=true;
     if(row>=hit.row)continue;hit.row=row;hit.height=float(u*q[tri[0]].y+v*q[tri[1]].y+w*q[tri[2]].y);
     const float side[]={1,-1,1,-1};hit.lateral=float(u*side[tri[0]]+v*side[tri[1]]+w*side[tri[2]]);
    }
   }
  }return hit;
 }
 float Closest(XMFLOAT2 p,size_t* result=nullptr,float* blend=nullptr)const{
  using Item=std::pair<float,int>;std::priority_queue<Item,std::vector<Item>,std::greater<Item>> queue;queue.emplace(nodes_[0].centre.Distance(p),0);
  float best=INFINITY,tBest=0;size_t rowBest=0;const auto& rows=native_.NativeRibbon();
  while(!queue.empty()){auto [bound,id]=queue.top();queue.pop();if(bound>best)continue;const auto& n=nodes_[id];
   if(n.left>=0){queue.emplace(nodes_[n.left].centre.Distance(p),n.left);queue.emplace(nodes_[n.right].centre.Distance(p),n.right);continue;}
   for(size_t k=n.first;k<n.last;++k){float t;size_t row=order_[k];float d=Distance(p,rows[row].centre,rows[row+1].centre,t);if(d<best||(d==best&&row<rowBest)){best=d;rowBest=row;tBest=t;}}
  }
  if(result)*result=rowBest;if(blend)*blend=tBest;return along_[rowBest]+tBest*(along_[rowBest+1]-along_[rowBest]);
 }
public:
 explicit Field(const RiverFlowField& source,river_graded::Settings settings={},float capInset=0):native_(source),model_(source,settings){
  const auto& rows=source.NativeRibbon();order_.resize(rows.size()-1);std::iota(order_.begin(),order_.end(),0);Build(0,order_.size());along_.resize(rows.size());
  const auto& samples=source.Samples();
  for(size_t k=0;k<rows.size();++k){auto it=std::lower_bound(samples.begin(),samples.end(),rows[k].nativeParameter,[](const auto& s,float t){return s.nativeParameter<t;});
   if(it==samples.begin())along_[k]=it->arcLengthMeters;else if(it==samples.end())along_[k]=samples.back().arcLengthMeters;else{auto& a=*(it-1);float t=(rows[k].nativeParameter-a.nativeParameter)/(it->nativeParameter-a.nativeParameter);along_[k]=a.arcLengthMeters+t*(it->arcLengthMeters-a.arcLengthMeters);}
  }
  river_graded::Check(std::isfinite(capInset)&&capInset>=0,"Invalid concealed cap inset");
  inletRow_=size_t(std::lower_bound(along_.begin(),along_.end(),capInset)-along_.begin());
  auto last=std::upper_bound(along_.begin(),along_.end(),along_.back()-capInset);
  river_graded::Check(inletRow_<along_.size()&&last!=along_.begin(),"River is too short for concealed ends");
  outletRow_=size_t(last-along_.begin()-1);river_graded::Check(outletRow_>inletRow_+1,"River is too short for concealed ends");
  inletAlong_=along_[inletRow_];outletAlong_=along_[outletRow_];
  ribbon_.assign(rows.begin()+inletRow_,rows.begin()+outletRow_+1);
 }
 const auto& Sections()const{return model_.Sections();}const auto& NativeRibbon()const{return ribbon_;}const RiverFlowField& Native()const{return native_;}
 RiverFlowSample PortSample(bool outlet)const{
  const auto& row=native_.NativeRibbon()[outlet?outletRow_:inletRow_];const auto& samples=native_.Samples();
  auto it=std::lower_bound(samples.begin(),samples.end(),row.nativeParameter,[](const auto& q,float t){return q.nativeParameter<t;});
  river_graded::Check(it!=samples.end(),"Native internal flow port missing");return *it;
 }
 float Discharge()const{return model_.Discharge();}auto At(float along)const{return model_.At(along);}
 river_graded::Section Nearest(XMFLOAT2 p)const{auto h=Find(p);auto q=model_.At(Closest(p));if(h.row!=~size_t(0))q.height=h.height;return q;}
 float SurfaceScale(XMFLOAT2 p)const{auto q=model_.At(Closest(p));return std::sqrt(1+q.grade*q.grade);}
 float PortSpeed(const RiverFlowSample& s)const{return model_.At(s.arcLengthMeters).speed/std::sqrt(1+s.grade*s.grade);}
 float Height(XMFLOAT2 p)const{auto h=Find(p);if(h.row!=~size_t(0))return h.height;return model_.At(Closest(p)).height;}
 RiverFlowQuery Evaluate(XMFLOAT2 p)const{
  RiverFlowQuery q;if(!std::isfinite(p.x)||!std::isfinite(p.y))return q;auto hit=Find(p);if(hit.row==~size_t(0)||hit.ambiguous){q.ambiguous=hit.ambiguous;return q;}
  size_t row;float t;float along=Closest(p,&row,&t);if(along<inletAlong_-1e-5f||along>outletAlong_+1e-5f)return q;auto section=model_.At(along);const auto& rows=native_.NativeRibbon();auto a=rows[row].centre,b=rows[row+1].centre;
  float length=std::hypot(b.x-a.x,b.z-a.z);q.valid=length>1e-6;q.directionXZ={(b.x-a.x)/length,(b.z-a.z)/length};q.speedMetersPerSecond=section.speed/std::sqrt(1+section.grade*section.grade);q.surfaceHeight=hit.height;q.grade=section.grade;q.alongMeters=along;q.lateralFraction=hit.lateral;
  float bank=INFINITY,u;
  for(size_t k=hit.row?hit.row-1:0;k<std::min(hit.row+3,rows.size()-1);++k){bank=std::min(bank,Distance(p,rows[k].rightBank,rows[k+1].rightBank,u));bank=std::min(bank,Distance(p,rows[k].leftBank,rows[k+1].leftBank,u));}
  bank=std::min(bank,Distance(p,rows.front().rightBank,rows.front().leftBank,u));bank=std::min(bank,Distance(p,rows.back().rightBank,rows.back().leftBank,u));q.distanceToBankMeters=std::sqrt(bank);return q;
 }
};
}
