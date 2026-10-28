#include "../EngineBridge/src/RiverWaterfallFlight.h"
#include "../EngineBridge/src/RiverCourseField.h"
#include "../EngineBridge/src/RiverGradedTransport.h"
#include "../EngineBridge/src/RiverFoamAppearanceGpu.h"
#include "../EngineBridge/src/RiverFlowSourceSelection.h"
#include "renegade/bridge/RiverSplineService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <fstream>
#include <cstring>
#include <unordered_set>
using namespace DirectX;using namespace renegade::bridge;using namespace wi::graphics;namespace fs=std::filesystem;
static void Require(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
#include "../EngineBridge/src/RiverFlowGather.inc"
static uint32_t Bits(float f){uint32_t n;std::memcpy(&n,&f,4);return n;}
static uint32_t SRV(const GPUResource& r){auto n=GetDevice()->GetDescriptorIndex(&r,SubresourceType::SRV);Require(n>=0,"Missing descriptor");return unsigned(n);}
static void Upload(Texture& t,const std::vector<XMFLOAT4>& data,unsigned w,unsigned h,bool writable=false){TextureDesc d;d.width=w;d.height=h;d.format=Format::R32G32B32A32_FLOAT;d.layout=ResourceState::SHADER_RESOURCE;d.bind_flags=BindFlag::SHADER_RESOURCE;if(writable)d.bind_flags|=BindFlag::UNORDERED_ACCESS;SubresourceData initial;initial.data_ptr=data.data();initial.row_pitch=w*sizeof(XMFLOAT4);initial.slice_pitch=initial.row_pitch*h;Require(GetDevice()->CreateTexture(&d,&initial,&t),"Texture allocation");}
struct Params{XMFLOAT4 bounds;XMUINT4 control;XMFLOAT4 settings;XMUINT4 foamControl;XMFLOAT4 foamSettings;XMUINT4 motion,detail,artwork,path;};
struct Push{XMUINT4 resolution;XMFLOAT4 bounds,settings;};
static wi::ecs::Entity Quad(wi::scene::Scene& s,const char* name,const std::vector<XMFLOAT3>& positions,const wi::scene::MaterialComponent& material){auto e=s.Entity_CreateMesh(name);auto& mesh=*s.meshes.GetComponent(e);mesh.vertex_positions.assign(positions.begin(),positions.end());mesh.vertex_uvset_0={{0,0},{8,0},{0,8},{8,8}};mesh.indices={0,1,2,2,1,3};auto& part=mesh.subsets.emplace_back();part.materialID=e;part.indexCount=6;mesh.SetDoubleSided(true);mesh.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);mesh.CreateRenderData();s.materials.Create(e)=material;s.transforms.Create(e).UpdateTransform();s.objects.Create(e).meshID=e;return e;}
struct NativeFoam{
 Texture foam[2],faces,source,velocity,path;GPUBuffer buffer;RiverFoamAppearanceGpu appearance;Shader ps,vs,shadow,simulation;Params params{};unsigned current=0,start=0,steps=0;float clock=0,step=0,maxRate=0;int shader=-1;std::string directory;river_graded_transport::Domain domain;
 void Configure(wi::scene::Scene& scene,wi::ecs::Entity receiving,const river_waterfall::Model& model,const std::string& dir){
 directory=dir;auto* gpu=GetDevice();std::string error;std::shared_ptr<const RiverFlowField> native;Require(BuildRiverFlowField(scene,receiving,{},native,error),error);river_graded::Settings settings;river_course::Field field(*native,settings);
 auto bounds=river_flow_detail::CaptureSourceBounds(*native);float extentX=bounds.maximum.x-bounds.minimum.x+1,extentZ=bounds.maximum.z-bounds.minimum.z+1,extent=std::max(extentX,extentZ);unsigned gridX=unsigned(std::clamp(std::ceil(384*extentX/extent),48.f,384.f)),gridZ=unsigned(std::clamp(std::ceil(384*extentZ/extent),48.f,384.f));domain=river_graded_transport::Build(field,GatherBounds(scene,receiving,bounds,nullptr,true),gridX,gridZ,.04f);const auto w=domain.mask.width,h=domain.mask.height;const float dx=domain.mask.bounds.z/w,dz=domain.mask.bounds.w/h;
 for(int k:domain.active){float x=domain.mask.bounds.x+(k%w+.5f)*dx;float z=domain.mask.bounds.y+(k/w+.5f)*dz;float along=(x-model.landing.x)*model.direction.x+(z-model.landing.z)*model.direction.z; if(along>=0&&along<4){float bank=std::clamp(std::min(domain.mask.cells[k].y,domain.mask.cells[k].z)/.35f,0.f,1.f);domain.source[k].w+=std::min(5.f,model.impactPower/20000.f)*std::exp(-along/1.1f)*bank;}}
 std::vector<XMFLOAT4> zero(size_t(w)*h);Upload(foam[0],zero,w,h,true);Upload(foam[1],zero,w,h,true);Upload(faces,domain.faces,w,h);Upload(source,domain.source,w,h);Upload(velocity,domain.velocity,w,h);Upload(path,river_waterfall::Path(model),1,2048);appearance.Configure(dir,w,h);Upload(appearance.waves,zero,w,h);maxRate=domain.maximumRate;
 wi::vector<uint8_t> bytes;auto load=[&](const char* n,ShaderStage stage,Shader& sh){Require(wi::helper::FileRead(dir+"/"+n,bytes),std::string("Missing proof shader ")+n);Require(gpu->CreateShader(stage,bytes.data(),bytes.size(),&sh),"Shader create");};load("RiverWaterfallPS.cso",ShaderStage::PS,ps);load("RiverCourseVS.cso",ShaderStage::VS,vs);load("RiverCourseShadowVS.cso",ShaderStage::VS,shadow);load("RiverRapidsCS.cso",ShaderStage::CS,simulation);
 wi::renderer::CustomShader custom;custom.name="G12A native WATER waterfall presentation";custom.filterMask=wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER;PipelineStateDesc p;p.vs=&vs;p.ps=&ps;p.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);p.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_TRANSPARENT);p.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);Require(gpu->CreatePipelineState(&p,&custom.pso[wi::enums::RENDERPASS_MAIN]),"Native water pipeline");p.vs=&shadow;p.ps=wi::renderer::GetShader(wi::enums::PSTYPE_SHADOW_TRANSPARENT);p.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENTSHADOW);p.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEPTHREAD);p.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_SHADOW_DOUBLESIDED);Require(gpu->CreatePipelineState(&p,&custom.pso[wi::enums::RENDERPASS_SHADOW]),"Water shadow pipeline");shader=wi::renderer::RegisterCustomShader(custom);
 params={domain.mask.bounds,{SRV(velocity),Bits(-1),Bits(1),0},{4,1.5f,0,1},{SRV(foam[0]),SRV(foam[1]),SRV(source),SRV(velocity)},{.3f,0,0,.45f},{SRV(appearance.waves),0,0,0},appearance.DetailControls(),appearance.ArtworkControls(),{SRV(path),Bits(model.totalTravel),Bits(1),1}};GPUBufferDesc bd;bd.size=sizeof(params);bd.bind_flags=BindFlag::SHADER_RESOURCE;bd.misc_flags=ResourceMiscFlag::BUFFER_RAW;Require(gpu->CreateBuffer(&bd,&params,&buffer),"Water params");
 std::cout<<"WATERFALL_RECEIVING_DOMAIN active="<<domain.active.size()<<" inflow="<<domain.inflow<<" outflow="<<domain.outflow<<" divergence="<<domain.maximumDivergence<<"\n";
 }
 void Prepare(wi::scene::Scene& s,wi::ecs::Entity sheet,float dt,bool nativeMode){steps=dt>0?unsigned(std::max(1.f,std::ceil(dt*maxRate/.75f))):0;start=current;step=steps?dt/steps:0;clock+=dt;current^=steps&1;appearance.Prepare(dt);auto* m=s.materials.GetComponent(sheet);if(nativeMode){m->DisableCustomShader();m->userdata={};}else{m->SetCustomShaderID(shader);m->userdata={SRV(buffer),0x52464c35,current|(appearance.State()<<1),Bits(clock)};}m->SetDirty();}
 void Dispatch(){auto* gpu=GetDevice();auto cmd=gpu->BeginCommandList();wi::renderer::BindCommonResources(cmd);unsigned state=start;auto& d=foam[0].GetDesc();for(unsigned k=0;k<steps;++k){unsigned next=1-state;auto b=GPUBarrier::Image(&foam[next],ResourceState::SHADER_RESOURCE,ResourceState::UNORDERED_ACCESS);gpu->Barrier(&b,1,cmd);gpu->BindComputeShader(&simulation,cmd);gpu->BindResource(&foam[state],0,cmd);gpu->BindResource(&faces,1,cmd);gpu->BindResource(&source,2,cmd);gpu->BindResource(&velocity,3,cmd);gpu->BindUAV(&foam[next],0,cmd);Push p{{d.width,d.height,15,0},params.bounds,{step,clock-(steps-1-k)*step,3.2f,1}};gpu->PushConstants(&p,sizeof(p),cmd);gpu->Dispatch((d.width+7)/8,(d.height+7)/8,1,cmd);b=GPUBarrier::Image(&foam[next],ResourceState::UNORDERED_ACCESS,ResourceState::SHADER_RESOURCE);gpu->Barrier(&b,1,cmd);state=next;}appearance.Dispatch(source,velocity,foam[current],params.bounds,clock,15);}
 double Mass(){GetDevice()->WaitForGPU();wi::vector<uint8_t> bytes;Require(wi::helper::saveTextureToMemory(foam[current],bytes),"Foam readback");double sum=0;for(size_t k=0;k<bytes.size();k+=sizeof(XMFLOAT4)){XMFLOAT4 q;std::memcpy(&q,bytes.data()+k,sizeof(q));Require(std::isfinite(q.x)&&q.x>=0,"Invalid waterfall foam");sum+=q.x;}return sum;}
};
struct Path:wi::RenderPath3D{NativeFoam foam;wi::ecs::Entity sheet=wi::ecs::INVALID_ENTITY;bool paused=false,nativeMode=false,fixed=false;void Update(float dt)override{float elapsed=paused?0:(fixed?1.f/60:std::min(dt,.05f));foam.Prepare(*scene,sheet,elapsed,nativeMode);wi::RenderPath3D::Update(elapsed);}void Render()const override{const_cast<Path*>(this)->foam.Dispatch();wi::RenderPath3D::Render();}};
static wi::ecs::Entity EmissionStrip(wi::scene::Scene& s,float width,XMFLOAT3 direction){auto e=s.Entity_CreateMesh("Proof-only emission strip");auto* mesh=s.meshes.GetComponent(e);mesh->vertex_positions={{-.025f,0,-width*.45f},{.025f,0,-width*.45f},{-.025f,0,width*.45f},{.025f,0,width*.45f}};for(auto& p:mesh->vertex_positions){float x=p.x,z=p.z;p={x*direction.x-z*direction.z,0,x*direction.z+z*direction.x};}mesh->indices={0,2,1,1,2,3};auto& part=mesh->subsets.emplace_back();part.indexCount=6;mesh->ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);mesh->CreateRenderData();return e;}

static wi::ecs::Entity FallingAerosolSurface(wi::scene::Scene& scene,const river_waterfall::Model& model){
    // This is a VFX emission domain, never a replacement for the connected WATER sheet.
    // Sampling by triangle area scatters droplets over the actual curved falling path.
    auto entity=scene.Entity_CreateMesh("Proof-only falling aerosol domain");
    auto& mesh=*scene.meshes.GetComponent(entity);
    constexpr size_t rowStride=3;
    uint32_t rows=0;
    for(size_t k=0;k<model.rows.size();k+=rowStride){
        const auto& row=model.rows[k];
        if(row.falling<.18f || row.centre.y>model.lip.y-.08f || row.centre.y<model.landing.y+.5f)
            continue;
        XMFLOAT3 centre{
            row.centre.x-model.lip.x+model.direction.x*.12f,
            row.centre.y-model.lip.y,
            row.centre.z-model.lip.z+model.direction.z*.12f
        };
        const float half=model.width*.44f;
        mesh.vertex_positions.push_back({centre.x+model.direction.z*half,centre.y,centre.z-model.direction.x*half});
        mesh.vertex_positions.push_back({centre.x-model.direction.z*half,centre.y,centre.z+model.direction.x*half});
        if(rows){
            uint32_t i=2*(rows-1);
            for(uint32_t idx:{i,i+1,i+2,i+2,i+1,i+3})mesh.indices.push_back(idx);
        }
        ++rows;
    }
    Require(rows>=3,"Falling aerosol must have a nonzero emission surface");
    mesh.SetDoubleSided(true);
    mesh.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
    mesh.CreateRenderData();
    return entity;
}
static std::vector<wi::ecs::Entity> Effects(wi::scene::Scene& scene,const river_waterfall::Model& m,const std::string& artwork){
    std::vector<wi::ecs::Entity> out;
    const auto strip=EmissionStrip(scene,m.width,m.direction);
    const auto aerosol=FallingAerosolSurface(scene,m);
    constexpr const char* names[]={
        "Waterfall lip spray","Waterfall falling aerosol",
        "Waterfall falling haze","Waterfall landing splash","Waterfall drifting mist"
    };
    constexpr float emissions[]={130.f,450.f,175.f,350.f,78.f};
    constexpr float lifetimes[]={.55f,.72f,1.65f,.9f,3.25f};
    constexpr float sizes[]={.05f,.06f,.55f,.1f,1.45f};
    constexpr float opacities[]={.22f,.24f,.065f,.35f,.15f};
    for(int kind=0;kind<5;++kind){
        const bool falling=kind==1,haze=kind==2,landing=kind==3,mist=kind==4;
        auto origin=(kind<3?m.lip:m.landing);
        origin.y+=(landing||mist?.1f:.16f);
        auto entity=scene.Entity_CreateEmitter(names[kind],origin);
        out.push_back(entity);
        auto* emitter=scene.emitters.GetComponent(entity);
        emitter->SetMaxParticleCount(mist?768:falling?1280:1024);
        emitter->shaderType=wi::EmittedParticleSystem::SOFT;
        emitter->meshID=(falling||haze)?aerosol:strip;
        emitter->normal_factor=0;
        emitter->random_factor=mist?.48f:haze?.56f:falling?.18f:.5f;
        emitter->count=emissions[kind];
        emitter->life=lifetimes[kind];
        emitter->random_life=falling?.15f:haze?.42f:.32f;
        emitter->size=sizes[kind];
        XMFLOAT3 velocity=falling?XMFLOAT3{.1f,-4.0f,0}:
            haze?XMFLOAT3{.2f,-.7f,0}:
            landing?XMFLOAT3{1.35f,2.8f,0}:
            mist?XMFLOAT3{.22f,.27f,0}:XMFLOAT3{1.0f,.6f,0};
        emitter->velocity={velocity.x*m.direction.x,velocity.y,velocity.x*m.direction.z};
        emitter->gravity={0,mist?.015f:haze?-.9f:falling?-4.2f:-9.80665f,0};
        emitter->drag=mist?.99f:haze?.96f:falling?.97f:1.f;
        emitter->scaleX=(mist||haze)?1.3f:1;
        emitter->scaleY=falling?2.5f:haze?1.55f:mist?1.25f:1;
        emitter->SetOpacityCurveControl(.07f,.53f);
        emitter->SetCollidersDisabled(true);
        auto* mat=scene.materials.GetComponent(entity);
        mat->userBlendMode=wi::enums::BLENDMODE_ALPHA;
        mat->SetBaseColor({.91f,.95f,1.f,opacities[kind]});
        mat->textures[wi::scene::MaterialComponent::BASECOLORMAP].name=
            artwork+((mist||haze)?"/mist.png":"/spray.png");
        mat->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource=
            wi::resourcemanager::Load(mat->textures[wi::scene::MaterialComponent::BASECOLORMAP].name);
        Require(mat->textures[0].resource.IsValid(),"Original waterfall particle texture");
        mat->SetDirty();
    }
    // Flow energy and river width scale the effects; a dry source emits no spray.
    const float strength=std::clamp(
        std::sqrt(m.impactPower/254972.9f)*std::sqrt(m.width/4.f),0.f,2.5f);
    for(auto entity:out)scene.emitters.GetComponent(entity)->count*=strength;
    return out;
}
int main(int argc,char** argv){
 if(argc<3){std::cerr<<"Usage: proof output-folder rock.rasset [--interactive|--reopen saved.wiscene]\n";return 2;}std::cout.setf(std::ios::unitbuf);auto out=fs::absolute(argv[1]);fs::create_directories(out);river_waterfall::Settings fixture;for(int k=3;k<argc;++k){std::string flag=argv[k];if(flag=="--height"&&k+1<argc)fixture.height=std::stof(argv[++k]);if(flag=="--width"&&k+1<argc)fixture.width=std::stof(argv[++k]);if(flag=="--heading"&&k+1<argc)fixture.heading=std::stof(argv[++k]);}const bool interactive=argc>=4&&std::string(argv[3])=="--interactive";const bool reopen=argc>=5&&std::string(argv[3])=="--reopen";
 HWND window=CreateWindowExW(0,L"STATIC",L"Renegade G12A-R5 - Waterfall Visual Review | arrows orbit, +/- zoom, P pause, E spray, V native, Esc close",WS_OVERLAPPEDWINDOW,60,60,1440,920,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(interactive){ShowWindow(window,SW_SHOW);SetForegroundWindow(window);}
 struct App:wi::Application{void Initialize()override{wi::Application::Initialize();alwaysactive=true;}}app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();auto* gpu=GetDevice();
 try{wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);wi::scene::Scene scene;auto settings=fixture;auto model=river_waterfall::Build(settings);auto world=[&](XMFLOAT3 p){return XMFLOAT3{p.x*model.direction.x-p.z*model.direction.z,p.y,p.x*model.direction.z+p.z*model.direction.x};};auto positions=[&](std::vector<XMFLOAT3> p){for(auto& q:p)q=world(q);return p;};wi::ecs::Entity sheet=wi::ecs::INVALID_ENTITY,receiving=wi::ecs::INVALID_ENTITY;std::vector<wi::ecs::Entity> emitters;auto exe=fs::u8path(wi::helper::GetExecutablePath()).parent_path();std::string directory=(exe/"Content/river").generic_string(),artwork=(exe/"Content/waterfall").generic_string();
 if(reopen){wi::Archive archive(fs::absolute(argv[4]).generic_string(),true);Require(archive.IsOpen(),"Saved waterfall archive");scene.Serialize(archive);sheet=scene.Entity_FindByName("Waterfall connected sheet");receiving=scene.Entity_FindByName("Waterfall receiving oracle");for(auto n:{"Waterfall lip spray","Waterfall falling aerosol","Waterfall falling haze","Waterfall landing splash","Waterfall drifting mist"})emitters.push_back(scene.Entity_FindByName(n));Require(scene.meshes.Contains(sheet)&&scene.splines.Contains(receiving),"Reopened authored fixture incomplete");Require(!scene.materials.GetComponent(sheet)->IsCustomShader(),"GPU shader leaked to archive");scene.Update(0);
 }else{
 auto weather=scene.Entity_CreateTransform("Waterfall daylight");auto& w=scene.weathers.Create(weather);w.SetRealisticSky(false);w.ambient={.5f,.53f,.58f};w.horizon={.65f,.73f,.79f};w.zenith={.25f,.4f,.6f};scene.Entity_CreateLight("Sun",{15,30,-15},{1,.96f,.9f},4,200,wi::scene::LightComponent::DIRECTIONAL);
 ReusableModelAssetDocument doc;std::string error;Require(ReadReusableModelAssetDocument(fs::absolute(argv[2]).generic_string(),doc,error),error);wi::scene::Scene rocks;const auto rockSource=(out/"rock-native-source.wiscene").generic_string();Require(wi::helper::FileWrite(rockSource,doc.payload.data(),doc.payload.size()),"Stage native rock payload");wi::Archive asset(rockSource,true);Require(asset.IsOpen(),"Native rock source");rocks.Serialize(asset);rocks.Update(0);wi::scene::MaterialComponent stone;Require(rocks.materials.GetCount()>0,"Rock material missing");stone=rocks.materials[0];stone.SetRoughness(.95f);stone.SetBaseColor({.55f,.52f,.43f,1});
 std::vector<wi::ecs::Entity> roots;for(size_t k=0;k<rocks.transforms.GetCount();++k){auto e=rocks.transforms.GetEntity(k);if(!rocks.hierarchy.Contains(e))roots.push_back(e);}scene.Merge(rocks);auto wrapper=scene.Entity_CreateTransform("Actual landing river rock");for(auto e:roots)scene.Component_Attach(e,wrapper);auto* t=scene.transforms.GetComponent(wrapper);t->translation_local=world({model.landingX+9,-.5f,.25f});t->scale_local={75,75,75};t->SetDirty();
 auto cliff=stone;cliff.SetBaseColor({.64f,.61f,.53f,1});Quad(scene,"Lower rocky basin",positions({{-2,-.65f,-12},{38,-.65f,-12},{-2,-.65f,12},{38,-.65f,12}}),stone);Quad(scene,"Upper rocky shelf",positions({{-16,settings.height-.35f,-10},{-.1f,settings.height-.35f,-10},{-16,settings.height-.35f,10},{-.1f,settings.height-.35f,10}}),cliff);Quad(scene,"Waterfall rock face",positions({{-.13f,-.65f,-10},{-.13f,settings.height-.35f,-10},{-.13f,-.65f,10},{-.13f,settings.height-.35f,10}}),cliff);
 RiverSplineOptions options;options.widthMeters=settings.width;options.surfaceOffsetMeters=0;options.subdivisionsPerSegment=24;CreateRiverSplineCommand create(scene,positions({{model.landingX,0,0},{model.landingX+5,0,0},{model.landingX+15,0,0},{model.landingX+30,0,0}}),options);Require(create.Execute(),"Receiving native spline");receiving=create.CreatedEntity();scene.names.GetComponent(receiving)->name="Waterfall receiving oracle";scene.Update(0);scene.objects.GetComponent(receiving)->SetRenderable(false);
 sheet=scene.Entity_CreateMesh("Waterfall connected sheet");river_waterfall::Mesh(model,*scene.meshes.GetComponent(sheet),sheet);scene.meshes.GetComponent(sheet)->ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);scene.transforms.Create(sheet).UpdateTransform();scene.objects.Create(sheet).meshID=sheet;scene.objects.GetComponent(sheet)->SetRequestPlanarReflection(true);auto& water=scene.materials.Create(sheet);water=*scene.materials.GetComponent(receiving);water.SetBaseColor({.031f,.125f,.169f,.377f});water.SetNormalMapStrength(1);water.texMulAdd={4,4,0,-.001f};emitters=Effects(scene,model,artwork);scene.Update(0);
 }
 std::cout<<"WATERFALL_RECEIVER_BEFORE_HIDE renderable="<<scene.objects.GetComponent(receiving)->IsRenderable()<<"\n";scene.objects.GetComponent(receiving)->SetRenderable(false);
 for(size_t k=0;k<scene.materials.GetCount();++k)for(const auto& t:scene.materials[k].textures)if(!t.name.empty())std::cout<<"WATERFALL_TEXTURE "<<t.name<<" resource="<<t.resource.IsValid()<<" texture="<<(t.resource.IsValid()&&t.resource.GetTexture().IsValid())<<" bytes="<<(t.resource.IsValid()?t.resource.GetFileData().size():0)<<"\n";
 for(size_t k=0;k<scene.materials.GetCount();++k)for(const auto& t:scene.materials[k].textures)if(!t.name.empty())Require(t.resource.IsValid()&&t.resource.GetTexture().IsValid(),"Missing native fixture texture: "+t.name);
 Path path;path.scene=&scene;path.sheet=sheet;path.init(1424,881,96);path.ResizeBuffers();wi::scene::CameraComponent camera;camera.CreatePerspective(1424,881,.05f,300,XMConvertToRadians(58));path.camera=&camera;path.setOcclusionCullingEnabled(false);path.setMotionBlurEnabled(false);path.setDepthOfFieldEnabled(false);wi::renderer::SetTemporalAAEnabled(false);path.fixed=!interactive;
 struct Drain{wi::Application& app;~Drain(){app.ActivatePath(nullptr);while(wi::renderer::IsPipelineCreationActive())Sleep(10);GetDevice()->WaitForGPU();}}drain{app};path.foam.Configure(scene,receiving,model,directory);app.ActivatePath(&path);float angle=-.85f+settings.heading,distance=settings.height<3?13.f:18.f,height=settings.height<3?4.f:7.f;auto view=[&](){XMFLOAT3 eye{model.landing.x+std::cos(angle)*distance,height,model.landing.z+std::sin(angle)*distance};camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMLoadFloat3(&eye),XMVectorSet(1.5f*model.direction.x,settings.height*.45f,1.5f*model.direction.z,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();};view();auto frames=[&](int n){for(int k=0;k<n;++k)app.Run();};frames(240);double mass=path.foam.Mass();Require(mass>0,"Landing foam absent");unsigned alive=0;for(auto e:emitters){Require(scene.emitters.Contains(e),"Native emitter absent");alive+=scene.emitters.GetComponent(e)->GetStatistics().aliveCount;}std::cout<<"WATERFALL_GPU_READY height="<<settings.height<<" width="<<settings.width<<" heading="<<settings.heading<<" nativeParticles="<<alive<<" foamMass="<<mass<<" flightSeconds="<<model.flightSeconds<<"\n";gpu->WaitForGPU();Require(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/"waterfall.png").generic_string()),"Waterfall capture");
 auto begin=std::chrono::steady_clock::now();frames(90);std::cout<<"WATERFALL_UNPACED_FPS "<<90/std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<"\n";
 if(!reopen){auto* m=scene.materials.GetComponent(sheet);auto shader=m->customShaderID;auto data=m->userdata;m->DisableCustomShader();m->userdata={};{wi::Archive archive((out/"waterfall-native.wiscene").generic_string(),false);Require(archive.IsOpen(),"Save proof fixture");archive.SetCompressionEnabled(true);scene.Serialize(archive);}m=scene.materials.GetComponent(sheet);m->SetCustomShaderID(shader);m->userdata=data;m->SetDirty();}
 if(interactive){std::vector<float> rates;for(auto e:emitters)rates.push_back(scene.emitters.GetComponent(e)->count);bool prevP=false,prevE=false,prevV=false,effects=true;MSG msg={};while(IsWindow(window)&&!(GetForegroundWindow()==window&&(GetAsyncKeyState(VK_ESCAPE)&0x8000))){while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}auto down=[&](int k){return GetForegroundWindow()==window&&(GetAsyncKeyState(k)&0x8000)!=0;};bool p=down('P'),ee=down('E'),v=down('V');if(p&&!prevP)path.paused=!path.paused;if(v&&!prevV)path.nativeMode=!path.nativeMode;if(ee&&!prevE){effects=!effects;for(size_t k=0;k<emitters.size();++k)scene.emitters.GetComponent(emitters[k])->count=effects?rates[k]:0;}prevP=p;prevE=ee;prevV=v;if(down(VK_LEFT))angle-=.012f;if(down(VK_RIGHT))angle+=.012f;if(down(VK_UP))height+=.08f;if(down(VK_DOWN))height-=.08f;if(down(VK_OEM_PLUS))distance=std::max(8.f,distance-.12f);if(down(VK_OEM_MINUS))distance=std::min(55.f,distance+.12f);view();app.Run();} }
 std::cout<<"WATERFALL_NATIVE_PROOF_PASS reopen="<<reopen<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"WATERFALL_NATIVE_PROOF_FAIL "<<e.what()<<"\n";return 1;}
}
