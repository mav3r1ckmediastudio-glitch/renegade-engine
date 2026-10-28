#include "../EngineBridge/src/RiverGradedTransport.h"
#include "../EngineBridge/src/RiverGradedClassification.h"
#include <iostream>
#include <algorithm>
using namespace renegade::bridge;using namespace DirectX;
static void Check(bool b,const char* t){if(!b)throw std::runtime_error(t);}
static wi::ecs::Entity Make(wi::scene::Scene& scene,float drop){
 auto root=scene.Entity_CreateTransform("Graded test native spline");auto& spline=scene.splines.Create(root);
 scene.materials.Create(root).shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
 const float x[]={0,5,10,15,20,25,32,40},y[]={0,0,0,-.45f,-1,-1,-1,-1};
 for(int k=0;k<8;++k){auto e=scene.Entity_CreateTransform("Control");auto* t=scene.transforms.GetComponent(e);t->translation_local={x[k],y[k]*drop,-.04f*x[k]};t->UpdateTransform();scene.Component_Attach(e,root);scene.splines.GetComponent(root)->spline_node_entities.push_back(e);}
 auto* s=scene.splines.GetComponent(root);s->width=2;s->mesh_generation_subdivision=24;s->SetDrawAligned(true);return root;
}
int main(){try{
 wi::scene::Scene scene;auto root=Make(scene,.9f);std::shared_ptr<const RiverFlowField> native;std::string error;
 Check(BuildRiverFlowField(scene,root,{},native,error),error.c_str());river_graded::Field field(*native);
 float maximum=0;for(const auto& s:field.Sections()){Check(std::abs(s.width*s.depth*s.speed-field.Discharge())<1e-5,"Cross section discharge");maximum=std::max(maximum,s.speed);}
 Check(maximum>2.5f&&field.Sections().back().speed<maximum*.8f,"Gravity speed and recovery");
 auto q=field.Evaluate({17,-.68f});Check(q.valid&&q.speedMetersPerSecond>2,"Native query graded speed");
 auto x=field.At(q.alongMeters);Check(std::abs(x.height-q.surfaceHeight)<.003,"Native interpolated surface height");
 // Construct an actual triangle bed from native row banks and derived depth.
 river_v2_classification::Surface bed;bed.terrain=true;const auto& rows=field.NativeRibbon();
 auto vertex=[&](size_t k,bool right){auto p=right?rows[k].rightBank:rows[k].leftBank;const auto depth=field.Nearest({rows[k].centre.x,rows[k].centre.z}).depth;p.y-=depth;return p;};
 for(size_t k=1;k<rows.size();++k){auto a=vertex(k-1,false),b=vertex(k-1,true),c=vertex(k,false),d=vertex(k,true);bed.triangles.push_back({a,b,c});bed.triangles.push_back({b,d,c});}
 // Large native terrain may contain millions of triangles outside the
 // river; that should not consume the bounded graded waterline raster budget.
 river_v2_classification::Surface farGraded;farGraded.terrain=true;
 farGraded.triangles.resize(250001,
     {{12000.f,-2.f,12000.f},{12001.f,-2.f,12000.f},
        {12000.f,-2.f,12001.f}});
 auto clippedGraded=river_graded_classification::Build(field,{farGraded},32,16);
 Check(clippedGraded.triangles==0,
    "G13E7 graded classifier counted remote terrain against local raster budget");
 farGraded.triangles.front().a.x=NAN;
 bool rejectedBadFar=false;
 try{(void)river_graded_classification::Build(field,{farGraded},32,16);}
 catch(...){rejectedBadFar=true;}
 Check(rejectedBadFar,"G13E7 graded classifier accepted nonfinite remote geometry");
 farGraded.triangles.front().a.x=12000.f;
 auto farDomain=river_graded_transport::Build(field,{bed,farGraded},192,96);
 farGraded.triangles.clear();
 auto domain=river_graded_transport::Build(field,{bed},192,96);
 Check(farDomain.active==domain.active &&
       farDomain.maximumDivergence==domain.maximumDivergence,
       "G13E7 distant 250001 graded triangles changed native transport");
 Check(domain.inflow>0&&std::abs(domain.inflow-domain.outflow)<1e-7&&domain.maximumDivergence<1e-4,"Projected volumetric conservation");
 Check(domain.rapidEmission>0&&domain.maximumSpeed>2,"Grade whitewater must have conservative fast flow");
 std::vector<XMFLOAT4> zero(domain.faces.size());auto state=river_rapids_transport::Step(domain,zero,.5f/domain.maximumRate,.1f);
 for(auto s:state)Check(std::isfinite(s.x)&&s.x>=0,"Finite positive foam step");
 wi::scene::Scene flat;auto flatRoot=Make(flat,0);std::shared_ptr<const RiverFlowField> flatNative;
 Check(BuildRiverFlowField(flat,flatRoot,{},flatNative,error),error.c_str());river_graded::Field flatField(*flatNative);
 for(auto s:flatField.Sections())Check(std::abs(s.speed-1)<1e-5,"Flat authored flow preserved");
 wi::scene::Scene steep;auto steepRoot=Make(steep,2);std::shared_ptr<const RiverFlowField> bad;
 Check(BuildRiverFlowField(steep,steepRoot,{},bad,error),error.c_str());bool refused=false;try{river_graded::Field unsupported(*bad);}catch(...){refused=true;}Check(refused,"Overheight refusal");
 float lastPeak=0;
 for(float drop:{0.f,.6f,.9f,1.15f}){wi::scene::Scene s;auto e=Make(s,drop);std::shared_ptr<const RiverFlowField> n;
  Check(BuildRiverFlowField(s,e,{},n,error),error.c_str());river_graded::Field f(*n);float peak=0;
  for(auto v:f.Sections())peak=std::max(peak,v.speed);
  Check(peak>lastPeak,"Increasing native drop must increase section peak");lastPeak=peak;}
 wi::scene::Scene uphill;auto uphillRoot=Make(uphill,-.6f);std::shared_ptr<const RiverFlowField> uphillNative;
 Check(BuildRiverFlowField(uphill,uphillRoot,{},uphillNative,error),error.c_str());bool uphillRefused=false;
 try{river_graded::Field unsupported(*uphillNative);}catch(...){uphillRefused=true;}Check(uphillRefused,"Net uphill refusal");
 std::cout<<"GRADED_CPU_PASS nativeRows="<<rows.size()<<" speedMax="<<maximum<<" recovery="<<field.Sections().back().speed<<" Q="<<domain.inflow<<" divergence="<<domain.maximumDivergence<<" FrMax="<<domain.maximumFroude<<" emission="<<domain.rapidEmission<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<"GRADED_CPU_FAIL "<<e.what()<<"\n";return 1;}}
