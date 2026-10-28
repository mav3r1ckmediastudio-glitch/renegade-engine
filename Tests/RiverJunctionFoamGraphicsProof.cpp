#include "RiverJunctionFoamTransport.h"
#include "renegade/bridge/SceneService.h"
#include <Windows.h>
#include "wiRawInput.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <chrono>
using namespace junction_proof;using namespace junction_foam;namespace fs=std::filesystem;
static uint32_t Bits(float f){uint32_t b;std::memcpy(&b,&f,4);return b;}
struct Params{XMFLOAT4 bounds;XMUINT4 control;XMFLOAT4 settings;XMUINT4 foamControl;XMFLOAT4 foamSettings;};
struct Push{XMUINT4 resolution;XMFLOAT4 bounds,settings;};
static_assert(sizeof(Params)==80&&sizeof(Push)==48);
static_assert(offsetof(Push,bounds)==16&&offsetof(Push,settings)==32);
struct Path:wi::RenderPath3D {
    bool active=false,paused=true,enabled=false,frozen=false;
    unsigned state=0,start=0,steps=0,sourceMask=7;float clock=0,manual=0,step=0,maximumRate=0,speed=1;
    wi::ecs::Entity river=wi::ecs::INVALID_ENTITY;XMFLOAT4 bounds;
    wi::graphics::Texture foam[2],flux,source,velocity;wi::graphics::Shader simulation;
    std::vector<wi::ecs::Entity> hiddenRibbons;
    void Update(float dt)override{
        const float total=active?(paused?manual:std::min(dt,.05f)):0;manual=0;
        steps=total>0?unsigned(std::max(1.f,std::ceil(total*maximumRate*speed/.75f))):0;
        start=state;step=steps?total/steps:0;clock+=total;state^=steps&1;
        if(enabled){auto* m=scene->materials.GetComponent(river);m->userdata.z=state;m->userdata.w=Bits(clock);m->SetDirty();}
        for(auto id:hiddenRibbons)if(auto* o=scene->objects.GetComponent(id))o->SetRenderable(false);
        // Keep GUI input responsive while native scene time is frozen by GameSpeed(0).
        wi::RenderPath3D::Update(dt);
        for(auto id:hiddenRibbons)if(auto* o=scene->objects.GetComponent(id))o->SetRenderable(false);
    }
    void Dump()const {
        auto* m=scene->materials.GetComponent(river);auto* o=scene->objects.GetComponent(river);
        std::cout<<"REFLECT visible="<<visibility_main.planar_reflection_visible<<" plane="<<visibility_main.reflectionPlane.x<<","<<visibility_main.reflectionPlane.y<<","<<visibility_main.reflectionPlane.z<<","<<visibility_main.reflectionPlane.w
            <<" material="<<int(m->shaderType)<<" expected="<<int(wi::scene::MaterialComponent::SHADERTYPE_WATER)
            <<" request="<<o->IsRequestPlanarReflection()<<" centreY="<<o->center.y<<" skyMap="<<scene->weather.skyMap.IsValid()<<"\n";
    }
    void Render()const override{
        using namespace wi::graphics;
        if(steps){auto* gpu=GetDevice();auto cmd=gpu->BeginCommandList();wi::renderer::BindCommonResources(cmd);
            unsigned current=start;const auto& desc=foam[0].GetDesc();
            for(unsigned n=0;n<steps;++n){unsigned next=1-current;
                auto barrier=GPUBarrier::Image(&foam[next],ResourceState::SHADER_RESOURCE,ResourceState::UNORDERED_ACCESS);
                gpu->Barrier(&barrier,1,cmd);gpu->BindComputeShader(&simulation,cmd);
                gpu->BindResource(&foam[current],0,cmd);gpu->BindResource(&flux,1,cmd);
                gpu->BindResource(&source,2,cmd);gpu->BindResource(&velocity,3,cmd);gpu->BindUAV(&foam[next],0,cmd);
                Push push{{desc.width,desc.height,sourceMask,0},bounds,{step,clock-(steps-1-n)*step,6,speed}};
                gpu->PushConstants(&push,sizeof(push),cmd);gpu->Dispatch((desc.width+7)/8,(desc.height+7)/8,1,cmd);
                barrier=GPUBarrier::Image(&foam[next],ResourceState::UNORDERED_ACCESS,ResourceState::SHADER_RESOURCE);
                gpu->Barrier(&barrier,1,cmd);current=next;
            }
        }
        wi::RenderPath3D::Render();
    }
};
static std::vector<Surface> Gather(const wi::scene::Scene& scene){
    std::vector<Surface> result;
    for(size_t n=0;n<scene.objects.GetCount();++n){
        const auto id=scene.objects.GetEntity(n);const auto* name=scene.names.GetComponent(id);if(!name)continue;
        const bool bed=name->name=="Shallow rocky confluence bed",rock=name->name=="Native mossy river rock";
        if(!bed&&!rock)continue;
        const auto& object=scene.objects[n];const auto* mesh=scene.meshes.GetComponent(object.meshID);
        Check(object.IsRenderable()&&mesh&&!mesh->IsSkinned(),"Native obstacle must be rigid/renderable");
        Surface s;s.terrain=bed;const auto world=scene.ComputeEntityMatrixRecursive(id);
        uint32_t first=0,last=0;mesh->GetLODSubsetRange(0,first,last);
        for(uint32_t part=first;part<last;++part){const auto& subset=mesh->subsets[part];
            Check(size_t(subset.indexOffset)+subset.indexCount<=mesh->indices.size(),"Native subset bounds");
            for(uint32_t k=subset.indexOffset;k+2<subset.indexOffset+subset.indexCount;k+=3){
                Triangle t;XMFLOAT3* v[]={&t.a,&t.b,&t.c};
                for(int j=0;j<3;++j){auto ix=mesh->indices[k+j];Check(ix<mesh->vertex_positions.size(),"Native vertex bounds");
                    XMStoreFloat3(v[j],XMVector3TransformCoord(XMLoadFloat3(&mesh->vertex_positions[ix]),world));}
                s.triangles.push_back(t);
            }
        }
        std::cout<<"ACTUAL_OBSTACLE terrain="<<bed<<" triangles="<<s.triangles.size()<<"\n";result.push_back(std::move(s));
    }
    Check(result.size()==4,"Native saved fixture must contain bed and three rocks");return result;
}
int main(int argc,char** argv){
    if(argc<5||argc>7)return 2;const bool interactive=std::string(argv[argc-1])=="--interactive";
    const auto out=fs::absolute(argv[1]);if(fs::exists(out)&&!fs::is_empty(out))return 2;fs::create_directories(out);
    std::cout.setf(std::ios::unitbuf);
    HWND window=CreateWindowExW(0,L"STATIC",L"Renegade River G7C - Tributary Foam",WS_OVERLAPPEDWINDOW,60,40,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    struct App:wi::Application{void Initialize()override{wi::Application::Initialize();alwaysactive=true;}}app;
    app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();auto* gpu=wi::graphics::GetDevice();
    try{
        wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
        SceneService service;Check(service.LoadScene(fs::absolute(argv[2]).generic_u8string()),service.LastError());
        auto& scene=service.GetScene();scene.Update(0);gpu->WaitForGPU();
        const auto river=scene.Entity_FindByName("G7B continuous native WATER");
        auto* material=scene.materials.GetComponent(river);Check(material&&!material->IsCustomShader()&&material->userdata.y==0&&material->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER,"Native WATER archive without transient bindings required");
        if(argc>=6&&std::string(argv[5]).find("--")!=0){
            SceneService skyReference;Check(skyReference.LoadScene(fs::absolute(argv[5]).generic_u8string()),skyReference.LastError());
            skyReference.GetScene().Update(0);scene.weather=skyReference.GetScene().weather;
        }
        // G7B's cached scene.weather was not an ECS component and was not archived.
        // Establish the same native sky settings as serializable fixture state.
        // Controlled native gradient lighting isolates this proof from atmospheric LUT startup.
        // Native WATER, planar reflection, refraction, lights and materials remain untouched.
        scene.weather.SetRealisticSky(false);scene.weather.horizon={.32f,.44f,.58f};
        scene.weather.zenith={.055f,.12f,.22f};scene.weather.ambient={.3f,.35f,.4f};scene.weather.skyExposure=1;
        if(scene.weathers.GetCount())scene.weathers[0]=scene.weather;
        else scene.weathers.Create(scene.Entity_CreateTransform("G7C native sky"))=scene.weather;
        std::cout<<"SKY_COMPONENTS="<<scene.weathers.GetCount()<<" realistic="<<scene.weathers[0].IsRealisticSky()<<" sun="<<scene.weather.sunColor.x<<" skyMap="<<scene.weather.skyMap.IsValid()<<" lights="<<scene.lights.GetCount()<<"\n";
        const auto optics=*material;const auto objects=scene.objects.GetCount(),meshes=scene.meshes.GetCount();
        auto network=Network(scene);std::shared_ptr<const RiverJunctionSurface> surface;std::string error;
        Check(BuildRiverJunctionSurface(*network,surface,error),error);
        auto domain=Build(*network,*surface,Gather(scene));
        double west=0,east=0;for(auto s:domain.source){west+=s.x;east+=s.y;}
        Check(west>0&&east>0,"Both actual tributary rocks must emit");
        std::cout<<"NATIVE_DOMAIN fluid="<<domain.active.size()<<" contacts="<<domain.mask.counts[RockContact]
            <<" solid="<<domain.mask.counts[SolidExcluded]<<" westRate="<<west<<" eastRate="<<east
            <<" stagnantCells="<<domain.disconnectedCells<<" divergence="<<domain.maximumDivergence<<" speed="<<domain.maximumSpeed<<"\n";
        Path path;path.scene=&scene;path.river=river;
        for(const auto& reach:Inputs(scene)) {
            auto* object=scene.objects.GetComponent(reach.river);Check(object!=nullptr,"Native spline object missing");
            std::cout<<"ARCHIVE_NATIVE_VISIBLE="<<object->IsRenderable()<<"\n";
            path.hiddenRibbons.push_back(reach.river);object->SetRenderable(false);
        }path.bounds=domain.mask.bounds;path.maximumRate=domain.maximumRate;
        auto upload=[&](wi::graphics::Texture& tex,const std::vector<XMFLOAT4>& data,bool writable=false){
            wi::graphics::TextureDesc td;td.width=domain.mask.width;td.height=domain.mask.height;
            td.format=wi::graphics::Format::R32G32B32A32_FLOAT;td.layout=wi::graphics::ResourceState::SHADER_RESOURCE;
            td.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;if(writable)td.bind_flags|=wi::graphics::BindFlag::UNORDERED_ACCESS;
            wi::graphics::SubresourceData initial;initial.data_ptr=data.data();initial.row_pitch=td.width*sizeof(XMFLOAT4);initial.slice_pitch=initial.row_pitch*td.height;
            Check(gpu->CreateTexture(&td,&initial,&tex),"Proof texture upload");
        };
        auto descriptor=[&](const wi::graphics::Texture& t){const int n=gpu->GetDescriptorIndex(&t,wi::graphics::SubresourceType::SRV);Check(n>=0,"Texture SRV");return uint32_t(n);};
        std::vector<XMFLOAT4> zero(domain.faces.size()),normal(zero.size());
        for(size_t k=0;k<normal.size();++k){auto q=surface->Evaluate({domain.mask.bounds.x+(k%domain.mask.width+.5f)/domain.mask.width*domain.mask.bounds.z,
            domain.mask.bounds.y+(k/domain.mask.width+.5f)/domain.mask.height*domain.mask.bounds.w});
            if(q.valid){float speed=std::hypot(q.velocityXZ.x,q.velocityXZ.y);normal[k]={q.velocityXZ.x/speed,q.velocityXZ.y/speed,speed,1};}}
        for(int k:domain.active)normal[k]=domain.velocity[k];
        wi::graphics::Texture normals;upload(normals,normal);upload(path.flux,domain.faces);upload(path.source,domain.source);upload(path.velocity,domain.velocity);
        upload(path.foam[0],zero,true);upload(path.foam[1],zero,true);
        Params params{domain.mask.bounds,{descriptor(normals),Bits(-1),Bits(1),0},{4,std::min(4.f,4.f/domain.maximumSpeed),0,1},
            {descriptor(path.foam[0]),descriptor(path.foam[1]),descriptor(path.source),descriptor(path.velocity)},{1,0,0,0}};
        wi::vector<uint8_t> bytes;wi::graphics::Shader pixel;
        Check(wi::helper::FileRead((fs::absolute(argv[3])/"RiverContactFoamPS.cso").generic_u8string(),bytes),"G7C lit foam shader");
        Check(gpu->CreateShader(wi::graphics::ShaderStage::PS,bytes.data(),bytes.size(),&pixel),"Foam shader create");
        Check(wi::helper::FileRead(fs::absolute(argv[4]).generic_u8string(),bytes),"Junction compute shader");
        Check(gpu->CreateShader(wi::graphics::ShaderStage::CS,bytes.data(),bytes.size(),&path.simulation),"Compute create");
        wi::renderer::CustomShader custom;custom.name="G7C shared-domain native WATER foam";custom.filterMask=wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER;
        wi::graphics::PipelineStateDesc pso;pso.vs=wi::renderer::GetShader(wi::enums::VSTYPE_OBJECT_COMMON);pso.ps=&pixel;
        pso.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);pso.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_TRANSPARENT);
        pso.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
        Check(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_MAIN]),"Native WATER main PSO");
        pso.vs=wi::renderer::GetShader(wi::enums::VSTYPE_SHADOW_TRANSPARENT);pso.ps=wi::renderer::GetShader(wi::enums::PSTYPE_SHADOW_TRANSPARENT);
        pso.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENTSHADOW);pso.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEPTHREAD);
        pso.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_SHADOW_DOUBLESIDED);
        Check(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_SHADOW]),"Native WATER shadow PSO");
        const auto shader=wi::renderer::RegisterCustomShader(custom);wi::graphics::GPUBuffer buffer;
        wi::scene::CameraComponent camera;camera.CreatePerspective(1264,761,.05f,500,XMConvertToRadians(60));
        path.camera=&camera;path.init(1264,761,96);path.setMotionBlurEnabled(false);path.setDepthOfFieldEnabled(false);
        path.setOcclusionCullingEnabled(false);path.ResizeBuffers();wi::renderer::SetTemporalAAEnabled(false);
        struct Guard{wi::Application& app;~Guard(){app.ActivatePath(nullptr);while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}}guard{app};
        wi::gui::Label label;label.Create("Tributary foam proof");label.SetPos({12,12});label.SetSize({1190,78});path.GetGUI().AddWidget(&label);
        wi::gui::Slider foamAmountSlider;
        foamAmountSlider.Create(0,4,1,400,"Foam amount (x): ");
        foamAmountSlider.SetPos({170,102});foamAmountSlider.SetSize({350,26});
        foamAmountSlider.valueInputField.SetFloatPrecision(2);
        foamAmountSlider.SetTooltip("0 = no visible foam; 1 = current look; 4 = dense. Changes coverage immediately, including while paused. Flow and foam history are retained.");
        wi::gui::Button foamDefault;foamDefault.Create("Default (1x)");foamDefault.SetPos({536,102});foamDefault.SetSize({118,26});
        if(interactive){path.GetGUI().AddWidget(&foamAmountSlider);path.GetGUI().AddWidget(&foamDefault);}
        float foamAmount=1;
        int mode=2;
        auto bind=[&](){gpu->WaitForGPU();material=scene.materials.GetComponent(river);path.enabled=mode>0;
            params.foamSettings.x=mode>=2?foamAmount:0.f;params.foamSettings.z=mode==3?1.f:0.f;params.control.z=Bits(path.speed);
            if(mode){wi::graphics::GPUBufferDesc bd;bd.size=sizeof(params);bd.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
                bd.misc_flags=wi::graphics::ResourceMiscFlag::BUFFER_RAW;Check(gpu->CreateBuffer(&bd,&params,&buffer),"Parameter buffer");
                material->SetCustomShaderID(shader);material->userdata={uint32_t(gpu->GetDescriptorIndex(&buffer,wi::graphics::SubresourceType::SRV)),0x52464c35,path.state,Bits(path.clock)};
            }else{material->DisableCustomShader();material->userdata=optics.userdata;}material->SetDirty();
            foamAmountSlider.SetEnabled(mode==2);foamDefault.SetEnabled(mode==2);
            label.SetText(std::string("RIVER G7C / TRIBUTARY FOAM / ")+(mode==0?"NATIVE WATER":mode==1?"DIRECTED WATER":mode==3?"DENSITY DIAGNOSTIC (ORANGE)":"DIRECTED WATER + CONTACT FOAM")+
                (path.paused?" / PAUSED":" / RUNNING")+(path.sourceMask?" / EMISSION ON":" / EMISSION OFF")+(path.speed==0?" / FLOW ZERO":"")+"\nF native/flow/foam   Space pause   K emission   Z zero flow   X reset   D density debug   T TAA   1-5 views   ESC close\nIsolated shared transport proof. Creator snapping and general hydraulics are deferred.");
        };
        auto setFoamAmount=[&](float value){
            const float bounded=std::isfinite(value)?std::clamp(value,0.f,4.f):1.f;
            foamAmountSlider.SetRange(0,4);foamAmountSlider.SetValue(bounded);
            if(foamAmount==bounded)return;
            foamAmount=bounded;bind();std::cout<<"FOAM_AMOUNT="<<foamAmount<<" clock="<<path.clock<<" state="<<path.state<<"\n";
        };
        foamAmountSlider.OnSlide([&](const wi::gui::EventArgs& args){setFoamAmount(args.fValue);});
        foamDefault.OnClick([&](const wi::gui::EventArgs&){setFoamAmount(1);});
        const XMFLOAT3 eyes[]={{8,1.3f,5},{-2,34,22},{-9,1.2f,-4},{-6,1.1f,8},{2,6,10}};
        const XMFLOAT3 targets[]={{-1,0,0},{-1,0,0},{-10,0,-7},{-7,0,5},{-2,0,0}};
        auto pose=[&](int n){camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMLoadFloat3(&eyes[n]),XMLoadFloat3(&targets[n]),XMVectorSet(0,1,0,0))));camera.UpdateCamera();};
        auto frames=[&](int n){for(int j=0;j<n;++j)app.Run();};
        auto clear=[&](){gpu->WaitForGPU();path.clock=0;path.state=0;upload(path.foam[0],zero,true);upload(path.foam[1],zero,true);
            params.foamControl.x=descriptor(path.foam[0]);params.foamControl.y=descriptor(path.foam[1]);bind();};
        auto advance=[&](float seconds){while(seconds>1e-6f){float dt=std::min(seconds,.1f);path.manual=dt;frames(1);seconds-=dt;}};
        auto read=[&](const std::string& name){
            gpu->WaitForGPU();wi::vector<uint8_t> raw;Check(wi::helper::saveTextureToMemory(path.foam[path.state],raw),"State readback");
            Check(raw.size()==zero.size()*sizeof(XMFLOAT4),"State readback size");std::vector<XMFLOAT4> data(zero.size());std::memcpy(data.data(),raw.data(),raw.size());
            std::ofstream dump(out/(name+".bin"),std::ios::binary);dump.write(reinterpret_cast<const char*>(raw.data()),raw.size());
            double mass=0,outlet=0;float maximum=0;
            for(size_t k=0;k<data.size();++k){const auto v=data[k];Check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.w)&&v.x>=-1e-7f,"Finite positive GPU state");
                Check(domain.index[k]>=0||v.x==0,"GPU foam outside native fluid/inside rock");mass+=v.x;maximum=std::max(maximum,v.x);
                if(domain.mask.bounds.x+(k%domain.mask.width+.5f)/domain.mask.width*domain.mask.bounds.z>5)outlet+=v.x;
            }
            std::cout<<"GPU_STATE="<<name<<" massCells="<<mass<<" outletCells="<<outlet<<" maximum="<<maximum<<"\n";return data;
        };
        auto capture=[&](const std::string& name){frames(16);while(gpu->GetFrameCount()%256)frames(1);gpu->WaitForGPU();
            Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/(name+".png")).generic_u8string()),"Native capture");path.Dump();std::cout<<"CAPTURE="<<name<<" time="<<path.clock<<"\n";};
        // Warm native WATER and atmosphere at eye height before freezing a deterministic proof.
        mode=0;pose(0);bind();app.ActivatePath(&path);frames(160);
        while(wi::renderer::IsPipelineCreationActive())frames(1);
        mode=2;bind();frames(32);pose(1);frames(32);
        while(wi::renderer::IsPipelineCreationActive())frames(1);
        std::cout<<"WARM_SKY realistic="<<scene.weather.IsRealisticSky()<<" sun="<<scene.weather.sunColor.x<<","<<scene.weather.sunColor.y<<","<<scene.weather.sunColor.z<<" exposure="<<scene.weather.skyExposure<<"\n";
        wi::renderer::SetGameSpeed(0);path.frozen=true;path.active=true;clear();
        if(std::string(argv[argc-1])=="--inspect"){
            mode=0;bind();pose(0);capture("inspect-native-eye");
            mode=2;bind();path.sourceMask=1;advance(8);capture("inspect-source-one");
            path.sourceMask=0;advance(12);capture("inspect-source-off");
            advance(32);capture("inspect-expired");
            clear();path.sourceMask=7;advance(24);capture("inspect-after-clear");
            mode=0;bind();path.GetGUI().RemoveWidget(&foamAmountSlider);path.GetGUI().RemoveWidget(&foamDefault);path.GetGUI().RemoveWidget(&label);return 0;
        }
        if(!interactive){
            mode=0;bind();pose(0);capture("native-eye-before");pose(1);capture("native-before");mode=2;bind();
            path.manual=.005f;frames(1);std::cout<<"DISPATCH clock="<<path.clock<<" steps="<<path.steps<<" state="<<path.state<<" start="<<path.start<<" dt="<<path.step<<" mask="<<path.sourceMask<<"\n";auto actual=read("one-step"),reference=Step(domain,zero,.005f,.005f);
            float maxError=0;for(size_t k=0;k<actual.size();++k)for(int c=0;c<4;++c)maxError=std::max(maxError,std::abs((&actual[k].x)[c]-(&reference[k].x)[c]));
            std::cout<<"GPU_CPU_MAX_ERROR="<<maxError<<"\n";Check(maxError<2e-5f,"GPU/CPU one-step parity");
            for(int n=0;n<32;++n){path.manual=.005f;frames(1);reference=Step(domain,reference,.005f,path.clock);}
            actual=read("multi-step");maxError=0;
            for(size_t k=0;k<actual.size();++k)for(int c=0;c<4;++c)maxError=std::max(maxError,std::abs((&actual[k].x)[c]-(&reference[k].x)[c]));
            std::cout<<"GPU_CPU_MULTI_STEP_MAX_ERROR="<<maxError<<"\n";Check(maxError<.0001f,"GPU/CPU advected state parity");
            pose(0);capture("after-parity-eye");pose(1);
            for(unsigned origin:{1u,2u}){clear();path.sourceMask=origin;advance(8);path.sourceMask=0;advance(12);
                auto state=read("branch-"+std::to_string(origin));double downstream=0;
                for(int k:domain.active)if(domain.mask.bounds.x+(k%domain.mask.width+.5f)/domain.mask.width*domain.mask.bounds.z>5)downstream+=state[k].x;
                Check(downstream>1e-5,"Actual tributary foam reaches common outlet");capture("branch-"+std::to_string(origin));
                advance(32);auto expired=read("expired-"+std::to_string(origin));double remaining=0;for(auto v:expired)remaining+=v.x;Check(remaining==0,"Actual foam finite lifetime");
            }
            pose(2);mode=2;bind();capture("expired-foam");mode=1;bind();capture("expired-flow");mode=2;bind();
            clear();path.sourceMask=7;advance(24);read("established");capture("foam-overhead");
            pose(0);capture("foam-eye");mode=1;bind();capture("flow-same-time");mode=2;bind();
            pose(2);capture("west-contact");
            auto amountHistory=read("amount-history-before");
            setFoamAmount(0);capture("amount-zero");mode=1;bind();capture("amount-flow-reference");mode=2;bind();
            setFoamAmount(.5f);capture("amount-half");setFoamAmount(4);capture("amount-high");
            setFoamAmount(1);capture("amount-default-restored");
            auto amountAfter=read("amount-history-after");
            Check(std::memcmp(amountHistory.data(),amountAfter.data(),amountHistory.size()*sizeof(XMFLOAT4))==0,"Amount control must retain exact simulation history");
            setFoamAmount(-1);Check(foamAmount==0,"Amount lower bound");setFoamAmount(10);Check(foamAmount==4,"Amount upper bound");setFoamAmount(1);
            mode=1;bind();capture("west-flow-same-time");mode=2;bind();
            pose(3);capture("east-contact");mode=1;bind();capture("east-flow-same-time");mode=2;bind();
            pose(4);capture("join");pose(2);
            for(int n=0;n<30;++n){advance(.1f);gpu->WaitForGPU();Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/("motion-"+std::to_string(n)+".png")).generic_u8string()),"Motion frame");}
            path.speed=0;clear();advance(3);auto stopped=read("zero-flow");for(auto v:stopped)Check(v.x==0,"Zero speed emits no foam");
            path.speed=1;mode=0;bind();pose(1);capture("native-restored");
            Check(scene.objects.GetCount()==objects&&scene.meshes.GetCount()==meshes,"Foam added geometry");
            {wi::Archive a((out/"native-junction.wiscene").generic_u8string(),false);scene.Serialize(a);}
            Check(wi::renderer::GetShaderErrorCount()==0,"Shader errors");
            path.active=true;path.paused=false;path.frozen=false;mode=2;bind();wi::renderer::SetGameSpeed(1);pose(0);frames(60);
            auto start=std::chrono::steady_clock::now();frames(180);
            std::cout<<"JUNCTION_FOAM_FPS="<<180/std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"\n";
            std::cout<<"G7C_GPU_PROOF=PASS shaderErrors=0\n";
        }else{
            // Live and verification paths both establish an explicit native eye reference.
            mode=0;pose(0);bind();capture("startup-native-eye");
            mode=2;bind();
            path.sourceMask=7;advance(24);path.paused=false;path.frozen=false;wi::renderer::SetGameSpeed(1);wi::renderer::SetTemporalAAEnabled(true);pose(2);bind();
            frames(32);gpu->WaitForGPU();
            Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/"interactive-ready.png").generic_u8string()),"Live native WATER capture");
            ShowWindow(window,SW_SHOW);SetForegroundWindow(window);std::cout<<"G7C_INTERACTIVE_READY=1\n";
            MSG msg{};bool live=true;
            while(live&&IsWindow(window)){
                while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
                    if(msg.hwnd==window&&msg.message==WM_INPUT&&GetForegroundWindow()==window)
                        wi::input::rawinput::ParseMessage(reinterpret_cast<void*>(msg.lParam));
                    if(msg.hwnd==window&&msg.message==WM_CHAR){
                        if(msg.wParam==VK_BACK)wi::gui::TextInputField::DeleteFromInput();
                        else if(msg.wParam!=VK_RETURN)wi::gui::TextInputField::AddInput(wchar_t(msg.wParam));
                    }
                    if(msg.hwnd==window&&msg.message==WM_KEYDOWN&&!(msg.lParam&(1<<30))&&
                        (msg.wParam==VK_ESCAPE||foamAmountSlider.valueInputField.GetState()!=wi::gui::ACTIVE)){
                        const int key=int(msg.wParam);if(key==VK_ESCAPE)live=false;
                        if(key=='P'){
                            gpu->WaitForGPU();const auto name="live-"+std::to_string(gpu->GetFrameCount());
                            read(name);std::cout<<"LIVE_CLOCK="<<path.clock<<" step="<<path.step<<" steps="<<path.steps<<" state="<<path.state<<"\n";
                            Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/(name+".png")).generic_u8string()),"Live render capture");
                            Check(wi::helper::saveTextureToFile(path.GetRenderResult2D(),(out/(name+"-hud.png")).generic_u8string()),"Live HUD capture");
                            std::cout<<"INTERACTIVE_CAPTURE="<<name<<" mode="<<mode<<" paused="<<path.paused<<" source="<<path.sourceMask<<" speed="<<path.speed<<" amount="<<foamAmount<<"\n";
                        }
                        if(key=='D'){mode=mode==3?2:3;bind();}if(key=='T'){wi::renderer::SetTemporalAAEnabled(!wi::renderer::GetTemporalAAEnabled());std::cout<<"TAA="<<wi::renderer::GetTemporalAAEnabled()<<"\n";}if(key=='F'){mode=(mode+1)%3;bind();}if(key=='K'){path.sourceMask=path.sourceMask?0:7;bind();}
                        if(key=='Z'){path.speed=path.speed?0:1;bind();}if(key=='X'){clear();}
                        if(key==VK_SPACE){path.paused=!path.paused;path.frozen=path.paused;wi::renderer::SetGameSpeed(path.paused?0:1);bind();}
                        if(key>='1'&&key<='5')pose(key-'1');
                    }
                    if(msg.message==WM_CLOSE||msg.message==WM_QUIT)live=false;TranslateMessage(&msg);DispatchMessageW(&msg);
                }frames(1);Sleep(1);
            }
        }
        mode=0;bind();path.GetGUI().RemoveWidget(&foamAmountSlider);path.GetGUI().RemoveWidget(&foamDefault);path.GetGUI().RemoveWidget(&label);return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";gpu->WaitForGPU();return 1;}
}
