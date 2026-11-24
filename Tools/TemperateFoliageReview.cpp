// Private six-plot visual trial using the approved meadow and native breeze.
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/RenderSettingsService.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace DirectX;
using namespace wi::scene;
namespace fs=std::filesystem;
using Entity=wi::ecs::Entity;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Preset {const char* name;const char* atlas;float length,width;uint32_t count;};
static const Preset presets[]={
    {"Approved meadow",nullptr,.5f,.9f,3600},
    {"Short grass","short_grass",.25f,.9f,1800},
    {"Dry grass","dry_grass",.45f,.9f,1800},
    {"Seedhead grass","seedhead_grass",.65f,.9f,1080},
    {"Low clover","low_clover",.12f,.9f,1440},
    {"Small wildflowers","small_wildflowers",.30f,.9f,288}
};
static float X(unsigned i){return -10.f+10.f*float(i%3);}
static float Z(unsigned i){return i<3?2.f:20.f;}
static Entity Emitter(Scene& s,unsigned i){
    Entity e=wi::ecs::CreateEntity();s.names.Create(e)=std::string(presets[i].name)+" emitter";s.transforms.Create(e);
    auto& m=s.meshes.Create(e);const float x=X(i),z=Z(i);
    m.vertex_positions={{x-3,.025f,z},{x-3,.025f,z+12},{x+3,.025f,z},{x+3,.025f,z+12}};
    m.vertex_normals.assign(4,{0,1,0});m.vertex_uvset_0={{0,0},{0,1},{1,0},{1,1}};m.indices={0,1,2,2,1,3};
    auto& subset=m.subsets.emplace_back();subset.indexOffset=0;subset.indexCount=6;m.CreateRenderData();return e;
}
static void Sign(Scene& s,unsigned i,const fs::path& file){
    Entity material=wi::ecs::CreateEntity();auto& m=s.materials.Create(material);
    m.shaderType=MaterialComponent::SHADERTYPE_UNLIT;m.baseColor={1,1,1,1};m.SetDoubleSided(true);
    auto& slot=m.textures[MaterialComponent::BASECOLORMAP];slot.name=file.generic_string();slot.resource=wi::resourcemanager::Load(slot.name);
    Check(slot.resource.GetTexture().IsValid(),"Missing plot label");m.CreateRenderData();
    Entity mesh=wi::ecs::CreateEntity();auto& g=s.meshes.Create(mesh);float x=X(i),z=Z(i)-1.5f;
    g.vertex_positions={{x-2.8f,2.45f,z},{x-2.8f,3.05f,z},{x+2.8f,2.45f,z},{x+2.8f,3.05f,z}};
    g.vertex_normals.assign(4,{0,0,-1});g.vertex_uvset_0={{0,1},{0,0},{1,1},{1,0}};g.indices={0,1,2,2,1,3};
    auto& subset=g.subsets.emplace_back();subset.materialID=material;subset.indexOffset=0;subset.indexCount=6;g.CreateRenderData();
    Entity object=wi::ecs::CreateEntity();s.names.Create(object)=std::string(presets[i].name)+" label";s.transforms.Create(object);s.objects.Create(object).meshID=mesh;
}
int main(int argc,char** argv){try{
    Check(argc==3,"usage: TemperateFoliageReview source-native-breeze.wiscene project-directory");
    fs::path project=fs::u8path(argv[2]);const auto assets=project/"Content/Vegetation/TemperateReview";const auto dest=project/"Content/Scenes/Main.wiscene";
    HWND window=CreateWindowExW(0,L"STATIC",L"Temperate foliage native author",WS_POPUP,0,0,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
    app.setTargetFrameRate(60);app.setFrameRateLock(true);
    wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
    Scene s;LoadModel(s,argv[1]);Check(s.hairs.GetCount()==1&&s.forces.GetCount()==0,"Expected approved native breeze source");
    auto baseline=s.hairs[0];const auto* baselineMaterial=s.materials.GetComponent(s.hairs.GetEntity(0));Check(baselineMaterial,"Missing approved material");const auto material=*baselineMaterial;
    Check(baseline.atlas_rects.size()==6&&baseline.segmentCount==2&&baseline.billboardCount==2,"Unexpected approved baseline");
    for(size_t i=0;i<s.terrains.GetCount();++i){s.terrains[i].Generation_Cancel();s.terrains[i].SetGrassEnabled(false);}
    s.hairs.Clear();
    for(unsigned i=0;i<6;++i){
        Entity emitter=Emitter(s,i),e=wi::ecs::CreateEntity();s.names.Create(e)=presets[i].name;s.transforms.Create(e);
        auto& h=s.hairs.Create(e);h=baseline;h.DeleteRenderData();h.meshID=emitter;h.indices.clear();h.vertex_lengths.assign(4,1.f);
        h.strandCount=presets[i].count;h.length=presets[i].length;h.width=presets[i].width;h.viewDistance=100;h.randomSeed=17;h.uniformity=-1;
        auto& m=s.materials.Create(e);m=material;
        if(i){
            const auto atlas=assets/(std::string(presets[i].atlas)+".png");
            std::ifstream rects(atlas.string()+".rects");unsigned w=0,ht=0,n=0;rects>>w>>ht>>n;Check(w==1024&&ht==1536&&n==6,"Invalid atlas sidecar");
            h.atlas_rects.clear();
            for(unsigned j=0;j<n;++j){float l,t,r,b;rects>>l>>t>>r>>b;Check(bool(rects)&&r>l&&b>t&&l>=0&&t>=0&&r<=w&&b<=ht,"Invalid crop");
                wi::HairParticleSystem::AtlasRect rect;rect.texMulAdd={(r-l)/w,(b-t)/ht,l/w,t/ht};rect.size=1;h.atlas_rects.push_back(rect);}
            auto& slot=m.textures[MaterialComponent::BASECOLORMAP];slot.name=atlas.generic_string();slot.resource=wi::resourcemanager::Load(slot.name);
            Check(slot.resource.GetTexture().IsValid(),"Plant atlas failed to load");
            m.baseColor={1,1,1,1};m.SetRoughness(.9f);m.SetReflectance(.02f);m.SetAlphaRef(.5f);
        }
        h.SetDirty();m.SetDirty();m.CreateRenderData();Sign(s,i,project/"Content/Data"/("temperate-plot-"+std::to_string(i)+".png"));
    }
    auto start=renegade::bridge::ResolvePlayerStart(s);Check(start.resolution==renegade::bridge::PlayerStartResolution::Success,"Missing Player Start");
    auto* pose=s.transforms.GetComponent(start.start.entity);pose->ClearTransform();pose->Translate(XMFLOAT3{0,.15f,-4});pose->UpdateTransform();
    auto renderSettings=renegade::bridge::CaptureRenderSettings(s);Check(renderSettings.antiAliasing==renegade::bridge::AntiAliasingMode::Off,"Approved review AA changed");
    Check(!renderSettings.motionBlurEnabled&&!renderSettings.depthOfFieldEnabled,"Unexpected review blur settings");
    {wi::Archive archive(dest.generic_string(),false,false);s.Serialize(archive);Check(archive.SaveFile(dest.generic_string()),"Save failed");}
    Scene cold;LoadModel(cold,dest.generic_string());Check(cold.hairs.GetCount()==6&&cold.forces.GetCount()==0&&cold.animations.GetCount()==0,"Reopen plot/wind mismatch");
    Check(cold.weathers.GetCount()&&cold.weathers[0].windDirection.x==.08f,"Native breeze changed");
    std::ofstream report(dest.parent_path()/"TemperateReview-receipt.txt");
    report<<"Six 72m2 plots; native breeze (0.08,0,0), speed 1, randomness 5; AA Off.\n";
    for(unsigned i=0;i<6;++i){
        const auto& h=cold.hairs[i];const auto* mesh=cold.meshes.GetComponent(h.meshID);
        Check(h.strandCount==presets[i].count&&h.length==presets[i].length&&h.width==presets[i].width&&h.atlas_rects.size()==6,"Reopened plant settings changed");
        Check(mesh&&mesh->subsets.size()==1&&mesh->subsets[0].indexCount==6,"Invalid reopened emitter");
        const auto* m=cold.materials.GetComponent(cold.hairs.GetEntity(i));Check(m&&m->textures[MaterialComponent::BASECOLORMAP].resource.GetTexture().IsValid(),"Reopened texture missing");
        report<<presets[i].name<<" strands="<<h.strandCount<<" length="<<h.length<<" width="<<h.width<<" sprites="<<h.atlas_rects.size()<<"\n";
    }
    report<<"TEMPERATE_SIX_PLOTS_COLD_REOPEN_PASS\n";report.flush();
    {
        CameraComponent camera;camera.CreatePerspective(1280,800,.05f,200);
        wi::RenderPath3D render;render.scene=&cold;render.camera=&camera;render.init(1280,800,96);
        render.setMotionBlurEnabled(false);render.setDepthOfFieldEnabled(false);render.setOcclusionCullingEnabled(false);render.ResizeBuffers();app.ActivatePath(&render);
        struct Detach{wi::Application& app;~Detach(){app.ActivatePath(nullptr);}}detach{app};
        for(unsigned i=0;i<6;++i){
            const float x=X(i),z=Z(i);const float height=i==4?.28f:.65f;
            camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(x,height,z,1),XMVectorSet(x,.08f,z+3,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
            for(unsigned frame=0;frame<90;++frame){app.is_window_active=true;app.Run();}
            for(size_t j=0;j<cold.hairs.GetCount();++j)Check(!cold.hairs[j].indices.empty(),"Emitter distribution lost");
            wi::graphics::GetDevice()->WaitForGPU();
            Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(dest.parent_path()/("temperate-plot-"+std::to_string(i)+".png")).generic_string()),"Capture failed");
        }
        app.ActivatePath(nullptr);
    }
    report<<"TEMPERATE_SIX_NATIVE_CAPTURES_PASS\n";report.close();
    cold.Clear();s.Clear();while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();
    std::cout<<"TEMPERATE_FOLIAGE_NATIVE_REVIEW_PASS"<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
