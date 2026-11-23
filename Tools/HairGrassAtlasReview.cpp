// Local visual comparison author; existing native hair and WISCENE formats.
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/GrassPalette.h"
#include <ModelImporter.h>
#include <random>
#include <set>
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace DirectX;
namespace fs=std::filesystem;
using wi::ecs::Entity;
using wi::scene::Scene;
using wi::scene::MaterialComponent;
static void Check(bool value,const char*message){if(!value)throw std::runtime_error(message);}
static void Describe(std::ostream&out,const char*name,const wi::HairParticleSystem&h){
 out<<name<<" length="<<h.length<<" width="<<h.width<<" segments="<<h.segmentCount<<" cards="<<h.billboardCount<<" randomness="<<h.randomness<<" sprites="<<h.atlas_rects.size()<<"\n";
 for(size_t i=0;i<h.atlas_rects.size();++i){const auto&r=h.atlas_rects[i];out<<" frame="<<i<<" size="<<r.size<<" uv="<<r.texMulAdd.x<<","<<r.texMulAdd.y<<","<<r.texMulAdd.z<<","<<r.texMulAdd.w<<"\n";}
}
static Entity Emitter(Scene&s,float x){
 Entity e=wi::ecs::CreateEntity();s.names.Create(e)="Comparison emitter";s.transforms.Create(e);
 auto&m=s.meshes.Create(e);m.vertex_positions={{x-3,.025f,2},{x-3,.025f,14},{x+3,.025f,2},{x+3,.025f,14}};
 m.vertex_normals.assign(4,{0,1,0});m.vertex_uvset_0={{0,0},{0,1},{1,0},{1,1}};m.indices={0,1,2,2,1,3};auto&subset=m.subsets.emplace_back();subset.indexOffset=0;subset.indexCount=6;m.CreateRenderData();return e;
}
static void Plot(Scene&s,float x,const char*name,const wi::HairParticleSystem&source,const MaterialComponent&sourceMat){
 Entity emitter=Emitter(s,x);Entity e=wi::ecs::CreateEntity();s.names.Create(e)=name;s.transforms.Create(e);
 auto&h=s.hairs.Create(e);h=source;h.DeleteRenderData();h.meshID=emitter;h.strandCount=900;
 h.vertex_lengths.assign(4,1.f);h.indices.clear();h.randomSeed=17;h.viewDistance=100.f;h.SetDirty();
 auto&m=s.materials.Create(e);m=sourceMat;m.SetDirty();m.CreateRenderData();
}
static void Sign(Scene&s,float x,const fs::path&png,float z=.5f){
 Entity mat=wi::ecs::CreateEntity();auto&m=s.materials.Create(mat);m.shaderType=MaterialComponent::SHADERTYPE_UNLIT;m.baseColor={1,1,1,1};m.SetDoubleSided(true);
 m.textures[MaterialComponent::BASECOLORMAP].name=png.generic_string();m.textures[MaterialComponent::BASECOLORMAP].resource=wi::resourcemanager::Load(png.generic_string());m.CreateRenderData();
 Entity mesh=wi::ecs::CreateEntity();auto&g=s.meshes.Create(mesh);
 g.vertex_positions={{x-2.8f,2.45f,z},{x-2.8f,3.05f,z},{x+2.8f,2.45f,z},{x+2.8f,3.05f,z}};
 g.vertex_normals.assign(4,{0,0,-1});g.vertex_uvset_0={{0,1},{0,0},{1,1},{1,0}};g.indices={0,1,2,2,1,3};
 auto&sub=g.subsets.emplace_back();sub.materialID=mat;sub.indexOffset=0;sub.indexCount=6;g.CreateRenderData();
 Entity obj=wi::ecs::CreateEntity();s.names.Create(obj)=png.stem().string();s.transforms.Create(obj);s.objects.Create(obj).meshID=mesh;
}


int main(int argc,char**argv){try{
 Check(argc==5||argc==6||argc==7,"usage: HairGrassAtlasReview source.wiscene destination.wiscene stock-terrain-directory atlas.png [--dense | --coverage previous-atlas.png]");
 const bool coverage=argc==7;const bool dense=argc==6||coverage;
 Check(!dense||std::string(argv[5])==(coverage?"--coverage":"--dense"),"Unknown review option");
 auto strands=[dense,coverage](size_t i){return dense?((coverage?(i==1||i==3):(i==2))?3600u:1800u):300u;};
 auto plotX=[dense,coverage](size_t i){return coverage?(-12.f+8.f*i):dense?(-8.f+8.f*i):(-4.f+8.f*i);};
 HWND window=CreateWindowExW(0,L"STATIC",L"Native hair grass review author",WS_POPUP,0,0,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
 Scene scene;wi::scene::LoadModel(scene,argv[1]);Check(scene.terrains.GetCount()>0,"Missing source terrain");
 const fs::path dest=fs::u8path(argv[2]),stockRoot=fs::u8path(argv[3]),atlas=fs::u8path(argv[4]);
 Scene stock;wi::scene::LoadModel(stock,(stockRoot/"grass.wiscene").generic_string());Check(stock.hairs.GetCount()>0,"Missing stock hair preset");
 auto baseline=stock.hairs[0];auto material=*stock.materials.GetComponent(stock.hairs.GetEntity(0));
 material.baseColor={1,1,1,1};material.SetRoughness(.9f);material.SetReflectance(.02f);material.SetAlphaRef(.5f);
 auto&stockSlot=material.textures[MaterialComponent::BASECOLORMAP];stockSlot.name=(stockRoot/"grassparticle.png").generic_string();stockSlot.resource=wi::resourcemanager::Load(stockSlot.name);
 baseline.length=.5f;baseline.width=.9f;baseline.segmentCount=2;baseline.billboardCount=2;baseline.randomness=.25f;
 float average=0;for(auto&r:baseline.atlas_rects)average+=r.size;average/=float(baseline.atlas_rects.size());for(auto&r:baseline.atlas_rects)r.size/=average;
 auto rebuilt=baseline;rebuilt.atlas_rects.clear();
 std::ifstream boxes(atlas.string()+".rects");unsigned width=0,height=0,count=0;boxes>>width>>height>>count;Check(width==1024&&height==1536&&count==6,"Invalid grass atlas sidecar");
 for(unsigned i=0;i<count;++i){float l,t,r,b;boxes>>l>>t>>r>>b;Check(bool(boxes)&&r>l&&b>t,"Invalid grass sprite crop");wi::HairParticleSystem::AtlasRect rect;rect.texMulAdd={(r-l)/width,(b-t)/height,l/width,t/height};rect.size=1;rebuilt.atlas_rects.push_back(rect);}
 auto newMaterial=material;if(coverage)newMaterial.baseColor={.670896f,.660796f,.778305f,1};auto&newSlot=newMaterial.textures[MaterialComponent::BASECOLORMAP];newSlot.name=atlas.generic_string();newSlot.resource=wi::resourcemanager::Load(newSlot.name);
 Check(newSlot.resource.GetTexture().IsValid(),"Rebuilt texture did not load");
 auto previous=baseline;auto previousMaterial=material;
 if(coverage){
  const fs::path old=fs::u8path(argv[6]);std::ifstream rects(old.string()+".rects");unsigned w=0,h=0,n=0;rects>>w>>h>>n;Check(w==1024&&h==1536&&n==6,"Invalid previous atlas");
  previous.atlas_rects.clear();for(unsigned i=0;i<n;++i){float l,t,r,b;rects>>l>>t>>r>>b;Check(bool(rects)&&r>l&&b>t,"Invalid previous crop");wi::HairParticleSystem::AtlasRect a;a.texMulAdd={(r-l)/w,(b-t)/h,l/w,t/h};a.size=1;previous.atlas_rects.push_back(a);}
  auto&slot=previousMaterial.textures[MaterialComponent::BASECOLORMAP];slot.name=old.generic_string();slot.resource=wi::resourcemanager::Load(slot.name);Check(slot.resource.GetTexture().IsValid(),"Previous atlas did not load");
 }

 for(size_t i=0;i<scene.terrains.GetCount();++i){scene.terrains[i].Generation_Cancel();scene.terrains[i].SetGrassEnabled(false);}
 scene.hairs.Clear();
 for(size_t i=0;i<scene.materials.GetCount();++i){auto&m=scene.materials[i];auto&slot=m.textures[MaterialComponent::BASECOLORMAP];if(slot.name.find("grassparticle.dds")!=std::string::npos){slot=stockSlot;m.SetDirty();}}
 if(coverage){Plot(scene,-12,"WICKED ART / DENSE",baseline,material);Plot(scene,-4,"PREVIOUS GRASS / VERY DENSE",previous,previousMaterial);Plot(scene,4,"RENEGADE OPEN GRASS / COVERAGE DENSE",rebuilt,newMaterial);Plot(scene,12,"RENEGADE OPEN GRASS / COVERAGE VERY DENSE",rebuilt,newMaterial);}
 else if(dense){Plot(scene,-8,"WICKED ART / DENSE",baseline,material);Plot(scene,0,"RENEGADE OPEN GRASS / DENSE",rebuilt,newMaterial);Plot(scene,8,"RENEGADE OPEN GRASS / VERY DENSE",rebuilt,newMaterial);}
 else{Plot(scene,-4,"WICKED ART / MATCHED SETTINGS",baseline,material);Plot(scene,4,"RENEGADE OPEN GRASS",rebuilt,newMaterial);}
 for(size_t i=0;i<scene.hairs.GetCount();++i){scene.hairs[i].strandCount=strands(i);scene.hairs[i].SetDirty();}
 const auto data=dest.parent_path().parent_path()/"Data";
 for(size_t i=0;i<scene.hairs.GetCount();++i)Sign(scene,plotX(i),data/("plot-"+std::to_string(i)+".png"));
 auto start=renegade::bridge::ResolvePlayerStart(scene);Check(start.resolution==renegade::bridge::PlayerStartResolution::Success,"Missing Player Start");
 auto*tr=scene.transforms.GetComponent(start.start.entity);tr->ClearTransform();tr->Translate(XMFLOAT3{coverage?4.f:0.f,.15f,-3});tr->UpdateTransform();
 wi::Archive archive(dest.generic_string(),false,false);scene.Serialize(archive);Check(archive.SaveFile(dest.generic_string()),"Scene save failed");
 Scene reopened;wi::scene::LoadModel(reopened,dest.generic_string());Check(reopened.hairs.GetCount()==(coverage?4:dense?3:2),"Unexpected native hair plot count");
 bool checked=false;
 for(size_t i=0;i<reopened.hairs.GetCount();++i){const auto&h=reopened.hairs[i];Check(h.strandCount==(strands(i))&&h.segmentCount==2&&h.billboardCount==2&&h.length==.5f&&h.width==.9f,"Matched hair settings changed after reopen");auto*n=reopened.names.GetComponent(reopened.hairs.GetEntity(i));if(n&&n->name.find("RENEGADE OPEN GRASS")==0){Check(h.atlas_rects.size()==6,"Lost grass variations");checked=true;}}
 Check(checked,"Missing rebuilt hair plot");
 std::ofstream report(dest.parent_path()/"HairGrassAtlasReview-receipt.txt");Describe(report,"Wicked matched",baseline);Describe(report,"Rebuilt matched",rebuilt);
 report<<"REBUILT_HAIR_ATLAS_COLD_REOPEN_PASS\n";
 report<<(coverage?"Four plots: Wicked 1800, previous 3600, revised 1800, revised 3600 strands.\n":dense?"Three plots: Wicked 1800, new 1800, new 3600 strands.\n":"Both plots: 300 strands.\n");
 report<<"Each plot 72 m2, length 0.5, width 0.9, 2 segments, 2 cards, randomness 0.25, seed 17.\nCrop rectangles end at the visible roots rather than including transparent cell padding.\n";
 {
  wi::scene::CameraComponent camera;camera.CreatePerspective(1280,800,.05f,200.f);wi::RenderPath3D render;render.scene=&reopened;render.camera=&camera;render.init(1280,800,96);
  render.setMotionBlurEnabled(false);render.setDepthOfFieldEnabled(false);render.setOcclusionCullingEnabled(false);render.ResizeBuffers();app.ActivatePath(&render);
  struct Disarm{wi::Application&app;~Disarm(){app.ActivatePath(nullptr);}}disarm{app};
  for(unsigned i=0;i<(coverage?4u:dense?3u:2u);++i){float x=plotX(i);camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(x,.65f,2,1),XMVectorSet(x,.15f,5,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
   for(int f=0;f<60;++f){app.is_window_active=true;app.Run();}
   wi::graphics::GetDevice()->WaitForGPU();Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(dest.parent_path()/("hair-plot-"+std::to_string(i)+".png")).generic_string()),"Native hair capture failed");}
 }
 report<<"REBUILT_HAIR_NATIVE_CAPTURES_PASS\n";report.close();
 reopened.Clear();scene.Clear();stock.Clear();while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();
 std::cout<<"REBUILT_HAIR_ATLAS_REVIEW_PASS"<<std::endl;return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
