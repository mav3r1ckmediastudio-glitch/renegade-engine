#include "RiverFoamTransport.h"
#include <iostream>
using namespace DirectX;using namespace renegade::bridge;using namespace river_g4;using namespace river_g5;
static int checks=0;static void Check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
static std::shared_ptr<const RiverFlowField> Field(bool reverse=false){
 wi::scene::Scene s;auto r=s.Entity_CreateTransform("G5 native CPU river");s.splines.Create(r);
 s.materials.Create(r).shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
 for(XMFLOAT3 p:{XMFLOAT3{0,0,0},XMFLOAT3{10,0,0}}){auto n=s.Entity_CreateTransform("Control");
  auto* t=s.transforms.GetComponent(n);t->translation_local=p;t->UpdateTransform();s.Component_Attach(n,r);
  s.splines.GetComponent(r)->spline_node_entities.push_back(n);}
 auto* sp=s.splines.GetComponent(r);sp->width=2;sp->mesh_generation_subdivision=12;sp->SetDrawAligned(true);
 RiverFlowSettings settings;settings.downstreamReversed=reverse;std::shared_ptr<const RiverFlowField> f;std::string e;
 Check(BuildRiverFlowField(s,r,settings,f,e),e.c_str());return f;
}
static Surface Box(float low=-1,float high=1){
 Surface s;XMFLOAT3 v[]={{4,low,-.5f},{6,low,-.5f},{6,low,.5f},{4,low,.5f},
 {4,high,-.5f},{6,high,-.5f},{6,high,.5f},{4,high,.5f}};
 const int ix[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
 for(int i=0;i<36;i+=3)s.triangles.push_back({v[ix[i]],v[ix[i+1]],v[ix[i+2]]});return s;
}
int main(){try{
 auto f=Field();auto mask=Build(*f,{Box()},256,128);auto t=Prepare(*f,mask);
 Check(t.rockRate>0,"Actual intersection must generate source");
 double upstream=0,downstream=0;
 for(size_t k=0;k<t.velocity.size();++k){const auto v=t.velocity[k],s=t.source[k];
  Check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(s.x),"Finite transport");
  Check(v.z<=2.20001f&&s.x>=0,"Bounded speed and positive source");
  if(mask.cells[k].x==SolidExcluded)Check(v.w==0&&s.x==0,"No solid foam source or transport");
  if(s.x>0)Check(mask.cells[k].x==RockContact,"Noise cannot create contact source");
  float x=mask.bounds.x+(k%mask.width+.5f)/mask.width*mask.bounds.z;
  if(x<4)upstream+=s.x;if(x>6)downstream+=s.x;
 }
 Check(upstream>downstream*2,"Upstream impact must dominate trailing emission");
 auto reverse=Field(true);auto rt=Prepare(*reverse,mask);double left=0,right=0;
 for(size_t k=0;k<rt.source.size();++k){float x=mask.bounds.x+(k%mask.width+.5f)/mask.width*mask.bounds.z;
  if(x<4)left+=rt.source[k].x;if(x>6)right+=rt.source[k].x;}
 Check(right>left*2,"Reverse flow swaps leading edge");
 auto zero=Prepare(*f,mask,0);Check(zero.rockRate==0&&zero.maximumSpeed==0,"Zero flow generates no impact foam");
 for(auto v:zero.source)Check(v.z==0,"Zero flow has no wake turbulence");
 for(auto box:{Box(1,2),Box(-2,-1)}){
  auto absent=Prepare(*f,Build(*f,{box},256,128));Check(absent.rockRate==0,"Above/submerged rock cannot emit surface contact");
 }
 auto removed=Prepare(*f,Build(*f,{},256,128));Check(removed.rockRate==0,"Removed rock leaves no source");
 bool refuse=false;try{Prepare(*f,mask,NAN);}catch(...){refuse=true;}Check(refuse,"Nonfinite speed refusal");
 auto wide=Prepare(*f,mask,1,.06f);Check(wide.rockRate>t.rockRate,"Source width changes real emission coverage");
 refuse=false;try{Prepare(*f,mask,1,0);}catch(...){refuse=true;}Check(refuse,"Invalid source width refusal");
 auto malformed=mask;malformed.cells[0].x=NAN;refuse=false;try{Prepare(*f,malformed);}catch(...){refuse=true;}Check(refuse,"Malformed mask refusal");
 std::cout<<"G5_CPU_TRANSPORT=PASS assertions="<<checks<<" upstreamRate="<<upstream<<" downstreamRate="<<downstream<<" maxSpeed="<<t.maximumSpeed<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"G5_CPU_FAIL: "<<e.what()<<"\n";return 1;}}
