#include "renegade/bridge/RiverCourseService.h"
#include "RiverSmoothDrop.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
using namespace DirectX;
namespace renegade::bridge {
namespace {
void Require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
bool Finite(XMFLOAT3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
float Length(XMFLOAT3 a,XMFLOAT3 b){return std::hypot(a.x-b.x,a.z-b.z);}
XMFLOAT3 Heading(XMFLOAT3 a,XMFLOAT3 b){float n=Length(a,b);Require(n>.001f,"A drop needs a neighbouring downstream heading");return {(b.x-a.x)/n,0,(b.z-a.z)/n};}
}
bool RiverCourseHasElevation(const wi::scene::Scene& scene,wi::ecs::Entity root) noexcept {
 try{auto* s=scene.splines.GetComponent(root);if(!s||s->spline_node_entities.size()<2)return false;
 float lo=INFINITY,hi=-INFINITY;
 for(auto e:s->spline_node_entities){XMFLOAT3 p;XMStoreFloat3(&p,scene.ComputeEntityMatrixRecursive(e).r[3]);if(!Finite(p))return true;lo=std::min(lo,p.y);hi=std::max(hi,p.y);}
 return hi-lo>.01f;
 }catch(...){return true;}
}
bool BuildRiverCourseSurface(const wi::scene::Scene& scene,wi::ecs::Entity root,float speed,RiverCourseSurface& result,std::string& error){
 try{
  const auto* source=scene.splines.GetComponent(root);const auto* material=scene.materials.GetComponent(root);
  Require(source&&material&&material->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER,"Course source is not native WATER");
  Require(!source->IsLooped()&&!source->IsFilled()&&source->spline_node_entities.size()>=2&&source->spline_node_entities.size()<=128,"Course supports 2-128 open controls");
  Require(std::isfinite(speed)&&speed>=0&&speed<=3,"Course inlet speed supports 0-3 m/s");
  const auto world=scene.ComputeEntityMatrixRecursive(root);
  const float sx=XMVectorGetX(XMVector3Length(world.r[0])),sy=XMVectorGetX(XMVector3Length(world.r[1])),sz=XMVectorGetX(XMVector3Length(world.r[2]));
  Require(std::isfinite(sx)&&sx>.001f&&std::abs(sx-sy)<1e-4f&&std::abs(sx-sz)<1e-4f&&std::abs(XMVectorGetX(world.r[1]))<1e-5f&&std::abs(XMVectorGetZ(world.r[1]))<1e-5f&&XMVectorGetY(world.r[1])>0,"Course roots require uniform scale");
  std::vector<XMFLOAT3> authored;std::vector<float> widths;
  RiverCourseSurface out;out.nativeHalfWidth=source->width*sx;
  Require(out.nativeHalfWidth>=.25f&&out.nativeHalfWidth<=6,"Elevated courses support full widths 0.5-12m");
  for(auto e:source->spline_node_entities){
   const auto* transform=scene.transforms.GetComponent(e);Require(transform,"Missing native course control");
   XMFLOAT3 p=transform->translation_local;Require(Finite(p),"Nonfinite course control");
   auto* parent=scene.hierarchy.GetComponent(e);Require(parent&&parent->parentID==root,"Course controls must belong to their native root");
   authored.push_back(p);widths.push_back(transform->scale_local.x);
   Require(std::isfinite(widths.back())&&widths.back()>.05f&&widths.back()<=4,"Invalid native course width scale");
  }
  out.minimumHeight=out.maximumHeight=authored.front().y*sx+XMVectorGetY(world.r[3]);
  for(auto p:authored){out.minimumHeight=std::min(out.minimumHeight,p.y*sx+XMVectorGetY(world.r[3]));out.maximumHeight=std::max(out.maximumHeight,p.y*sx+XMVectorGetY(world.r[3]));}
  Require(out.maximumHeight-out.minimumHeight<=2.5f,"This course supports up to 2.5m total elevation change");
  Require(authored.back().y<=authored.front().y+.02f/sx,"River must finish at or below its upstream level");
  std::vector<XMFLOAT3> points;std::vector<float> scales;std::vector<XMFLOAT3> toes;std::vector<float> toeDrops;
  auto append=[&](XMFLOAT3 p,float w){if(!points.empty()&&XMVectorGetX(XMVector3Length(XMLoadFloat3(&p)-XMLoadFloat3(&points.back())))<1e-5f)return;points.push_back(p);scales.push_back(w);};
  float minimumDrop=2.f;
  for(size_t i=0;i<authored.size();++i){
   if(i+1<authored.size()){
    const auto a=authored[i],b=authored[i+1];const float drop=a.y-b.y,run=Length(a,b);
    if(drop*sx>=.15f&&run<drop*2){
     Require(i>0&&i+2<authored.size(),"A short drop needs an upstream and downstream control");
     const auto up=Heading(authored[i-1],a),down=Heading(b,authored[i+2]);
     const float radius=std::min({std::clamp(drop*sx*.5f,.2f,.8f)/sx,Length(authored[i-1],a)*.35f,Length(b,authored[i+2])*.35f});
     Require(radius*sx>=.08f,"Move neighbouring controls farther apart around this drop");
     const XMFLOAT3 start{a.x-up.x*radius,a.y,a.z-up.z*radius},end{b.x+down.x*radius,b.y,b.z+down.z*radius};
     // Internal shoulders and cosine-spaced cubic supports provide native C1 joins.
     append({start.x-up.x*radius,start.y,start.z-up.z*radius},widths[i]);
     for(int k=0;k<=16;++k){float t=.5f-.5f*std::cos(XM_PI*k/16.f),u=1-t;
      append({u*u*u*start.x+3*u*u*t*a.x+3*u*t*t*b.x+t*t*t*end.x,
       u*u*u*start.y+3*u*u*t*a.y+3*u*t*t*b.y+t*t*t*end.y,
       u*u*u*start.z+3*u*u*t*a.z+3*u*t*t*b.z+t*t*t*end.z},widths[i]+t*(widths[i+1]-widths[i]));
     }
     append({end.x+down.x*radius,end.y,end.z+down.z*radius},widths[i+1]);toes.push_back(end);toeDrops.push_back(drop*sx);
     out.hasCascade=true;minimumDrop=std::min(minimumDrop,drop*sx);++i;continue;
    }
   }
   append(authored[i],widths[i]);
  }
  Require(points.size()>=2&&points.size()<=512,"Derived course support budget exceeded");
  wi::scene::Scene scratch;const auto river=scratch.Entity_CreateTransform("Transient native course");
  auto* rt=scratch.transforms.GetComponent(river);XMVECTOR scale,rotation,translation;
  Require(XMMatrixDecompose(&scale,&rotation,&translation,world),"Course root cannot be decomposed");
  XMStoreFloat3(&rt->scale_local,scale);XMStoreFloat4(&rt->rotation_local,rotation);XMStoreFloat3(&rt->translation_local,translation);rt->UpdateTransform();
  auto& spline=scratch.splines.Create(river);spline.width=source->width;spline.SetDrawAligned(true);
  spline.mesh_generation_subdivision=out.hasCascade?int(std::clamp(std::ceil(72.f*1.1f/minimumDrop),72.f,180.f)):std::max(12,source->mesh_generation_subdivision);
  Require((points.size()-1)*spline.mesh_generation_subdivision<=8192,"Derived native row budget exceeded");
  for(size_t k=0;k<points.size();++k){
   auto node=scratch.Entity_CreateTransform("Transient support");auto* t=scratch.transforms.GetComponent(node);scratch.Component_Attach(node,river);t->translation_local=points[k];t->scale_local.x=scales[k];
   t->rotation_local=river_smooth_drop::Frame(points[k?k-1:k],points[std::min(k+1,points.size()-1)]);t->UpdateTransform();
   out.nativeControls.push_back(*t);scratch.splines.GetComponent(river)->spline_node_entities.push_back(node);
  }
  scratch.materials.Create(river).shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
  RiverFlowSettings settings;settings.baseSpeedMetersPerSecond=std::max(speed,.05f);settings.sampleStepMeters=.35f;
  if(!BuildRiverFlowField(scratch,river,settings,out.flow,error))throw std::runtime_error(error);
  out.nativeSubdivisions=scratch.splines.GetComponent(river)->mesh_generation_subdivision;
  // Extend this SAME indexed surface past each authored endpoint into uncut
  // terrain. These rows share the terminal vertices/UVs/shader; they create no
  // additional object, water plane, cap material, or terrain excavation.
  auto rows=out.flow->NativeRibbon();
  const float extension=out.nativeHalfWidth+.5f;
  auto extend=[&](RiverFlowRibbonRow row,XMFLOAT3 direction,float sign){
   const float dx=direction.x*extension*sign,dz=direction.z*extension*sign;
   for(auto* p:{&row.centre,&row.rightBank,&row.leftBank}){p->x+=dx;p->z+=dz;}
   return row;
  };
  const auto first=rows.front(),last=rows.back();
  rows.insert(rows.begin(),extend(first,Heading(first.centre,rows[1].centre),-1));
  rows.push_back(extend(last,Heading(rows[rows.size()-2].centre,last.centre),1));
  float distance=-extension,travel=-extension/std::max(speed,.05f),previousSpeed=std::max(speed,.05f);
  std::vector<float> arc(rows.size()),velocity(rows.size());std::vector<size_t> toeRows;
  for(auto toe:toes){XMStoreFloat3(&toe,XMVector3TransformCoord(XMLoadFloat3(&toe),world));size_t nearest=0;float best=INFINITY;for(size_t k=0;k<rows.size();++k){float d=XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&toe)-XMLoadFloat3(&rows[k].centre)));if(d<best){nearest=k;best=d;}}toeRows.push_back(nearest);}
  for(size_t k=0;k<toeRows.size();++k){auto parameter=rows[toeRows[k]].nativeParameter;const auto& samples=out.flow->Samples();auto sample=std::lower_bound(samples.begin(),samples.end(),parameter,[](const auto& q,float t){return q.nativeParameter<t;});Require(sample!=samples.end(),"Native impact arc missing");out.impacts.push_back({sample->arcLengthMeters,toeDrops[k]});}
  constexpr unsigned across=16;
  for(size_t k=0;k<rows.size();++k){
   const auto& row=rows[k];if(k)distance+=XMVectorGetX(XMVector3Length(XMLoadFloat3(&row.centre)-XMLoadFloat3(&rows[k-1].centre)));arc[k]=distance;
   float drop=std::max(0.f,rows.front().centre.y-row.centre.y);
   float v=std::sqrt(std::max(speed,.05f)*std::max(speed,.05f)+2*9.80665f*drop);
   // Return smoothly to the authored speed over the receiving reach.
   for(auto index:toeRows)if(k>=index){float ds=distance-arc[index];float w=std::clamp(ds/5.f,0.f,1.f);w=w*w*(3-2*w);v+=w*(std::max(speed,.05f)-v);}
   if(k)travel+=2*(distance-arc[k-1])/(previousSpeed+v);previousSpeed=v;velocity[k]=v;
   for(unsigned j=0;j<=across;++j){float u=float(j)/across;XMFLOAT3 p;XMStoreFloat3(&p,XMVectorLerp(XMLoadFloat3(&row.rightBank),XMLoadFloat3(&row.leftBank),u));
    out.positions.push_back(p);out.uv.push_back({u,distance/std::max(.5f,out.nativeHalfWidth*2)});out.travelUV.push_back({u,travel});}
  }
  // Path lookup values are resampled uniformly in travel time, not world X.
  out.totalTravel=travel;out.path.resize(2048);
  for(size_t k=0;k<out.path.size();++k){
   const float target=travel*float(k)/float(out.path.size()-1);auto it=std::lower_bound(out.travelUV.begin(),out.travelUV.end(),target,[](XMFLOAT2 q,float t){return q.y<t;});
   size_t row=std::min(rows.size()-1,size_t(it-out.travelUV.begin())/(across+1));if(row&&out.travelUV[row*(across+1)].y>target)--row;
   const size_t next=std::min(row+1,rows.size()-1);float t0=out.travelUV[row*(across+1)].y,t1=out.travelUV[next*(across+1)].y;
   float u=t1>t0?std::clamp((target-t0)/(t1-t0),0.f,1.f):0;float along=arc[row]+u*(arc[next]-arc[row]),height=rows[row].centre.y+u*(rows[next].centre.y-rows[row].centre.y);
   float offset=1000,entrainment=0;
   for(auto index:toeRows){if(along<=arc[index]+6){offset=along-arc[index];break;}offset=along-arc[index];}
   const float ds=std::max(.00001f,Length(rows[row].centre,rows[next].centre));float grade=std::max(0.f,(rows[row].centre.y-rows[next].centre.y)/ds);
   entrainment=std::clamp((grade-.25f)/.75f,0.f,1.f)*(1-std::exp(-.8f*std::max(0.f,rows.front().centre.y-height)));
   float width=std::hypot(rows[row].rightBank.x-rows[row].leftBank.x,rows[row].rightBank.z-rows[row].leftBank.z);
   out.path[k]={offset,entrainment,width,velocity[row]+u*(velocity[next]-velocity[row])};
  }
  for(size_t k=0;k+1<rows.size();++k)for(unsigned j=0;j<across;++j){uint32_t a=uint32_t(k*(across+1)+j),c=a+across+1;for(auto n:{a,a+1,c,c,a+1,c+1})out.indices.push_back(n);}
  result=std::move(out);error.clear();return true;
 }catch(const std::exception& ex){error=ex.what();return false;}
}
}
