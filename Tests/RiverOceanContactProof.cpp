#include "renegade/bridge/SceneService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cmath>
using namespace DirectX;
namespace fs=std::filesystem;
static void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv)
{
    if(argc<3 || argc>4)return 2;
    const fs::path out=fs::absolute(argv[1]);
    const fs::path source=fs::absolute(argv[2]);
    const bool interactive=argc==4 && std::string(argv[3])=="--interactive";
    std::cout.setf(std::ios::unitbuf);
    HWND window=CreateWindowExW(0,L"STATIC",L"Renegade Ocean Contact Foam - Real River",
        WS_OVERLAPPEDWINDOW,0,0,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    struct App : wi::Application {void Initialize()override{wi::Application::Initialize();alwaysactive=true;}} app;
    app.allow_hdr=false;app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    auto* gpu=wi::graphics::GetDevice();
    struct Drain{~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}} drain;
    try{
        wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
        renegade::bridge::SceneService service;
        Check(service.LoadScene(source.generic_u8string()),service.LastError().c_str());
        auto& scene=service.GetScene();
        Check(scene.terrains.GetCount()==1,"expected real native terrain");
        // Remove only rejected test overlays in this disposable loaded copy.
        for(const char* name:{"Rock Foam Contact - Test","Rock Foam Wake - Test"})
            if(auto e=scene.Entity_FindByName(name);e!=wi::ecs::INVALID_ENTITY)scene.Entity_Remove(e);
        const auto river=scene.Entity_FindByName("River Water 5");
        auto* m=scene.materials.GetComponent(river);
        Check(m && m->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER,"native flat river missing");
        Check(m->textures[wi::scene::MaterialComponent::NORMALMAP].resource.GetTexture().IsValid(),"native water normal missing");
        const auto optics=*m;
        for(size_t i=0;i<scene.materials.GetCount();++i)
            if(scene.materials[i].shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER)
                scene.materials[i].texAnimFrameRate=0;
        const auto opaqueCount=scene.objects.GetCount();
        wi::scene::CameraComponent camera;
        camera.CreatePerspective(1264,761,.05f,1000,XMConvertToRadians(60));
        wi::RenderPath3D render;render.scene=&scene;render.camera=&camera;
        render.init(1264,761,96);render.setMotionBlurEnabled(false);
        render.setDepthOfFieldEnabled(false);render.setOcclusionCullingEnabled(false);
        render.ResizeBuffers();
        struct Guard{wi::Application& app;~Guard(){app.ActivatePath(nullptr);}} guard{app};
        wi::renderer::SetTemporalAAEnabled(false);
        app.ActivatePath(&render);
        auto pose=[&](int angle){
            const XMVECTOR eyes[]={XMVectorSet(0,1.8f,64,1),XMVectorSet(3,1.8f,69.5f,1),
                XMVectorSet(0,7,64,1),XMVectorSet(-4,1.2f,67,1),XMVectorSet(0,1.2f,65.8f,1)};
            camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
                eyes[angle],XMVectorSet(0,.15f,68,1),XMVectorSet(0,1,0,0))));
            camera.UpdateCamera();
        };
        pose(0);
        for(int i=0;i<100;++i){app.Run();Sleep(4);}
        while(wi::renderer::IsPipelineCreationActive())Sleep(10);
        wi::renderer::SetGameSpeed(0);
        auto capture=[&](const std::string& tag){
            for(int i=0;i<20;++i){app.Run();Sleep(2);}gpu->WaitForGPU();
            Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/(tag+".png")).generic_u8string()),"capture failed");
        };
        capture("native-player");
        wi::graphics::Shader pixel;wi::vector<uint8_t> bytes;
        Check(wi::helper::FileRead((out/"RiverOceanContactPS.cso").generic_u8string(),bytes),"CSO missing");
        Check(gpu->CreateShader(wi::graphics::ShaderStage::PS,bytes.data(),bytes.size(),&pixel),"PS create failed");
        wi::renderer::CustomShader shader;shader.name="River Ocean Contact Proof";
        shader.filterMask=wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER;
        wi::graphics::PipelineStateDesc desc;
        desc.vs=wi::renderer::GetShader(wi::enums::VSTYPE_OBJECT_COMMON);desc.ps=&pixel;
        desc.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);
        desc.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_TRANSPARENT);
        desc.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
        Check(gpu->CreatePipelineState(&desc,&shader.pso[wi::enums::RENDERPASS_MAIN]),"PSO create failed");
        const int id=wi::renderer::RegisterCustomShader(shader);
        float width=1.f;
        auto bind=[&](bool enabled){
            auto* material=scene.materials.GetComponent(river);
            if(enabled){material->SetCustomShaderID(id);std::memcpy(&material->userdata.x,&width,sizeof(width));}
            else{material->DisableCustomShader();material->userdata=optics.userdata;}
            material->SetDirty();
        };
        bind(true);capture("ocean-original-player");
        width=.25f;bind(true);capture("ocean-narrow-player");
        pose(1);bind(false);capture("native-reverse");bind(true);capture("ocean-narrow-reverse");
        pose(2);bind(false);capture("native-high");bind(true);capture("ocean-narrow-high");
        pose(3);bind(false);capture("native-low");bind(true);capture("ocean-narrow-low");
        pose(0);capture("animation-start");
        wi::renderer::SetGameSpeed(1);
        for(int i=0;i<100;++i){app.Run();Sleep(16);}
        wi::renderer::SetGameSpeed(0);capture("animation-end");
        bind(false);
        // Keep native scene data only: shader IDs/userdata are process-local.
        scene.materials.GetComponent(river)->texAnimFrameRate=optics.texAnimFrameRate;
        const auto saved=out/"native-contact-scene.wiscene";
        {wi::Archive ar(saved.generic_u8string(),false);scene.Serialize(ar);}
        {wi::Archive ar(saved.generic_u8string(),true);wi::scene::Scene fresh;fresh.Serialize(ar);
            const auto* fm=fresh.materials.GetComponent(fresh.Entity_FindByName("River Water 5"));
            Check(fm && !fm->IsCustomShader() && fm->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER,"save contaminated native water");
            Check(fm->textures[wi::scene::MaterialComponent::NORMALMAP].resource.GetTexture().IsValid(),"reopened normal missing");
        }
        Check(scene.objects.GetCount()==opaqueCount,"foam must not add planes");
        Check(wi::renderer::GetShaderErrorCount()==0,"shader errors");
        std::cout<<"OCEAN_CONTACT_NATIVE_SCENE=PASS angles=4 no_overlay_meshes=1 save_reopen=PASS shader_errors=0\n";
        if(interactive){
            pose(4);bind(true);wi::renderer::SetGameSpeed(1);
            ShowWindow(window,SW_SHOW);SetForegroundWindow(window);
            wi::gui::Label heading;heading.Create("Ocean Contact Test");
            heading.SetText("OCEAN CONTACT FOAM - REAL RIVER\nF toggle | 1-5 viewpoints | +/- width\nWASD move | arrows look | Esc close");
            heading.SetPos({12,12});heading.SetSize({370,72});render.GetGUI().AddWidget(&heading);
            bool enabled=true;MSG msg{};
            XMFLOAT3 eye{0,1.2f,65.8f};float yaw=0,pitch=-.445f;
            while(IsWindow(window)){
                while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
                if(!IsWindow(window))break;
                if(GetAsyncKeyState(VK_ESCAPE)&1)break;
                for(int i=0;i<5;++i)if(GetAsyncKeyState('1'+i)&1){
                    pose(i);
                    const XMFLOAT3 positions[]={{0,1.8f,64},{3,1.8f,69.5f},{0,7,64},{-4,1.2f,67},{0,1.2f,65.8f}};
                    eye=positions[i];yaw=std::atan2(-eye.x,68-eye.z);
                    pitch=std::atan2(.15f-eye.y,std::hypot(eye.x,68-eye.z));
                }
                if(GetAsyncKeyState('F')&1){enabled=!enabled;bind(enabled);}
                if(GetAsyncKeyState(VK_OEM_PLUS)&1){width=std::min(2.f,width+.05f);bind(enabled);}
                if(GetAsyncKeyState(VK_OEM_MINUS)&1){width=std::max(.05f,width-.05f);bind(enabled);}
                bool move=false;
                if(GetAsyncKeyState(VK_LEFT)&0x8000){yaw-=.012f;move=true;}
                if(GetAsyncKeyState(VK_RIGHT)&0x8000){yaw+=.012f;move=true;}
                if(GetAsyncKeyState(VK_UP)&0x8000){pitch=std::min(.8f,pitch+.012f);move=true;}
                if(GetAsyncKeyState(VK_DOWN)&0x8000){pitch=std::max(-1.4f,pitch-.012f);move=true;}
                if(GetAsyncKeyState('W')&0x8000){eye.x+=std::sin(yaw)*.035f;eye.z+=std::cos(yaw)*.035f;move=true;}
                if(GetAsyncKeyState('S')&0x8000){eye.x-=std::sin(yaw)*.035f;eye.z-=std::cos(yaw)*.035f;move=true;}
                if(GetAsyncKeyState('A')&0x8000){eye.x-=std::cos(yaw)*.035f;eye.z+=std::sin(yaw)*.035f;move=true;}
                if(GetAsyncKeyState('D')&0x8000){eye.x+=std::cos(yaw)*.035f;eye.z-=std::sin(yaw)*.035f;move=true;}
                if(move){auto e=XMLoadFloat3(&eye);auto dir=XMVectorSet(std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch),0);
                    camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(e,e+dir,XMVectorSet(0,1,0,0))));camera.UpdateCamera();}
                app.Run();Sleep(8);
            }
            render.GetGUI().RemoveWidget(&heading);
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
