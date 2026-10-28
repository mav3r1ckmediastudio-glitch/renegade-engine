#include "RiverJunctionFixture.h"
#include "renegade/bridge/SceneService.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <chrono>
#include <iostream>
#include <map>
using namespace junction_proof;using namespace renegade::bridge;
namespace fs=std::filesystem;
struct Params {XMFLOAT4 bounds;XMUINT4 control;XMFLOAT4 settings;};
static_assert(sizeof(Params)==48);
static std::uint32_t Bits(float f){std::uint32_t b;std::memcpy(&b,&f,4);return b;}
struct Path:wi::RenderPath3D {bool frozen=false;void Update(float dt)override{wi::RenderPath3D::Update(frozen?0:dt);}};
static void Mesh(wi::scene::Scene& scene,wi::ecs::Entity entity,const std::vector<XMFLOAT3>& vertices,
    const std::vector<std::uint32_t>& indices,float tile)
{
    auto& mesh=scene.meshes.Create(entity);mesh.vertex_positions.assign(vertices.begin(),vertices.end());
    mesh.indices.assign(indices.begin(),indices.end());mesh.SetDoubleSided(true);
    for(auto p:vertices){mesh.vertex_normals.push_back({0,1,0});mesh.vertex_tangents.push_back({0,0,1,1});
        mesh.vertex_uvset_0.push_back({p.x/tile,p.z/tile});}
    auto& subset=mesh.subsets.emplace_back();subset.materialID=entity;subset.indexCount=uint32_t(indices.size());
    mesh.CreateRenderData();scene.objects.GetComponent(entity)->meshID=entity;
}
static void NativeCoverage(wi::scene::Scene& scene,const RiverJunctionSurface& surface)
{
    struct Triangle{XMFLOAT3 a,b,c;};std::vector<Triangle> native;
    for(const auto& input:Inputs(scene)) {
        const auto* m=scene.meshes.GetComponent(input.river);Check(m&&!m->indices.empty(),"Native generated triangles missing");
        const auto world=scene.ComputeEntityMatrixRecursive(input.river);
        auto position=[&](std::uint32_t n){XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMLoadFloat3(&m->vertex_positions[n]),world));return p;};
        for(std::size_t k=0;k<m->indices.size();k+=3)
            native.push_back({position(m->indices[k]),position(m->indices[k+1]),position(m->indices[k+2])});
    }
    std::size_t samples=0,overlap=0;
    const auto& v=surface.Positions();const auto& indices=surface.Indices();
    for(int iz=0;iz<160;++iz)for(int ix=0;ix<280;++ix) {
        const XMFLOAT2 p{-28+(ix+.373f)*.22f,-15+(iz+.619f)*.19f};
        unsigned original=0,joined=0;
        for(const auto& t:native)original+=Inside(p,t.a,t.b,t.c,true);
        for(std::size_t k=0;k<indices.size();k+=3)joined+=Inside(p,v[indices[k]],v[indices[k+1]],v[indices[k+2]],true);
        Check((original>0)==(joined>0),"Actual native triangle union footprint mismatch");
        Check(joined<=1,"Union draws water more than once");overlap+=original>1;++samples;
    }
    Check(overlap>20,"Fixture must exercise real native transparent overlap");
    std::cout<<"ACTUAL_NATIVE_UNION=PASS sampled="<<samples<<" originalOverlap="<<overlap
        <<" joinedOverlap=0 triangles="<<indices.size()/3<<" area="<<surface.AreaSquareMetres()<<"\n";
}
int main(int argc,char** argv)
{
    if(argc<4||argc>6)return 2;
    const bool interactive=argc>=5&&std::string(argv[4])=="--interactive";
    const auto out=fs::absolute(argv[1]);if(fs::exists(out)&&!fs::is_empty(out))return 2;fs::create_directories(out);
    std::cout.setf(std::ios::unitbuf);
    HWND window=CreateWindowExW(0,L"STATIC",L"Renegade River G7B - Confluence Geometry",
        WS_OVERLAPPEDWINDOW,60,40,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    struct App:wi::Application{void Initialize()override{wi::Application::Initialize();alwaysactive=true;}}app;
    app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
    auto* gpu=wi::graphics::GetDevice();
    try {
        wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
        if(argc>=5&&std::string(argv[4])=="--reopen") {
            SceneService reopened;Check(reopened.LoadScene(fs::absolute(argv[2]).generic_u8string()),reopened.LastError());
            auto& scene=reopened.GetScene();
            const auto river=scene.Entity_FindByName("G7B continuous native WATER");
            const auto* m=scene.materials.GetComponent(river);
            Check(m&&!m->IsCustomShader()&&m->userdata.y==0,"Fresh native archive has transient binding");
            Check(m->textures[wi::scene::MaterialComponent::NORMALMAP].resource.GetTexture().IsValid(),"Fresh normal texture");
            Path render;wi::scene::CameraComponent camera;camera.CreatePerspective(1264,761,.05f,500,XMConvertToRadians(60));
            camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(8,1.3f,5,1),XMVectorSet(-1,0,0,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
            render.scene=&scene;render.camera=&camera;render.init(1264,761,96);render.ResizeBuffers();
            struct Scope {wi::Application& app;~Scope(){app.ActivatePath(nullptr);while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}} scope{app};
            app.ActivatePath(&render);for(int n=0;n<160;++n)app.Run();while(wi::renderer::IsPipelineCreationActive())app.Run();gpu->WaitForGPU();
            Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/"fresh-native.png").generic_u8string()),"Fresh capture");
            Check(wi::renderer::GetShaderErrorCount()==0,"Fresh shader errors");
            std::cout<<"G7B_FRESH_NATIVE_ARCHIVE=PASS\n";return 0;
        }
        SceneService source;Check(source.LoadScene(fs::absolute(argv[2]).generic_u8string()),source.LastError());
        const auto* original=source.GetScene().materials.GetComponent(source.GetScene().Entity_FindByName("River Water 5"));
        Check(original&&original->textures[wi::scene::MaterialComponent::NORMALMAP].resource.GetTexture().IsValid(),"Textured native WATER fixture missing");
        auto optics=*original;optics.DisableCustomShader();optics.userdata={};optics.texMulAdd={1,1,0,0};
        optics.texAnimDirection={0,.04f};optics.normalMapStrength=.8f;
        wi::scene::Scene scene;scene.weather=source.GetScene().weather;scene.weather.SetRealisticSky(true);
        Create(scene);
        for(const auto& input:Inputs(scene)) *scene.materials.GetComponent(input.river)=optics;
        const auto network=Network(scene);std::shared_ptr<const RiverJunctionSurface> surface;std::string error;
        Check(BuildRiverJunctionSurface(*network,surface,error),error);
        // First native Scene::Update generates the independent geometry oracle.
        scene.Update(0);gpu->WaitForGPU();NativeCoverage(scene,*surface);
        const auto joined=scene.Entity_CreateObject("G7B continuous native WATER");
        scene.materials.Create(joined)=optics;Mesh(scene,joined,surface->Positions(),surface->Indices(),4);
        for(const auto& input:Inputs(scene))scene.objects.GetComponent(input.river)->SetRenderable(false);
        // Reuse licensed native project rock/material resources in this disposable scene.
        wi::ecs::Entity rockMesh=wi::ecs::INVALID_ENTITY;wi::scene::MaterialComponent ground=optics;
        ground.shaderType=wi::scene::MaterialComponent::SHADERTYPE_PBR;ground.DisableCustomShader();ground.baseColor={.55f,.5f,.39f,1};ground.normalMapStrength=.65f;
        for(std::size_t n=0;n<source.GetScene().materials.GetCount();++n) {
            const auto& m=source.GetScene().materials[n];
            if(m.textures[wi::scene::MaterialComponent::BASECOLORMAP].name.find("default_grass")!=std::string::npos) {
                ground=m;ground.DisableCustomShader();ground.shaderType=wi::scene::MaterialComponent::SHADERTYPE_PBR;ground.texMulAdd={1,1,0,0};ground.baseColor={.7f,.7f,.65f,1};break;
            }
        }
        for(std::size_t n=0;n<source.GetScene().meshes.GetCount();++n) {
            const auto& mesh=source.GetScene().meshes[n];if(mesh.IsSkinned())continue;
            bool rock=false;for(const auto& subset:mesh.subsets) {
                const auto* m=source.GetScene().materials.GetComponent(subset.materialID);
                if(m&&m->textures[wi::scene::MaterialComponent::BASECOLORMAP].name.find("gltfimport")!=std::string::npos)rock=true;
            }
            if(!rock)continue;rockMesh=source.GetScene().meshes.GetEntity(n);scene.meshes.Create(rockMesh)=mesh;
            for(const auto& subset:mesh.subsets)if(const auto* m=source.GetScene().materials.GetComponent(subset.materialID))
                scene.materials.Create(subset.materialID)=*m;
            break;
        }
        if(argc==6) {
            ground=wi::scene::MaterialComponent{};ground.baseColor={1,1,1,1};ground.roughness=.95f;ground.metalness=0;ground.normalMapStrength=.7f;
            const std::pair<int,const char*> maps[]={{wi::scene::MaterialComponent::BASECOLORMAP,"rocky_basecolor.png"},
                {wi::scene::MaterialComponent::NORMALMAP,"rocky_normal_dx.png"},{wi::scene::MaterialComponent::SURFACEMAP,"rocky_surface.png"}};
            for(const auto& item:maps) {
                auto& tex=ground.textures[item.first];tex.name=(fs::absolute(argv[5])/item.second).generic_u8string();
                tex.resource=wi::resourcemanager::Load(tex.name);Check(tex.resource.GetTexture().IsValid(),"Owned rocky PBR payload missing");
            }
        }
        Check(ground.textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.GetTexture().IsValid(),"Native ground texture missing");
        // Actual sloped bed mesh below/around the union; it is opaque ground, not a water/foam overlay.
        std::map<std::pair<std::uint32_t,std::uint32_t>,unsigned> adjacency;
        const auto& waterV=surface->Positions();const auto& waterI=surface->Indices();
        for(std::size_t k=0;k<waterI.size();k+=3)for(int e=0;e<3;++e){auto a=waterI[k+e],b=waterI[k+(e+1)%3];if(a>b)std::swap(a,b);++adjacency[{a,b}];}
        std::vector<std::pair<XMFLOAT3,XMFLOAT3>> banks;
        for(const auto& e:adjacency)if(e.second==1)banks.push_back({waterV[e.first.first],waterV[e.first.second]});
        auto distance=[&](XMFLOAT2 p) {
            double nearest=INFINITY;
            for(const auto& edge:banks) {
                const auto a=edge.first,b=edge.second;const double x=b.x-a.x,z=b.z-a.z,den=x*x+z*z;
                const double t=den>0?std::clamp(((p.x-a.x)*x+(p.y-a.z)*z)/den,0.0,1.0):0;
                nearest=std::min(nearest,std::hypot(p.x-a.x-t*x,p.y-a.z-t*z));
            }return float(nearest);
        };
        std::vector<XMFLOAT3> bed;std::vector<std::uint32_t> bedIndices;
        constexpr int nx=140,nz=100;
        for(int z=0;z<=nz;++z)for(int x=0;x<=nx;++x) {
            const float wx=-35+x*.5f,wz=-25+z*.5f,d=distance({wx,wz});
            const bool wet=surface->Evaluate({wx,wz}).valid;
            const float h=wet?-.08f-.55f*std::min(d/1.6f,1.f):-.08f+.45f*std::min(d/1.4f,1.f);
            bed.push_back({wx,h+.035f*std::sin(wx*.5f)*std::sin(wz*.43f)*std::min(d,1.f),wz});
        }
        for(int z=0;z<nz;++z)for(int x=0;x<nx;++x) {
            const auto a=std::uint32_t(z*(nx+1)+x),b=a+1,c=a+nx+1,d=c+1;
            bedIndices.insert(bedIndices.end(),{a,c,b,b,c,d});
        }
        const auto floor=scene.Entity_CreateObject("Shallow rocky confluence bed");
        scene.materials.Create(floor)=ground;Mesh(scene,floor,bed,bedIndices,1.5f);
        scene.meshes.GetComponent(floor)->ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
        if(rockMesh!=wi::ecs::INVALID_ENTITY)for(const XMFLOAT3 p:{XMFLOAT3{-7,-.3f,6},XMFLOAT3{-10,-.25f,-7},XMFLOAT3{9,-.3f,2.2f}}) {
            const auto e=scene.Entity_CreateObject("Native mossy river rock");scene.objects.GetComponent(e)->meshID=rockMesh;
            const auto* mesh=scene.meshes.GetComponent(rockMesh);
            XMFLOAT3 lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
            for(auto v:mesh->vertex_positions) {lo.x=std::min(lo.x,v.x);lo.y=std::min(lo.y,v.y);lo.z=std::min(lo.z,v.z);
                hi.x=std::max(hi.x,v.x);hi.y=std::max(hi.y,v.y);hi.z=std::max(hi.z,v.z);}
            const float scale=1.8f/std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});
            auto* t=scene.transforms.GetComponent(e);
            t->translation_local={p.x-(lo.x+hi.x)*.5f*scale,p.y-lo.y*scale,p.z-(lo.z+hi.z)*.5f*scale};
            t->scale_local={scale,scale,scale};t->SetDirty();
        }
        const auto sun=scene.Entity_CreateLight("Confluence sunlight",{0,10,0},{1,.94f,.84f},3);
        auto* light=scene.lights.GetComponent(sun);light->SetType(wi::scene::LightComponent::DIRECTIONAL);light->intensity=10;light->SetCastShadow(true);scene.weather.ambient={.3f,.35f,.4f};scene.weather.skyExposure=1;
        auto* sunT=scene.transforms.GetComponent(sun);XMStoreFloat4(&sunT->rotation_local,XMQuaternionRotationRollPitchYaw(.8f,-.65f,0));sunT->SetDirty();
        constexpr std::uint32_t tw=512,th=256;
        std::vector<XMFLOAT4> raster(tw*th);
        for(std::uint32_t z=0;z<th;++z)for(std::uint32_t x=0;x<tw;++x) {
            const auto q=surface->Evaluate({-29+(x+.5f)/tw*63,-16+(z+.5f)/th*32});
            if(q.valid) {const float speed=std::hypot(q.velocityXZ.x,q.velocityXZ.y);
                raster[z*tw+x]={q.velocityXZ.x/speed,q.velocityXZ.y/speed,speed,1};}
        }
        wi::graphics::Texture velocityTexture;wi::graphics::TextureDesc td;
        td.width=tw;td.height=th;td.format=wi::graphics::Format::R32G32B32A32_FLOAT;td.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
        wi::graphics::SubresourceData initial;initial.data_ptr=raster.data();initial.row_pitch=tw*sizeof(XMFLOAT4);initial.slice_pitch=initial.row_pitch*th;
        Check(gpu->CreateTexture(&td,&initial,&velocityTexture),"Float32 velocity texture");
        const auto descriptor=gpu->GetDescriptorIndex(&velocityTexture,wi::graphics::SubresourceType::SRV);
        Params params{{-29,-16,63,32},{uint32_t(descriptor),Bits(2),Bits(1),0},{4,4,0,1}};
        wi::graphics::Shader pixel;wi::vector<uint8_t> bytes;
        Check(wi::helper::FileRead((fs::absolute(argv[3])/"RiverFlowNormalPS.cso").generic_u8string(),bytes),"Accepted G3 normal shader missing");
        Check(gpu->CreateShader(wi::graphics::ShaderStage::PS,bytes.data(),bytes.size(),&pixel),"Normal shader");
        wi::renderer::CustomShader custom;custom.name="G7B single confluence WATER";custom.filterMask=wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER;
        wi::graphics::PipelineStateDesc pso;
        pso.vs=wi::renderer::GetShader(wi::enums::VSTYPE_OBJECT_COMMON);pso.ps=&pixel;
        pso.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);
        pso.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_TRANSPARENT);
        pso.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
        Check(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_MAIN]),"Native WATER main pipeline");
        pso.vs=wi::renderer::GetShader(wi::enums::VSTYPE_SHADOW_TRANSPARENT);pso.ps=wi::renderer::GetShader(wi::enums::PSTYPE_SHADOW_TRANSPARENT);
        pso.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENTSHADOW);pso.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEPTHREAD);
        pso.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_SHADOW_DOUBLESIDED);
        Check(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_SHADOW]),"Native transparent shadow pipeline");
        const auto shader=wi::renderer::RegisterCustomShader(custom);
        wi::graphics::GPUBuffer buffer;
        Path path;wi::scene::CameraComponent camera;camera.CreatePerspective(1264,761,.05f,500,XMConvertToRadians(60));
        path.scene=&scene;path.camera=&camera;path.init(1264,761,96);path.setMotionBlurEnabled(false);path.setDepthOfFieldEnabled(false);
        path.setOcclusionCullingEnabled(false);path.ResizeBuffers();wi::renderer::SetTemporalAAEnabled(false);
        struct Guard {wi::Application& app;~Guard(){app.ActivatePath(nullptr);while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}} guard{app};
        const XMFLOAT3 eyes[]={{8,1.3f,5},{-2,34,22},{-13,1.5f,-5},{-13,1.5f,7},{2,6,10}};
        auto pose=[&](int n) {camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
            XMLoadFloat3(&eyes[n]),XMVectorSet(-1,0,0,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();};
        wi::gui::Label heading;heading.Create("G7B confluence status");heading.SetPos({12,12});heading.SetSize({1150,76});path.GetGUI().AddWidget(&heading);
        bool enabled=true,arrows=false,wire=false;int view=0;
        auto bind=[&]() {
            gpu->WaitForGPU();auto* m=scene.materials.GetComponent(joined);
            m->DisableCustomShader();m->userdata=optics.userdata;
            if(enabled) {
                wi::graphics::GPUBufferDesc bd;bd.size=sizeof(params);bd.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;bd.misc_flags=wi::graphics::ResourceMiscFlag::BUFFER_RAW;
                Check(gpu->CreateBuffer(&bd,&params,&buffer),"Confluence parameter buffer");
                m->SetCustomShaderID(shader);m->userdata={uint32_t(gpu->GetDescriptorIndex(&buffer,wi::graphics::SubresourceType::SRV)),0x52464c33,0,0};
            }
            m->SetDirty();
            heading.SetText(std::string("RIVER G7B / SINGLE CONTINUOUS SURFACE / ")+(enabled?(params.control.w?"VELOCITY DIAGNOSTIC":"DIRECTED NATIVE WATER"):"NATIVE WATER")+
                "\nF native/flow   V velocity   A arrows   W mesh edges   1-5 views   ESC close\nGeometry and velocity proof only. Foam transfer and creator snapping are not enabled.");
        };
        auto frames=[&](int count) {for(int n=0;n<count;++n)app.Run();};
        auto capture=[&](const std::string& name) {
            frames(16);while(gpu->GetFrameCount()%256)frames(1);gpu->WaitForGPU();
            Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/(name+".png")).generic_u8string()),"Native capture");
            std::cout<<"CAPTURE="<<name<<"\n";
        };
        pose(1);bind();app.ActivatePath(&path);frames(100);
        path.frozen=true;wi::renderer::SetGameSpeed(0);
        if(!interactive) {
            enabled=false;bind();capture("native-before");
            enabled=true;bind();capture("flow-overhead-t2");
            params.control.w=1;bind();capture("velocity-overhead");
            params.control.w=0;bind();pose(0);capture("flow-eye");
            params.control.y=Bits(2.4f);bind();capture("flow-eye-t24");
            pose(2);capture("west-eye");pose(3);capture("east-eye");
            pose(1);enabled=false;bind();capture("native-restored");
            {wi::Archive archive((out/"confluence-native.wiscene").generic_u8string(),false);scene.Serialize(archive);}
            {wi::Archive archive((out/"confluence-native.wiscene").generic_u8string(),true);wi::scene::Scene fresh;fresh.Serialize(archive);
                const auto* m=fresh.materials.GetComponent(fresh.Entity_FindByName("G7B continuous native WATER"));
                Check(m&&!m->IsCustomShader()&&m->userdata.y==optics.userdata.y,"Transient junction shader serialized");}
            enabled=true;params.control.y=Bits(-1);bind();path.frozen=false;wi::renderer::SetGameSpeed(1);pose(0);
            const auto start=std::chrono::steady_clock::now();
            for(int n=0;n<150;++n)app.Run();
            std::cout<<"CONFLUENCE_FPS="<<150/std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"\n";
            for(int n=0;n<30;++n) {
                params.control.y=Bits(2+n*.1f);bind();frames(2);gpu->WaitForGPU();
                Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/("motion-"+std::to_string(n)+".png")).generic_u8string()),"Motion capture");
            }
            enabled=false;bind();Check(wi::renderer::GetShaderErrorCount()==0,"Native shader errors");
            std::cout<<"G7B_NATIVE_WATER_PROOF=PASS shaderErrors=0\n";
        } else {
            wi::renderer::SetTemporalAAEnabled(true);
            params.control.y=Bits(-1);bind();path.frozen=false;wi::renderer::SetGameSpeed(1);pose(0);
            ShowWindow(window,SW_SHOW);SetForegroundWindow(window);
            MSG msg{};bool live=true;std::cout<<"G7B_INTERACTIVE_READY=1\n";
            while(live&&IsWindow(window)) {
                while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
                    if(msg.hwnd==window&&msg.message==WM_KEYDOWN&&!(msg.lParam&(1<<30))) {
                        const int key=int(msg.wParam);
                        if(key==VK_ESCAPE)live=false;
                        if(key=='F'){enabled=!enabled;bind();}
                        if(key=='V'){params.control.w=1-params.control.w;enabled=true;bind();}
                        if(key=='A')arrows=!arrows;
                        if(key=='W')wire=!wire;
                        if(key>='1'&&key<='5'){view=key-'1';pose(view);}
                    }
                    if(msg.message==WM_QUIT||msg.message==WM_CLOSE)live=false;
                    TranslateMessage(&msg);DispatchMessageW(&msg);
                }
                if(arrows)for(float z=-12;z<12;z+=1.5f)for(float x=-26;x<30;x+=1.5f) {
                    auto q=surface->Evaluate({x,z});if(!q.valid)continue;
                    const XMFLOAT3 a{x,.07f,z},b{x+q.velocityXZ.x,.07f,z+q.velocityXZ.y};
                    wi::renderer::RenderableLine line;line.start=a;line.end=b;line.color_start={1,.65f,.1f,1};line.color_end=line.color_start;wi::renderer::DrawLine(line);
                    wi::renderer::DrawPoint({b,.06f,{1,.65f,.1f,1}});
                }
                if(wire)for(std::size_t k=0;k<waterI.size();k+=3)for(int edge=0;edge<3;++edge) {
                    auto a=waterV[waterI[k+edge]],b=waterV[waterI[k+(edge+1)%3]];a.y+=.015f;b.y+=.015f;
                    wi::renderer::RenderableLine line;line.start=a;line.end=b;line.color_start={.2f,.9f,1,1};line.color_end=line.color_start;wi::renderer::DrawLine(line);
                }
                app.Run();Sleep(1);
            }
            enabled=false;bind();
        }
        return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";gpu->WaitForGPU();return 1;}
}
