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
    struct Case { const char* name; float time, gradient, aurora, stars; };
    const Case cases[] = {
        {"day-native",12,0,0,.85f}, {"day-extensions",12,.85f,.75f,.85f},
        {"night-empty",0,0,0,0}, {"night-native-stars",0,0,0,.85f},
        {"night-gradient",0,.85f,0,.85f}, {"night-aurora",0,.85f,.75f,.85f},
        {"dusk",18.2f,.85f,.75f,.85f}, {"dawn",6.5f,.85f,.75f,.85f}
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
