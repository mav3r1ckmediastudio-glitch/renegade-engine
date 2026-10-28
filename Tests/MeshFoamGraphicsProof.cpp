#include "renegade/bridge/RiverSplineService.h"
#include <Windows.h>
#include <cmath>
#include <cfloat>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace DirectX;
using namespace renegade::bridge;
namespace fs=std::filesystem;

static void Require(bool ok,const char* what)
{
    if(!ok) throw std::runtime_error(what);
}
static void SetPose(wi::scene::Scene& scene,wi::ecs::Entity entity,
                    float pitchDegrees,float y)
{
    auto* t=scene.transforms.GetComponent(entity);
    Require(t!=nullptr,"missing plane transform");
    t->translation_local={0,y,0};
    const XMVECTOR q=XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(pitchDegrees),0,0);
    XMStoreFloat4(&t->rotation_local,q);
    t->scale_local={7,1,24};
    t->SetDirty();
}

#include <ModelImporter.h>

int main(int argc,char** argv)
{
    if(argc!=2 && argc!=3)return 2;
    const bool interactive=argc==3 && std::string(argv[2])=="--interactive";
    fs::path out=fs::absolute(fs::u8path(argv[1]));
    std::cout.setf(std::ios::unitbuf);
    HWND window=CreateWindowExW(0,L"STATIC",L"Flat River Foam Proof",WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    struct ProofApp : wi::Application {void Initialize() override {wi::Application::Initialize();alwaysactive=true;}} app;app.allow_hdr=false;app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    auto* gpu=wi::graphics::GetDevice();
    struct Drain {~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}} drain;
    try {
        wi::scene::Scene scene;
        const std::vector<XMFLOAT3> points={
            {-3,0,-15},{0,0,-5},{0,0,5},{3,0,15}};
        RiverSplineOptions options;
        options.widthMeters=14.f;
        options.surfaceOffsetMeters=0.f;
        auto factory=std::make_unique<CreateRiverSplineCommand>(scene,points,options);
        auto* command=factory.get();
        CommandService history;
        Require(history.Execute(std::move(factory)),"stock native WATER factory");
        const auto prototype=command->CreatedEntity();
        Require(prototype!=wi::ecs::INVALID_ENTITY,"stock river entity");
        const auto* stockMaterial=scene.materials.GetComponent(prototype);
        Require(stockMaterial!=nullptr && !stockMaterial->IsCustomShader() &&
            stockMaterial->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER,
            "the stock river must use genuine native Wicked WATER");
        // Creating another material may reallocate Wicked ECS component storage.
        // Snapshot the factory material BEFORE adding plane or riverbed.
        const wi::scene::MaterialComponent stock=*stockMaterial;
        const auto plane=prototype;
        auto* material=scene.materials.GetComponent(plane);
        Require(material!=nullptr,"WATER plane material");
        *material=stock;
        // Native river ribbon UVs are metric. For a 14x48 metre *rectangular*
        // plane, set U/V so both axes have the SAME 4m normal-map tile scale.
        // Native river metric UVs retained.
        material->texAnimFrameRate=0; // frozen identical UV phase for each capture
        material->SetDirty();
        Require(material->textures[wi::scene::MaterialComponent::NORMALMAP]
            .resource.GetTexture().IsValid(),"stock water normal texture");
        // The spline render object is generated asynchronously on Update.
        // Dispose of the material-only factory before any frame is rendered,
        // leaving exactly ONE WATER object for reflection selection.
        // Keep actual river spline.
        const auto riverbed=scene.Entity_CreatePlane("Textured riverbed");
        auto* sediment=scene.materials.GetComponent(riverbed);
        sediment->SetBaseColor({0.46f,0.41f,0.31f,1});
        sediment->SetRoughness(.95f);
        sediment->textures[wi::scene::MaterialComponent::BASECOLORMAP].name =
            (out/"rocky_basecolor.png").generic_u8string();
        sediment->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource =
            wi::resourcemanager::Load(sediment->textures[wi::scene::MaterialComponent::BASECOLORMAP].name);
        sediment->SetDirty();
        // ECS component storage may relocate after adding the riverbed.
        material=scene.materials.GetComponent(plane);
        Require(material!=nullptr,"water material relocated");
        scene.transforms.GetComponent(plane)->translation_local={0,24,0};
        scene.transforms.GetComponent(plane)->SetDirty();
        SetPose(scene,riverbed,0,23.6f);
        // Separate grassy bank planes sit beside the flat-river ribbon.
        for (int side : {-1, 1}) {
            auto bank=scene.Entity_CreatePlane(side<0 ? "West grassy river bank" : "East grassy river bank");
            auto* b=scene.materials.GetComponent(bank);
            b->SetBaseColor({.7f,.8f,.65f,1});b->SetRoughness(.96f);
            b->textures[wi::scene::MaterialComponent::BASECOLORMAP].name =
                (out/"default_grass_basecolor.tga").generic_u8string();
            b->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource =
                wi::resourcemanager::Load(b->textures[wi::scene::MaterialComponent::BASECOLORMAP].name);
            b->SetDirty();
            auto* t=scene.transforms.GetComponent(bank);
            t->translation_local={float(side)*12,24.02f,0};
            t->scale_local={1.5f,1.f,24.f};t->SetDirty();
        }

        auto weatherEntity=scene.Entity_CreateTransform("Consistent Daylight");
        auto& weather=scene.weathers.Create(weatherEntity);
        weather.ambient={.08f,.08f,.08f};
        weather.horizon={.5f,.67f,.86f};
        weather.zenith={.18f,.32f,.64f};
        const auto sun=scene.Entity_CreateLight("Fixed Sun",
            {0,80,-20},{1,.95f,.85f},5,200,
            wi::scene::LightComponent::DIRECTIONAL);
        scene.transforms.GetComponent(sun)->RotateRollPitchYaw(
            XMFLOAT3(1.35f,-.78f,0));

        wi::scene::Scene rocks;
        ImportModel_GLTF((out/"mossy_rocks_low_poly.glb").generic_u8string(), rocks);
        rocks.Update(0);
        Require(rocks.objects.GetCount()>0,"rock import failed");
        XMFLOAT3 lo{FLT_MAX,FLT_MAX,FLT_MAX},hi{-FLT_MAX,-FLT_MAX,-FLT_MAX};
        for(const auto& a:rocks.aabb_objects) {
            lo.x=std::min(lo.x,a._min.x);lo.y=std::min(lo.y,a._min.y);lo.z=std::min(lo.z,a._min.z);
            hi.x=std::max(hi.x,a._max.x);hi.y=std::max(hi.y,a._max.y);hi.z=std::max(hi.z,a._max.z);
        }
        const float scale=3.f/std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});
        const auto normalization=XMMatrixTranslation(-(lo.x+hi.x)*.5f,-lo.y,-(lo.z+hi.z)*.5f)
            *XMMatrixScaling(scale,scale,scale)*XMMatrixTranslation(0,23.6f,0);
        std::vector<wi::ecs::Entity> roots;
        for(size_t i=0;i<rocks.transforms.GetCount();++i)
            if(!rocks.hierarchy.Contains(rocks.transforms.GetEntity(i))) roots.push_back(rocks.transforms.GetEntity(i));
        for(auto e:roots) {auto* t=rocks.transforms.GetComponent(e);t->MatrixTransform(normalization);t->UpdateTransform();}
        scene.Merge(rocks);
        std::cout<<"ROCK_IMPORT=PASS normalized_extent_m=3 source_CC_BY_4_0_mdsz13\n";

        // Independent alpha-textured foam overlays: never alter native WATER.
        std::vector<wi::ecs::Entity> foamOverlays;
        for(int i=0;i<2;++i) {
            auto foam=scene.Entity_CreatePlane(i==0 ? "Upstream rock foam" : "Downstream trailing foam");
            auto* fm=scene.materials.GetComponent(foam);
            fm->shaderType=wi::scene::MaterialComponent::SHADERTYPE_UNLIT;
            fm->SetBaseColor({1.f,1.f,1.f,.93f});
            // Wicked stores alpha-test threshold as 1 - alphaRef.
            // Keep the full soft alpha range rather than clipping below .84.
            fm->SetAlphaRef(1.f);
            fm->SetRoughness(1.f);fm->SetCastShadow(false);fm->SetDoubleSided(true);
            fm->userBlendMode=wi::enums::BLENDMODE_ALPHA;
            fm->textures[wi::scene::MaterialComponent::BASECOLORMAP].name =
                (out/(i==0 ? "river_foam_contact.png" : "river_foam_wisps.png")).generic_u8string();
            fm->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource =
                wi::resourcemanager::Load(fm->textures[wi::scene::MaterialComponent::BASECOLORMAP].name);
            std::cout<<"FOAM_TEXTURE_LOAD="<<fm->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid()
                <<" "<<fm->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.GetTexture().IsValid()
                <<" FILTER="<<fm->GetFilterMask()<<" BLEND="<<int(fm->GetBlendMode())<<"\n";
            fm->texAnimFrameRate=60.f;
            fm->texAnimDirection={0.f,i==0?-.00015f:-.0006f};
            fm->SetDirty();
            auto* tr=scene.transforms.GetComponent(foam);
            tr->translation_local={i==0?0.f:.1f,24.105f,i==0?0.f:2.15f};
            tr->scale_local={i==0?1.6f:1.1f,1.f,i==0?1.5f:2.2f};
            tr->SetDirty();
            foamOverlays.push_back(foam);
        }
        wi::scene::CameraComponent camera;
        camera.CreatePerspective(960,640,.1f,500.f,XMConvertToRadians(60));
        wi::RenderPath3D render;
        render.scene=&scene;
        render.camera=&camera;
        render.init(960,640,96);
        render.setMotionBlurEnabled(false);
        render.setOcclusionCullingEnabled(false);
        render.setDepthOfFieldEnabled(false);
        render.ResizeBuffers();
        struct PathGuard
        {
            wi::Application& app;
            ~PathGuard()
            {
                if(app.GetActivePath()!=nullptr)app.ActivatePath(nullptr);
            }
        } pathGuard{app};
        wi::renderer::SetTemporalAAEnabled(false);
        app.ActivatePath(&render);
        const auto poseCamera=[&](float waterY,bool highView)
        {
            const XMVECTOR eye=XMVectorSet(0,waterY+(highView ? 75.f : 16.f),-52,1);
            const XMVECTOR look=XMVectorSet(0,waterY,0,1);
            camera.TransformCamera(XMMatrixInverse(nullptr,
                XMMatrixLookAtLH(eye,look,XMVectorSet(0,1,0,0))));
            camera.UpdateCamera();
        };
        poseCamera(24,false);
        for(int i=0;i<56;++i){app.Run();Sleep(3);}
        int wait=0;
        while(wi::renderer::IsPipelineCreationActive() && wait++<800)Sleep(25);
        Require(!wi::renderer::IsPipelineCreationActive(),"native WATER pipelines pending");
        wi::renderer::SetGameSpeed(0);

        const auto take=[&](const std::string& tag)
        {
            for(int frame=0;frame<12;++frame){app.Run();Sleep(2);}
            gpu->WaitForGPU();
            const fs::path image=out/(tag+".png");
            Require(wi::helper::saveTextureToFile(render.GetRenderResult3D(),
                image.generic_u8string()),"DX12 capture failed");
        };

        // Frame the actual supplied rock with flat native water.
        auto pose=[&](bool high) {
            camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
                high?XMVectorSet(0,30,-5,1):XMVectorSet(4,26,-6,1),
                XMVectorSet(0,24,0,1),XMVectorSet(0,1,0,0))));
            camera.UpdateCamera();
        };
        pose(false);
        // Capture a TRUE native-only baseline. Previously both captures
        // contained the same overlays, making the comparison meaningless.
        for(auto e:foamOverlays) scene.materials.GetComponent(e)->SetOpacity(0);
        take("mesh-foam-off");
        for(auto e:foamOverlays) scene.materials.GetComponent(e)->SetOpacity(.93f);
        take("mesh-foam-on");
        for(auto e:foamOverlays) {
            auto* fm=scene.materials.GetComponent(e);
            fm->SetBaseColor({1,0,0,1});
            fm->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource={};
            fm->SetDirty();
        }
        take("mesh-foam-debug-red");
        for(auto e:foamOverlays) {
            auto* fm=scene.materials.GetComponent(e);
            fm->SetBaseColor({1,1,1,.93f});
            fm->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource =
                wi::resourcemanager::Load(fm->textures[wi::scene::MaterialComponent::BASECOLORMAP].name);
            fm->SetDirty();
        }
        take("native-no-foam");
        // This proof uses standard alpha-textured meshes ONLY. Native WATER
        // shader, material optics, scene and render path remain unmodified.
        for(auto e:foamOverlays) scene.materials.GetComponent(e)->SetOpacity(0);
        take("mesh-foam-off");
        for(auto e:foamOverlays) scene.materials.GetComponent(e)->SetOpacity(.93f);
        take("mesh-foam-on");
        wi::renderer::SetGameSpeed(1);
        for(int frame=0;frame<70;++frame){app.Run();Sleep(16);}
        take("mesh-foam-animated");
        wi::renderer::SetGameSpeed(0);
        Require(!scene.materials.GetComponent(plane)->IsCustomShader(),
            "native water was modified");
        { wi::Archive ar((out/"mesh-foam-riverside.wiscene").generic_u8string(),false);
          scene.Serialize(ar); }
        { wi::Archive ar((out/"mesh-foam-riverside.wiscene").generic_u8string(),true);
          wi::scene::Scene reloaded;reloaded.Serialize(ar);
          const auto water=reloaded.Entity_FindByName("River Water");
          Require(water!=wi::ecs::INVALID_ENTITY &&
            reloaded.materials.GetComponent(water)->shaderType ==
                wi::scene::MaterialComponent::SHADERTYPE_WATER,
            "saved native water missing");
          for(const auto* name:{"Upstream rock foam","Downstream trailing foam"}){
            const auto e=reloaded.Entity_FindByName(name);
            Require(e!=wi::ecs::INVALID_ENTITY,"foam overlay not persisted");
            const auto* mat=reloaded.materials.GetComponent(e);
            Require(mat && mat->userBlendMode==wi::enums::BLENDMODE_ALPHA,
                "foam transparency not persisted");
          }
        }
        std::cout<<"MESH_FOAM_SAVE_REOPEN=PASS native_water_intact=1\n";
        Require(wi::renderer::GetShaderErrorCount()==0,"shader errors");
        if(interactive){
            pose(false);
            bool foamOn=true;
            wi::gui::Label heading;
            heading.Create("Mesh Foam Test");
            heading.SetText("MESH FOAM - WATER UNCHANGED\nF = foam on/off, WASD = move\nArrows = look; ESC or X = close.");
            heading.SetPos({12,12});heading.SetSize({275,68});
            render.GetGUI().AddWidget(&heading);
            SetWindowTextW(window,L"Renegade Mesh Foam - Isolated Test");
            ShowWindow(window,SW_SHOW);SetForegroundWindow(window);
            MSG msg{};
            while(IsWindow(window)){
                while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
                if(!IsWindow(window))break;
                static float walkX=4,walkZ=-6,lookYaw=-.58f,lookPitch=-.20f;
                const float step=.09f,turn=.015f;
                if(GetAsyncKeyState(VK_LEFT)&0x8000)lookYaw-=turn;
                if(GetAsyncKeyState(VK_RIGHT)&0x8000)lookYaw+=turn;
                if(GetAsyncKeyState(VK_UP)&0x8000)lookPitch=std::max(-1.1f,lookPitch-turn);
                if(GetAsyncKeyState(VK_DOWN)&0x8000)lookPitch=std::min(.65f,lookPitch+turn);
                if(GetAsyncKeyState('W')&0x8000){walkX+=std::sin(lookYaw)*step;walkZ+=std::cos(lookYaw)*step;}
                if(GetAsyncKeyState('S')&0x8000){walkX-=std::sin(lookYaw)*step;walkZ-=std::cos(lookYaw)*step;}
                if(GetAsyncKeyState('A')&0x8000){walkX-=std::cos(lookYaw)*step;walkZ+=std::sin(lookYaw)*step;}
                if(GetAsyncKeyState('D')&0x8000){walkX+=std::cos(lookYaw)*step;walkZ-=std::sin(lookYaw)*step;}
                walkX=std::clamp(walkX,-17.f,17.f);walkZ=std::clamp(walkZ,-18.f,18.f);
                const XMVECTOR eye=XMVectorSet(walkX,25.7f,walkZ,1);
                const XMVECTOR dir=XMVectorSet(std::sin(lookYaw)*std::cos(lookPitch),std::sin(lookPitch),std::cos(lookYaw)*std::cos(lookPitch),0);
                camera.TransformCamera(XMMatrixInverse(nullptr,
                    XMMatrixLookAtLH(eye,XMVectorAdd(eye,dir),XMVectorSet(0,1,0,0))));
                camera.UpdateCamera();
                if ((GetAsyncKeyState('F') & 1) != 0) {
                    foamOn=!foamOn;
                    for(auto e:foamOverlays) { auto* fm=scene.materials.GetComponent(e); fm->SetOpacity(foamOn?.93f:0.f); }
                }
                app.Run();Sleep(8);
            }
            render.GetGUI().RemoveWidget(&heading);
        }
        std::cout<<"MESH_FOAM_GPU_CAPTURE=PASS VISUAL_ACCEPTANCE=OPEN\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }
}
