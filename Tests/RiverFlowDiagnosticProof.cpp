#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/RiverFlowFieldService.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace DirectX;
using namespace renegade::bridge;
namespace fs=std::filesystem;
static void Check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
static XMFLOAT3 Mix(XMFLOAT3 a,XMFLOAT3 b,float t)
{return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
struct DiagnosticLine {XMFLOAT3 a,b;XMFLOAT4 colour;};
struct DiagnosticMark {XMFLOAT3 position;XMFLOAT4 colour;float size=.12f;};
struct DiagnosticBatch {
    std::vector<DiagnosticLine> lines;
    std::vector<DiagnosticMark> marks;
    std::size_t arrows=0,valid=0,outside=0,ambiguous=0;
};
static const char* Modes[]={"FLOW ARROWS | length = authored speed; colour = bank distance",
    "VALID COVERAGE | green = water; grey = outside native ribbon",
    "TANGENT | red = +X; blue = +Z; arrowhead = downstream",
    "SPEED | blue = 0; orange = 1; red = 2 m/s",
    "LATERAL | cyan = left; white = centre; orange = right",
    "PROJECTION CHECK | green = valid; magenta = ambiguous; grey = outside"};

static void Arrow(DiagnosticBatch& batch,XMFLOAT3 p,const RiverFlowQuery& q,
    XMFLOAT4 colour,const RiverFlowField& field)
{
    // Debug strokes only; anchor AND arrow tips must belong to the actual native ribbon.
    const float length=.45f+.55f*q.speedMetersPerSecond;
    XMFLOAT3 a{p.x-q.directionXZ.x*length*.5f,p.y+.045f,p.z-q.directionXZ.y*length*.5f};
    XMFLOAT3 b{p.x+q.directionXZ.x*length*.5f,p.y+.045f,p.z+q.directionXZ.y*length*.5f};
    if(!field.Evaluate({a.x,a.z}).valid || !field.Evaluate({b.x,b.z}).valid)return;
    const float rx=-q.directionXZ.y,rz=q.directionXZ.x;
    XMFLOAT3 h0{b.x-q.directionXZ.x*length*.28f+rx*length*.20f,b.y,
        b.z-q.directionXZ.y*length*.28f+rz*length*.20f};
    XMFLOAT3 h1{b.x-q.directionXZ.x*length*.28f-rx*length*.20f,b.y,
        b.z-q.directionXZ.y*length*.28f-rz*length*.20f};
    // Three close strokes improve readability without a persistent mesh or material.
    for(float offset:{-.012f,0.f,.012f}) {
        auto shift=[&](XMFLOAT3 x){return XMFLOAT3{x.x+rx*offset,x.y,x.z+rz*offset};};
        batch.lines.push_back({shift(a),shift(b),colour});
        batch.lines.push_back({shift(b),shift(h0),colour});
        batch.lines.push_back({shift(b),shift(h1),colour});
    }
    ++batch.arrows;
}
static std::array<DiagnosticBatch,6> MakeBatches(const RiverFlowField& field)
{
    std::array<DiagnosticBatch,6> batches;
    const auto& rows=field.NativeRibbon();
    for(std::size_t i=1;i<rows.size();++i) {
        for(float t:{.25f,.75f}) for(float lateral:{-.60f,0.f,.60f}) {
            const auto right=Mix(rows[i-1].rightBank,rows[i].rightBank,t);
            const auto left=Mix(rows[i-1].leftBank,rows[i].leftBank,t);
            auto p=Mix(left,right,(lateral+1)*.5f);
            const auto q=field.Evaluate({p.x,p.z});
            Check(q.valid&&!q.ambiguous,"Native ribbon diagnostic anchor invalid");
            p.y=q.surfaceHeight;
            const float bank=std::clamp(q.distanceToBankMeters/2.f,0.f,1.f);
            Arrow(batches[0],p,q,{1.f-.75f*bank,.85f,bank,1},field);
            Arrow(batches[2],p,q,{.5f+.5f*q.directionXZ.x,.18f,.5f+.5f*q.directionXZ.y,1},field);
        }
    }
    float minX=INFINITY,maxX=-INFINITY,minZ=INFINITY,maxZ=-INFINITY;
    for(const auto& row:rows) for(const auto p:{row.rightBank,row.leftBank}) {
        minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);
        minZ=std::min(minZ,p.z);maxZ=std::max(maxZ,p.z);
    }
    // A bounded point-sampled diagnostic mask, not a new WATER shader or an overlay plane.
    const float step=std::max(.5f,std::max(maxX-minX,maxZ-minZ)/64.f);
    std::size_t cells=0;
    for(float z=minZ-.5f;z<=maxZ+.5f;z+=step)for(float x=minX-.5f;x<=maxX+.5f;x+=step) {
        Check(++cells<=8192,"Diagnostic grid cap exceeded");
        const auto q=field.Evaluate({x,z});
        const XMFLOAT3 p{x,(q.valid?q.surfaceHeight:rows.front().centre.y)+.065f,z};
        const XMFLOAT4 grey{.22f,.22f,.22f,1},green{.08f,1,.25f,1},magenta{1,0,1,1};
        batches[1].marks.push_back({p,q.valid?green:grey,.09f});
        batches[5].marks.push_back({p,q.ambiguous?magenta:(q.valid?green:grey),q.ambiguous?.2f:.09f});
        for(auto& b:batches){b.valid+=q.valid;b.outside+=!q.valid;b.ambiguous+=q.ambiguous;}
        if(q.valid) {
            const float speed=std::clamp(q.speedMetersPerSecond/2.f,0.f,1.f);
            const XMFLOAT4 speedColour=speed<=.5f ?
                XMFLOAT4{2*speed,.35f+.4f*speed,1-2*speed,1} :
                XMFLOAT4{1,.55f-.9f*(speed-.5f),0,1};
            batches[3].marks.push_back({p,speedColour,.10f+.08f*speed});
            const float side=q.lateralFraction;
            const XMFLOAT4 colour=side<0?XMFLOAT4{1+side,1,1,1}:XMFLOAT4{1,1-.45f*side,1-side,1};
            batches[4].marks.push_back({p,colour,.13f});
        }
    }
    Check(batches[0].arrows>10 && batches[1].valid>10 && batches[1].outside>0,
        "Diagnostic fixture must contain arrows, water coverage and outside coverage");
    return batches;
}
struct DiagnosticPath final:wi::RenderPath3D {
    const DiagnosticBatch* batch=nullptr;
    bool enabled=false,frozen=true;
    std::size_t submittedLines=0,submittedMarks=0;
    void Update(float dt)override {
        wi::RenderPath3D::Update(frozen?0.f:dt);
        submittedLines=submittedMarks=0;
        if(!enabled || !batch)return;
        for(const auto& p:batch->lines) {
            wi::renderer::RenderableLine line;
            line.start=p.a;line.end=p.b;line.color_start=line.color_end=p.colour;
            wi::renderer::DrawLine(line,true);++submittedLines;
        }
        for(const auto& p:batch->marks) {
            wi::renderer::RenderablePoint point;
            point.position=p.position;point.color=p.colour;point.size=p.size;
            wi::renderer::DrawPoint(point,true);++submittedMarks;
        }
    }
};
static std::array<std::size_t,5> Counts(const wi::scene::Scene& s)
{return {s.names.GetCount(),s.transforms.GetCount(),s.meshes.GetCount(),s.objects.GetCount(),s.materials.GetCount()};}
int main(int argc,char** argv)
{
    if(argc<3||argc>4)return 2;
    const bool interactive=argc==4&&std::string(argv[3])=="--interactive";
    const bool reopen=argc==4&&std::string(argv[3])=="--reopen";
    if(argc==4&&!interactive&&!reopen)return 2;
    std::cout.setf(std::ios::unitbuf);
    const auto out=fs::absolute(argv[1]);
    if(fs::exists(out)&&!fs::is_empty(out)){std::cerr<<"Use a new empty proof directory\n";return 2;}
    fs::create_directories(out);
    HWND window=CreateWindowExW(0,L"STATIC",L"Renegade River V2 - Flow Diagnostics",
        WS_OVERLAPPEDWINDOW,60,60,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    struct App:wi::Application {void Initialize()override{wi::Application::Initialize();alwaysactive=true;}} app;
    app.allow_hdr=false;app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    auto* gpu=wi::graphics::GetDevice();
    struct Drain{~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU();}} drain;
    try {
        wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
        SceneService service;
        Check(service.LoadScene(fs::absolute(argv[2]).generic_u8string()),service.LastError());
        auto& scene=service.GetScene();
        Check(scene.terrains.GetCount()==1,"Expected textured native terrain");
        for(const char* name:{"Rock Foam Contact - Test","Rock Foam Wake - Test"})
            Check(scene.Entity_FindByName(name)==wi::ecs::INVALID_ENTITY,"Rejected overlay present in input");
        const auto river=scene.Entity_FindByName("River Water 5");
        const auto* water=scene.materials.GetComponent(river);
        Check(water&&!water->IsCustomShader()&&water->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER,
            "Stock native WATER required");
        Check(water->textures[wi::scene::MaterialComponent::NORMALMAP].resource.GetTexture().IsValid(),"Normal texture missing");
        const auto optics=*water;const auto counts=Counts(scene);
        RiverFlowSettings settings;
        std::shared_ptr<const RiverFlowField> forward,backward;std::string error;
        Check(BuildRiverFlowField(scene,river,settings,forward,error),error);
        settings.downstreamReversed=true;
        Check(BuildRiverFlowField(scene,river,settings,backward,error),error);
        auto positive=MakeBatches(*forward),negative=MakeBatches(*backward);
        Check(positive[0].arrows==negative[0].arrows,"Reverse changed arrow count");
        for(const auto& row:forward->NativeRibbon()) {
            const auto a=forward->Evaluate({row.centre.x,row.centre.z}),b=backward->Evaluate({row.centre.x,row.centre.z});
            Check(a.valid&&b.valid,"Native row coverage");
            Check(a.directionXZ.x*b.directionXZ.x+a.directionXZ.y*b.directionXZ.y<-.99999f,"Reverse direction mismatch");
            Check(a.speedMetersPerSecond==b.speedMetersPerSecond&&a.speedMetersPerSecond==1.f,"Uncommanded acceleration");
        }
        std::cout<<"G2_CPU_DIAGNOSTICS=PASS arrows="<<positive[0].arrows
            <<" validPoints="<<positive[1].valid<<" outsidePoints="<<positive[1].outside
            <<" ambiguousPoints="<<positive[1].ambiguous<<" speed=1m/s hash="<<forward->DeterministicHash()<<"\n";
        std::ofstream table(out/"flow-arrows.csv");
        table<<"reverse,x,y,z,dirX,dirZ,speed,bankDistance\n";
        for(int reversed=0;reversed<2;++reversed)for(const auto& row:forward->NativeRibbon()) {
            const auto q=(reversed?backward:forward)->Evaluate({row.centre.x,row.centre.z});
            table<<reversed<<","<<row.centre.x<<","<<q.surfaceHeight<<","<<row.centre.z<<","
                <<q.directionXZ.x<<","<<q.directionXZ.y<<","<<q.speedMetersPerSecond<<","<<q.distanceToBankMeters<<"\n";
        }
        wi::scene::CameraComponent camera;
        camera.CreatePerspective(1264,761,.05f,1000,XMConvertToRadians(60));
        DiagnosticPath render;render.scene=&scene;render.camera=&camera;
        render.init(1264,761,96);render.setMotionBlurEnabled(false);render.setDepthOfFieldEnabled(false);
        render.setOcclusionCullingEnabled(false);render.ResizeBuffers();
        struct Guard{wi::Application& app;~Guard(){app.ActivatePath(nullptr);}} guard{app};
        wi::renderer::SetTemporalAAEnabled(false);wi::renderer::SetGameSpeed(0);
        wi::gui::Label heading;heading.Create("River V2 Diagnostic Status");
        heading.SetPos({12,12});heading.SetSize({1050,132});
        render.GetGUI().AddWidget(&heading);
        int mode=0,view=0;bool reversed=false,visible=true;
        const XMFLOAT3 eyes[]={{0,1.2f,64},{0,28,48},{-12,2.2f,64},{12,2.2f,64},{0,8,68}};
        const XMFLOAT3 targets[]={{0,.15f,68},{0,.15f,68},{0,.15f,68},{0,.15f,68},{12,.15f,69}};
        XMFLOAT3 eye=eyes[0];float yaw=0,pitch=0;
        auto pose=[&](int n) {
            view=n;eye=eyes[n];const auto target=targets[n];
            yaw=std::atan2(target.x-eye.x,target.z-eye.z);
            pitch=std::atan2(target.y-eye.y,std::hypot(target.x-eye.x,target.z-eye.z));
            camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMLoadFloat3(&eye),
                XMLoadFloat3(&target),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
        };
        auto status=[&](){
            render.batch=&(reversed?negative:positive)[mode];
            const std::string text=std::string("RIVER V2 / G2  |  ")+(render.enabled?"DIAGNOSTICS":"NATIVE WATER")+
                "  |  "+(reversed?"REVERSED":"FORWARD")+"  |  speed 1.00 m/s (authored, uniform)  |  "+(render.frozen?"PAUSED":"RUNNING")+"\n"+
                (render.enabled?Modes[mode]:"Stock Wicked WATER / no debug strokes")+
                "\nF native/debug   R reverse   M mode   1-5 viewpoints   Space pause\n"+
                "6 coverage  7 tangent  8 speed  9 lateral  0 projection  |  WASD / arrows  |  Esc close\n"+
                "Diagnostics only: water motion remains stock. No foam or obstacle response.";
            heading.SetText(text);
        };
        auto key=[&](WPARAM k) {
            if(k=='F')render.enabled=!render.enabled;
            if(k=='R')reversed=!reversed;
            if(k=='M')mode=(mode+1)%6;
            if(k>='1'&&k<='5')pose(int(k-'1'));
            if(k>='6'&&k<='9'){mode=int(k-'6')+1;render.enabled=true;}
            if(k=='0'){mode=5;render.enabled=true;}
            if(k==VK_SPACE){render.frozen=!render.frozen;wi::renderer::SetGameSpeed(render.frozen?0:1);}
            if(k==VK_ESCAPE)visible=false;
            status();
        };
        auto pump=[&](){
            MSG msg{};
            while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
                if(msg.hwnd==window&&msg.message==WM_KEYDOWN&&!(msg.lParam&(1<<30)))key(msg.wParam);
                if(msg.message==WM_QUIT)visible=false;
                TranslateMessage(&msg);DispatchMessageW(&msg);
            }
        };
        auto press=[&](WPARAM k){Check(PostMessageW(window,WM_KEYDOWN,k,0)!=0,"Key injection failed");pump();};
        auto frames=[&](int n){for(int i=0;i<n;++i){pump();app.Run();}};
        auto capture=[&](const std::string& name) {
            frames(20);gpu->WaitForGPU();
            Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/(name+".png")).generic_u8string()),"3D capture failed");
            Check(wi::helper::saveTextureToFile(render.GetRenderResult2D(),(out/(name+"-hud.png")).generic_u8string()),"HUD capture failed");
        };
        pose(0);status();app.ActivatePath(&render);
        // Native terrain/probe GPU initialization requires positive dt. Freeze
        // only AFTER this real native warm-up, never on the first loaded frame.
        render.frozen=false;wi::renderer::SetGameSpeed(1);frames(120);
        render.frozen=true;wi::renderer::SetGameSpeed(0);frames(20);
        while(wi::renderer::IsPipelineCreationActive())Sleep(10);
        if(reopen) {
            Check(!render.enabled&&render.submittedLines==0&&render.submittedMarks==0,"Reopen diagnostics must default off");
            capture("fresh-native-waterline");pose(1);capture("fresh-native-overhead");
            Check(Counts(scene)==counts,"Reopen diagnostics changed scene membership");
            Check(!water->IsCustomShader(),"Reopen shader contamination");
            std::cout<<"G2_FRESH_PROCESS_REOPEN=PASS debugDefaultOff=1 countsStable=1 hash="
                <<forward->DeterministicHash()<<"\n";
        } else if(!interactive) {
            const auto start=std::chrono::steady_clock::now();frames(90);
            const double nativeFps=90/std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            std::cout<<"G2_NATIVE_FPS="<<nativeFps<<"\n";
            pose(1);capture("native-before");
            const auto phase=gpu->GetFrameCount()%256;
            press('F');Check(render.enabled,"F enable failed");capture("flow-forward-overhead");
            Check(render.submittedLines>0,"No debug lines submitted");
            press('R');Check(reversed,"R reverse failed");capture("flow-reverse-overhead");
            press('R');
            for(int i=0;i<6;++i) {
                mode=i;status();capture("mode-"+std::to_string(i));
                std::cout<<"G2_MODE="<<i<<" lines="<<render.submittedLines<<" marks="<<render.submittedMarks<<"\n";
            }
            for(int i=0;i<5;++i){press('1'+i);mode=0;status();capture("view-"+std::to_string(i+1));}
            press('2');mode=0;status();
            // Serialize with diagnostics ON: all drawings are renderer queues, absent from ECS.
            const auto saved=out/"native-diagnostic-scene.wiscene";
            {wi::Archive archive(saved.generic_u8string(),false);scene.Serialize(archive);}
            Check(Counts(scene)==counts,"Debug primitives leaked into scene membership");
            press('F');Check(!render.enabled,"F disable failed");frames(20);
            Check(render.submittedLines==0&&render.submittedMarks==0,"Debug submissions linger after disable");
            // Match native blue-noise frame phase with all scene/time updates frozen.
            while(gpu->GetFrameCount()%256!=phase)frames(1);
            gpu->WaitForGPU();
            Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/"native-restored.png").generic_u8string()),"Restore capture failed");
            const auto later=std::chrono::steady_clock::now();frames(90);
            std::cout<<"G2_HIDDEN_FPS="<<90/std::chrono::duration<double>(std::chrono::steady_clock::now()-later).count()<<"\n";
            {wi::Archive archive(saved.generic_u8string(),true);wi::scene::Scene fresh;fresh.Serialize(archive);
                Check(Counts(fresh)==counts,"Serialized debug mesh/entity contamination");
                const auto* m=fresh.materials.GetComponent(fresh.Entity_FindByName("River Water 5"));
                Check(m&&!m->IsCustomShader()&&m->userdata.x==optics.userdata.x&&m->userdata.y==optics.userdata.y&&
                    m->userdata.z==optics.userdata.z&&m->userdata.w==optics.userdata.w,
                    "Serialized debug shader or userdata contamination");
                std::shared_ptr<const RiverFlowField> rebuilt;
                Check(BuildRiverFlowField(fresh,fresh.Entity_FindByName("River Water 5"),{},rebuilt,error),error);
                Check(rebuilt->DeterministicHash()==forward->DeterministicHash(),"Saved flow rebuild mismatch");
            }
            Check(water->baseColor.x==optics.baseColor.x&&water->baseColor.w==optics.baseColor.w&&
                water->normalMapStrength==optics.normalMapStrength&&water->refraction==optics.refraction&&
                water->texAnimFrameRate==optics.texAnimFrameRate&&!water->IsCustomShader(),"Optics changed");
            Check(wi::renderer::GetShaderErrorCount()==0,"Shader errors");
            std::cout<<"G2_CONTROLS_SAVE_RESTORE=PASS sceneCountsStable=1 shader_errors=0 nativeFramePhase="<<phase<<"\n";
        }
        if(interactive) {
            pose(0);mode=0;reversed=false;render.enabled=true;render.frozen=false;
            wi::renderer::SetGameSpeed(1);status();
            ShowWindow(window,SW_SHOW);SetForegroundWindow(window);
            std::cout<<"G2_INTERACTIVE_READY=1 hwnd="<<reinterpret_cast<std::uintptr_t>(window)<<"\n";
            auto previous=std::chrono::steady_clock::now();
            while(visible&&IsWindow(window)) {
                pump();
                const auto now=std::chrono::steady_clock::now();
                const float dt=std::min(.05f,float(std::chrono::duration<double>(now-previous).count()));previous=now;
                if(GetForegroundWindow()==window) {
                    auto held=[](int code){return (GetAsyncKeyState(code)&0x8000)!=0;};
                    bool moved=false;
                    if(held(VK_LEFT)){yaw-=dt;moved=true;}if(held(VK_RIGHT)){yaw+=dt;moved=true;}
                    if(held(VK_UP)){pitch=std::min(1.3f,pitch+dt);moved=true;}
                    if(held(VK_DOWN)){pitch=std::max(-1.5f,pitch-dt);moved=true;}
                    const float speed=held(VK_SHIFT)?7.f:3.f;
                    float forwardInput=float(held('W'))-float(held('S')),rightInput=float(held('D'))-float(held('A'));
                    if(forwardInput||rightInput) {
                        eye.x+=(std::sin(yaw)*forwardInput+std::cos(yaw)*rightInput)*dt*speed;
                        eye.z+=(std::cos(yaw)*forwardInput-std::sin(yaw)*rightInput)*dt*speed;moved=true;
                    }
                    if(moved) {
                        const auto e=XMLoadFloat3(&eye);
                        const auto d=XMVectorSet(std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch),0);
                        camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(e,e+d,XMVectorSet(0,1,0,0))));camera.UpdateCamera();
                    }
                }
                app.Run();Sleep(2);
            }
        }
        render.GetGUI().RemoveWidget(&heading);
        std::cout<<"G2_PROOF_EXIT=PASS\n";return 0;
    }catch(const std::exception& e){std::cerr<<"G2_FAIL: "<<e.what()<<"\n";return 1;}
}
