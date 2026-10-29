#pragma once
#if defined(_WIN32)
#include "../Tools/night_sky/CloudStudioReplay.h"
#include "CloudOpacityReadback.h"
inline int RunCloudStudioReplayProof(int argc,char** argv)
{
    namespace fs=std::filesystem;
    using namespace renegade::cloudqa;
    wi::arguments::Parse(argc,argv);
    if(argc<5)return 101;
    const auto output=fs::absolute(fs::u8path(argv[2]));
    const auto input=fs::absolute(fs::u8path(argv[4]));
    ReplayState state;if(!state.Read(input/"state.txt"))return 102;
    if(state.referenceQuality!=wi::arguments::HasArgument("cloud-reference-quality"))return 103;
    fs::create_directories(output);
    HWND window=CreateWindowExW(0,L"STATIC",L"Renegade actual Studio cloud replay",
        WS_OVERLAPPEDWINDOW,0,0,state.width,state.height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 104;
    auto* app=new wi::Application();app->allow_hdr=false;app->SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    auto* scene=new wi::scene::Scene();
    wi::Archive archive((input/"scene.wiscene").generic_string());
    if(!archive.IsOpen())return 105;
    scene->Serialize(archive);archive=wi::Archive();
    renegade::bridge::RestoreNightSky(*scene);
    auto* render=new wi::RenderPath3D();auto* camera=new wi::scene::CameraComponent();
    render->scene=scene;render->camera=camera;render->init(state.width,state.height,96);
    state.Apply(*render,*camera);render->ResizeBuffers();state.Apply(*render,*camera);
    auto* device=wi::graphics::GetDevice();
    using namespace wi::graphics;
    GPUQueryHeap query;GPUQueryHeapDesc qd;qd.type=GpuQueryType::TIMESTAMP;qd.query_count=2;
    GPUBuffer timestamps;GPUBufferDesc bd;bd.usage=Usage::READBACK;bd.size=2*sizeof(uint64_t);
    if(!device->CreateQueryHeap(&qd,&query)||!device->CreateBuffer(&bd,nullptr,&timestamps))return 106;
    double gpuMs=0;
    for(int i=0;i<128;++i)
    {
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
        const auto begin=device->BeginCommandList();device->QueryEnd(&query,0,begin);
        render->PreUpdate();render->Update(0);render->PreRender();render->Render();
        const auto end=device->BeginCommandList();device->QueryEnd(&query,1,end);
        device->QueryResolve(&query,0,2,&timestamps,0,end);
        device->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();device->WaitForGPU();
        const auto* values=static_cast<const uint64_t*>(timestamps.mapped_data);
        if(i>=120&&values&&values[1]>=values[0])
            gpuMs+=double(values[1]-values[0])*1000.0/double(device->GetTimestampFrequency())/8;
    }
    const auto& clouds=render->volumetriccloudResources;
    const auto& opacity=clouds.texture_reproject[clouds.GetTemporalOutputIndex()];
    if(opacity.desc.width!=state.cloudWidth||opacity.desc.height!=state.cloudHeight)return 107;
    if(!wi::helper::saveTextureToFile(render->GetRenderResult3D(),(output/"replay.png").generic_string())||
        !wi::helper::saveTextureToFile(opacity,(output/"replay-opacity.png").generic_string())||
        !SaveCloudOpacityGrid(opacity,output/"replay-opacity.pgm"))return 108;
    std::ofstream info(output/"replay.txt");
    const auto& c=scene->weather.volumetricCloudParameters;
    info<<"adapter="<<device->GetAdapterName()<<"\nshader_format="<<int(device->GetShaderFormat())
        <<"\ngpu_frame_ms="<<gpuMs<<"\nformation="<<c.layerFirst.padding0
        <<"\ncoverage="<<c.layerFirst.coverageAmount<<"\nbase="<<c.cloudStartHeight
        <<"\ndepth="<<c.cloudThickness<<"\ncloud_width="<<opacity.desc.width
        <<"\ncloud_height="<<opacity.desc.height<<"\nanimation="<<c.animationMultiplier<<"\n";
    // Moon visibility diagnostics on a serialized Studio QA scene only.
    // The camera and Moon authoring are transient; original scene bytes and
    // the real owner's Studio windows/projects are never modified.
    if(wi::arguments::HasArgument("cloud-moon-studio-check"))
    {
        if(scene->weathers.GetCount()==0)return 110;
        const auto weatherEntity=scene->weathers.GetEntity(0);
        auto* wc=scene->weathers.GetComponent(weatherEntity);
        const auto initial=renegade::bridge::CaptureMoon(*scene,weatherEntity);
        std::ofstream moonLog(output/"moon-studio-check.txt");
        moonLog<<"saved_moon_enabled="<<initial.enabled
            <<"\nsaved_moon_auto="<<initial.automatic
            <<"\nsaved_moon_brightness="<<initial.brightness
            <<"\nsaved_realistic_sky="<<wc->IsRealisticSky()<<"\n";
        const auto originalSunState=renegade::bridge::CaptureSun(*scene,weatherEntity);
        auto sunState=originalSunState;
        renegade::bridge::SetSunTime(sunState,0);
        renegade::bridge::ApplySun(*scene,weatherEntity,sunState);
        auto moon=initial;moon.enabled=true;moon.automatic=false;
        moon.phase=.5f;moon.brightness=.9f;moon.diameter=8.0f;
        XMFLOAT3 forward;
        XMStoreFloat3(&forward,XMVector3Normalize(XMVector3TransformNormal(
            XMVectorSet(0,0,1,0),XMLoadFloat4x4(&state.inverseView))));
        moon.azimuth=XMConvertToDegrees(std::atan2(forward.x,forward.z));
        moon.elevation=XMConvertToDegrees(std::asin(std::clamp(forward.y,-1.0f,1.0f)));
        const auto savedMoonDirection=renegade::bridge::MoonDirection(initial,originalSunState.direction);
        const float savedFacing=std::clamp(savedMoonDirection.x*forward.x +
            savedMoonDirection.y*forward.y + savedMoonDirection.z*forward.z,-1.0f,1.0f);
        moonLog<<"saved_moon_y="<<savedMoonDirection.y
            <<"\nsaved_moon_camera_separation_degrees="
            <<XMConvertToDegrees(std::acos(savedFacing))<<"\n"
            <<"fixed_moon_azimuth="<<moon.azimuth
            <<"\nfixed_moon_elevation="<<moon.elevation<<"\n";
        if(!renegade::bridge::StoreMoon(*scene,weatherEntity,moon))return 111;
        const auto frame=[&]()
        {
            wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
            render->PreUpdate();render->Update(0);render->PreRender();render->Render();
            device->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();device->WaitForGPU();
        };
        wc->SetRealisticSky(true);
        wc->SetVolumetricClouds(false);
        scene->weather=*wc;
        for(int i=0;i<32;++i)frame();
        if(!wi::helper::saveTextureToFile(render->GetRenderResult3D(),
            (output/"moon-clear-replay.png").generic_string()))return 112;
        wc->SetVolumetricClouds(true);scene->weather=*wc;
        render->volumetriccloudResources.ResetFrame();
        for(int i=0;i<32;++i)frame();
        if(!wi::helper::saveTextureToFile(render->GetRenderResult3D(),
            (output/"moon-with-clouds-replay.png").generic_string()))return 113;
        moonLog<<"source_scene_untouched=true\n";
        moonLog.close();
    }
    info.close();if(!info)return 109;
    std::cout<<"Actual Studio scene/camera replay saved; "<<gpuMs<<" ms GPU frame"<<std::endl;
    std::fflush(stdout);std::_Exit(0);
}
#endif
