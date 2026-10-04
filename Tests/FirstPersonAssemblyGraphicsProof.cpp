// Manual owner-supplied asset inspection; assets never enter the repository.
#include "renegade/bridge/ModelImportCandidateService.h"
#include "renegade/bridge/ModelAnimationPreviewService.h"
#include "../Studio/src/ModelImportPreview.h"
#include <WickedEngine.h>
#include "../WickedEngine/Editor/ModelImporter.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
namespace fs = std::filesystem;
using namespace renegade::bridge;
static LRESULT CALLBACK WindowProc(HWND w, UINT m, WPARAM a, LPARAM b)
{ return DefWindowProcW(w,m,a,b); }
static bool Capture(wi::scene::Scene& scene, const fs::path& file, bool firstPerson=false)
{
    renegade::studio::ModelImportPreview preview;
    std::string error;
    if (!preview.Prepare(scene,error)) { std::cerr << error << '\n'; return false; }
    if(firstPerson) {
        preview.camera->CreatePerspective(512,320,0.005f,10.0f,XMConvertToRadians(80.0f));
        preview.camera->TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
            XMVectorSet(0,0.08f,-1.65f,1),XMVectorSet(0,-1,-1.65f,1),XMVectorSet(0,0,-1,0))));
        preview.camera->UpdateCamera();
    }
    // Preserve all simultaneously selected tracks on the private preview copy.
    for (size_t i=0;i<scene.animations.GetCount();++i)
    {
        preview.scene->animations[i].amount=scene.animations[i].amount;
        preview.scene->animations[i].timer=scene.animations[i].timer;
        preview.scene->animations[i].last_update_time=-std::numeric_limits<float>::max();
    }
    for (int frame=0; frame<3000 && !preview.IsReady(); ++frame)
    {
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
        preview.PreUpdate(); preview.Update(1.0f/60); preview.PreRender(); preview.Render();
        wi::graphics::GetDevice()->SubmitCommandLists();
        wi::renderer::UpdateGPUSuballocator(); Sleep(10);
    }
    std::vector<uint8_t> png;
    if (!preview.CapturePng(png,error)) { std::cerr << error << '\n'; return false; }
    std::ofstream out(file,std::ios::binary);
    out.write(reinterpret_cast<const char*>(png.data()),png.size());
    return out.good();
}
static void Pose(wi::scene::Scene& scene,float time)
{
    for (size_t i=0;i<scene.animations.GetCount();++i)
    {
        auto& clip=scene.animations[i]; clip.Pause(); clip.RootMotionOff();
        clip.amount=1; clip.timer=std::min(time,clip.end);
        clip.last_update_time=-std::numeric_limits<float>::max();
    }
    scene.Update(1.0f/60);
}
static wi::ecs::Entity Find(wi::scene::Scene& scene,const std::string& name)
{
    for(size_t i=0;i<scene.names.GetCount();++i)
        if(scene.names[i].name==name) return scene.names.GetEntity(i);
    return wi::ecs::INVALID_ENTITY;
}
static bool Assembly(wi::scene::Scene& arms,wi::scene::Scene& weapon,
    const fs::path& output,const std::string& action)
{
    const auto anchor=Find(arms,"weapon_r");
    if(anchor==wi::ecs::INVALID_ENTITY) return false;
    std::vector<wi::ecs::Entity> roots;
    for(size_t i=0;i<weapon.transforms.GetCount();++i)
    {
        const auto entity=weapon.transforms.GetEntity(i);
        if(!weapon.hierarchy.Contains(entity)) roots.push_back(entity);
    }
    if(roots.size()!=1) {std::cerr<<"Unexpected weapon roots\n";return false;}
    arms.Merge(weapon);
    // Provisional fixture placement: align weapon Handle reference pivot to right hand.
    // Native local attachment is retained; production socket offset remains to be authored.
    const auto hand=arms.transforms.GetComponent(Find(arms,"hand_r"))->GetPosition();
    auto* weaponRoot=arms.transforms.GetComponent(roots.front());
    weaponRoot->ClearTransform();
    // Calibrated lateral correction for this pack: camera right is imported -X.
    // Move the whole assembly toward the palm, including its authored shell bones.
    constexpr float gripLateralCorrection=0.025f;
    weaponRoot->Translate(XMFLOAT3(hand.x+gripLateralCorrection,hand.y-0.184208f,hand.z-0.00225067f));
    weaponRoot->UpdateTransform();
    arms.Component_Attach(roots.front(),anchor,false);
    for(float time : {0.0f,0.6f,1.2f,2.0f,2.9f})
    {
        Pose(arms,time);
        const auto p=arms.transforms.GetComponent(anchor)->GetPosition();
        std::cout<<action<<" t="<<time<<" attachment="<<p.x<<","<<p.y<<","<<p.z<<"\n";
        if(!Capture(arms,output/(action+"-"+std::to_string(time)+".png"))) return false;
        if(!Capture(arms,output/(action+"-fp-"+std::to_string(time)+".png"),true)) return false;
    }
    wi::Archive archive((output/(action+".wiscene")).generic_u8string(),false,false);
    arms.Serialize(archive);
    if(!archive.SaveFile((output/(action+".wiscene")).generic_u8string())) return false;
    wi::scene::Scene reopened;
    wi::Archive saved((output/(action+".wiscene")).generic_u8string(),true,false);
    if(!saved.IsOpen()) return false;
    reopened.Serialize(saved); Pose(reopened,1.2f);
    if(!Capture(reopened,output/(action+"-reopened.png"))) return false;
    return true;
}

static void ViewAssembly(wi::Application& app,HWND window,wi::scene::Scene& scene)
{
    renegade::studio::ModelImportPreview preview;
    std::string error;
    if(!preview.Prepare(scene,error)) {std::cerr<<error<<"\n";return;}
    preview.camera->CreatePerspective(512,320,0.005f,10.0f,XMConvertToRadians(80.0f));
    preview.camera->TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
        XMVectorSet(0,0.08f,-1.65f,1),XMVectorSet(0,-1,-1.65f,1),XMVectorSet(0,0,-1,0))));
    preview.camera->UpdateCamera();
    for(size_t i=0;i<preview.scene->animations.GetCount();++i)
    {
        auto& clip=preview.scene->animations[i];clip.Pause();clip.amount=1;clip.timer=0;
        clip.SetLooped(false);clip.last_update_time=-std::numeric_limits<float>::max();
    }
    app.ActivatePath(&preview);
    SetWindowTextW(window,L"Renegade assembly DIAGNOSTIC - R: reload | Space: pause/resume | Esc: close");
    SetWindowPos(window,nullptr,180,120,1000,650,SWP_NOZORDER|SWP_NOACTIVATE);
    app.SetWindow(window);
    RECT client={};GetClientRect(window,&client);
    preview.init(client.right,client.bottom,96);
    preview.camera->CreatePerspective(client.right,client.bottom,0.005f,10.0f,XMConvertToRadians(80.0f));
    preview.camera->UpdateCamera();preview.ResizeBuffers();
    ShowWindow(window,SW_SHOWNOACTIVATE);
    bool previousR=false,previousSpace=false;
    MSG message={};bool running=true;
    while(running && IsWindow(window))
    {
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))
        {
            if(message.message==WM_QUIT || (message.message==WM_KEYDOWN && message.wParam==VK_ESCAPE))
                running=false;
            TranslateMessage(&message);DispatchMessageW(&message);
        }
        if(!running) break;
        const bool focused=GetForegroundWindow()==window;
        const bool r=focused && (GetAsyncKeyState('R')&0x8000);
        const bool space=focused && (GetAsyncKeyState(VK_SPACE)&0x8000);
        if((r&&!previousR)||(space&&!previousSpace))
            for(size_t i=0;i<preview.scene->animations.GetCount();++i)
            {
                auto& clip=preview.scene->animations[i];
                if(r&&!previousR){clip.timer=clip.start;clip.Play();}
                else if(clip.IsPlaying()) clip.Pause();else clip.Play();
                clip.last_update_time=-std::numeric_limits<float>::max();
            }
        previousR=r;previousSpace=space;
        app.Run();Sleep(1);
    }
    app.ActivatePath(nullptr);
    DestroyWindow(window);
}

static bool RelinkDiagnosticMaterials(wi::scene::Scene& scene,const fs::path& input)
{
    using Material=wi::scene::MaterialComponent;
    for(size_t i=0;i<scene.materials.GetCount();++i)
    {
        auto& material=scene.materials[i];
        const auto* name=scene.names.GetComponent(scene.materials.GetEntity(i));
        std::cout<<"MATERIAL "<<(name?name->name:"")<<"\n";
        std::string refs;
        for(const auto& texture:material.textures) refs+=texture.name+";";
        std::string base,normal,surface;
        if(refs.find("T_Manny_01")!=std::string::npos) {
            base="Textures/Manny/T_Manny_01_D.PNG"; normal="Textures/Manny/T_Manny_01_N.PNG";
        } else if(refs.find("T_Manny_02")!=std::string::npos) {
            base="Textures/Manny/T_Manny_02_D.PNG"; normal="Textures/Manny/T_Manny_02_N.PNG";
        } else if(refs.find("Shell_Red")!=std::string::npos) {
            base="Meshes/Textures/Shell/T_Shell_Red_BaseColor.PNG";
            normal="Meshes/Textures/Shell/T_Shell_Red_Normal.PNG";
            surface="Meshes/Textures/Shell/T_Shell_Red_OcclusionRoughnessMetallic.PNG";
        } else if(refs.find("Sawed_Off_Shotgun")!=std::string::npos) {
            base="Meshes/Textures/Weapon/T_Sawed_Off_Shotgun_BaseColor.PNG";
            normal="Meshes/Textures/Weapon/T_Sawed_Off_Shotgun_Normal.PNG";
            surface="Meshes/Textures/Weapon/T_Sawed_Off_Shotgun_OcclusionRoughnessMetallic.PNG";
        } else {std::cerr<<"Unmapped material: "<<refs<<"\n";return false;}
        for(auto& texture:material.textures) {texture.name.clear();texture.resource={};}
        std::vector<std::pair<int,std::string>> slots={{Material::BASECOLORMAP,base},{Material::NORMALMAP,normal}};
        if(!surface.empty()) slots.push_back({Material::SURFACEMAP,surface});
        for(const auto& slot: slots)
        {
            auto& texture=material.textures[slot.first];
            texture.name=(input/slot.second).generic_u8string();
            texture.resource=wi::resourcemanager::Load(texture.name,
                wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA);
            if(!texture.resource.IsValid()||!texture.resource.GetTexture().IsValid()) {
                std::cerr<<"Cannot decode "<<texture.name<<"\n";return false;
            }
            std::cout<<"RELINK "<<slot.first<<" "<<slot.second<<"\n";
        }
        material.baseColor=XMFLOAT4(1,1,1,1);
        material.normalMapStrength=1;material.roughness=surface.empty()?0.6f:1.0f;
        if(!surface.empty()) {material.metalness=1;material.SetOcclusionEnabled_Primary(true);}
        material.SetDirty();
    }
    return true;
}

int main(int argc,char** argv)
{
    if(argc<3 || argc>4) { std::cerr<<"Usage: proof pack-folder output-folder\n"; return 2; }
    const fs::path input=fs::u8path(argv[1]), output=fs::u8path(argv[2]);
    fs::create_directories(output);
    const wchar_t* clsName=L"RenegadeFirstPersonAssemblyProof";
    WNDCLASSEXW cls={};cls.cbSize=sizeof(cls);cls.lpfnWndProc=WindowProc;
    cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=clsName;
    RegisterClassExW(&cls);
    HWND window=CreateWindowExW(0,clsName,L"Renegade isolated assembly proof",
        WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,cls.hInstance,nullptr);
    wi::Application app; app.allow_hdr=false; app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain { ~Drain(){ while(wi::renderer::IsPipelineCreationActive()) Sleep(10);
        wi::graphics::GetDevice()->WaitForGPU(); } } drain;
    const char* paths[]={"Animations/AS_Shotgun_Idle.FBX",
        "Animations/Weapon/AS_Shotgun_Idle.FBX","Animations/AS_Shotgun_Reload.FBX",
        "Animations/Weapon/AS_Shotgun_Reload.FBX"};
    wi::allocator::shared_ptr<wi::scene::Scene> scenes[4];
    for(int i=0;i<4;++i)
    {
        auto candidate=ModelImportCandidateService().PrepareModel((input/paths[i]).generic_u8string());
        if(!candidate.IsReady()) {
            std::cout<<"GOVERNED IMPORT BLOCKED: "<<candidate.Error()<<'\n';
            scenes[i]=wi::allocator::make_shared<wi::scene::Scene>();
            ImportModel_FBX((input/paths[i]).generic_u8string(),*scenes[i]);
            if(!RelinkDiagnosticMaterials(*scenes[i],input)) return 7;
            std::cout<<"DIAGNOSTIC ONLY: native converter, explicit supplied-texture relink; not governed import acceptance\n";
        } else scenes[i]=candidate.ReleaseScene();
        auto& scene=*scenes[i];
        Pose(scene,0);
        wi::primitive::AABB bounds;
        if(!ComputeVisibleModelBounds(scene,bounds)) return 4;
        auto lo=bounds.getMin(),hi=bounds.getMax();
        std::cout<<paths[i]<<" objects="<<scene.objects.GetCount()<<" bones="<<ImportService::SummarizeModelEvidence(scene).armatureBones
            <<" clips="<<scene.animations.GetCount()<<" bounds "<<lo.x<<","<<lo.y<<","<<lo.z
            <<" to "<<hi.x<<","<<hi.y<<","<<hi.z<<'\n';
        for(size_t a=0;a<scene.animations.GetCount();++a)
            std::cout<<" duration="<<scene.animations[a].end-scene.animations[a].start
                <<" channels="<<scene.animations[a].channels.size()<<'\n';
        for(size_t n=0;n<scene.names.GetCount();++n)
        {
            const auto& name=scene.names[n].name;
            if(name=="hand_r"||name=="hand_l"||name=="Sawed_off_Shotgun_Handle"||name=="Sawed_off_Shotgun_Barrel"||name=="weapon_r"||name=="ik_hand_gun"||name=="Sawed_off_Shotgun_root")
            {
                auto e=scene.names.GetEntity(n);auto* t=scene.transforms.GetComponent(e);
                if(t){auto p=t->GetPosition();auto r=t->GetRotation();std::cout<<"rot="<<r.x<<","<<r.y<<","<<r.z<<","<<r.w<<" "<<name<<" entity="<<e<<" position="
                    <<p.x<<","<<p.y<<","<<p.z<<'\n';}
            }
        }
        if(!Capture(scene,output/("source-"+std::to_string(i)+".png"))) return 5;
    }
    if(!Assembly(*scenes[0],*scenes[1],output,"idle") || !Assembly(*scenes[2],*scenes[3],output,"reload")) return 6;
    std::cout<<"FOUR SOURCE CAPTURES AND PAIRED ASSEMBLY CAPTURES COMPLETE\n";
    if(argc==4 && std::string(argv[3])=="--view") ViewAssembly(app,window,*scenes[2]);
    return 0;
}
