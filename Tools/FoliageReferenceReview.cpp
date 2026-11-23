// Local visual comparison author; existing native hair and WISCENE formats.
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/GrassPalette.h"
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
 m.vertex_normals.assign(4,{0,1,0});m.vertex_uvset_0={{0,0},{0,1},{1,0},{1,1}};m.indices={0,1,2,2,1,3};m.CreateRenderData();return e;
}
static void Plot(Scene&s,float x,const char*name,const wi::HairParticleSystem&source,const MaterialComponent&sourceMat){
 Entity emitter=Emitter(s,x);Entity e=wi::ecs::CreateEntity();s.names.Create(e)=name;s.transforms.Create(e);
 auto&h=s.hairs.Create(e);h=source;h.DeleteRenderData();h.meshID=emitter;h.strandCount=900;
 h.vertex_lengths.assign(4,1.f);h.indices.clear();h.randomSeed=17;h.viewDistance=100.f;h.SetDirty();
 auto&m=s.materials.Create(e);m=sourceMat;m.SetDirty();m.CreateRenderData();
}
static void Sign(Scene&s,float x,const fs::path&png){
 Entity mat=wi::ecs::CreateEntity();auto&m=s.materials.Create(mat);m.shaderType=MaterialComponent::SHADERTYPE_UNLIT;m.baseColor={1,1,1,1};m.SetDoubleSided(true);
 m.textures[MaterialComponent::BASECOLORMAP].name=png.generic_string();m.textures[MaterialComponent::BASECOLORMAP].resource=wi::resourcemanager::Load(png.generic_string());m.CreateRenderData();
 Entity mesh=wi::ecs::CreateEntity();auto&g=s.meshes.Create(mesh);
 g.vertex_positions={{x-2.8f,2.45f,.5f},{x-2.8f,3.05f,.5f},{x+2.8f,2.45f,.5f},{x+2.8f,3.05f,.5f}};
 g.vertex_normals.assign(4,{0,0,-1});g.vertex_uvset_0={{0,1},{0,0},{1,1},{1,0}};g.indices={0,1,2,2,1,3};
 auto&sub=g.subsets.emplace_back();sub.materialID=mat;sub.indexOffset=0;sub.indexCount=6;g.CreateRenderData();
 Entity obj=wi::ecs::CreateEntity();s.names.Create(obj)=png.stem().string();s.transforms.Create(obj);s.objects.Create(obj).meshID=mesh;
}
int main(int argc,char**argv){try{
 Check(argc==4,"usage: FoliageReferenceReview source.wiscene destination.wiscene stock-terrain-directory");
 HWND window=CreateWindowExW(0,L"STATIC",L"Foliage comparison author",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 Scene scene;wi::scene::LoadModel(scene,argv[1]);Check(scene.terrains.GetCount()>0,"Missing review terrain");
 wi::HairParticleSystem current;MaterialComponent currentMat;bool found=false;
 for(size_t i=0;i<scene.hairs.GetCount();++i){auto e=scene.hairs.GetEntity(i);auto*n=scene.names.GetComponent(e);
  if(n&&n->name=="Painted grass"&&scene.hairs[i].atlas_rects.size()==2){current=scene.hairs[i];currentMat=*scene.materials.GetComponent(e);found=true;break;}}
 Check(found,"Missing grass and daisy mixed layer");
const auto projectRoot=fs::u8path(argv[2]).parent_path().parent_path().parent_path(); const auto plants=fs::u8path(wi::helper::GetCurrentPath())/"Content/terrain/foliage"; renegade::bridge::GrassAtlas meadow;std::string atlasError;Check(renegade::bridge::BuildGrassAtlas(projectRoot,{plants/"01_short_grass_albedo.png",plants/"06_daisies_albedo.png"},meadow,atlasError),atlasError.c_str());currentMat.textures[MaterialComponent::BASECOLORMAP].name=meadow.path;currentMat.textures[MaterialComponent::BASECOLORMAP].resource=wi::resourcemanager::Load(meadow.path);
 Scene stock;const fs::path stockRoot=fs::u8path(argv[3]);wi::scene::LoadModel(stock,(stockRoot/"grass.wiscene").generic_string());Check(stock.hairs.GetCount()>0,"Missing Wicked stock preset");
 auto native=stock.hairs[0];auto nativeMat=*stock.materials.GetComponent(stock.hairs.GetEntity(0));
 nativeMat.textures[MaterialComponent::BASECOLORMAP].name=(stockRoot/"grassparticle.png").generic_string();
 nativeMat.textures[MaterialComponent::BASECOLORMAP].resource=wi::resourcemanager::Load((stockRoot/"grassparticle.png").generic_string());
 const fs::path dest=fs::u8path(argv[2]);fs::create_directories(dest.parent_path());std::ofstream report(dest.parent_path()/"FoliageComparison-receipt.txt");
 Describe(report,"Wicked stock",native);Describe(report,"Renegade current",current);report<<"Current atlas "<<currentMat.textures[MaterialComponent::BASECOLORMAP].name<<"\n";
 for(size_t i=0;i<scene.terrains.GetCount();++i){scene.terrains[i].Generation_Cancel();scene.terrains[i].SetGrassEnabled(false);}
 scene.hairs.Clear();
 auto tuned=current;tuned.length=current.length*.35f;tuned.width=current.width*.6f;tuned.segmentCount=3;tuned.billboardCount=2;tuned.randomness=.35f;
 tuned.atlas_rects[0].size=.8f;tuned.atlas_rects[1].size=.45f;Describe(report,"Renegade tuned",tuned);
 Plot(scene,-8,"WICKED ORIGINAL",native,nativeMat);Plot(scene,0,"RENEGADE CURRENT",current,currentMat);Plot(scene,8,"RENEGADE TUNED",tuned,currentMat);
 for(int i=0;i<3;++i)Sign(scene,float(i-1)*8,dest.parent_path().parent_path()/"Data"/("plot-"+std::to_string(i)+".png"));
 auto start=renegade::bridge::ResolvePlayerStart(scene);Check(start.resolution==renegade::bridge::PlayerStartResolution::Success,"Missing Player Start");
 auto*t=scene.transforms.GetComponent(start.start.entity);Check(t!=nullptr,"Missing Player Start transform");t->ClearTransform();t->Translate(XMFLOAT3{0,.15f,-12});t->UpdateTransform();
 wi::Archive archive(dest.generic_string(),false,false);scene.Serialize(archive);Check(archive.SaveFile(dest.generic_string()),"Save failed");
 Scene reopened;wi::scene::LoadModel(reopened,dest.generic_string());Check(reopened.hairs.GetCount()==3,"Cold reopen did not retain three plots");bool checked=false;
 for(size_t i=0;i<reopened.hairs.GetCount();++i){auto*n=reopened.names.GetComponent(reopened.hairs.GetEntity(i));if(n&&n->name=="RENEGADE TUNED"){const auto&h=reopened.hairs[i];Check(h.segmentCount==3&&h.billboardCount==2&&std::abs(h.atlas_rects[1].size-.45f)<.0001f,"Tuned parameters changed after reopen");checked=true;}}
 Check(checked,"Tuned plot missing");report<<"THREE_PLOT_SAVE_COLD_REOPEN_PASS\nEqual emitter area 72 m2; equal strand count 900; equal seed 17.\n";report.close();
 std::cout<<"THREE_PLOT_SAVE_COLD_REOPEN_PASS "<<dest.generic_string()<<std::endl;reopened.Clear();scene.Clear();stock.Clear();while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
