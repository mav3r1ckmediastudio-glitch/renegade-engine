#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/RiverFlowFieldService.h"
#include <Windows.h>
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
struct Params {XMFLOAT4 bounds;XMUINT4 control;XMFLOAT4 settings;};
static_assert(sizeof(Params)==48);
struct FlowPath:wi::RenderPath3D {bool frozen=false;void Update(float dt)override{wi::RenderPath3D::Update(frozen?0.f:dt);}};
int main(int argc,char** argv)
{
    if(argc<4||argc>5)return 2;
    const bool interactive=argc==5&&std::string(argv[4])=="--interactive";
    std::cout.setf(std::ios::unitbuf);
    const auto out=fs::absolute(argv[1]),shaderDir=fs::absolute(argv[3]);
    if(fs::exists(out)&&!fs::is_empty(out))return 2;
    fs::create_directories(out);
    HWND window=CreateWindowExW(0,L"STATIC",L"Renegade River V2 - Directed Water G3",
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
        const auto optics=*water;
        const auto meshCount=scene.meshes.GetCount(),objectCount=scene.objects.GetCount();
        std::shared_ptr<const RiverFlowField> field;std::string error;
        Check(BuildRiverFlowField(scene,river,{},field,error),error);
        float minX=INFINITY,minZ=INFINITY,maxX=-INFINITY,maxZ=-INFINITY;
        for(const auto& row:field->NativeRibbon())for(auto p:{row.rightBank,row.leftBank}){
            minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);
            minZ=std::min(minZ,p.z);maxZ=std::max(maxZ,p.z);
        }
        minX-=.25f;minZ-=.25f;maxX+=.25f;maxZ+=.25f;
        constexpr uint32_t width=256,height=128;
        std::vector<XMFLOAT4> raster(width*height);
        std::size_t valid=0;
        float maxSpeed=0;
        for(uint32_t z=0;z<height;++z)for(uint32_t x=0;x<width;++x){
            const float wx=minX+(x+.5f)/width*(maxX-minX),wz=minZ+(z+.5f)/height*(maxZ-minZ);
            const auto q=field->Evaluate({wx,wz});
            if(q.valid){raster[z*width+x]={q.directionXZ.x,q.directionXZ.y,q.speedMetersPerSecond,1};
                ++valid;maxSpeed=std::max(maxSpeed,q.speedMetersPerSecond);}
        }
        Check(valid>100&&valid<raster.size(),"Invalid velocity raster coverage");
        wi::graphics::Texture texture;
        wi::graphics::TextureDesc td;td.width=width;td.height=height;
        td.format=wi::graphics::Format::R32G32B32A32_FLOAT;td.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
        wi::graphics::SubresourceData initial;initial.data_ptr=raster.data();initial.row_pitch=width*sizeof(XMFLOAT4);
        initial.slice_pitch=initial.row_pitch*height;
        Check(gpu->CreateTexture(&td,&initial,&texture),"DX12 float32 velocity texture unavailable");
        const int textureIndex=gpu->GetDescriptorIndex(&texture,wi::graphics::SubresourceType::SRV);
        Check(textureIndex>=0,"Velocity texture SRV missing");
        const float tileMetres=field->Samples().front().widthMeters/optics.texMulAdd.x;
        Check(std::abs(tileMetres-4)<.01f,"Proof expects native four-metre normal tiles");
        Params params{{minX,minZ,maxX-minX,maxZ-minZ},{uint32_t(textureIndex),Bits(-1),Bits(1),0},
            {tileMetres,std::min(4.f,tileMetres/std::max(maxSpeed,.001f)),0,1}};
        std::cout<<"G3_FLOAT32_RASTER=PASS size=256x128 valid="<<valid<<" cpuHash="<<field->DeterministicHash()
            <<" tileMetres="<<tileMetres<<" period="<<params.settings.y<<"\n";
        wi::graphics::GPUBuffer buffer;
        wi::graphics::Shader pixel;wi::vector<uint8_t> bytes;
        Check(wi::helper::FileRead((shaderDir/"RiverFlowNormalPS.cso").generic_u8string(),bytes),"Flow CSO missing");
        Check(gpu->CreateShader(wi::graphics::ShaderStage::PS,bytes.data(),bytes.size(),&pixel),"Flow shader create failed");
        wi::renderer::CustomShader custom;custom.name="River V2 G3 normal advection proof";
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
        wi::gui::Label heading;heading.Create("G3 flow normal status");heading.SetPos({12,12});heading.SetSize({1100,112});
        render.GetGUI().AddWidget(&heading);
        bool enabled=false,reversed=false,paused=false,live=true;int debug=0,view=0;
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
                water->SetCustomShaderID(shaderID);water->userdata={uint32_t(descriptor),0x52464c33,0,0};
            } else {water->DisableCustomShader();water->userdata=optics.userdata;}
            water->SetDirty();
            heading.SetText(std::string("RIVER V2 / G3  |  ")+(enabled?(debug?"VELOCITY DEBUG":"DIRECTED NORMALS"):"NATIVE WICKED WATER")+
                "  |  "+(reversed?"REVERSED":"FORWARD")+"  |  "+(paused?"PAUSED":"RUNNING")+
                "\nF native/flow   R reverse   V velocity map   Space pause   N step 0.05s   1-5 viewpoints\n"+
                "Original WATER optics / normal strength / 4m scale retained. Flow 1m/s. No foam or obstacle response.");
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
        if(!interactive) {
            pose(1);capture("native-before");
            enabled=true;params.settings.w=0;bind();capture("native-fallback");
            params.settings.w=1;params.control.y=Bits(2);bind();capture("flow-t2");
            params.control.y=Bits(2.25f);bind();capture("flow-t225");
            params.control.z=Bits(-1);bind();capture("flow-reverse-t225");
            params.control.z=Bits(1);params.control.w=1;bind();capture("velocity-debug");
            params.control.w=0;enabled=false;bind();capture("native-restored");
            pose(0);capture("native-waterline");
            enabled=true;params.control.y=Bits(2);bind();capture("flow-waterline");
            for(float t:{3.999f,4.f,4.001f}) {
                params.control.y=Bits(t);bind();capture("seam-"+std::to_string(t));
            }
            params.control.y=Bits(2);bind();
            for(int i=0;i<30;++i) {
                params.control.y=Bits(2+i*.1f);bind();frames(2);gpu->WaitForGPU();
                Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
                    (out/("animation-"+std::to_string(i)+".png")).generic_u8string()),"Animation capture failed");
            }
            enabled=false;bind();
            const auto saved=out/"native-flow-proof.wiscene";
            {wi::Archive archive(saved.generic_u8string(),false);scene.Serialize(archive);}
            {wi::Archive archive(saved.generic_u8string(),true);wi::scene::Scene fresh;fresh.Serialize(archive);
                const auto* m=fresh.materials.GetComponent(fresh.Entity_FindByName("River Water 5"));
                Check(m&&!m->IsCustomShader()&&std::memcmp(&m->userdata,&optics.userdata,sizeof(m->userdata))==0,
                    "Transient flow descriptor serialized");
            }
            Check(scene.meshes.GetCount()==meshCount&&scene.objects.GetCount()==objectCount,"Flow added scene geometry");
            Check(water->baseColor.x==optics.baseColor.x&&water->baseColor.w==optics.baseColor.w&&
                water->refraction==optics.refraction&&water->normalMapStrength==optics.normalMapStrength&&
                water->texMulAdd.x==optics.texMulAdd.x,"Optical material changed");
            Check(wi::renderer::GetShaderErrorCount()==0,"Shader errors");
            std::cout<<"G3_GPU_NORMAL_PROOF=PASS nativeArchive=1 shader_errors=0\n";
        } else {
            enabled=true;paused=false;render.frozen=false;wi::renderer::SetGameSpeed(1);bind();pose(0);
            ShowWindow(window,SW_SHOW);SetForegroundWindow(window);
            std::cout<<"G3_INTERACTIVE_READY=1\n";
            MSG msg{};
            while(live&&IsWindow(window)){
                bool change=false;
                while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
                    if(msg.hwnd==window&&msg.message==WM_KEYDOWN&&!(msg.lParam&(1<<30))){
                        const auto key=msg.wParam;
                        if(key=='F'){enabled=!enabled;change=true;}
                        if(key=='R'){reversed=!reversed;params.control.z=Bits(reversed?-1.f:1.f);change=true;}
                        if(key=='V'){debug=1-debug;params.control.w=debug;enabled=true;change=true;}
                        if(key>='1'&&key<='5')pose(int(key-'1'));
                        if(key==VK_SPACE){
                            paused=!paused;render.frozen=paused;wi::renderer::SetGameSpeed(paused?0:1);
                            if(paused)params.control.y=Bits(render.frameCB.time+params.settings.z);
                            else {float time;std::memcpy(&time,&params.control.y,4);params.settings.z=time-render.frameCB.time;params.control.y=Bits(-1);}
                            change=true;
                        }
                        if(key=='N'&&paused){float time;std::memcpy(&time,&params.control.y,4);
                            params.control.y=Bits(time+.05f);change=true;}
                        if(key==VK_ESCAPE)live=false;
                    }
                    TranslateMessage(&msg);DispatchMessageW(&msg);
                }
                if(change)bind();app.Run();Sleep(2);
            }
        }
        render.GetGUI().RemoveWidget(&heading);return 0;
    }catch(const std::exception& e){std::cerr<<"G3_FAIL: "<<e.what()<<"\n";return 1;}
}
