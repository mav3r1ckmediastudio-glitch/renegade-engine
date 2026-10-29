#pragma once
#if defined(_WIN32)
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <iostream>

// Explicit GPU proof: renders real Wicked frames without borrowing the user's
// mouse/keyboard or opening their projects. Not part of headless CTest runs.
inline int RunNightSkyGraphicsProof(int argc, char** argv)
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;
    wi::arguments::Parse(argc, argv);
    const fs::path output = fs::absolute(fs::u8path(argv[2]));
    fs::create_directories(output);
    HWND window = CreateWindowExW(0, L"STATIC", L"Renegade Night Sky GPU proof",
        WS_OVERLAPPEDWINDOW, 0, 0, 960, 540, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return 30;
    auto* app = new wi::Application();
    app->allow_hdr = false;
    app->SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    wi::renderer::SetTemporalAAEnabled(false);
    auto* scene = new wi::scene::Scene();
    const auto entity = wi::ecs::CreateEntity();
    auto& weather = scene->weathers.Create(entity);
    weather.SetRealisticSky(true);
    weather.skyExposure = 1.0f;
    weather.stars = 0.85f;
    scene->weather = weather;
    (void)EnsurePrimarySunLight(*scene, entity);
    auto* render = new wi::RenderPath3D();
    auto* camera = new wi::scene::CameraComponent();
    render->scene = scene;
    render->camera = camera;
    render->init(960, 540, 96);
    render->setDepthOfFieldEnabled(false);
    render->setMotionBlurEnabled(false);
    render->ResizeBuffers();
    camera->CreatePerspective(960, 540, 0.1f, 1000.0f, XMConvertToRadians(70.0f));
    camera->TransformCamera(XMMatrixInverse(nullptr, XMMatrixLookAtLH(
        XMVectorSet(0, 2, 0, 1), XMVectorSet(0, 2.45f, 1, 1), XMVectorSet(0, 1, 0, 0))));
    camera->UpdateCamera();
    const auto frame = [&]()
    {
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT, 0);
        render->PreUpdate();
        render->Update(1.0f / 60.0f);
        render->PreRender();
        render->Render();
        wi::graphics::GetDevice()->SubmitCommandLists();
        wi::renderer::UpdateGPUSuballocator();
        wi::graphics::GetDevice()->WaitForGPU();
    };
    const auto capture = [&](const char* name)
    {
        wi::vector<uint8_t> png;
        if (!wi::helper::saveTextureToMemoryFile(render->GetRenderResult3D(), "PNG", png))
            return false;
        std::ofstream file(output / (std::string(name) + ".png"), std::ios::binary);
        file.write(reinterpret_cast<const char*>(png.data()), png.size());
        return bool(file);
    };
    // Gate 2 upper-cloud validation: unlike the prior ground-based lunar
    // studies this captures the cloud TOPS from above their authored layer.
    // It checks that actual direct lunar illumination remains visible even
    // when the Moon is angularly far from the camera's viewing direction.
    if (wi::arguments::HasArgument("g2-above-cloud-lunar-study"))
    {
        auto* wc=scene->weathers.GetComponent(entity);
        wc->SetVolumetricClouds(true);
        wc->volumetricCloudParameters.layerFirst.coverageAmount=.80f;
        wc->volumetricCloudParameters.layerSecond.coverageAmount=0;
        wc->volumetricCloudParameters.cloudStartHeight=2280;
        wc->volumetricCloudParameters.cloudThickness=3190;
        wc->volumetricCloudParameters.animationMultiplier=0;
        wc->skyExposure=1;wc->stars=.45f;
        auto sun=CaptureSun(*scene,entity);
        SetSunTime(sun,0.0f);
        ApplySun(*scene,entity,sun);
        MoonState moon;moon.enabled=true;moon.automatic=true;
        moon.phase=.5f;moon.brightness=.9f;moon.azimuth=0;
        std::ofstream m(output/"g2-above-cloud-lunar.csv");
        m<<"formation,camera_height,pitch,moon_enabled,sun_y,file\n";
        for(int formation : {1,2,4,5})
        {
            StoreCloudFormation(*scene,entity,
                static_cast<CloudFormation>(formation));
            for(float height : {2.0f,6200.0f})
            {
                const float pitch=height>6000.0f?-25.0f:35.0f;
                const float p=XMConvertToRadians(pitch);
                const XMVECTOR eye=XMVectorSet(0,height,0,1);
                const XMVECTOR ray=XMVectorSet(0,sin(p),cos(p),0);
                camera->TransformCamera(XMMatrixInverse(nullptr,
                    XMMatrixLookAtLH(eye,XMVectorAdd(eye,ray),
                        XMVectorSet(0,1,0,0))));
                camera->UpdateCamera();
                for(bool enabled:{true,false})
                {
                    moon.enabled=enabled;StoreMoon(*scene,entity,moon);
                    render->volumetriccloudResources.ResetFrame();
                    for(int i=0;i<12;++i)frame();
                    const auto label=std::string("g2-above-f")+
                        std::to_string(formation)+"-h"+
                        std::to_string(int(height))+
                        (enabled?"-moon-on":"-moon-off");
                    if(!capture(label.c_str()))return 109;
                    m<<formation<<','<<height<<','<<pitch<<','
                        <<(enabled?1:0)<<','<<sun.direction.y<<','
                        <<label<<'\n';m.flush();
                }
            }
        }
        std::fflush(stdout);std::_Exit(0);
    }
    // Gate 2 reference: camera-to-Moon angular scattering at constant time,
    // coverage and cloud shape. Keep this identically scoped before/after
    // any lighting change. Each test runs offscreen in disposable memory.
    if (wi::arguments::HasArgument("g2-lunar-proximity-study"))
    {
        auto* wc = scene->weathers.GetComponent(entity);
        wc->SetVolumetricClouds(true);
        wc->volumetricCloudParameters.layerFirst.coverageAmount = 0.80f;
        wc->volumetricCloudParameters.layerSecond.coverageAmount = 0.0f;
        wc->volumetricCloudParameters.cloudStartHeight = 2280.0f;
        wc->volumetricCloudParameters.cloudThickness = 3190.0f;
        wc->volumetricCloudParameters.animationMultiplier = 0.0f;
        wc->skyExposure=1;wc->stars=.45f;
        auto sun=CaptureSun(*scene,entity);
        SetSunTime(sun,19.0f);
        ApplySun(*scene,entity,sun);
        MoonState moon;
        moon.enabled=true;moon.automatic=false;
        moon.azimuth=0;moon.elevation=32;moon.phase=.5f;
        moon.diameter=2;moon.brightness=.9f;
        std::ofstream m(output/"g2-lunar-proximity.csv");
        m<<"formation,camera_yaw,moon_enabled,sun_y,file\n";
        for(int formation : {1,2,4,5})
        {
            StoreCloudFormation(*scene,entity,
                static_cast<CloudFormation>(formation));
            for(float yaw: {0.0f,35.0f,70.0f})
            {
                const float p=XMConvertToRadians(32.0f);
                const float y=XMConvertToRadians(yaw);
                const XMVECTOR eye=XMVectorSet(0,2,0,1);
                const XMVECTOR ray=XMVectorSet(sin(y)*cos(p),
                    sin(p),cos(y)*cos(p),0);
                camera->TransformCamera(XMMatrixInverse(nullptr,
                    XMMatrixLookAtLH(eye,XMVectorAdd(eye,ray),
                        XMVectorSet(0,1,0,0))));
                camera->UpdateCamera();
                for(bool enabled: {true,false})
                {
                    if(!enabled && yaw!=0.0f)continue;
                    moon.enabled=enabled;
                    StoreMoon(*scene,entity,moon);
                    render->volumetriccloudResources.ResetFrame();
                    for(int i=0;i<12;++i)frame();
                    const auto label=std::string("g2-f")+
                        std::to_string(formation)+"-yaw"+
                        std::to_string(int(yaw))+(enabled?"-full":"-off");
                    if(!capture(label.c_str()))return 107;
                    m<<formation<<','<<yaw<<','<<(enabled?1:0)<<','
                        <<sun.direction.y<<','<<label<<'\n';
                    m.flush();
                }
            }
        }
        std::fflush(stdout);std::_Exit(0);
    }
    // Gate 2 baseline/continuity proof. Entirely disposable GPU scene,
    // fixed camera and frozen cloud motion; it never opens owner Studio.
    // Hours/solar Y recorded instead of assuming a particular latitude.
    if (wi::arguments::HasArgument("g2-cloud-twilight-study"))
    {
        auto* wc = scene->weathers.GetComponent(entity);
        wc->SetVolumetricClouds(true);
        wc->volumetricCloudParameters.layerFirst.coverageAmount = .8f;
        wc->volumetricCloudParameters.layerFirst.padding0 = 1.0f; // Cumulus
        wc->volumetricCloudParameters.layerSecond.coverageAmount = 0.0f;
        wc->volumetricCloudParameters.cloudStartHeight = 2280.0f;
        wc->volumetricCloudParameters.cloudThickness = 3190.0f;
        wc->volumetricCloudParameters.animationMultiplier = 0.0f;
        wc->stars = .45f;
        wc->skyExposure = 1.0f;
        scene->weather = *wc;
        const XMVECTOR observer = XMVectorSet(0,2,0,1);
        const float pitch = XMConvertToRadians(35.0f);
        const XMVECTOR direction = XMVectorSet(0,sin(pitch),cos(pitch),0);
        camera->TransformCamera(XMMatrixInverse(nullptr,
            XMMatrixLookAtLH(observer,XMVectorAdd(observer,direction),
                XMVectorSet(0,1,0,0))));
        camera->UpdateCamera();
        auto sun = CaptureSun(*scene,entity);
        MoonState moon; moon.enabled=true;moon.automatic=false;
        moon.azimuth=0;moon.elevation=32;moon.phase=.5f;
        moon.diameter=2;moon.brightness=.9f;
        std::ofstream measurements(output / "g2-twilight-baseline.csv");
        measurements << "hour,sun_y,moon_enabled,phase,moon_y,case\n";
        for(float hour : {0.0f,5.0f,6.0f,6.5f,7.0f,8.0f,12.0f,
                          16.0f,17.0f,17.5f,18.0f,18.1f,18.2f,18.3f,18.4f,18.5f,18.6f,18.8f,19.0f,20.0f})
        {
            SetSunTime(sun,hour);
            ApplySun(*scene,entity,sun);
            for(int moonCase=0;moonCase<2;++moonCase)
            {
                moon.enabled=(moonCase==1);
                StoreMoon(*scene,entity,moon);
                render->volumetriccloudResources.ResetFrame();
                for(unsigned i=0;i<12;++i)frame();
                const auto label=std::string("g2-hour-")+
                    std::to_string(int(round(hour*10.0f)))+
                    (moon.enabled?"-moon-full":"-moon-off");
                measurements << hour << ',' << sun.direction.y << ','
                    << (moon.enabled?1:0) << ',' << moon.phase << ','
                    << MoonDirection(moon,sun.direction).y << ','
                    << label << '\n';
                measurements.flush();
                if(!capture(label.c_str()))return 90;
            }
        }
        std::fflush(stdout);std::_Exit(0);
    }

    // Gate 1 Moon visibility audit: a fixed above-horizon full Moon centred
    // on the fixture camera, with clouds disabled/enabled. Never touches an
    // owner project or persistent Studio state.
    if (wi::arguments::HasArgument("cloud-moon-visibility-study"))
    {
        auto sun = CaptureSun(*scene, entity);
        SetSunTime(sun,0); ApplySun(*scene,entity,sun);
        MoonState moon; moon.enabled=true; moon.automatic=false;
        moon.azimuth=0; moon.elevation=30; moon.phase=.5f;
        moon.diameter=8; moon.brightness=.9f;
        StoreMoon(*scene,entity,moon);
        const auto eye=XMVectorSet(0,2,0,1);
        const float elevation=XMConvertToRadians(30.0f);
        const auto look=XMVectorSet(0,std::sin(elevation),std::cos(elevation),0);
        camera->TransformCamera(XMMatrixInverse(nullptr,
            XMMatrixLookAtLH(eye,XMVectorAdd(eye,look),XMVectorSet(0,1,0,0))));
        camera->UpdateCamera();
        auto* wc=scene->weathers.GetComponent(entity);
        for(unsigned pass=0;pass<4;++pass)
        {
            wc->SetVolumetricClouds(pass>=2);
            wc->volumetricCloudParameters.layerFirst.coverageAmount=.8f;
            wc->volumetricCloudParameters.layerFirst.padding0=1.0f;
            wc->volumetricCloudParameters.layerSecond.coverageAmount=0;
            wc->volumetricCloudParameters.cloudStartHeight=1500;
            wc->volumetricCloudParameters.cloudThickness=5000;
            moon.enabled=(pass%2)==0;
            StoreMoon(*scene,entity,moon);
            render->volumetriccloudResources.ResetFrame();
            for(unsigned i=0;i<24;++i) frame();
            const char* label=pass==0?"clear-full-moon":
                pass==1?"clear-moon-disabled":
                pass==2?"cumulus80-full-moon":"cumulus80-moon-disabled";
            if(!capture(label)) return 89;
        }
        moon.enabled=true;
        wc->SetVolumetricClouds(true);
        for(float brightness:{.1f,.2f,.4f})
        {
            moon.brightness=brightness;
            StoreMoon(*scene,entity,moon);
            render->volumetriccloudResources.ResetFrame();
            for(unsigned i=0;i<24;++i) frame();
            const auto name="cumulus80-lunar-strength-"+std::to_string(int(brightness*100));
            if(!capture(name.c_str())) return 89;
        }
        moon.enabled=true;moon.brightness=.9f;
        for(float phase:{0.0f,.25f,.75f})
        {
            moon.phase=phase;
            StoreMoon(*scene,entity,moon);
            render->volumetriccloudResources.ResetFrame();
            for(unsigned i=0;i<24;++i) frame();
            const auto name="cumulus80-lunar-phase-"+std::to_string(int(phase*100));
            if(!capture(name.c_str())) return 89;
        }
        moon.phase=.5f;moon.elevation=-20;
        StoreMoon(*scene,entity,moon);
        render->volumetriccloudResources.ResetFrame();
        for(unsigned i=0;i<24;++i) frame();
        if(!capture("cumulus80-moon-below-horizon")) return 89;
        moon.enabled=false;StoreMoon(*scene,entity,moon);
        SetSunTime(sun,12);ApplySun(*scene,entity,sun);
        render->volumetriccloudResources.ResetFrame();
        for(unsigned i=0;i<24;++i) frame();
        if(!capture("cumulus80-daylight-reference")) return 89;
        std::fflush(stdout);std::_Exit(0);
    }
    struct Case { const char* name; float time, gradient, aurora, stars, milkyWay = 0; };
    const Case cases[] = {
        {"day-native",12,0,0,.85f}, {"day-extensions",12,.85f,.75f,.85f,1},
        {"night-empty",0,0,0,0}, {"night-native-stars",0,0,0,.85f},
        {"night-gradient",0,.85f,0,.85f}, {"night-aurora",0,.85f,.75f,.85f},
        {"dusk",18.2f,.85f,.75f,.85f,1}, {"dawn",6.5f,.85f,.75f,.85f,1},
        {"milky-way-only",0,0,0,0,1}, {"milky-way-stars",0,.85f,0,.85f,.8f},
        {"milky-way-aurora",0,.85f,.6f,.85f,.8f}
    };
    std::ofstream metrics(output / "frame-times.csv");
    metrics << "case,cpu_gpu_serialized_ms\n";
    for (const auto& c : cases)
    {
        auto sun = CaptureSun(*scene, entity);
        SetSunTime(sun, c.time);
        ApplySun(*scene, entity, sun);
        scene->weathers.GetComponent(entity)->stars = c.stars;
        NightSkyState night;
        night.gradient = c.gradient;
        night.aurora = c.aurora;
        night.milkyWay = c.milkyWay;
        night.auroraCoverage = .85f;
        StoreNightSky(*scene, entity, night);
        for (unsigned i = 0; i < 24; ++i) frame();
        const auto start = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < 32; ++i) frame();
        const double ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count() / 32.0;
        metrics << c.name << ',' << ms << '\n';
        metrics.flush();
        if (!capture(c.name)) return 31;
        std::cout << c.name << " " << ms << " ms (CPU+GPU serialized)" << std::endl;
    }
    auto sun = CaptureSun(*scene, entity);
    SetSunTime(sun, 0);
    ApplySun(*scene, entity, sun);
    auto night = CaptureNightSky(*scene, entity);
    night.milkyWay = 0;
    night.aurora = .8f;
    night.auroraSpeed = 1.0f;
    StoreNightSky(*scene, entity, night);
    for (unsigned i = 0; i < 24; ++i) frame();
    if (!capture("animation-a")) return 32;
    for (unsigned i = 0; i < 120; ++i) frame();
    if (!capture("animation-b")) return 33;
    // Coverage acceptance views: all compass directions plus the upper sky.
    struct View { const char* name; float yaw, pitch; };
    const View views[] = {{"front",0,30},{"right",90,30},{"back",180,30},
                          {"left",270,30},{"overhead",0,70}};
    night.auroraSpeed = 0;
    for (float coverage : {0.0f, 0.3f, 0.6f, 1.0f})
    {
        night.auroraCoverage = coverage;
        StoreNightSky(*scene, entity, night);
        for (const auto& view : views)
        {
            const float yaw = XMConvertToRadians(view.yaw), pitch = XMConvertToRadians(view.pitch);
            const auto eye = XMVectorSet(0,2,0,1);
            const auto direction = XMVectorSet(std::sin(yaw)*std::cos(pitch),
                std::sin(pitch),std::cos(yaw)*std::cos(pitch),0);
            camera->TransformCamera(XMMatrixInverse(nullptr,
                XMMatrixLookAtLH(eye, XMVectorAdd(eye,direction), XMVectorSet(0,1,0,0))));
            camera->UpdateCamera();
            for (unsigned i=0;i<8;++i) frame();
            const std::string name = "coverage-" + std::to_string(int(coverage*100)) + "-" + view.name;
            if (!capture(name.c_str())) return 34;
        }
    }

    // Moon-specific native GPU proof; all previous captures above use legacy moon-off.
    night.aurora=0; night.milkyWay=.6f; night.gradient=.8f;
    StoreNightSky(*scene,entity,night);
    SetSunTime(sun,0); ApplySun(*scene,entity,sun);
    const auto eye=XMVectorSet(0,2,0,1);
    const auto aim=XMVectorSet(0,.5f,.8660254f,0);
    camera->TransformCamera(XMMatrixInverse(nullptr,
        XMMatrixLookAtLH(eye,XMVectorAdd(eye,aim),XMVectorSet(0,1,0,0))));
    camera->UpdateCamera();
    MoonState moon;moon.enabled=true;moon.automatic=false;moon.azimuth=0;
    moon.elevation=30;moon.diameter=8;moon.brightness=.9f;
    for(int i=0;i<8;++i)
    {
        moon.phase=i/8.0f;StoreMoon(*scene,entity,moon);
        for(unsigned j=0;j<12;++j) frame();
        const auto name="moon-phase-"+std::to_string(i);
        if(!capture(name.c_str())) return 35;
    }
    moon.phase=.5f; moon.diameter=1; StoreMoon(*scene,entity,moon);
    for(unsigned j=0;j<12;++j) frame();
    if(!capture("moon-realistic-size")) return 36;
    moon.diameter=8; moon.elevation=-30; StoreMoon(*scene,entity,moon);
    for(unsigned j=0;j<12;++j) frame();
    if(!capture("moon-below-horizon")) return 36;
    moon.elevation=30;moon.automatic=false;StoreMoon(*scene,entity,moon);
    SetSunTime(sun,12);ApplySun(*scene,entity,sun);
    for(unsigned j=0;j<16;++j) frame();
    if(!capture("moon-daylight")) return 36;
    moon.automatic=true;StoreMoon(*scene,entity,moon);
    for(float hour:{18.0f,21.0f,0.0f,3.0f,6.0f})
    {
        SetSunTime(sun,hour);ApplySun(*scene,entity,sun);
        const auto d=MoonDirection(moon,sun.direction);
        // Fixed observer, wide enough to see the moving moon along its path.
        const auto look=XMVectorSet(d.x,d.y,d.z,0);
        camera->TransformCamera(XMMatrixInverse(nullptr,
            XMMatrixLookAtLH(eye,XMVectorAdd(eye,look),XMVectorSet(0,1,0,0))));
        camera->UpdateCamera();
        for(unsigned j=0;j<12;++j) frame();
        const auto name="moon-orbit-"+std::to_string(int(hour));
        if(!capture(name.c_str())) return 36;
    }

    // Explicit meteor sequence: baseline, movement, fading, off, daylight.
    moon.enabled=false;StoreMoon(*scene,entity,moon);
    night.aurora=0;night.milkyWay=0;StoreNightSky(*scene,entity,night);
    SetSunTime(sun,0);ApplySun(*scene,entity,sun);
    camera->TransformCamera(XMMatrixInverse(nullptr,
        XMMatrixLookAtLH(eye,XMVectorAdd(eye,aim),XMVectorSet(0,1,0,0))));
    camera->UpdateCamera();
    scene->weathers.GetComponent(entity)->stars=0;
    ShootingStarsState meteors;meteors.enabled=true;meteors.frequency=2;meteors.brightness=2;meteors.trailLength=12;
    for(bool enabled:{false,true})
    {
        meteors.enabled=enabled;StoreShootingStars(*scene,entity,meteors);
        for(unsigned j=0;j<12;++j)frame();
        const auto begin=std::chrono::steady_clock::now();
        for(unsigned j=0;j<64;++j)frame();
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/64;
        metrics<<(enabled?"meteor-enabled-night":"meteor-disabled-night")<<','<<ms<<'\n';metrics.flush();
    }
    StoreShootingStars(*scene,entity,meteors);
    for(unsigned j=0;j<8;++j)frame();
    if(!capture("meteor-before"))return 37;
    PreviewShootingStar(*scene,entity,scene->time);
    for(unsigned j=0;j<15;++j)frame();
    if(!capture("meteor-a"))return 37;
    for(unsigned j=0;j<12;++j)frame();
    if(!capture("meteor-b"))return 37;
    for(unsigned j=0;j<60;++j)frame();
    if(!capture("meteor-faded"))return 37;
    meteors.enabled=false;StoreShootingStars(*scene,entity,meteors);
    for(unsigned j=0;j<8;++j)frame();
    if(!capture("meteor-off"))return 37;
    meteors.enabled=true;StoreShootingStars(*scene,entity,meteors);
    SetSunTime(sun,12);ApplySun(*scene,entity,sun);
    PreviewShootingStar(*scene,entity,scene->time);
    for(unsigned j=0;j<15;++j)frame();
    if(!capture("meteor-daylight"))return 37;
    meteors.enabled=false;StoreShootingStars(*scene,entity,meteors);
    for(unsigned j=0;j<8;++j)frame();
    if(!capture("meteor-daylight-off"))return 37;

    // Natural scheduling, no preview trigger, wide sky and no native star flicker.
    SetSunTime(sun,0);ApplySun(*scene,entity,sun);
    meteors.enabled=true;meteors.frequency=120;StoreShootingStars(*scene,entity,meteors);
    camera->CreatePerspective(960,540,.1f,1000,XMConvertToRadians(130));
    camera->UpdateCamera();
    for(unsigned shot=0;shot<30;++shot)
    {
        for(unsigned j=0;j<30;++j)frame();
        const auto name="meteor-natural-"+std::to_string(shot);
        if(!capture(name.c_str()))return 38;
    }
    camera->CreatePerspective(960,540,.1f,1000,XMConvertToRadians(70));
    camera->UpdateCamera();
    night.milkyWay=.65f;night.aurora=.5f;StoreNightSky(*scene,entity,night);
    moon.enabled=true;moon.automatic=false;moon.elevation=30;moon.diameter=2;StoreMoon(*scene,entity,moon);
    scene->weathers.GetComponent(entity)->stars=.85f;
    meteors.frequency=2;StoreShootingStars(*scene,entity,meteors);
    PreviewShootingStar(*scene,entity,scene->time);
    for(unsigned j=0;j<15;++j)frame();
    if(!capture("meteor-combined-sky"))return 38;

    // Moonlit Clouds: render real volumetric clouds (not just the lunar disc).
    // Preserve the sun used by atmosphere/aerial perspective while testing the
    // cloud raymarch's lunar source for full/quarter/new and off/horizon cases.
    meteors.enabled=false;StoreShootingStars(*scene,entity,meteors);
    night.gradient=.7f;night.aurora=0;night.milkyWay=0;StoreNightSky(*scene,entity,night);
    auto* cloudWeather=scene->weathers.GetComponent(entity);
    cloudWeather->SetVolumetricClouds(true);
    cloudWeather->volumetricCloudParameters.layerFirst.coverageAmount=.92f;
    cloudWeather->volumetricCloudParameters.layerSecond.coverageAmount=.55f;
    cloudWeather->volumetricCloudParameters.cloudStartHeight=650;
    cloudWeather->volumetricCloudParameters.cloudThickness=2200;
    cloudWeather->stars=0;
    camera->TransformCamera(XMMatrixInverse(nullptr,
        XMMatrixLookAtLH(eye,XMVectorAdd(eye,aim),XMVectorSet(0,1,0,0))));
    camera->UpdateCamera();
    SetSunTime(sun,0);ApplySun(*scene,entity,sun);
    moon.automatic=false;moon.azimuth=0;moon.elevation=32;moon.phase=.5f;
    moon.diameter=2;moon.brightness=.9f;
    struct LunarCloudCase { const char* name; bool enabled; float phase; float elevation; float sunTime; };
    const LunarCloudCase moonCloudCases[]={
        {"clouds-night-moon-off",false,.5f,32,0},
        {"clouds-night-new-moon",true,0,32,0},
        {"clouds-night-quarter",true,.25f,32,0},
        {"clouds-night-full",true,.5f,32,0},
        {"clouds-night-below-horizon",true,.5f,-20,0},
        {"clouds-day-moon-on",true,.5f,32,12},
        {"clouds-day-moon-off",false,.5f,32,12},
    };
    for(const auto& c:moonCloudCases)
    {
        SetSunTime(sun,c.sunTime);ApplySun(*scene,entity,sun);
        moon.enabled=c.enabled;moon.phase=c.phase;moon.elevation=c.elevation;
        StoreMoon(*scene,entity,moon);
        for(unsigned j=0;j<24;++j)frame();
        const auto begin=std::chrono::steady_clock::now();
        for(unsigned j=0;j<32;++j)frame();
        const double ms=std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-begin).count()/32.0;
        metrics<<c.name<<','<<ms<<'\n';metrics.flush();
        if(!capture(c.name))return 39;
    }
    // Clouds V2 Gate 1 — upward-looking views actually intersect the 3D
    // cloud body. The older moon comparisons above look near the horizon.
    // Check diffuse daylight shape, taller formations and moonlit detail.
    struct CloudDetailView
    {
        const char* name;
        float pitch;
        float coverage;
        float height;
        float thickness;
        float sunHour;
        float coverageFloor;
        float eyeX;
    };
    const CloudDetailView cloudDetailViews[] = {
        {"clouds-v2-day-up35-scattered",35,0.68f,700,3600,12,0.0f,0},
        {"clouds-v2-day-up70-scattered",70,0.68f,700,3600,12,0.0f,0},
        {"clouds-v2-day-up35-thick",35,0.98f,700,4700,12,0.0f,0},
        {"clouds-v2-night-up35-thick",35,0.98f,700,4700,0,0.0f,0},
        {"clouds-v2-day-up70-overcast",70,0.98f,700,4700,12,0.7f,0},
        {"clouds-v2-day-up35-coverage80",35,0.80f,700,4700,12,0.0f,0},
        {"clouds-v2-day-up35-coverage95",35,0.95f,700,4700,12,0.0f,0},
        {"clouds-v2-day-up35-coverage100",35,1.0f,700,4700,12,0.0f,0},
        {"clouds-v2-day-up70-coverage100",70,1.0f,700,4700,12,0.0f,0},
        {"clouds-v2-night-up35-coverage100",35,1.0f,700,4700,0,0.0f,0},
        {"clouds-v2-day-up35-highbase80",35,0.80f,10000,10000,12,0.0f,0},
        {"clouds-v2-night-up35-highbase80",35,0.80f,10000,10000,0,0.0f,0},
        {"clouds-v2-night-up60-highbase80",60,0.80f,10000,10000,0,0.0f,0},
        {"clouds-v2-night-up35-midbase80",35,0.80f,1600,4200,0,0.0f,0},
        {"clouds-v2-day-up35-midbase80",35,0.80f,1600,4200,12,0.0f,0},
        {"clouds-v2-night-up12-base1500-80",12,0.80f,1500,5000,0,0.0f,0},
        {"clouds-v2-night-up25-base1500-80",25,0.80f,1500,5000,0,0.0f,0},
        {"clouds-v2-night-up35-base1500-80",35,0.80f,1500,5000,0,0.0f,0},
        {"clouds-v2-day-up35-base1500-80",35,0.80f,1500,5000,12,0.0f,0},
        {"clouds-v2-day-up35-base1500-95",35,0.95f,1500,5000,12,0.0f,0},
        {"clouds-v2-day-up35-base1500-100",35,1.0f,1500,5000,12,0.0f,0},
        {"clouds-v2-day-up55-parallax-left",55,0.84f,700,4700,12,0.42f,-400},
        {"clouds-v2-day-up55-parallax-right",55,0.84f,700,4700,12,0.42f,400},
    };
    for (const auto& v : cloudDetailViews)
    {
        auto* cloud = scene->weathers.GetComponent(entity);
        cloud->volumetricCloudParameters.layerFirst.coverageAmount = v.coverage;
        cloud->volumetricCloudParameters.layerFirst.coverageMinimum = v.coverageFloor;
        cloud->volumetricCloudParameters.layerSecond.coverageAmount = 0.16f;
        cloud->volumetricCloudParameters.cloudStartHeight = v.height;
        cloud->volumetricCloudParameters.cloudThickness = v.thickness;
        const float p = XMConvertToRadians(v.pitch);
        const auto look = XMVectorSet(0, std::sin(p), std::cos(p), 0);
        const auto cloudEye = XMVectorSet(v.eyeX, 2, 0, 1);
        camera->TransformCamera(XMMatrixInverse(nullptr,
            XMMatrixLookAtLH(cloudEye, XMVectorAdd(cloudEye, look), XMVectorSet(0,1,0,0))));
        camera->UpdateCamera();
        SetSunTime(sun,v.sunHour);ApplySun(*scene,entity,sun);
        moon.enabled = v.sunHour < 6;
        moon.phase = 0.5f;
        moon.elevation = 32;
        StoreMoon(*scene,entity,moon);
        for(unsigned j=0;j<28;++j)frame();
        const auto begin=std::chrono::steady_clock::now();
        for(unsigned j=0;j<32;++j)frame();
        const double ms=std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-begin).count()/32.0;
        metrics<<v.name<<','<<ms<<'\n';
        metrics.flush();
        if(!capture(v.name))return 40;
    }
    metrics.close();
    std::cout << "Native GPU frames captured successfully" << std::endl;
    std::fflush(stdout);
    // Engine teardown follows the existing explicit graphics-proof convention.
    // Process exit releases the device after all GPU work/readback completes.
    std::_Exit(0);
}

#else
inline int RunNightSkyGraphicsProof(int, char**)
{
    std::fprintf(stderr, "Explicit GPU proof currently requires Windows.\n");
    return 30;
}
#endif
