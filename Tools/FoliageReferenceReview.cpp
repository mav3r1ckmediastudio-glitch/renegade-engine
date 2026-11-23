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
 m.vertex_normals.assign(4,{0,1,0});m.vertex_uvset_0={{0,0},{0,1},{1,0},{1,1}};m.indices={0,1,2,2,1,3};m.CreateRenderData();return e;
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

static void MeshPlot(Scene&s,const fs::path&file,std::ostream&report){
 Scene imported;ImportModel_GLTF(file.generic_string(),imported);imported.Update(0);
 struct Prototype{Entity object,mesh;XMFLOAT4X4 matrix;};
 std::vector<Prototype> shortGrass,tallGrass;
 for(size_t i=0;i<imported.objects.GetCount();++i){
  auto e=imported.objects.GetEntity(i);auto*n=imported.names.GetComponent(e);if(!n)continue;
  const bool isShort=n->name.find("Grass main")!=std::string::npos;
  const bool isTall=n->name.find("Grass tall")!=std::string::npos;if(!isShort&&!isTall)continue;
  Prototype p{e,imported.objects[i].meshID,{}};auto*t=imported.transforms.GetComponent(e);Check(t!=nullptr,"Missing imported transform");
  p.matrix=t->world;(isShort?shortGrass:tallGrass).push_back(p);
 }
 Check(shortGrass.size()==9&&tallGrass.size()==6,"Expected nine short and six tall mesh variants");
 for(size_t i=0;i<imported.materials.GetCount();++i){auto&m=imported.materials[i];m.SetDoubleSided(true);m.SetAlphaRef(.45f);m.SetUseWind(true);m.SetDirty();}
 for(size_t i=0;i<imported.meshes.GetCount();++i){
  auto&m=imported.meshes[i];float low=1e9f,high=-1e9f;for(auto&v:m.vertex_positions){low=std::min(low,v.y);high=std::max(high,v.y);}
  m.vertex_windweights.clear();for(auto&v:m.vertex_positions){float weight=std::max(0.f,std::min(1.f,(v.y-low)/std::max(.001f,high-low)));m.vertex_windweights.push_back(uint8_t(weight*weight*64));}
  m.CreateRenderData();
 }
 s.Merge(imported);for(auto p:shortGrass)s.objects.Remove(p.object);for(auto p:tallGrass)s.objects.Remove(p.object);
 std::mt19937 rng(23);std::uniform_real_distribution<float> unit(0,1);
 for(unsigned i=0;i<240;++i){
  const auto&p=i<216?shortGrass[i%shortGrass.size()]:tallGrass[(i-216)%tallGrass.size()];
  Entity e=wi::ecs::CreateEntity();s.names.Create(e)="Uploaded grass clump";s.objects.Create(e).meshID=p.mesh;
  auto&t=s.transforms.Create(e);float size=.85f+.3f*unit(rng);float x=13.f+6.f*unit(rng),z=2.f+12.f*unit(rng);
  t.MatrixTransform(XMLoadFloat4x4(&p.matrix)*XMMatrixScaling(size,size,size)*XMMatrixRotationY(unit(rng)*XM_2PI)*XMMatrixTranslation(x,.025f,z));t.UpdateTransform();
 }
 report<<"UPLOADED_GRASS_SHARED_MESH_PLOT prototypes=15 placements=240 short=216 tall=24 area_m2=72\n";
}


static void AdditionalPlot(Scene&s,const fs::path&file,const char*label,float x,float z,unsigned count,unsigned expected,std::ostream&report){
 Scene imported;ImportModel_GLTF(file.generic_string(),imported);imported.Update(0);
 struct Prototype{Entity object,mesh;XMFLOAT4X4 matrix;};std::vector<Prototype> variants;
 for(size_t i=0;i<imported.objects.GetCount();++i){auto e=imported.objects.GetEntity(i);auto*t=imported.transforms.GetComponent(e);Check(t!=nullptr,"Imported object has no transform");variants.push_back({e,imported.objects[i].meshID,t->world});}
 Check(variants.size()==expected,"Unexpected variant count in additional pack");
 unsigned baseMaps=0,normalMaps=0;
 for(size_t i=0;i<imported.materials.GetCount();++i){auto&m=imported.materials[i];m.SetDoubleSided(true);m.SetAlphaRef(.45f);m.SetUseWind(true);m.SetDirty();baseMaps+=m.textures[MaterialComponent::BASECOLORMAP].resource.IsValid()?1:0;normalMaps+=m.textures[MaterialComponent::NORMALMAP].resource.IsValid()?1:0;}
 Check(baseMaps>0&&normalMaps>0,"Additional pack lost its colour or normal textures");
 for(size_t i=0;i<imported.meshes.GetCount();++i){auto&m=imported.meshes[i];float low=1e9f,high=-1e9f;for(auto&v:m.vertex_positions){low=std::min(low,v.y);high=std::max(high,v.y);}m.vertex_windweights.clear();for(auto&v:m.vertex_positions){float w=std::max(0.f,std::min(1.f,(v.y-low)/std::max(.001f,high-low)));m.vertex_windweights.push_back(uint8_t(w*w*24));}m.CreateRenderData();}
 s.Merge(imported);for(auto p:variants)s.objects.Remove(p.object);
 std::mt19937 rng(41);std::uniform_real_distribution<float>unit(0,1);
 for(unsigned i=0;i<count;++i){const auto&p=variants[i%variants.size()];Entity e=wi::ecs::CreateEntity();s.names.Create(e)=label;s.objects.Create(e).meshID=p.mesh;auto&t=s.transforms.Create(e);float scale=.9f+.2f*unit(rng);
 t.MatrixTransform(XMLoadFloat4x4(&p.matrix)*XMMatrixScaling(scale,scale,scale)*XMMatrixRotationY(unit(rng)*XM_2PI)*XMMatrixTranslation(x-3+6*unit(rng),.025f,z+12*unit(rng)));t.UpdateTransform();}
 report<<"ADDITIONAL_SHARED_MESH_PLOT "<<label<<" prototypes="<<variants.size()<<" placements="<<count<<" base_maps="<<baseMaps<<" normal_maps="<<normalMaps<<"\n";
}
static void CheckAdditional(const Scene&s,const char*label,unsigned count,unsigned expected){
 unsigned found=0;std::set<Entity>meshes;for(size_t i=0;i<s.objects.GetCount();++i){auto*n=s.names.GetComponent(s.objects.GetEntity(i));if(n&&n->name==label){++found;meshes.insert(s.objects[i].meshID);}}
 Check(found==count&&meshes.size()==expected,"Additional pack lost instances or shared meshes after cold reopen");
}


static void CaptureUploadedPlots(wi::Application&app,Scene&scene,const fs::path&out){
 wi::scene::CameraComponent camera;camera.CreatePerspective(1280,800,.05f,200.f);
 wi::RenderPath3D render;render.scene=&scene;render.camera=&camera;render.init(1280,800,96);
 render.setMotionBlurEnabled(false);render.setDepthOfFieldEnabled(false);render.setOcclusionCullingEnabled(false);render.ResizeBuffers();
 app.ActivatePath(&render);
 struct Disarm{wi::Application&app;~Disarm(){app.ActivatePath(nullptr);}}disarm{app};
 const float xs[]={16,-8,0,8};const float zs[]={2,22,22,22};const float ys[]={.85f,1.0f,.7f,.7f};
 for(unsigned i=0;i<4;++i){
  camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(xs[i],ys[i],zs[i],1),XMVectorSet(xs[i],.15f,zs[i]+3,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
  for(int f=0;f<60;++f){app.is_window_active=true;app.Run();}
  wi::graphics::GetDevice()->WaitForGPU();
  Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/("uploaded-plot-"+std::to_string(i)+".png")).generic_string()),"Native mesh plot capture failed");
 }
}

int main(int argc,char**argv){try{
 Check(argc==4||argc==5||argc==8,"usage: FoliageReferenceReview source.wiscene destination.wiscene stock-terrain-directory [grass-pack.glb]");
 HWND window=CreateWindowExW(0,L"STATIC",L"Foliage comparison author",WS_POPUP,0,0,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
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
 if(argc>=5){MeshPlot(scene,fs::u8path(argv[4]),report);Sign(scene,16,dest.parent_path().parent_path()/"Data"/"plot-3.png");}
 if(argc==8){AdditionalPlot(scene,fs::u8path(argv[5]),"Uploaded daisy",-8,20,36,9,report);AdditionalPlot(scene,fs::u8path(argv[6]),"Uploaded undergrowth",0,20,90,15,report);AdditionalPlot(scene,fs::u8path(argv[7]),"Uploaded grass nine",8,20,180,9,report);for(int i=0;i<3;++i)Sign(scene,float(i-1)*8,dest.parent_path().parent_path()/"Data"/("plot-"+std::to_string(i+4)+".png"),18.5f);}
 auto start=renegade::bridge::ResolvePlayerStart(scene);Check(start.resolution==renegade::bridge::PlayerStartResolution::Success,"Missing Player Start");
 auto*t=scene.transforms.GetComponent(start.start.entity);Check(t!=nullptr,"Missing Player Start transform");t->ClearTransform();t->Translate(XMFLOAT3{argc>=5?16.f:0.f,.15f,argc>=5?-2.f:-12.f});t->UpdateTransform();
 wi::Archive archive(dest.generic_string(),false,false);scene.Serialize(archive);Check(archive.SaveFile(dest.generic_string()),"Save failed");
 Scene reopened;wi::scene::LoadModel(reopened,dest.generic_string());Check(reopened.hairs.GetCount()==3,"Cold reopen did not retain three plots");bool checked=false;
 for(size_t i=0;i<reopened.hairs.GetCount();++i){auto*n=reopened.names.GetComponent(reopened.hairs.GetEntity(i));if(n&&n->name=="RENEGADE TUNED"){const auto&h=reopened.hairs[i];Check(h.segmentCount==3&&h.billboardCount==2&&std::abs(h.atlas_rects[1].size-.45f)<.0001f,"Tuned parameters changed after reopen");checked=true;}}
 Check(checked,"Tuned plot missing");if(argc>=5){unsigned count=0;std::set<Entity>shared;for(size_t i=0;i<reopened.objects.GetCount();++i){auto*n=reopened.names.GetComponent(reopened.objects.GetEntity(i));if(n&&n->name=="Uploaded grass clump"){++count;shared.insert(reopened.objects[i].meshID);}}Check(count==240&&shared.size()==15,"Mesh instances lost shared resources after reopen");report<<"UPLOADED_MESH_SHARED_RESOURCES_COLD_REOPEN_PASS placements="<<count<<" unique_meshes="<<shared.size()<<"\n";}report<<"THREE_PLOT_SAVE_COLD_REOPEN_PASS\nEqual emitter area 72 m2; equal strand count 900; equal seed 17.\n";if(argc==8){CheckAdditional(reopened,"Uploaded daisy",36,9);CheckAdditional(reopened,"Uploaded undergrowth",90,15);CheckAdditional(reopened,"Uploaded grass nine",180,9);report<<"ALL_FOUR_UPLOADED_PACKS_COLD_REOPEN_PASS variants=48 placements=546\n";}
 if(argc==8){CaptureUploadedPlots(app,reopened,dest.parent_path());report<<"ALL_FOUR_UPLOADED_NATIVE_CAPTURES_PASS\n";}
 report.close();
 std::cout<<"THREE_PLOT_SAVE_COLD_REOPEN_PASS "<<dest.generic_string()<<std::endl;reopened.Clear();scene.Clear();stock.Clear();while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
