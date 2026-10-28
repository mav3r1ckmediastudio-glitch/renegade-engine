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
#include "renegade/bridge/RiverFoamService.h"

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
        const auto riverbed=scene.Entity_CreatePlane("Constant 40cm Underlay");
        auto* sediment=scene.materials.GetComponent(riverbed);
        sediment->SetBaseColor({0.28f,0.25f,0.19f,1});
        sediment->SetRoughness(.95f);
        sediment->SetDirty();
        // ECS component storage may relocate after adding the riverbed.
        material=scene.materials.GetComponent(plane);
        Require(material!=nullptr,"water material relocated");
        scene.transforms.GetComponent(plane)->translation_local={0,24,0};
        scene.transforms.GetComponent(plane)->SetDirty();
        SetPose(scene,riverbed,0,23.6f);
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
        pose(false); take("native-no-foam");
        wi::graphics::Shader pixel;
        wi::vector<uint8_t> bytes;
        Require(wi::helper::FileRead((out/"FlatRiverFoamPS.cso").generic_u8string(),bytes),"foam CSO missing");
        Require(gpu->CreateShader(wi::graphics::ShaderStage::PS,bytes.data(),bytes.size(),&pixel),"foam PS create");
        wi::renderer::CustomShader shader;
        shader.name="Flat River Contact Foam GPU Experiment";
        shader.filterMask=wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER;
        wi::graphics::PipelineStateDesc desc;
        desc.vs=wi::renderer::GetShader(wi::enums::VSTYPE_OBJECT_COMMON);
        desc.ps=&pixel;
        desc.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);
        desc.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_TRANSPARENT);
        desc.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
        Require(gpu->CreatePipelineState(&desc,&shader.pso[wi::enums::RENDERPASS_MAIN]),"foam PSO create");
        const int shaderID=wi::renderer::RegisterCustomShader(shader);
        material=scene.materials.GetComponent(plane);
        material->SetCustomShaderID(shaderID);material->SetDirty();

        RiverFoamRegion region;region.centre={0,24,0};
        wi::graphics::GPUBuffer regionBuffer;
        const auto authored=[&](const std::vector<RiverFoamRegion>& regions){
            Require(history.Execute(std::make_unique<SetRiverFoamRegionsCommand>(scene,plane,regions)),"author foam regions");
            std::vector<RiverFoamRegion> captured;std::string error;
            Require(ReadRiverFoamRegions(scene,plane,captured,error),"read foam regions");
            const auto packed=PackRiverFoamRegions(captured);
            wi::graphics::GPUBufferDesc bd;bd.size=packed.size()*sizeof(XMFLOAT4);
            bd.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
            bd.misc_flags=wi::graphics::ResourceMiscFlag::BUFFER_RAW;
            // Fixture serializes GPU work before replacing immutable resources.
            gpu->WaitForGPU();
            Require(gpu->CreateBuffer(&bd,packed.data(),&regionBuffer),"upload regions");
            auto* m=scene.materials.GetComponent(plane);
            m->userdata={uint32_t(gpu->GetDescriptorIndex(&regionBuffer,wi::graphics::SubresourceType::SRV)),uint32_t(captured.size()),0x52464f41,0};
            m->SetDirty();
        };
        authored({region});

        take("contact-foam-near");
        pose(true);take("contact-foam-overhead");
        wi::renderer::SetGameSpeed(1);
        for(int i=0;i<90;++i){app.Run();Sleep(16);}
        take("contact-foam-animated");

        // Rotate and relocate via authored commands; renderer reads the same packed schema.
        region.headingDegrees=75;authored({region});take("foam-direction-75");
        region.centre.x=2;region.wakeLength=2;region.strength=.4f;
        authored({region});take("foam-moved-short");
        Require(history.Undo(),"foam edit undo");
        // Save only native optics plus authoritative metadata; never transient bindings.
        material=scene.materials.GetComponent(plane);
        material->DisableCustomShader();material->userdata={0,0,0,0};material->SetDirty();
        {
            wi::Archive saved((out/"flat-river-foam.wiscene").generic_u8string(),false);
            scene.Serialize(saved);
        }
        {
            wi::Archive saved((out/"flat-river-foam.wiscene").generic_u8string(),true);
            wi::scene::Scene reopened;reopened.Serialize(saved);
            std::vector<RiverFoamRegion> savedRegions;std::string error;
            const auto savedRiver=reopened.Entity_FindByName("River Water");
            Require(ReadRiverFoamRegions(reopened,savedRiver,savedRegions,error)&&savedRegions.size()==1&&
                savedRegions[0].headingDegrees==75&&savedRegions[0].centre.x==0,"disk foam read/undo persistence");
            Require(!reopened.materials.GetComponent(savedRiver)->IsCustomShader()&&
                reopened.materials.GetComponent(savedRiver)->userdata.z==0,"no serialized renderer handles");
            std::cout<<"FLAT_RIVER_FOAM_DISK_REOPEN=PASS\n";
        }

        take("native-restored");
        Require(wi::renderer::GetShaderErrorCount()==0,"shader errors");

        if(interactive){
            region=RiverFoamRegion{};region.centre={0,24,0};
            if(fs::exists(out/"interactive-foam.wiscene")){
                wi::Archive disk((out/"interactive-foam.wiscene").generic_u8string(),true);
                wi::scene::Scene saved;saved.Serialize(disk);
                std::vector<RiverFoamRegion> values;std::string error;
                if(ReadRiverFoamRegions(saved,saved.Entity_FindByName("River Water"),values,error)&&values.size()==1)
                    region=values.front();
            }
            authored({region});
            material=scene.materials.GetComponent(plane);
            material->SetCustomShaderID(shaderID);material->SetDirty();
            pose(true);
            wi::gui::Label heading;
            heading.Create("Flat River Foam Test");
            heading.SetText("FLAT RIVER FOAM - EXPERIMENT\nContact foam + authored wake\nMove the region to match the rocks.");
            heading.SetPos({12,12});heading.SetSize({255,64});
            render.GetGUI().AddWidget(&heading);
            std::array<wi::gui::Slider,8> sliders;
            float* values[]={&region.centre.x,&region.centre.z,&region.headingDegrees,&region.wakeLength,
                &region.wakeWidth,&region.strength,&region.speed,&region.obstacleRadius};
            const char* names[]={"Position X","Position Z","Direction","Wake length","Wake width","Strength","Flow speed","Rock radius"};
            float low[]={-4,-4,-180,.1f,.02f,0,0,.05f};
            float high[]={4,4,180,10,2,1,3,3};
            bool pending=false;
            for(size_t n=0;n<sliders.size();++n){
                sliders[n].Create(low[n],high[n],*values[n],1000,names[n]);
                sliders[n].SetPos({110,100+float(n)*35});sliders[n].SetSize({130,22});
                sliders[n].OnSlide([&,n](const wi::gui::EventArgs& e){*values[n]=e.fValue;pending=true;});
                render.GetGUI().AddWidget(&sliders[n]);
            }
            wi::gui::Button save,reset;
            save.Create("Save test");save.SetPos({12,400});save.SetSize({110,28});
            reset.Create("Reset");reset.SetPos({135,400});reset.SetSize({110,28});
            bool savePending=false;
            save.OnClick([&](const wi::gui::EventArgs&){savePending=true;});
            reset.OnClick([&](const wi::gui::EventArgs&){
                region=RiverFoamRegion{};region.centre={0,24,0};
                for(size_t n=0;n<sliders.size();++n)sliders[n].SetValue(*values[n]);
                pending=true;
            });
            render.GetGUI().AddWidget(&save);render.GetGUI().AddWidget(&reset);
            SetWindowTextW(window,L"Renegade Flat River Foam - Experimental Test");
            ShowWindow(window,SW_SHOW);SetForegroundWindow(window);
            MSG msg{};
            while(IsWindow(window)){
                while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
                if(!IsWindow(window))break;
                if(pending){authored({region});pending=false;}
                if(savePending){
                    auto* m=scene.materials.GetComponent(plane);
                    const auto data=m->userdata;
                    m->DisableCustomShader();m->userdata={0,0,0,0};m->SetDirty();
                    {wi::Archive disk((out/"interactive-foam.wiscene").generic_u8string(),false);scene.Serialize(disk);}
                    m=scene.materials.GetComponent(plane);
                    m->SetCustomShaderID(shaderID);m->userdata=data;m->SetDirty();
                    savePending=false;std::cout<<"INTERACTIVE_FOAM_SAVE=PASS\n";
                }
                app.Run();Sleep(8);
            }
            render.GetGUI().RemoveWidget(&heading);render.GetGUI().RemoveWidget(&save);render.GetGUI().RemoveWidget(&reset);
            for(auto& slider:sliders)render.GetGUI().RemoveWidget(&slider);
        }
        std::cout<<"FLAT_RIVER_FOAM_GPU_CAPTURE=PASS VISUAL_ACCEPTANCE=OPEN\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }
}
