#include "renegade/bridge/RiverUnderwaterService.h"
#include "renegade/bridge/SceneService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdexcept>
using namespace DirectX;
using namespace renegade::bridge;
namespace fs=std::filesystem;
void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct RiverProofPath : wi::RenderPath3D
{
    RiverUnderwaterRenderer water;
    bool enabled=true;
    void PreRender() override
    {
        water.Update(*this);
        if(!enabled)custom_post_processes.clear();
        wi::RenderPath3D::PreRender();
    }
};
int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    const auto output=fs::absolute(fs::u8path(argv[2]));
    if(fs::exists(output)&&!fs::is_empty(output))return 2;
    fs::create_directories(output);
    HWND window=CreateWindowExW(0,L"STATIC",L"River Underwater DX12 Proof",WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
    auto* device=wi::graphics::GetDevice();
    try
    {
        SceneService scenes;Require(scenes.LoadScene(fs::absolute(fs::u8path(argv[1])).generic_u8string()),"fixture scene load");
        auto& scene=scenes.GetScene();
        wi::scene::CameraComponent camera;camera.CreatePerspective(960,640,.05f,500,XMConvertToRadians(60));
        RiverProofPath path;path.scene=&scene;path.camera=&camera;path.init(960,640,96);
        path.setMotionBlurEnabled(false);path.setOcclusionCullingEnabled(false);path.setDepthOfFieldEnabled(false);path.ResizeBuffers();
        struct Disarm
        {
            wi::Application& app;wi::scene::Scene& scene;
            ~Disarm()
            {
                // Join native producer jobs before scene/path resource destruction.
                for(std::size_t i=0;i<scene.terrains.GetCount();++i)scene.terrains[i].Generation_Cancel();
                while(wi::renderer::IsPipelineCreationActive())Sleep(10);
                app.ActivatePath(nullptr);wi::graphics::GetDevice()->WaitForGPU();
            }
        } disarm{app,scene};
        app.ActivatePath(&path);wi::renderer::SetTemporalAAEnabled(false);
        auto frames=[&](){for(int i=0;i<24;++i){app.Run();Sleep(8);}device->WaitForGPU();};
        frames();
        const auto prisms=CaptureRiverWaterPrisms(scene);Require(!prisms.empty(),"fixture has no linked river volume");
        XMFLOAT3 point{};bool found=false;
        // Use centre of a real river triangle whose terrain has sufficient clearance.
        for(const auto& prism:prisms)
        {
            XMFLOAT3 candidate{(prism.minimum.x+prism.maximum.x)*.5f,prism.maximum.y-.35f,(prism.minimum.z+prism.maximum.z)*.5f};
            float a,b;
            if(!IntersectRiverWaterPrism(prism,candidate,{0,1,0},100,a,b)||a>.0001f)continue;
            auto hit=wi::scene::Pick(wi::primitive::Ray(XMFLOAT3(candidate.x,candidate.y+1000,candidate.z),XMFLOAT3(0,-1,0)),wi::enums::FILTER_TERRAIN,~0u,scene);
            if(hit.entity!=wi::ecs::INVALID_ENTITY&&hit.position.y<candidate.y-.1f){point=candidate;found=true;break;}
        }
        Require(found,"no submerged test position above actual riverbed");
        auto pose=[&](float y,float pitch){camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(point.x,y,point.z,1),XMVectorSet(point.x+5,y+pitch,point.z+5,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();};
        auto capture=[&](const char* name){frames();Require(path.water.Error().empty(),path.water.Error().c_str());Require(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(output/name).generic_u8string()),"capture failed");};
        pose(point.y,0);capture("underwater-enabled.png");Require(!path.custom_post_processes.empty(),"underwater pass not installed");
        path.enabled=false;capture("underwater-disabled.png");
        path.enabled=true;pose(point.y+.35f,0);capture("waterline.png");
        pose(point.y+1.5f,0);capture("above-water.png");Require(path.custom_post_processes.empty(),"above-water camera still dispatches underwater");
        std::cout<<"GPU_UNDERWATER=PASS prisms="<<prisms.size()<<" point="<<point.x<<","<<point.y<<","<<point.z<<"\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
