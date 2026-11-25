// Native matched-scene diagnostic; no production renderer or shader changes.
#include "renegade/bridge/PlayerService.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
using namespace DirectX;
using namespace wi::scene;
namespace fs = std::filesystem;
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct TimedApplication : wi::Application { float Delta() const { return deltaTime; } };
struct Detach { wi::Application& app; ~Detach() { app.ActivatePath(nullptr); } };
static void Weather(Scene& s, XMFLOAT3 direction, float speed, float randomness) {
    auto apply = [&](WeatherComponent& w) { w.windDirection=direction; w.windSpeed=speed; w.windRandomness=randomness; };
    apply(s.weather);
    for (size_t i=0; i<s.weathers.GetCount(); ++i) apply(s.weathers[i]);
}
int main(int argc, char** argv) { try {
    Check(argc==3, "usage: WindIsolationReview prairie.wiscene fresh-output-directory");
    fs::path out=fs::u8path(argv[2]);
    Check(!fs::exists(out), "Output already exists; supply a fresh directory");
    fs::create_directories(out);
    HWND window=CreateWindowExW(0,L"STATIC",L"Native wind isolation",WS_POPUP,0,0,1920,1009,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    TimedApplication app; app.allow_hdr=false; app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
    app.setTargetFrameRate(75); app.setFrameRateLock(true);
    std::ofstream summary(out/"summary.csv");
    summary<<"mode,strands,forces,animations,wind_x,wind_speed,wind_randomness,samples,median_ms,p95_ms,p99_ms,max_ms,over_25ms,over_33ms\n";
    const char* modes[]={"travelling-gusts","no-wind","steady-force","native-breeze"};
    for (int mode=0; mode<4; ++mode) {
        Scene s; LoadModel(s,argv[1]);
        Check(s.hairs.GetCount()==1 && s.forces.GetCount()==5, "Unexpected source prairie");
        auto& hair=s.hairs[0];
        const auto strands=hair.strandCount;
        const auto rects=hair.atlas_rects;
        const auto length=hair.length, width=hair.width, drag=hair.drag, stiffness=hair.stiffness;
        Check(strands==204800 && !s.meshes.GetComponent(hair.meshID)->subsets.empty(), "Invalid emitter or density");
        Weather(s,{0,0,0},0,0);
        if (mode) {
            s.animations.Clear(); s.animation_datas.Clear();
            if (mode==2) {
                for (size_t i=s.forces.GetCount(); i>0; --i) {
                    auto entity=s.forces.GetEntity(i-1);
                    auto* name=s.names.GetComponent(entity);
                    if (!name || name->name!="Prairie ambient breeze") s.Entity_Remove(entity);
                }
            } else s.forces.Clear();
        }
        if (mode==3) Weather(s,{0.08f,0,0},1,5);
        fs::path folder=out/modes[mode]; fs::create_directories(folder);
        fs::path file=folder/"Main.wiscene";
        { wi::Archive archive(file.generic_string(),false,false); s.Serialize(archive); Check(archive.SaveFile(file.generic_string()), "Save failed"); }
        Scene cold; LoadModel(cold,file.generic_string());
        Check(cold.hairs.GetCount()==1, "Missing reopened grass");
        const auto& ch=cold.hairs[0];
        Check(ch.strandCount==strands && ch.length==length && ch.width==width && ch.drag==drag && ch.stiffness==stiffness, "Grass settings changed");
        Check(ch.atlas_rects.size()==rects.size() && std::memcmp(ch.atlas_rects.data(),rects.data(),rects.size()*sizeof(rects[0]))==0, "Atlas changed");
        Check(cold.forces.GetCount()==(mode==0?5u:mode==2?1u:0u), "Force count changed");
        Check(cold.animations.GetCount()==(mode==0?5u:0u), "Animation count changed");
        if (mode==3) Check(cold.weathers.GetCount() && cold.weathers[0].windDirection.x==0.08f, "Native breeze did not persist");
        cold.Clear();
        CameraComponent camera; camera.CreatePerspective(1920,1009,.05f,300);
        camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(-8,2,-7,1),XMVectorSet(0,.15f,26,1),XMVectorSet(0,1,0,0))));
        camera.UpdateCamera();
        wi::RenderPath3D render; render.scene=&s; render.camera=&camera;
        render.init(1920,1009,96); render.setMotionBlurEnabled(false); render.setDepthOfFieldEnabled(false); render.ResizeBuffers();
        app.ActivatePath(&render); Detach detach{app};
        std::vector<float> times; times.reserve(900);
        // No captures, readbacks or log writes during this measurement window.
        for (int f=0; f<1080; ++f) {
            app.is_window_active=true; app.Run();
            if (f>=180) times.push_back(app.Delta()*1000);
        }
        auto sorted=times; std::sort(sorted.begin(),sorted.end());
        std::ofstream raw(folder/"frame-times.csv"); raw<<"frame,delta_ms\n";
        for (size_t i=0; i<times.size(); ++i) raw<<i<<","<<times[i]<<"\n";
        summary<<modes[mode]<<","<<strands<<","<<s.forces.GetCount()<<","<<s.animations.GetCount()<<","<<s.weather.windDirection.x<<","<<s.weather.windSpeed<<","<<s.weather.windRandomness<<","<<times.size()<<","<<sorted[450]<<","<<sorted[855]<<","<<sorted[891]<<","<<sorted.back()<<","<<std::count_if(times.begin(),times.end(),[](float v){return v>25;})<<","<<std::count_if(times.begin(),times.end(),[](float v){return v>33.333f;})<<"\n";
        summary.flush();
        // Visual sequence is a separate, intentionally untimed phase.
        for (int f=0; f<120; ++f) {
            app.is_window_active=true; app.Run();
            if (f%4==0) {
                char name[40]; sprintf_s(name,"frame-%03d.png",f/4);
                wi::graphics::GetDevice()->WaitForGPU();
                Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(folder/name).generic_string()), "Capture failed");
            }
        }
        app.ActivatePath(nullptr); s.Clear();
        std::cout<<modes[mode]<<" SAVE_REOPEN_TIMING_CAPTURE_PASS"<<std::endl;
    }
    while (wi::renderer::IsPipelineCreationActive()) Sleep(10);
    wi::graphics::GetDevice()->WaitForGPU();
    std::cout<<"WIND_ISOLATION_FOUR_MATCHED_MODES_PASS"<<std::endl;
    return 0;
} catch (const std::exception& e) { std::cerr<<e.what()<<std::endl; return 1; } }
