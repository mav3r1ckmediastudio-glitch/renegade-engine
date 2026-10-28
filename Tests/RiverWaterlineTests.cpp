#include "RiverWaterlineClassifier.h"
#include "../EngineBridge/src/RiverFlowWaterline.h"
#include "../EngineBridge/src/RiverRapidsTransport.h"
#include <iostream>
#include <cstring>
using namespace DirectX;using namespace renegade::bridge;using namespace river_g4;
static int checks=0;
static void Check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
static Surface Box(float low=-1,float high=1,XMMATRIX world=XMMatrixIdentity()){
    Surface s;XMFLOAT3 v[]={{4,low,-.5f},{6,low,-.5f},{6,low,.5f},{4,low,.5f},
        {4,high,-.5f},{6,high,-.5f},{6,high,.5f},{4,high,.5f}};
    for(auto& p:v)XMStoreFloat3(&p,XMVector3TransformCoord(XMLoadFloat3(&p),world));
    const int ids[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
    for(int i=0;i<36;i+=3)s.triangles.push_back({v[ids[i]],v[ids[i+1]],v[ids[i+2]]});
    return s;
}
static std::shared_ptr<const RiverFlowField> Field(bool reverse=false,bool slope=false){
    wi::scene::Scene s;auto r=s.Entity_CreateTransform("G4 native CPU river");
    s.splines.Create(r);s.materials.Create(r).shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
    for(XMFLOAT3 p:{XMFLOAT3{0,0,0},XMFLOAT3{10,slope?1.f:0.f,0}}){
        auto n=s.Entity_CreateTransform("Control");auto* t=s.transforms.GetComponent(n);
        t->translation_local=p;t->UpdateTransform();s.Component_Attach(n,r);s.splines.GetComponent(r)->spline_node_entities.push_back(n);
    }
    auto* spline=s.splines.GetComponent(r);spline->width=2;spline->mesh_generation_subdivision=12;spline->SetDrawAligned(true);
    RiverFlowSettings settings;settings.downstreamReversed=reverse;
    std::shared_ptr<const RiverFlowField> f;std::string e;
    Check(BuildRiverFlowField(s,r,settings,f,e),e.c_str());return f;
}
int main(){
 try{
    auto f=Field();auto b=Box();auto m=Build(*f,{b},256,128);
    Check(m.counts[RockContact]>0&&m.counts[SolidExcluded]>0,"Crossing box requires true waterline and occluded footprint");
    Check(m.counts[Open]>100,"Open water remains");
    Check(m.rockLines.size()==8,"Box slice contains eight triangle-derived segments");
    for(auto line:m.rockLines)for(auto p:{line.a,line.b})
        Check(std::min({std::abs(p.x-4),std::abs(p.x-6),std::abs(p.y-.5f),std::abs(p.y+.5f)})<1e-5f,"Actual sliced box edge geometry");
    for(const auto& c:m.cells)if(c.x==RockContact)Check(c.z<=.120001f&&c.w==1,"Every contact pixel within geometric band and WATER");
    auto above=Build(*f,{Box(1,2)},256,128),under=Build(*f,{Box(-2,-1)},256,128),removed=Build(*f,{},256,128);
    Check(above.rockLines.empty()&&above.counts[RockContact]==0&&above.counts[AboveExcluded]>0,"Above-water rock emits no waterline");
    Check(under.rockLines.empty()&&under.counts[RockContact]==0&&under.counts[Submerged]>0,"Submerged rock emits no false contact");
    Check(removed.rockLines.empty()&&removed.counts[RockContact]==0&&removed.counts[SolidExcluded]==0,"Removed obstacle leaves no stale mask");
    auto shifted=Build(*f,{Box(-1,1,XMMatrixTranslation(2,0,0))},256,128);
    auto centre=[](const Mask& x){double sum=0;for(auto s:x.rockLines)sum+=(s.a.x+s.b.x)*.5;return sum/x.rockLines.size();};
    Check(std::abs(centre(shifted)-centre(m)-2)<1e-5,"Moved rock waterline follows actual world mesh");
    auto rotated=Build(*f,{Box(-1,1,XMMatrixTranslation(-5,0,0)*XMMatrixScaling(.7f,1.3f,1.8f)*XMMatrixRotationRollPitchYaw(.2f,.7f,.15f)*XMMatrixTranslation(5,0,0))},256,128);
    Check(rotated.rockLines.size()>0&&rotated.counts[RockContact]>0,"Rotation scale tilt retain actual triangle intersection");
    Surface terrain;terrain.terrain=true;terrain.triangles={{{0,-2,-2},{10,-2,-2},{10,2,2}},{{0,-2,-2},{10,2,2},{0,2,2}}};
    auto bank=Build(*f,{terrain},256,128);
    Check(bank.counts[Bank]>0&&bank.bankLines.size()==2,"Terrain bank uses real triangle height and intersection");
    for(auto line:bank.bankLines)Check(std::abs(line.a.y)<1e-6f&&std::abs(line.b.y)<1e-6f,"Terrain waterline exactly z0");
    river_v2_classification::Surface distant;distant.terrain=true;
    distant.triangles.resize(250001,
        {{12000.f,-2.f,12000.f},{12001.f,-2.f,12000.f},
          {12000.f,-2.f,12001.f}});
    auto farMask=river_v2_classification::Build(*f,{distant},32,16);
    Check(farMask.triangles==0,
        "G13E7 flat classifier counted offscreen terrain against the raster cap");
    river_v2_classification::Surface bedFlat;bedFlat.terrain=true;
    bedFlat.triangles={{{0.f,-1.f,-2.f},{10.f,-1.f,-2.f},{10.f,-1.f,2.f}},
                       {{0.f,-1.f,-2.f},{10.f,-1.f,2.f},{0.f,-1.f,2.f}}};
    const auto flatMask=river_v2_classification::Build(*f,{bedFlat},32,16);
    const auto depthA=river_rapids_transport::MeasureDepth(flatMask,{bedFlat},0.f);
    const auto depthB=river_rapids_transport::MeasureDepth(flatMask,{bedFlat,distant},0.f);
    Check(depthA==depthB,
        "G13E7 distant terrain triangles altered flat rapid bed transport");
    distant.triangles[0].a.x=NAN;
    bool badFar=false;
    try{(void)river_v2_classification::Build(*f,{distant},32,16);}
    catch(...){badFar=true;}
    Check(badFar,"G13E7 flat classifier allowed a nonfinite far triangle");
    auto reverse=Field(true);auto rm=Build(*reverse,{b},256,128);
    Check(m.cells.size()==rm.cells.size()&&std::memcmp(m.cells.data(),rm.cells.data(),m.cells.size()*sizeof(XMFLOAT4))==0,"Reversing flow cannot move physical waterline");
    auto repeat=Build(*f,{b},256,128);
    Check(std::memcmp(m.cells.data(),repeat.cells.data(),m.cells.size()*sizeof(XMFLOAT4))==0,"Deterministic geometry mask");
    bool refused=false;try{Build(*f,{b},4096,128);}catch(const std::exception&){refused=true;}Check(refused,"Raster budget refusal");
    b.triangles[0].a.x=NAN;refused=false;try{Build(*f,{b},256,128);}catch(const std::exception&){refused=true;}Check(refused,"Nonfinite geometry refusal");
    refused=false;try{auto sloped=Field(false,true);Build(*sloped,{},256,128);}catch(const std::exception&){refused=true;}Check(refused,"Unsupported sloped water refuses flat classifier");
    Check(Slice({{0,0,0},{1,0,0},{0,0,1}},0).empty(),"Coplanar face cannot invent unique waterline");
    std::cout<<"G4_CPU_CLASSIFICATION=PASS assertions="<<checks<<" contactPixels="<<m.counts[RockContact]<<" bankPixels="<<bank.counts[Bank]<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"G4_CPU_FAIL: "<<e.what()<<"\n";return 1;}
}
