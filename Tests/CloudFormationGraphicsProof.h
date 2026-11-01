#pragma once
#if defined(_WIN32)
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include "CloudDensityVolumeAudit.h"
#include "CloudOpacityReadback.h"
inline int RunCloudFormationGraphicsProof(int argc,char** argv)
{
    namespace fs=std::filesystem;
    using namespace renegade::bridge;
    wi::arguments::Parse(argc,argv);
    const fs::path output=fs::absolute(fs::u8path(argv[2]));fs::create_directories(output);
    const int width=argc>4 ? std::atoi(argv[4]) : 1920;
    if(width!=1920 && width!=2560) return 61;
    const int height=width*9/16;
    HWND window=CreateWindowExW(0,L"STATIC",L"Renegade Cloud Formation GPU proof",
        WS_OVERLAPPEDWINDOW,0,0,width,height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window) return 62;
    auto* app=new wi::Application();app->allow_hdr=false;app->SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    wi::renderer::SetTemporalAAEnabled(wi::arguments::HasArgument("cloud-studio-taa"));
    auto* scene=new wi::scene::Scene();
    const auto entity=wi::ecs::CreateEntity();auto& weather=scene->weathers.Create(entity);
    weather.SetRealisticSky(true);weather.SetVolumetricClouds(true);
    weather.SetVolumetricCloudsCastShadow(true);weather.skyExposure=1;weather.stars=0;
    auto& params=weather.volumetricCloudParameters;
    params.cloudStartHeight=1500;params.cloudThickness=5000;
    params.layerSecond.coverageAmount=0;params.animationMultiplier=0;
    scene->weather=weather;(void)EnsurePrimarySunLight(*scene,entity);
    auto* render=new wi::RenderPath3D();auto* camera=new wi::scene::CameraComponent();
    render->scene=scene;render->camera=camera;render->init(width,height,96);
    render->setDepthOfFieldEnabled(false);render->setMotionBlurEnabled(false);
    render->ResizeBuffers();camera->CreatePerspective(width,height,.1f,1000,XMConvertToRadians(70));
    auto* device=wi::graphics::GetDevice();
    std::ofstream backend(output/"backend.txt");
    backend<<"adapter="<<device->GetAdapterName()<<"\nshader_format="<<int(device->GetShaderFormat())
           <<"\nrequested="<<(wi::arguments::HasArgument("vulkan")?"vulkan":"dx12")<<"\n";
    backend.close();
    if(std::string(argv[1])=="--density-audit")
    {
        using namespace wi::graphics;
        Texture texture;TextureDesc desc;desc.type=TextureDesc::Type::TEXTURE_3D;
        desc.width=64;desc.height=64;desc.depth=64;desc.mip_levels=1;
        desc.format=Format::R8G8B8A8_UNORM;
        desc.bind_flags=BindFlag::SHADER_RESOURCE|BindFlag::UNORDERED_ACCESS;
        desc.layout=ResourceState::SHADER_RESOURCE_COMPUTE;
        if(!device->CreateTexture(&desc,nullptr,&texture)) return 68;
        Shader shader;
        if(!wi::renderer::LoadShader(ShaderStage::CS,shader,"volumetricCloud_shapenoiseCS.cso")) return 69;
        auto cmd=device->BeginCommandList();
        auto barrier=GPUBarrier::Image(&texture,desc.layout,ResourceState::UNORDERED_ACCESS);
        device->Barrier(&barrier,1,cmd);device->BindComputeShader(&shader,cmd);
        device->BindUAV(&texture,0,cmd);device->Dispatch(8,8,8,cmd);
        barrier=GPUBarrier::Image(&texture,ResourceState::UNORDERED_ACCESS,desc.layout);
        device->Barrier(&barrier,1,cmd);device->SubmitCommandLists();device->WaitForGPU();
        wi::vector<uint8_t> bytes;if(!wi::helper::saveTextureToMemory(texture,bytes)) return 70;
        std::ofstream raw(output/"native-shape-rgba8-64x64x64.bin",std::ios::binary);
        raw.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());raw.close();
        Texture detailTexture;desc.width=32;desc.height=32;desc.depth=32;
        if(!device->CreateTexture(&desc,nullptr,&detailTexture)) return 83;
        if(!wi::renderer::LoadShader(ShaderStage::CS,shader,"volumetricCloud_detailnoiseCS.cso")) return 84;
        cmd=device->BeginCommandList();
        barrier=GPUBarrier::Image(&detailTexture,desc.layout,ResourceState::UNORDERED_ACCESS);
        device->Barrier(&barrier,1,cmd);device->BindComputeShader(&shader,cmd);
        device->BindUAV(&detailTexture,0,cmd);device->Dispatch(4,4,4,cmd);
        barrier=GPUBarrier::Image(&detailTexture,ResourceState::UNORDERED_ACCESS,desc.layout);
        device->Barrier(&barrier,1,cmd);device->SubmitCommandLists();device->WaitForGPU();
        bytes.clear();if(!wi::helper::saveTextureToMemory(detailTexture,bytes)) return 85;
        std::ofstream fine(output/"native-detail-rgba8-32x32x32.bin",std::ios::binary);
        fine.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());fine.close();
        std::cout<<"Native base and detail volume audits saved"<<std::endl;
        std::fflush(stdout);std::_Exit(0);
    }
    wi::graphics::GPUQueryHeap query;
    wi::graphics::GPUQueryHeapDesc queryDesc;
    queryDesc.type=wi::graphics::GpuQueryType::TIMESTAMP;queryDesc.query_count=2;
    if(!device->CreateQueryHeap(&queryDesc,&query)) return 66;
    wi::graphics::GPUBuffer readback;wi::graphics::GPUBufferDesc readbackDesc;
    readbackDesc.usage=wi::graphics::Usage::READBACK;readbackDesc.size=2*sizeof(uint64_t);
    if(!device->CreateBuffer(&readbackDesc,nullptr,&readback)) return 67;
    double gpuMs=0;
    float frameDelta=0;
    const auto frame=[&]()
    {
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
        const auto begin=device->BeginCommandList();device->QueryEnd(&query,0,begin);
        render->PreUpdate();render->Update(frameDelta);render->PreRender();render->Render();
        const auto end=device->BeginCommandList();device->QueryEnd(&query,1,end);
        device->QueryResolve(&query,0,2,&readback,0,end);
        wi::graphics::GetDevice()->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();
        wi::graphics::GetDevice()->WaitForGPU();
        const auto* timestamps=static_cast<const uint64_t*>(readback.mapped_data);
        gpuMs=timestamps && timestamps[1]>=timestamps[0] ?
            double(timestamps[1]-timestamps[0])*1000.0/double(device->GetTimestampFrequency()) : -1.0;
    };
    std::ofstream csv(output/"captures.csv");
    csv<<"name,formation,coverage,hour,pitch,yaw,eye_x,eye_z,eye_y,width,height,serialized_ms,cloud_width,cloud_height,gpu_frame_ms,cloud_base,cloud_depth,local_weather,moon_automatic,moon_phase,reference_quality,animation_multiplier\n";
    MoonState moon;moon.enabled=true;moon.automatic=false;moon.phase=.5f;moon.elevation=40;
    moon.diameter=1;moon.brightness=.9f;
    NightSkyState night;night.gradient=.7f;StoreNightSky(*scene,entity,night);
    auto sun=CaptureSun(*scene,entity);
    float localWeatherStudy=wi::arguments::HasArgument("cloud-analytic-bodies") ? 1.8f : 0;
    float cameraHeightStudy=0;
    const bool noBodySkip=wi::arguments::HasArgument("cloud-body-no-skip");
    const bool metricsOnly=wi::arguments::HasArgument("cloud-metrics-only");
    const auto shot=[&](int type,float coverage,float hour,float pitch,float yaw,float x,float z,const std::string& suffix)
    {
        params.layerFirst.coverageAmount=coverage;
        StoreCloudFormation(*scene,entity,static_cast<CloudFormation>(type));
        SetSunTime(sun,hour);ApplySun(*scene,entity,sun);
        moon.enabled=hour<6;StoreMoon(*scene,entity,moon);
        if(localWeatherStudy!=0) params.layerFirst.padding1=localWeatherStudy;
        if(noBodySkip) params.layerFirst.padding1+=params.layerFirst.padding1<0 ? -.1f : .1f;
        if(localWeatherStudy!=0 || noBodySkip) scene->weather=weather;
        const float p=XMConvertToRadians(pitch),y=XMConvertToRadians(yaw);
        const float eyeHeight=cameraHeightStudy>0 ? cameraHeightStudy :
            suffix.find("-above")!=std::string::npos ? 9000.0f : 2.0f;
        const auto eye=XMVectorSet(x,eyeHeight,z,1);
        const auto look=XMVectorSet(std::sin(y)*std::cos(p),std::sin(p),std::cos(y)*std::cos(p),0);
        camera->TransformCamera(XMMatrixInverse(nullptr,
            XMMatrixLookAtLH(eye,XMVectorAdd(eye,look),XMVectorSet(0,1,0,0))));
        camera->UpdateCamera();render->volumetriccloudResources.ResetFrame();
        for(int i=0;i<24;++i) frame();
        const auto start=std::chrono::steady_clock::now();
        double gpuSum=0;for(int i=0;i<8;++i) {frame();gpuSum+=gpuMs;}
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/8;
        const std::string name="type"+std::to_string(type)+"-c"+std::to_string(int(coverage*100))+
            "-h"+std::to_string(int(hour))+"-p"+std::to_string(int(pitch))+suffix;
        if(!metricsOnly && !wi::helper::saveTextureToFile(render->GetRenderResult3D(),(output/(name+".png")).string())) return false;
        const auto& res=render->volumetriccloudResources;
        const auto& cloud=res.texture_reproject[res.GetTemporalOutputIndex()];
        if(!metricsOnly && wi::arguments::HasArgument("cloud-raw-study") &&
            !wi::helper::saveTextureToFile(res.texture_cloudRender,
                (output/(name+"-raw-rays.png")).string())) return false;
        if(!cloud.IsValid()) return false;
        if((metricsOnly || wi::arguments::HasArgument("cloud-stream-metrics")) &&
            !SaveCloudOpacityGrid(cloud,output/(name+"-opacity.pgm"))) return false;
        if(!metricsOnly && !wi::helper::saveTextureToFile(cloud,(output/(name+"-opacity.png")).string())) return false;
        csv<<name<<','<<type<<','<<coverage<<','<<hour<<','<<pitch<<','<<yaw<<','<<x<<','<<z<<','
            <<eyeHeight<<','<<width<<','<<height<<','<<ms<<','<<cloud.desc.width<<','<<cloud.desc.height<<','<<gpuSum/8<<','
            <<params.cloudStartHeight<<','<<params.cloudThickness<<','<<params.layerFirst.padding1<<','
            <<moon.automatic<<','<<moon.phase<<','
            <<wi::arguments::HasArgument("cloud-reference-quality")<<','<<params.animationMultiplier<<'\n';csv.flush();
        std::cout<<name<<" "<<ms<<" ms serialized"<<std::endl;
        return true;
    };
    if(argc>5 && std::string(argv[5])=="cloud-performance-matrix")
    {
        // Same camera and lighting within each pairing; change animation alone
        // to isolate work caused by drift from the base cloud-ray cost.
        std::ofstream perf(output/"cloud-performance-matrix.csv");
        perf<<"formation,coverage,base,depth,animation,gpu_ms,serialized_ms\\n";
        frameDelta=1.0f/60.0f;
        moon.automatic=true;weather.stars=.6f;
        weather.SetVolumetricCloudsCastShadow(false);
        for(int type:{1,2})
        {
            params.cloudStartHeight=type==1 ? 2280.0f : 5320.0f;
            params.cloudThickness=type==1 ? 3190.0f : 4400.0f;
            const float coverage=type==1 ? .8f : .7f;
            for(float speed:{0.0f,2.0f,8.0f})
            {
                params.animationMultiplier=speed;
                scene->weather=weather;
                if(!shot(type,coverage,0.0f,35.0f,0,0,0,
                    "-perf-speed"+std::to_string(int(speed)))) return 119;
                const float bodyGpu=static_cast<float>(gpuMs);
                perf<<type<<","<<coverage<<","<<params.cloudStartHeight<<","
                    <<params.cloudThickness<<","<<speed<<","<<bodyGpu<<",0\\n";
                perf.flush();
                std::cout<<"CLOUD_PERF type="<<type<<" speed="<<speed
                    <<" latest_gpu_ms="<<bodyGpu<<std::endl;
            }
        }
        perf.close();csv.close();std::fflush(stdout);std::_Exit(0);
    }
    if(argc>5 && std::string(argv[5])=="live-wind-visible-proof")
    {
        // Unlike all other deterministic proof modes, advance real scene time
        // to test whether full cloud silhouettes move at the native defaults.
        // Capture the same camera/weather twice, with temporal history reset.
        params.cloudStartHeight=2280;
        params.cloudThickness=3190;
        params.animationMultiplier=2;
        params.layerFirst.windSpeed=15;
        params.layerFirst.coverageWindSpeed=30;
        params.layerFirst.windAngle=.36f;
        params.layerFirst.coverageWindAngle=.36f;
        params.layerSecond.coverageAmount=0;
        scene->weather=weather;
        for(int type:{1,2})
        {
            params.animationMultiplier=2;
            scene->weather=weather;
            if(!shot(type,.8f,12,35,0,0,0,"-live-wind-start"))return 114;
            frameDelta=60;
            frame(); // single 60s elapsed-time tick, not 3600 heavy GPU frames
            frameDelta=0;
            if(!shot(type,.8f,12,35,0,0,0,"-live-wind-after60s"))return 115;
        }
        csv.close();std::fflush(stdout);std::_Exit(0);
    }
    if(argc>5 && std::string(argv[5])=="long-wind-height-study")
    {
        moon.automatic=true;weather.stars=.6f;
        params.cloudStartHeight=2280;params.cloudThickness=3190;
        params.animationMultiplier=1;scene->weather=weather;
        frameDelta=600;frame();frameDelta=0;
        for(float up:{0.0f,.5f})
        {
            params.layerFirst.windUpAmount=up;
            const auto label=up>0 ? "up-half" : "up-zero";
            if(!shot(1,.8f,12,35,0,0,0,std::string("-long-wind-")+label))return 110;
            if(!SaveCloudDensityVolumeAudit(output/label,*camera,weather))return 111;
        }
        csv.close();std::fflush(stdout);std::_Exit(0);
    }
    if(argc>5 && std::string(argv[5])=="density-volume-study")
    {
        params.cloudStartHeight=2280;params.cloudThickness=3190;
        for(float coverage:{0.0f,.8f,1.0f})
        {
            const auto name="c"+std::to_string(int(coverage*100));
            if(!shot(1,coverage,12,35,0,0,0,"-volume")) return 86;
            if(!SaveCloudDensityVolumeAudit(output/name,*camera,weather)) return 87;
        }
        csv.close();std::fflush(stdout);std::_Exit(0);
    }
    // Compact deterministic replay of the owner's thin-layer night screenshot.
    // Keep this identical before/after reconstruction changes.
    if(argc>5 && std::string(argv[5])=="quality-comparison")
    {
        moon.automatic=true;weather.stars=.6f;
        params.cloudStartHeight=2280;params.cloudThickness=3190;
        for(int type:{0,1})
            for(float hour:{12.0f,0.0f})
                if(!shot(type,.8f,hour,35,0,0,0,"-owner-layer")) return 76;
        csv.close();std::fflush(stdout);std::_Exit(0);
    }
    // Isolate local weather from noise, lighting and the request's admission.
    if(argc>5 && std::string(argv[5])=="local-weather-study")
    {
        moon.automatic=true;weather.stars=.6f;
        const auto peaks=wi::arguments::HasArgument("local-weather-low") ?
            std::initializer_list<float>{.25f,.30f,.35f,.40f,.45f,.50f} :
            std::initializer_list<float>{.60f,.65f,.70f,.75f,.80f,.85f};
        for(float peak:peaks)
        {
            localWeatherStudy=peak;
            for(float hour:{12.0f,0.0f})
                if(!shot(1,.8f,hour,35,0,0,0,"-local"+std::to_string(int(peak*100+.5f)))) return 80;
        }
        csv.close();std::fflush(stdout);std::_Exit(0);
    }
    if(argc>5 && std::string(argv[5])=="single-cloud-body-study")
    {
        moon.automatic=true;weather.stars=.6f;localWeatherStudy=wi::arguments::HasArgument("cloud-cached-body") ? 3.8f : 2.8f;
        params.cloudStartHeight=1500;params.cloudThickness=5000;
        for(float hour:{9.0f,0.0f})
        {
            cameraHeightStudy=2;
            if(!shot(1,.8f,hour,22,0,0,-6500,"-single-below")) return 94;
            cameraHeightStudy=2600;
            if(!shot(1,.8f,hour,0,-90,6500,0,"-single-side")) return 95;
            cameraHeightStudy=6500;
            if(!shot(1,.8f,hour,-31,0,0,-6500,"-single-top")) return 96;
        }
        csv.close();std::fflush(stdout);std::_Exit(0);
    }
    if(argc>5 && std::string(argv[5])=="single-cloud-variant-study")
    {
        moon.automatic=true;weather.stars=.6f;
        const float basePeak=wi::arguments::HasArgument("cloud-cached-body") ? 3.8f : 2.8f;
        params.cloudStartHeight=1500;params.cloudThickness=5000;
        for(uint32_t variant=0;variant<4;++variant)
        {
            localWeatherStudy=basePeak+float(variant)/30.0f;
            const std::string label="-variant"+std::to_string(variant);
            cameraHeightStudy=2;
            if(!shot(1,.8f,9,22,0,0,-6500,label+"-below")) return 94;
            cameraHeightStudy=2600;
            if(!shot(1,.8f,9,0,-90,6500,0,label+"-side")) return 95;
            cameraHeightStudy=6500;
            if(!shot(1,.8f,9,-31,0,0,-6500,label+"-top")) return 96;
        }
        csv.close();std::fflush(stdout);std::_Exit(0);
    }
    if(argc>5 && std::string(argv[5])=="macro-holdout-review")
    {
        moon.automatic=true;weather.stars=.6f;
        for(bool thin:{false,true})
        {
            params.cloudStartHeight=thin ? 2280.0f : 1500.0f;
            params.cloudThickness=thin ? 3190.0f : 5000.0f;
            const std::string layer=thin ? "-thin" : "-deep";
            for(float x:{-3000.0f,4500.0f})
                for(float pitch:{20.0f,55.0f,85.0f})
                    for(float hour:{9.0f,0.0f})
                        if(!shot(1,.8f,hour,pitch,x<0 ? 35 : 215,x,
                            x<0 ? -2500 : 7000,layer+"-holdout"+std::to_string(int(x)))) return 92;
            if(!shot(1,.8f,9,-35,35,0,0,layer+"-above")) return 93;
        }
        csv.close();std::fflush(stdout);std::_Exit(0);
    }
    if(argc>5 && std::string(argv[5])=="macro-body-review")
    {
        moon.automatic=true;weather.stars=.6f;
        for(float hour:{9.0f,15.0f,0.0f})
            for(float pitch:{0.0f,35.0f,70.0f})
                if(!shot(1,.8f,hour,pitch,0,0,0,"-canonical")) return 88;
        for(float x:{-6000.0f,6000.0f})
            if(!shot(1,.8f,9,35,90,x,4000,
                "-move"+std::to_string(int(x)))) return 89;
        for(float coverage:{0.0f,1.0f})
            for(float hour:{12.0f,0.0f})
                if(!shot(1,coverage,hour,35,0,0,0,"-endpoints")) return 90;
        params.cloudStartHeight=2280;params.cloudThickness=3190;
        for(float hour:{9.0f,15.0f,0.0f})
            if(!shot(1,.8f,hour,35,0,0,0,"-owner-layer")) return 91;
        csv.close();std::fflush(stdout);std::_Exit(0);
    }
    // Controlled architecture probe: native base noise contains four cells per
    // repeat. Express the dominant body width in metres rather than guessing
    // an opaque noise frequency. Hold local density admission and light fixed.
    if(argc>5 && std::string(argv[5])=="body-scale-study")
    {
        moon.automatic=true;weather.stars=.6f;localWeatherStudy=.4f;
        params.cloudStartHeight=2280;params.cloudThickness=3190;
        for(float bodyWidth:{400.0f,800.0f,1600.0f})
        {
            params.layerFirst.totalNoiseScale=1.0f/(4.0f*bodyWidth);
            for(float hour:{12.0f,0.0f})
                if(!shot(1,.8f,hour,35,0,0,0,
                    "-body-metres"+std::to_string(int(bodyWidth)))) return 82;
        }
        csv.close();std::fflush(stdout);std::_Exit(0);
    }
    // Calibrate screen occupancy from spatial admission with body state fixed.
    if(argc>5 && (std::string(argv[5])=="macro-admission-study" ||
        std::string(argv[5])=="macro-admission-owner-study"))
    {
        if(std::string(argv[5])=="macro-admission-owner-study")
        {params.cloudStartHeight=2280;params.cloudThickness=3190;}
        localWeatherStudy=wi::arguments::HasArgument("cloud-analytic-bodies") ? -1.8f : -.8f;moon.automatic=true;
        for(float request:{.10f,.20f,.30f,.40f,.50f,.60f,.70f,.80f})
            for(float pitch:{35.0f,70.0f})
                for(float x:{-6000.0f,0.0f,6000.0f})
                    if(!shot(1,request,12,pitch,x==0 ? 0 : 90,x,x==0 ? 0 : 4000,
                        "-macro"+std::to_string(int(x)))) return 81;
        csv.close();std::fflush(stdout);std::_Exit(0);
    }
    // Internal visual architecture review before any replacement owner preview.
    if(argc>5 && std::string(argv[5])=="formation-study")
    {
        moon.automatic=true;weather.stars=.6f;
        for(int type=1;type<=5;++type)
            for(float hour:{12.0f,0.0f})
                if(!shot(type,.8f,hour,35,0,0,0,"")) return 77;
        for(float hour:{12.0f,0.0f})
            if(!shot(1,1.0f,hour,35,0,0,0,"")) return 78;
        params.cloudStartHeight=2280;params.cloudThickness=3190;
        for(float hour:{12.0f,0.0f})
            if(!shot(1,.8f,hour,35,0,0,0,"-owner-layer")) return 79;
        csv.close();std::fflush(stdout);std::_Exit(0);
    }
    // Legacy baseline uses precisely the same scene, resolution, camera, time and light.
    for(int type=0;type<=5;++type)
        for(float hour:{12.0f,0.0f})
            for(float coverage:{.8f,1.0f})
                if(!shot(type,coverage,hour,35,0,0,0,"")) return 63;
    if(width==1920)
    {
        for(float hour:{12.0f,18.2f,0.0f})
            for(float coverage:{0.0f,.25f,.5f,.8f,.95f,1.0f})
                for(float pitch:{0.0f,35.0f,70.0f})
                    if(!shot(1,coverage,hour,pitch,0,0,0,"-matrix")) return 64;
        for(int type=1;type<=5;++type)
            for(float x:{-6000.0f,6000.0f})
                if(!shot(type,.8f,12,55,90,x,4000,"-move"+std::to_string(int(x)))) return 65;
    }
    for(int type:{1,2,3,5})
        if(!shot(type,.8f,12,-35,0,0,0,"-above")) return 71;
    for(float phase:{0.0f,.25f,.5f})
    {
        moon.phase=phase;
        if(!shot(1,.8f,0,35,0,0,0,"-phase"+std::to_string(int(phase*100)))) return 75;
    }
    moon.phase=.5f;
    // Continuous wind/history proof followed by an identical-time settled image.
    if(!shot(1,.8f,12,35,0,0,0,"-motion-start")) return 72;
    params.animationMultiplier=2;frameDelta=1.0f/60.0f;
    for(int i=0;i<180;++i) frame();
    const auto saveMotion=[&](const char* name)
    {
        const auto& res=render->volumetriccloudResources;
        return wi::helper::saveTextureToFile(render->GetRenderResult3D(),(output/(std::string(name)+".png")).string()) &&
            wi::helper::saveTextureToFile(res.texture_reproject[res.GetTemporalOutputIndex()],
                (output/(std::string(name)+"-opacity.png")).string());
    };
    if(!saveMotion("motion-warm")) return 73;
    frameDelta=0;render->volumetriccloudResources.ResetFrame();
    for(int i=0;i<24;++i) frame();
    if(!saveMotion("motion-settled")) return 74;
    csv.close();std::cout<<"Cloud formation GPU proof complete; GPU frame timestamps and serialized timings saved"<<std::endl;
    std::fflush(stdout);std::_Exit(0);
}
#endif
