#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/RiverFlowFieldService.h"
#include <Windows.h>
#include "RiverWaterlineClassifier.h"
#include <unordered_set>
#include <fstream>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace DirectX;
using namespace renegade::bridge;
namespace fs=std::filesystem;
static void Check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
static uint32_t Bits(float value){uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
struct Params {XMFLOAT4 bounds;XMUINT4 control;};
static_assert(sizeof(Params)==32);
struct FlowPath:wi::RenderPath3D {bool frozen=false;void Update(float dt)override{wi::RenderPath3D::Update(frozen?0.f:dt);}};

static std::unordered_set<wi::ecs::Entity> TerrainChunks(const wi::scene::Scene& scene){
    std::unordered_set<wi::ecs::Entity> ids;
    for(size_t i=0;i<scene.terrains.GetCount();++i)for(const auto& entry:scene.terrains[i].chunks)ids.insert(entry.second.entity);
    return ids;
}
static std::vector<river_g4::Surface> Gather(const wi::scene::Scene& scene,wi::ecs::Entity river,
    const RiverFlowField& flow,std::vector<wi::ecs::Entity>* rocks=nullptr){
    auto terrain=TerrainChunks(scene);std::vector<river_g4::Surface> result;
    float minX=INFINITY,minZ=INFINITY,maxX=-INFINITY,maxZ=-INFINITY;
    for(const auto& row:flow.NativeRibbon())for(auto p:{row.leftBank,row.rightBank}){
        minX=std::min(minX,p.x);minZ=std::min(minZ,p.z);maxX=std::max(maxX,p.x);maxZ=std::max(maxZ,p.z);
    }
    size_t inspected=0;
    for(size_t i=0;i<scene.objects.GetCount();++i){
        const auto entity=scene.objects.GetEntity(i);const auto& object=scene.objects[i];
        if(entity==river||!object.IsRenderable())continue;
        const auto* mesh=scene.meshes.GetComponent(object.meshID);if(!mesh||!mesh->IsRenderable())continue;
        river_g4::Surface surface;surface.terrain=terrain.count(entity)||terrain.count(object.meshID);
        const auto world=scene.ComputeEntityMatrixRecursive(entity);
        uint32_t first=0,last=0;mesh->GetLODSubsetRange(0,first,last);
        for(uint32_t subset=first;subset<last;++subset){
            const auto& part=mesh->subsets[subset];
            const auto* material=scene.materials.GetComponent(part.materialID);
            if(material&&material->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER)continue;
            Check(size_t(part.indexOffset)+part.indexCount<=mesh->indices.size(),"Invalid native subset range");
            for(uint32_t j=part.indexOffset;j+2<part.indexOffset+part.indexCount;j+=3){
                Check(++inspected<=2000000,"G4 native triangle scan limit");
                river_g4::Triangle triangle;
                XMFLOAT3* output[]={&triangle.a,&triangle.b,&triangle.c};
                for(int k=0;k<3;++k){
                    auto index=mesh->indices[j+k];Check(index<mesh->vertex_positions.size(),"Invalid native mesh index");
                    XMStoreFloat3(output[k],XMVector3TransformCoord(XMLoadFloat3(&mesh->vertex_positions[index]),world));
                    Check(river_g4::Finite(*output[k]),"Nonfinite native geometry");
                }
                const auto& a=triangle.a;const auto& b=triangle.b;const auto& c=triangle.c;
                if(std::max({a.x,b.x,c.x})<minX-.5f||std::min({a.x,b.x,c.x})>maxX+.5f||
                    std::max({a.z,b.z,c.z})<minZ-.5f||std::min({a.z,b.z,c.z})>maxZ+.5f)continue;
                surface.triangles.push_back(triangle);
            }
        }
        if(!surface.triangles.empty()){
            if(rocks){float lo=INFINITY,hi=-INFINITY;for(const auto& t:surface.triangles)for(auto v:{t.a,t.b,t.c}){lo=std::min(lo,v.y);hi=std::max(hi,v.y);}
                std::cout<<"G4_SOURCE_GEOMETRY terrain="<<surface.terrain<<" worldY="<<lo<<","<<hi<<" triangles="<<surface.triangles.size()<<"\n";}
            if(!surface.terrain&&rocks)rocks->push_back(entity);
            result.push_back(std::move(surface));
        }
    }
    return result;
}

int main(int argc,char** argv)
{
    if(argc<4||argc>5)return 2;
    const bool interactive=argc==5&&std::string(argv[4])=="--interactive";
    const bool reopen=argc==5&&std::string(argv[4])=="--reopen";
    std::cout.setf(std::ios::unitbuf);
    const auto out=fs::absolute(argv[1]),shaderDir=fs::absolute(argv[3]);
    if(fs::exists(out)&&!fs::is_empty(out))return 2;
    fs::create_directories(out);
    HWND window=CreateWindowExW(0,L"STATIC",L"Renegade River V2 - Waterline Classification G4",
        WS_OVERLAPPEDWINDOW,60,60,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    struct App:wi::Application{void Initialize()override{wi::Application::Initialize();alwaysactive=true;}}app;
    app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
    auto* gpu=wi::graphics::GetDevice();
    try {
        wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
        SceneService service;
        Check(service.LoadScene(fs::absolute(argv[2]).generic_u8string()),service.LastError());
        auto& scene=service.GetScene();
        const auto river=scene.Entity_FindByName("River Water 5");
        auto* water=scene.materials.GetComponent(river);
        Check(water&&!water->IsCustomShader()&&water->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER,
            "Native WATER source required");
        // Disposable G4 bank fixture: lower WATER 0.30m into the EXISTING cut bed.
        // The inherited diagnostic plane was 0.15m ABOVE the highest terrain
        // (terrain maxY0); it has no physical bank-water intersection to classify.
        // Never sculpt or alter source terrain to fabricate a contact.
        auto* nativeSpline=scene.splines.GetComponent(river);Check(nativeSpline!=nullptr,"Native spline missing");
        if(!reopen)for(auto node:nativeSpline->spline_node_entities){
            XMMATRIX parent=XMMatrixIdentity();
            if(const auto* h=scene.hierarchy.GetComponent(node))parent=scene.ComputeEntityMatrixRecursive(h->parentID);
            const auto local=scene.ComputeEntityMatrixRecursive(node)*XMMatrixTranslation(0,-.30f,0)*XMMatrixInverse(nullptr,parent);
            XMVECTOR scale,rotation,translation;Check(XMMatrixDecompose(&scale,&rotation,&translation,local),"G4 water fixture transform refused");
            auto* t=scene.transforms.GetComponent(node);XMStoreFloat3(&t->scale_local,scale);
            XMStoreFloat4(&t->rotation_local,rotation);XMStoreFloat3(&t->translation_local,translation);
            t->SetDirty();t->UpdateTransform();
        }
        nativeSpline->prev_mesh_generation_nodes=-1;scene.Update(0);
        water=scene.materials.GetComponent(river);
        const auto optics=*water;
        const auto meshCount=scene.meshes.GetCount(),objectCount=scene.objects.GetCount();
        std::shared_ptr<const RiverFlowField> field;std::string error;
        Check(BuildRiverFlowField(scene,river,{},field,error),error);
        std::vector<wi::ecs::Entity> rocks;
        auto snapshot=Gather(scene,river,*field,&rocks);
        Check(!rocks.empty()&&scene.terrains.GetCount()>0,"Expected native rock and terrain candidates");
        std::vector<wi::scene::TransformComponent> originalTransforms;
        std::vector<bool> originalVisible;std::vector<XMFLOAT4X4> originalWorld;
        for(auto id:rocks){auto* t=scene.transforms.GetComponent(id);Check(t!=nullptr,"Rock transform missing");
            originalTransforms.push_back(*t);originalVisible.push_back(scene.objects.GetComponent(id)->IsRenderable());
            XMFLOAT4X4 m;XMStoreFloat4x4(&m,scene.ComputeEntityMatrixRecursive(id));originalWorld.push_back(m);}
        std::cout<<"G4_CANDIDATES="<<snapshot.size()<<" rockObjects="<<rocks.size()<<"\n";
        river_g4::Mask mask;wi::graphics::Texture texture;Params params{};
        auto rebuild=[&](const std::string& label){
            gpu->WaitForGPU();
            mask=river_g4::Build(*field,Gather(scene,river,*field));
            wi::graphics::TextureDesc td;td.width=mask.width;td.height=mask.height;
            td.format=wi::graphics::Format::R32G32B32A32_FLOAT;td.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
            wi::graphics::SubresourceData initial;initial.data_ptr=mask.cells.data();initial.row_pitch=td.width*sizeof(XMFLOAT4);
            initial.slice_pitch=initial.row_pitch*td.height;
            Check(gpu->CreateTexture(&td,&initial,&texture),"DX12 classification texture creation failed");
            const int index=gpu->GetDescriptorIndex(&texture,wi::graphics::SubresourceType::SRV);
            Check(index>=0,"Classification SRV missing");params.bounds=mask.bounds;params.control={uint32_t(index),mask.width,mask.height,0};
            std::cout<<"G4_MASK="<<label<<" triangles="<<mask.triangles<<" work="<<mask.work
                <<" rockLines="<<mask.rockLines.size()<<" bankLines="<<mask.bankLines.size()<<" counts=";
            for(auto n:mask.counts)std::cout<<n<<",";std::cout<<"\n";
            uint64_t hash=1469598103934665603ull;
            const auto* bytes=reinterpret_cast<const unsigned char*>(mask.cells.data());
            for(size_t k=0;k<mask.cells.size()*sizeof(XMFLOAT4);++k){hash^=bytes[k];hash*=1099511628211ull;}
            std::cout<<"G4_MASK_HASH="<<label<<" value="<<hash<<"\n";
            std::ofstream csv(out/(label+"-waterline.csv"));csv<<"kind,ax,az,bx,bz\n";
            for(auto line:mask.rockLines)csv<<"rock,"<<line.a.x<<","<<line.a.y<<","<<line.b.x<<","<<line.b.y<<"\n";
            for(auto line:mask.bankLines)csv<<"bank,"<<line.a.x<<","<<line.a.y<<","<<line.b.x<<","<<line.b.y<<"\n";
        };
        auto variant=[&](int n){
            for(size_t i=0;i<rocks.size();++i){
                auto* t=scene.transforms.GetComponent(rocks[i]);*t=originalTransforms[i];
                scene.objects.GetComponent(rocks[i])->SetRenderable(originalVisible[i]);
                if(n==1||n==3||n==4){
                    const float dx=n==1?2.f:0.f,dy=n==3?20.f:n==4?-20.f:0.f;
                    XMMATRIX parent=XMMatrixIdentity();
                    if(const auto* h=scene.hierarchy.GetComponent(rocks[i]))parent=scene.ComputeEntityMatrixRecursive(h->parentID);
                    const auto local=XMLoadFloat4x4(&originalWorld[i])*XMMatrixTranslation(dx,dy,0)*XMMatrixInverse(nullptr,parent);
                    XMVECTOR scale,rotation,translation;Check(XMMatrixDecompose(&scale,&rotation,&translation,local),"Diagnostic world transform cannot decompose");
                    XMStoreFloat3(&t->scale_local,scale);XMStoreFloat4(&t->rotation_local,rotation);XMStoreFloat3(&t->translation_local,translation);
                }
                if(n==2){t->scale_local.x*=1.4f;t->scale_local.y*=.7f;
                    XMStoreFloat4(&t->rotation_local,XMQuaternionMultiply(XMLoadFloat4(&t->rotation_local),XMQuaternionRotationRollPitchYaw(.15f,.7f,0)));}
                if(n==5)scene.objects.GetComponent(rocks[i])->SetRenderable(false);
                t->SetDirty();t->UpdateTransform();
            }
        };
        rebuild("original");
        Check(mask.counts[river_g4::RockContact]>0&&mask.counts[river_g4::Bank]>0,"Real fixture requires both bank and rock contact");
        wi::graphics::GPUBuffer buffer;
        wi::graphics::Shader pixel;wi::vector<uint8_t> bytes;
        Check(wi::helper::FileRead((shaderDir/"RiverWaterlinePS.cso").generic_u8string(),bytes),"Flow CSO missing");
        Check(gpu->CreateShader(wi::graphics::ShaderStage::PS,bytes.data(),bytes.size(),&pixel),"Flow shader create failed");
        wi::renderer::CustomShader custom;custom.name="River V2 G4 waterline classification";
        custom.filterMask=wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER;
        wi::graphics::PipelineStateDesc pso;
        pso.vs=wi::renderer::GetShader(wi::enums::VSTYPE_OBJECT_COMMON);pso.ps=&pixel;
        pso.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);
        pso.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_TRANSPARENT);
        pso.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
        Check(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_MAIN]),"Flow main PSO failed");
        pso.vs=wi::renderer::GetShader(wi::enums::VSTYPE_SHADOW_TRANSPARENT);
        pso.ps=wi::renderer::GetShader(wi::enums::PSTYPE_SHADOW_TRANSPARENT);
        pso.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENTSHADOW);
        pso.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEPTHREAD);
        pso.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_SHADOW_DOUBLESIDED);
        Check(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_SHADOW]),"Native shadow PSO failed");
        const int shaderID=wi::renderer::RegisterCustomShader(custom);
        FlowPath render;wi::scene::CameraComponent camera;
        camera.CreatePerspective(1264,761,.05f,1000,XMConvertToRadians(60));
        render.scene=&scene;render.camera=&camera;render.init(1264,761,96);
        render.setMotionBlurEnabled(false);render.setDepthOfFieldEnabled(false);render.setOcclusionCullingEnabled(false);
        render.ResizeBuffers();wi::renderer::SetTemporalAAEnabled(false);
        struct Guard{wi::Application& app;wi::graphics::GraphicsDevice* gpu;~Guard(){
            app.ActivatePath(nullptr);while(wi::renderer::IsPipelineCreationActive())Sleep(10);gpu->WaitForGPU();}}guard{app,gpu};
        wi::gui::Label heading;heading.Create("G4 waterline classification");heading.SetPos({12,12});heading.SetSize({1100,112});
        render.GetGUI().AddWidget(&heading);
        bool enabled=false,paused=false,live=true;int debug=0,view=0,currentVariant=0;
        auto bind=[&](){
            // Diagnostic control changes occur before app.Run, after all old GPU use drains.
            gpu->WaitForGPU();
            water=scene.materials.GetComponent(river);
            if(enabled){
                wi::graphics::GPUBufferDesc bd;bd.size=sizeof(params);bd.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
                bd.misc_flags=wi::graphics::ResourceMiscFlag::BUFFER_RAW;
                Check(gpu->CreateBuffer(&bd,&params,&buffer),"Flow parameter buffer upload failed");
                const int descriptor=gpu->GetDescriptorIndex(&buffer,wi::graphics::SubresourceType::SRV);
                Check(descriptor>=0,"Flow buffer SRV missing");
                water->SetCustomShaderID(shaderID);water->userdata={uint32_t(descriptor),0x52474c34,0,0};
            } else {water->DisableCustomShader();water->userdata=optics.userdata;}
            water->SetDirty();
            heading.SetText(std::string("RIVER V2 / G4  |  ")+(enabled?"WATERLINE CLASSIFICATION":"NATIVE WICKED WATER")+
                "  |  scenario "+std::to_string(currentVariant)+"\nF native/mask   M all/rock/bank   C scenario   1-5 viewpoints   Esc close\n"+
                "Blue open | Yellow bank | RED rock contact | Cyan submerged | Purple above/excluded | Dark occluded\n"+
                "Diagnostic colours on WATER only. NO FOAM. Exact triangle slices; snapshot rebuilt only on change.");
        };
        const XMFLOAT3 eyes[]={{0,1.2f,64},{0,28,48},{-12,2.2f,64},{12,2.2f,64},{0,8,68}};
        const XMFLOAT3 targets[]={{0,.15f,68},{0,.15f,68},{0,.15f,68},{0,.15f,68},{12,.15f,69}};
        auto pose=[&](int n){view=n;camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
            XMLoadFloat3(&eyes[n]),XMLoadFloat3(&targets[n]),XMVectorSet(0,1,0,0))));camera.UpdateCamera();};
        auto frames=[&](int n){for(int i=0;i<n;++i)app.Run();};
        auto capture=[&](const std::string& name,bool align=true){
            frames(20);
            if(align)while(gpu->GetFrameCount()%256!=0)frames(1);
            gpu->WaitForGPU();
            Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/(name+".png")).generic_u8string()),"Capture failed");
            Check(wi::helper::saveTextureToFile(render.GetRenderResult2D(),(out/(name+"-hud.png")).generic_u8string()),"HUD capture failed");
            std::cout<<"CAPTURE="<<name<<"\n";
        };
        pose(0);bind();app.ActivatePath(&render);frames(120);
        paused=true;render.frozen=true;wi::renderer::SetGameSpeed(0);bind();
        if(reopen){
            pose(0);capture("fresh-native-waterline");
            enabled=true;bind();pose(1);capture("fresh-classification-overhead");
            std::cout<<"G4_FRESH_PROCESS_REOPEN=PASS nativeDefaultOff=1\n";
        } else if(!interactive) {
            pose(1);capture("native-before");pose(0);capture("native-waterline");
            enabled=true;bind();pose(1);capture("classification-overhead");
            pose(0);capture("classification-waterline");
            pose(2);capture("classification-left");
            pose(3);capture("classification-right");
            for(int n=1;n<=5;++n){
                variant(n);currentVariant=n;rebuild("scenario-"+std::to_string(n));bind();pose(1);capture("scenario-"+std::to_string(n));
                if(n==3||n==4||n==5)Check(mask.counts[river_g4::RockContact]==0,"Non-intersecting/removed obstacle emitted rock contact");
            }
            variant(0);currentVariant=0;rebuild("restored");enabled=false;bind();pose(1);capture("native-restored");
            const auto saved=out/"native-waterline-proof.wiscene";
            {wi::Archive archive(saved.generic_u8string(),false);scene.Serialize(archive);}
            {wi::Archive archive(saved.generic_u8string(),true);wi::scene::Scene fresh;fresh.Serialize(archive);
                const auto* m=fresh.materials.GetComponent(fresh.Entity_FindByName("River Water 5"));
                Check(m&&!m->IsCustomShader()&&std::memcmp(&m->userdata,&optics.userdata,sizeof(m->userdata))==0,"Transient G4 descriptor serialized");}
            Check(scene.meshes.GetCount()==meshCount&&scene.objects.GetCount()==objectCount,"G4 changed scene geometry ownership");
            Check(wi::renderer::GetShaderErrorCount()==0,"Shader errors");
            std::cout<<"G4_NATIVE_GEOMETRY_PROOF=PASS nativeArchive=1 shader_errors=0\n";
        } else {
            enabled=true;bind();pose(0);ShowWindow(window,SW_SHOW);SetForegroundWindow(window);
            std::cout<<"G4_INTERACTIVE_READY=1\n";MSG msg{};
            while(live&&IsWindow(window)){
                bool change=false;
                while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
                    if(msg.hwnd==window&&msg.message==WM_KEYDOWN&&!(msg.lParam&(1<<30))){
                        const auto key=msg.wParam;
                        if(key=='F'){enabled=!enabled;change=true;}
                        if(key=='M'){debug=(debug+1)%3;params.control.w=debug;enabled=true;change=true;}
                        if(key=='C'){currentVariant=(currentVariant+1)%6;variant(currentVariant);
                            rebuild("live-scenario-"+std::to_string(currentVariant));params.control.w=debug;change=true;}
                        if(key>='1'&&key<='5')pose(int(key-'1'));
                        if(key==VK_ESCAPE)live=false;
                    }
                    TranslateMessage(&msg);DispatchMessageW(&msg);
                }
                if(change)bind();app.Run();Sleep(2);
            }
        }
        render.GetGUI().RemoveWidget(&heading);return 0;
    }catch(const std::exception& e){std::cerr<<"G4_FAIL: "<<e.what()<<"\n";return 1;}
}
