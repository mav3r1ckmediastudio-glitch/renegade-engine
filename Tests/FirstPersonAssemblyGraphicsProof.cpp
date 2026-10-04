// Manual owner-supplied asset inspection; assets never enter the repository.
#include "renegade/bridge/ModelImportCandidateService.h"
#include "renegade/bridge/ModelAnimationPreviewService.h"
#include "../Studio/src/ModelImportPreview.h"
#include <WickedEngine.h>
#include "renegade/bridge/ModelImportCommitService.h"
#include "renegade/bridge/MatchingRigAnimationService.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "../WickedEngine/Editor/json.hpp"
#include "../WickedEngine/Editor/ModelImporter.h"
#include <Windows.h>
#include "renegade/bridge/FirstPersonAssemblyService.h"
#include "renegade/bridge/PlayerService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <iomanip>
#include <sstream>
#include "../Runtime/src/RuntimePlayerViewAnimation.h"
#include "../Runtime/src/RuntimePlayerViewAsset.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/TestLevelSnapshotService.h"
namespace fs = std::filesystem;
using namespace renegade::bridge;
static LRESULT CALLBACK WindowProc(HWND w, UINT m, WPARAM a, LPARAM b)
{ return DefWindowProcW(w,m,a,b); }
static bool Capture(wi::scene::Scene& scene, const fs::path& file, bool firstPerson=false, bool normalized=false, bool allowEmpty=false)
{
    renegade::studio::ModelImportPreview preview;
    std::string error;
    if (!preview.Prepare(scene,error)) { std::cerr << error << '\n'; return false; }
    if(firstPerson) {
        preview.camera->CreatePerspective(512,320,0.005f,10.0f,XMConvertToRadians(80.0f));
        preview.camera->TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
            XMVectorSet(0,0.08f,-1.65f,1),XMVectorSet(0,-1,-1.65f,1),XMVectorSet(0,0,-1,0))));
        if(normalized)preview.camera->TransformCamera(XMMatrixIdentity());
        preview.camera->UpdateCamera();
    }
    // Preserve all simultaneously selected tracks on the private preview copy.
    for (size_t i=0;i<scene.animations.GetCount();++i)
    {
        preview.scene->animations[i].amount=scene.animations[i].amount;
        preview.scene->animations[i].timer=scene.animations[i].timer;
        preview.scene->animations[i].last_update_time=-std::numeric_limits<float>::max();
    }
    std::vector<uint8_t> png;
    bool visible=false;
    for (int frame=0; frame<3000; ++frame)
    {
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
        preview.PreUpdate(); preview.Update(1.0f/60); preview.PreRender(); preview.Render();
        wi::graphics::GetDevice()->SubmitCommandLists();
        wi::renderer::UpdateGPUSuballocator(); Sleep(10);
        if(!preview.IsReady())continue;
        if(!preview.CapturePng(png,error)){std::cerr<<error<<"\n";return false;}
        const auto decoded=wi::resourcemanager::Load("assembly-pixels-"+GenerateStableId()+".png",
            wi::resourcemanager::Flags::NONE,png.data(),png.size());
        wi::vector<uint8_t> pixels;
        if(!decoded.IsValid() || !decoded.GetTexture().IsValid() ||
            !wi::helper::saveTextureToMemory(decoded.GetTexture(),pixels) ||
            pixels.size()<512*320*4)return false;
        size_t contrasting=0, litModelPixels=0;
        for(size_t p=0;p+4<=512*320*4;p+=4) {
            if(pixels[p]>100 || pixels[p+1]>100 || pixels[p+2]>100)++litModelPixels;
            if(std::abs(int(pixels[p])-int(pixels[0]))+
                std::abs(int(pixels[p+1])-int(pixels[1]))+
                std::abs(int(pixels[p+2])-int(pixels[2]))>20)++contrasting;
        }
        if(allowEmpty||(firstPerson?litModelPixels:contrasting)>50){visible=true;break;}
    }
    if(!visible){std::cerr<<"No rendered assembly pixels: "<<file.generic_u8string()<<"\n";return false;}
    std::ofstream out(file,std::ios::binary);
    out.write(reinterpret_cast<const char*>(png.data()),png.size());
    return out.good();
}
static void Pose(wi::scene::Scene& scene,float time,const std::string& action={})
{
    for (size_t i=0;i<scene.animations.GetCount();++i)
    {
        auto& clip=scene.animations[i]; clip.Pause(); clip.RootMotionOff();
        clip.amount=1;
        if(!action.empty()) {
            const auto* metadata=scene.metadatas.GetComponent(scene.animations.GetEntity(i));
            if(metadata && metadata->string_values.has(CreatorCharacterAnimationActionMetadataKey))
                clip.amount=metadata->string_values.get(CreatorCharacterAnimationActionMetadataKey)==action?1.0f:0.0f;
        }
        clip.timer=std::min(time,clip.end);
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
    const auto anchor=Find(arms,"ik_hand_gun");
    if(anchor==wi::ecs::INVALID_ENTITY) return false;
    std::vector<wi::ecs::Entity> roots;
    for(size_t i=0;i<weapon.transforms.GetCount();++i)
    {
        const auto entity=weapon.transforms.GetEntity(i);
        if(!weapon.hierarchy.Contains(entity)) roots.push_back(entity);
    }
    if(roots.size()!=1) {std::cerr<<"Unexpected weapon roots\n";return false;}
    arms.Merge(weapon);
    // Original demo SKM_Weapon attaches to ik_hand_gun. Preserve its authored
    // relative transform, not a hand-joint/Handle-pivot alignment.
    // UE centimetres -> this FBX/Wicked basis: (x,-y,-z), metres.
    // UE Rotator (6.552304,-182.929938,-10.254553) converted in that basis.
    auto* weaponRoot=arms.transforms.GetComponent(roots.front());
    arms.Component_Attach(roots.front(),anchor,true);
    weaponRoot->ClearTransform();
    weaponRoot->rotation_local=XMFLOAT4(-0.059182247f,0.087738050f,0.994176416f,-0.020316230f);
    weaponRoot->Translate(XMFLOAT3(-0.03466970f,0.27336276f,-0.04505738f));
    weaponRoot->UpdateTransform();
    for(float time : {0.0f,0.6f,1.2f,2.0f,2.9f})
    {
        Pose(arms,time,action=="idle"?"Idle":"Reload");
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
    reopened.Serialize(saved); Pose(reopened,1.2f,action=="idle"?"Idle":"Reload");
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

static bool WorkflowProof(const fs::path& input,const fs::path& output)
{
    const auto project=output/("project-"+GenerateStableId().substr(0,8));
    fs::create_directories(project);
    const auto projectId=GenerateStableId();
    wi::allocator::shared_ptr<wi::scene::Scene> retained[2];
    for(int part=0;part<2;++part) {
        using M=wi::scene::MaterialComponent;
        std::vector<ModelImportTextureRelink> relinks;
        auto add=[&](uint32_t material,uint32_t slot,const std::string& path){
            relinks.push_back({material,slot,(input/path).generic_u8string()});};
        if(part==0) {
            add(0,M::BASECOLORMAP,"Textures/Manny/T_Manny_02_D.PNG");
            add(0,M::NORMALMAP,"Textures/Manny/T_Manny_02_N.PNG");
            add(1,M::BASECOLORMAP,"Textures/Manny/T_Manny_01_D.PNG");
            add(1,M::NORMALMAP,"Textures/Manny/T_Manny_01_N.PNG");
        } else {
            for(uint32_t material=0;material<2;++material) {
                const std::string prefix=material==0?"Meshes/Textures/Weapon/T_Sawed_Off_Shotgun_":
                    "Meshes/Textures/Shell/T_Shell_Red_";
                add(material,M::BASECOLORMAP,prefix+"BaseColor.PNG");
                add(material,M::NORMALMAP,prefix+"Normal.PNG");
                add(material,M::SURFACEMAP,prefix+"OcclusionRoughnessMetallic.PNG");
            }
        }
        const auto idle=input/(part==0?"Animations/AS_Shotgun_Idle.FBX":"Animations/Weapon/AS_Shotgun_Idle.FBX");
        const auto reload=input/(part==0?"Animations/AS_Shotgun_Reload.FBX":"Animations/Weapon/AS_Shotgun_Reload.FBX");
        auto candidate=ModelImportCandidateService().PrepareModel(idle.generic_u8string(),relinks);
        if(!candidate.IsReady()){std::cerr<<"WORKFLOW PREPARE "<<candidate.Error()<<"\n";return false;}
        if(part==0) {
            auto duplicate=relinks;duplicate.push_back(relinks.front());
            if(ModelImportCandidateService().PrepareModel(idle.generic_u8string(),duplicate).IsReady())return false;
            const auto copy=output/"dependency-change.png";
            fs::copy_file(fs::u8path(relinks.front().sourcePath),copy,fs::copy_options::overwrite_existing);
            auto changedRelinks=relinks;changedRelinks.front().sourcePath=copy.generic_u8string();
            auto changed=ModelImportCandidateService().PrepareModel(idle.generic_u8string(),changedRelinks);
            if(!changed.IsReady())return false;
            std::ofstream(copy,std::ios::binary|std::ios::app).put('x');
            ModelImportCommitRequest rejected;rejected.projectRoot=project.generic_u8string();
            rejected.projectId=projectId;rejected.assetName="Changed Dependency";rejected.characterAsset=true;
            const auto failure=ModelImportCommitService().CommitModel(rejected,changed);
            if(failure.succeeded || failure.committed || failure.error.find("changed")==std::string::npos ||
                fs::exists(project/"Content/Models/Changed Dependency.rasset"))return false;
            fs::remove(copy);
            std::cout<<"DUPLICATE RELINK AND CHANGED-DEPENDENCY COMMIT REJECTION PASS\n";
        }
        const auto before=candidate.Evidence();
        std::vector<wi::vector<XMFLOAT4>> originalWeights;
        for(size_t m=0;m<candidate.PeekScene()->meshes.GetCount();++m)
            originalWeights.push_back(candidate.PeekScene()->meshes[m].vertex_boneweights);
        std::string error;
        if(!ModelImportCandidateService().AppendExternalAnimations(candidate,reload.generic_u8string(),error))
        {std::cerr<<"MATCHING CLIP "<<error<<"\n";return false;}
        if(candidate.Summary().animations!=2 || candidate.ExternalAnimations().size()!=1 ||
            !candidate.ExternalAnimations()[0].matchingRig ||
            candidate.Evidence().skinIndexFingerprint!=before.skinIndexFingerprint ||
            candidate.Evidence().inverseBindFingerprint!=before.inverseBindFingerprint)
        {std::cerr<<"Native matching rig altered geometry or used retargeting: clips="<<candidate.Summary().animations<<" exact="<<candidate.ExternalAnimations()[0].matchingRig<<" weights="<<before.skinWeightFingerprint<<"/"<<candidate.Evidence().skinWeightFingerprint<<" binds="<<before.inverseBindFingerprint<<"/"<<candidate.Evidence().inverseBindFingerprint<<"\n";return false;}
        float maxWeightChange=0;
        for(size_t m=0;m<originalWeights.size();++m) {
            const auto& weights=candidate.PeekScene()->meshes[m].vertex_boneweights;
            if(weights.size()!=originalWeights[m].size())return false;
            for(size_t v=0;v<weights.size();++v)
                for(int axis=0;axis<4;++axis)
                    maxWeightChange=std::max(maxWeightChange,std::abs((&weights[v].x)[axis]-(&originalWeights[m][v].x)[axis]));
        }
        std::cout<<"MATCHING RIG "<<part<<" max clone weight normalization="<<maxWeightChange<<"\n";
        if(maxWeightChange>0.000001f)return false;
        ModelImportCommitRequest request;
        request.projectRoot=project.generic_u8string();request.projectId=projectId;
        request.assetName=part==0?"Shotgun Arms":"Shotgun Weapon";
        request.characterAsset=true;request.animationActions={"Idle","Reload"};
        auto result=ModelImportCommitService().CommitModel(request,candidate);
        if(!result.succeeded){std::cerr<<"WORKFLOW COMMIT "<<result.error<<"\n";return false;}
        auto reopened=CreatorAssetWorkflowService().PrepareModelPlacement(request.projectRoot,projectId,result.assetId);
        if(!reopened.IsReady()){std::cerr<<"WORKFLOW REOPEN "<<reopened.Result().error<<"\n";return false;}
        retained[part]=reopened.ReleaseScene();
        if(retained[part]->animations.GetCount()!=2){std::cerr<<"Retained clip count changed\n";return false;}
        ReusableModelAssetDocument document;
        if(!ReadReusableModelAssetDocument((project/result.assetProjectRelativePath).generic_u8string(),document,error))return false;
        CreatorModelImportRecipe recipe;
        if(!ParseCreatorModelImportOptions(nlohmann::json::parse(document.manifest.settingsJson).at("options").dump(),recipe,error) ||
            recipe.textureRelinks.size()!=relinks.size() || recipe.externalAnimations.size()!=1 ||
            !recipe.externalAnimations[0].matchingRig)return false;
        wi::scene::Scene rebuilt;
        ImportModel_FBX((project/result.sourceProjectRelativePath).generic_u8string(),rebuilt);
        if(!ApplyCreatorModelImportRecipe(rebuilt,request.projectRoot,projectId,recipe,error))
        {std::cerr<<"RETAINED RECIPE "<<error<<"\n";return false;}
        if(rebuilt.animations.GetCount()!=2)return false;
        for(size_t i=0;i<rebuilt.materials.GetCount();++i) {
            auto& material=rebuilt.materials[i];
            for(const auto& texture:material.textures)
                if(!texture.name.empty() && (!texture.resource.IsValid() ||
                    !texture.resource.GetTexture().IsValid()))return false;
        }
        if(!Capture(rebuilt,output/("retained-recipe-"+std::to_string(part)+".png")))return false;
        for(size_t i=0;i<retained[part]->materials.GetCount();++i) {
            auto& material=retained[part]->materials[i];
            material.baseColor=XMFLOAT4(1,1,1,1);material.normalMapStrength=1;
            material.roughness=part==0?0.6f:1.0f;
            if(part!=0){material.metalness=1;material.SetOcclusionEnabled_Primary(true);}
            material.SetDirty();
        }
        std::cout<<"GOVERNED PART "<<part<<" matching rig clips retained/rebuilt; asset "<<result.assetId<<"\n";
    }
    wi::scene::Scene idleArms,idleWeapon;
    for(int part=0;part<2;++part) {
        Pose(*retained[part],0,"Idle");
        wi::Archive clone;retained[part]->Serialize(clone);clone.SetReadModeAndResetPos(true);
        (part==0?idleArms:idleWeapon).Serialize(clone);
    }
    if(!Assembly(idleArms,idleWeapon,output,"idle") ||
        !Assembly(*retained[0],*retained[1],output,"reload"))return false;
    std::ofstream(output/"latest-project.txt")<<fs::absolute(project).generic_u8string();
    std::cout<<"GOVERNED TEXTURES, NATIVE MATCHING CLIPS, COMMIT, REOPEN, RETAINED RECIPE AND ASSEMBLY PASS\n";
    return true;
}

static bool ColdWorkflowProof(const fs::path& project,const fs::path& output)
{
    wi::allocator::shared_ptr<wi::scene::Scene> scenes[2];
    for(int part=0;part<2;++part) {
        const auto path=project/"Content/Models"/(part==0?"Shotgun Arms.rasset":"Shotgun Weapon.rasset");
        ReusableModelAssetDocument document;std::string error;
        if(!ReadReusableModelAssetDocument(path.generic_u8string(),document,error))return false;
        auto loaded=CreatorAssetWorkflowService().PrepareModelPlacement(project.generic_u8string(),
            document.manifest.projectId,document.manifest.assetId);
        if(!loaded.IsReady()){std::cerr<<loaded.Result().error<<"\n";return false;}
        scenes[part]=loaded.ReleaseScene();
        if(scenes[part]->animations.GetCount()!=2)return false;
        for(size_t i=0;i<scenes[part]->materials.GetCount();++i) {
            auto& material=scenes[part]->materials[i];
            for(const auto& texture:material.textures) {
                if(texture.name.empty())continue;
                if(!texture.resource.IsValid() || !texture.resource.GetTexture().IsValid() ||
                    texture.name.find(".__renegade_preview")!=std::string::npos)return false;
            }
            material.baseColor=XMFLOAT4(1,1,1,1);material.normalMapStrength=1;
            material.roughness=part==0?0.6f:1.0f;
            if(part!=0){material.metalness=1;material.SetOcclusionEnabled_Primary(true);}
            material.SetDirty();
        }
        Pose(*scenes[part],0,"Idle");
    }
    if(!Assembly(*scenes[0],*scenes[1],output,"reload"))return false;
    std::cout<<"FRESH PROCESS STABLE-ID LOAD, TEXTURES, BOTH CLIP PAIRS AND RELOAD ASSEMBLY PASS\n";
    return true;
}


static bool ProductAssemblyProof(const fs::path& project,const fs::path& output,bool cold) {
 ReusableModelAssetDocument ad,wd;std::string error;
 if(!ReadReusableModelAssetDocument((project/"Content/Models/Shotgun Arms.rasset").generic_u8string(),ad,error)||
 !ReadReusableModelAssetDocument((project/"Content/Models/Shotgun Weapon.rasset").generic_u8string(),wd,error)){
 std::cerr<<error<<"\n";return false;}
 FirstPersonAssemblyService service;FirstPersonAssemblySettings settings;
 StableId asset;
 if(cold){
  std::ifstream f(output.parent_path()/"assembly-id.txt");f>>asset;
  if(!service.ReadSettings(project.generic_u8string(),ad.manifest.projectId,asset,settings,error)){std::cerr<<error<<"\n";return false;}
 }else{
 settings.armsAssetId=ad.manifest.assetId;settings.weaponAssetId=wd.manifest.assetId;
 auto part=ReusableAssetService().PrepareModelAssetPlacement({project.generic_u8string(),ad.manifest.projectId,settings.armsAssetId});
 std::vector<PlayerViewBoneChoice> bones;
 if(!part.IsReady()||!CollectPlayerViewBones(*part.PeekScene(),bones,error))return false;
 for(const auto& b:bones){const auto* n=part.PeekScene()->names.GetComponent(b.entity);if(n&&n->name=="ik_hand_gun")settings.parentBonePath=b.path;}
 settings.weaponPosition={-0.03466970f,0.27336276f,-0.04505738f};
 settings.weaponRotation={-0.059182247f,0.087738050f,0.994176416f,-0.020316230f};
 settings.cameraPosition={0,-1.65f,0.08f};
 settings.cameraRotation={0,0.707106781f,-0.707106781f,0};
 settings.pairs={{"Idle",0,0},{"Reload",1,1}};
 wi::scene::Scene invalid;
 auto bad=settings;bad.parentBonePath="[\"missing\"]";
 if(service.Prepare(project.generic_u8string(),ad.manifest.projectId,bad,invalid,error))return false;
 bad=settings;bad.pairs[0].weaponClip=9999;
 if(service.Prepare(project.generic_u8string(),ad.manifest.projectId,bad,invalid,error))return false;
 bad=settings;bad.weaponRotation.w=5;
 if(service.Prepare(project.generic_u8string(),ad.manifest.projectId,bad,invalid,error))return false;
 if(!service.Save(project.generic_u8string(),ad.manifest.projectId,"Shotgun Assembly "+GenerateStableId().substr(0,8),settings,{},asset,error)){
 std::cerr<<error<<"\n";return false;}
 std::ofstream(output/"assembly-id.txt")<<asset;
 }
 auto product=ReusableAssetService().PrepareModelAssetPlacement({project.generic_u8string(),ad.manifest.projectId,asset});
 if(!product.IsReady()){std::cerr<<product.Result().error<<"\n";return false;}
 auto& scene=*product.PeekMutableScene();
 if(scene.armatures.GetCount()!=2||scene.animations.GetCount()!=4)return false;
 size_t retainedTextures=0;
 for(size_t m=0;m<scene.materials.GetCount();++m)for(const auto& t:scene.materials[m].textures){
  if(t.name.empty())continue;
  std::cout<<"ASSEMBLY TEXTURE "<<t.name<<" valid="<<t.resource.IsValid()<<"\n";
  if(!t.resource.IsValid()||!t.resource.GetTexture().IsValid())return false;
 ++retainedTextures;
 }
 if(retainedTextures!=10)return false;
 FirstPersonAssemblySettings reopened;
 if(!service.ReadSettings(project.generic_u8string(),ad.manifest.projectId,asset,reopened,error))return false;
 std::string a,b;
 if(!SerializeFirstPersonAssemblySettings(settings,a,error)||!SerializeFirstPersonAssemblySettings(reopened,b,error)||a!=b)return false;
 for(const auto* action:{"Idle","Reload"})for(float t:{0.0f,0.6f,1.2f,2.0f,2.9f}){
 if(!service.Pose(scene,action,t,error))return false;
 if(!Capture(scene,output/(std::string(action)+"-"+std::to_string(t)+".png"),true,true))return false;
 }
 if(!cold) {
 wi::scene::Scene level;
 CreatePlayerStartCommand create(level,{});
 if(!create.Execute())return false;
 auto playerSettings=CapturePlayerControllerSettings(level,create.CreatedEntity());
 playerSettings.firstPersonArmsAssetId=asset;
 SetPlayerControllerSettingsCommand assign(level,create.CreatedEntity(),playerSettings);
 if(!assign.Execute())return false;
 fs::create_directories(project/"Content/Scenes");
 auto scenePath=project/"Content/Scenes/AssemblyProof.wiscene";
 wi::Archive archive(scenePath.generic_u8string(),false,false);level.Serialize(archive);
 if(!archive.SaveFile(scenePath.generic_u8string()))return false;archive=wi::Archive();
 wi::scene::Scene reopened;wi::Archive read(scenePath.generic_u8string(),true,false);reopened.Serialize(read);
 auto start=ResolvePlayerStart(reopened);
 if(start.resolution!=PlayerStartResolution::Success||start.start.settings.firstPersonArmsAssetId!=asset)return false;
 std::ofstream descriptor(project/"AssemblyProof.renegade");
 descriptor<<"format = renegade-project\nversion = 1\n\n[project]\nproject_id = "<<ad.manifest.projectId
 <<"\nname = Assembly Proof\nstartup_scene = Content/Scenes/AssemblyProof.wiscene\n";
 }
 std::cout<<"GOVERNED ASSEMBLY SAVE, EXACT RECIPE REOPEN, BOTH RIGS AND PAIRED CAMERA PREVIEW PASS "<<asset<<"\n";
 return true;
}

static bool RuntimeAssemblyProof(const fs::path& input, const fs::path& output, bool packaged)
{
 using namespace renegade::runtime;
 std::string error; StableId projectId, assetId;
 fs::path root=input;
 if(packaged) {
  std::ifstream manifest(input/"GameData/content-manifest.json");
  nlohmann::json j; manifest>>j;
  projectId=j.at("project_id").get<std::string>();
  assetId=j.at("files")[0].at("asset_id").get<std::string>();
 } else {
  ProjectMetadata project;
  if(!ProjectService().InspectProject(fs::absolute(input).generic_u8string(),project,error)) {
   std::cerr<<error<<"\n";return false;
  }
  root=fs::u8path(project.rootPath);projectId=project.projectId;
  SceneService scenes;
  if(!scenes.LoadScene((root/fs::u8path(project.startupScene)).generic_u8string()))return false;
  auto start=ResolvePlayerStart(scenes.GetScene());
  if(start.resolution!=PlayerStartResolution::Success)return false;
  assetId=start.start.settings.firstPersonArmsAssetId;
  CommandService commands;TestLevelSnapshotService snapshots(scenes,commands);TestLevelSnapshot snapshot;
  if(!snapshots.Create(project,snapshot,error)){std::cerr<<error<<"\n";return false;}
  SceneService reopened;
  if(!reopened.LoadScene(snapshot.scenePath))return false;
  auto copied=ResolvePlayerStart(reopened.GetScene());
  if(copied.resolution!=PlayerStartResolution::Success||copied.start.settings.firstPersonArmsAssetId!=assetId)return false;
  std::ofstream(output/"snapshot-descriptor.txt")<<snapshot.descriptorPath;
  std::cout<<"SNAPSHOT_DESCRIPTOR="<<snapshot.descriptorPath<<"\n";
  auto product=ReusableAssetService().PrepareModelAssetPlacement({project.rootPath,projectId,assetId});
  if(!product.IsReady()){std::cerr<<product.Result().error<<"\n";return false;}
  const auto relative=product.Result().assetProjectRelativePath;
  auto package=output/"isolated-package";
  fs::create_directories(package/"GameData"/fs::u8path(relative).parent_path());
  fs::copy_file(root/fs::u8path(relative),package/"GameData"/fs::u8path(relative),fs::copy_options::overwrite_existing);
  std::ifstream data(root/fs::u8path(relative),std::ios::binary);
  std::uint64_t hash=1469598103934665603ull;for(char c;data.get(c);){hash^=static_cast<unsigned char>(c);hash*=1099511628211ull;}
  std::ostringstream digest;digest<<"fnv1a64:"<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;
  nlohmann::json manifest={{"format","renegade-content-manifest"},{"schema_version",1},{"project_id",projectId},
   {"files",nlohmann::json::array({{{"asset_id",assetId},{"path","GameData/"+relative},{"source_hash",digest.str()}}})}};
  std::ofstream(package/"GameData/content-manifest.json")<<manifest.dump();
 }
 wi::scene::Scene scene;const auto player=scene.Entity_CreateTransform("Runtime proof player");
 scene.transforms.GetComponent(player)->Translate(XMFLOAT3(0,-1.65f,0));scene.transforms.GetComponent(player)->UpdateTransform();
 RuntimePlayerViewRigState rig;RuntimePlayerViewRigSettings rigSettings;rigSettings.createProofGeometry=false;
 if(!SpawnRuntimePlayerViewRig(scene,rig,player,1.65f,error,rigSettings))return false;
 bool loaded=packaged?LoadPackagedRuntimePlayerViewAsset(scene,rig,root.generic_u8string(),projectId,assetId,error):
  LoadRuntimePlayerViewAsset(scene,rig,root.generic_u8string(),projectId,assetId,error);
 if(!loaded){std::cerr<<error<<"\n";return false;}
 RuntimePlayerViewAnimationState animation;
 if(!InitializeRuntimePlayerViewAnimations(scene,rig,animation,error)||!animation.pairedAssembly) {
  std::cerr<<"Paired initialization: "<<error<<"\n";return false;
 }
 if(scene.armatures.GetCount()!=2||scene.characters.GetCount()!=0||scene.rigidbodies.GetCount()!=0)return false;
 for(float dt:{0.0f,0.35f,0.75f,6.5f}) {
  UpdateRuntimePlayerViewAnimations(scene,animation,PlayerViewAction::Walk,dt);
  scene.Update(1.0f/60);
  if(animation.activeWeaponClip==wi::ecs::INVALID_ENTITY||
   scene.animations.GetComponent(animation.activeClip)->amount!=1||
   scene.animations.GetComponent(animation.activeWeaponClip)->amount!=1)return false;
  if(!Capture(scene,output/("paired-"+std::to_string(animation.pairedTime)+".png"),true,true))return false;
 }
 auto t=animation.pairedTime;const auto previousClip=animation.activeClip;
 UpdateRuntimePlayerViewAnimations(scene,animation,PlayerViewAction::Sprint,0);
 if(previousClip==animation.activeClip?t!=animation.pairedTime:animation.pairedTime!=0)return false;
 ResetRuntimePlayerViewAnimations(scene,animation);DespawnRuntimePlayerViewRig(scene,rig);
 if(scene.transforms.GetCount()!=1)return false;
 std::cout<<"REAL ASSEMBLY "<<(packaged?"ISOLATED PACKAGED":"PROJECT AND TEST SNAPSHOT")
 <<" PAIRED RUNTIME LOAD, POSE, PAUSE AND CLEANUP PASS "<<assetId<<"\n";
 return true;
}


// Run only against a disposable project copy, never the owner's live assets.
static bool UpdateAssemblyProof(const fs::path& descriptor,const fs::path& output)
{
 std::string error;ProjectMetadata project;
 if(!ProjectService().InspectProject(fs::absolute(descriptor).generic_u8string(),project,error))return false;
 const fs::path root=fs::u8path(project.rootPath);
 SceneService scenes;if(!scenes.LoadScene((root/fs::u8path(project.startupScene)).generic_u8string()))return false;
 auto start=ResolvePlayerStart(scenes.GetScene());
 if(start.resolution!=PlayerStartResolution::Success)return false;
 const auto asset=start.start.settings.firstPersonArmsAssetId;
 AssetRegistry before;if(!CreatorAssetWorkflowService().RefreshRegistryFromDisk(project.rootPath,project.projectId,before,error))return false;
 const auto record=std::find_if(before.records.begin(),before.records.end(),[&](const auto& r){return r.assetId==asset;});
 if(record==before.records.end())return false;
 const auto expectedHash=record->contentHash;
 FirstPersonAssemblyService service;FirstPersonAssemblySettings original,changed,reopened;
 if(!service.ReadSettings(project.rootPath,project.projectId,asset,original,error))return false;
 changed=original;changed.weaponPosition.x+=0.005f;changed.cameraPosition.z+=0.01f;
 auto bytes=[](const fs::path& path){std::ifstream f(path,std::ios::binary);
 return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f),{});};
 std::vector<std::pair<fs::path,std::vector<std::uint8_t>>> untouched;
 for(const auto& r:before.records)if(r.assetId==asset||
 std::any_of(before.importedProducts.begin(),before.importedProducts.end(),[&](const auto& p){
 return p.productAssetId==asset&&p.sourceAssetId==r.assetId;}))
 untouched.push_back({root/fs::u8path(r.projectRelativePath),bytes(root/fs::u8path(r.projectRelativePath))});
 for(const auto& path:{root/AssetRegistryDocumentName,root/AssetCatalogueMetadataDocumentName,
 root/fs::u8path(ResolveReusableModelManagedProjectionPath(record->projectRelativePath))})
 untouched.push_back({path,bytes(path)});
 const auto levelPath=root/fs::u8path(project.startupScene);
 const auto levelBytes=bytes(levelPath);
 auto fail=[](ProjectDocumentTransactionStage stage,size_t,const std::string&,std::string& e){
 if(stage==ProjectDocumentTransactionStage::AfterReplace){e="Injected assembly replacement failure";return ProjectDocumentTransactionHookAction::Fail;}
 return ProjectDocumentTransactionHookAction::Continue;};
 if(service.Update(project.rootPath,project.projectId,asset,expectedHash,changed,{},error,fail)||
 error.find("Injected assembly replacement failure")==std::string::npos)return false;
 for(const auto& entry:untouched)if(bytes(entry.first)!=entry.second){std::cerr<<"Assembly rollback changed "<<entry.first<<"\n";return false;}
 if(!service.Update(project.rootPath,project.projectId,asset,expectedHash,changed,{},error)){std::cerr<<error<<"\n";return false;}
 if(!service.ReadSettings(project.rootPath,project.projectId,asset,reopened,error))return false;
 std::string wanted,actual;
 if(!SerializeFirstPersonAssemblySettings(changed,wanted,error)||
 !SerializeFirstPersonAssemblySettings(reopened,actual,error)||wanted!=actual)return false;
 AssetRegistry after;if(!ReadAssetRegistry(project.rootPath,project.projectId,after,error))return false;
 if(after.records.size()!=before.records.size()||after.importedProducts.size()!=before.importedProducts.size())return false;
 const auto oldProduct=std::find_if(before.importedProducts.begin(),before.importedProducts.end(),[&](const auto& p){return p.productAssetId==asset;});
 const auto newProduct=std::find_if(after.importedProducts.begin(),after.importedProducts.end(),[&](const auto& p){return p.productAssetId==asset;});
 if(oldProduct==before.importedProducts.end()||newProduct==after.importedProducts.end()||
 oldProduct->sourceAssetId!=newProduct->sourceAssetId)return false;
 if(service.Update(project.rootPath,project.projectId,asset,expectedHash,original,{},error))return false;
 if(bytes(levelPath)!=levelBytes||CapturePlayerControllerSettings(scenes.GetScene(),start.start.entity).firstPersonArmsAssetId!=asset)return false;
 StableId copy;
 if(!service.Save(project.rootPath,project.projectId,"Assembly lifecycle variant",changed,{},copy,error)||
 copy==asset||bytes(levelPath)!=levelBytes)return false;
 wi::scene::Scene rendered;if(!service.Prepare(project.rootPath,project.projectId,reopened,rendered,error)||
 !Capture(rendered,output/"updated-assembly.png",true,true))return false;
 std::cout<<"UPDATE SAME ID, RECIPE ID, EXACT REOPEN, STALE REJECTION, ROLLBACK AND SAVE AS NEW PASS\n";
 return RuntimeAssemblyProof(descriptor,output,false);
}

static bool FullLibraryProof(const fs::path& input,const fs::path& project)
{
 fs::create_directories(project);
 const auto projectId=GenerateStableId();
 const std::array<const char*,14> armsFiles={"Idle","Reload","Walk","Sprint","Fire","Draw",
 "Holster","ADS_In","ADS_Out","ADS_Fire","Jump_Start","Jump_Loop","Jump_Land","Reload_Partial"};
 const std::array<const char*,4> weaponFiles={"Idle","Reload","Fire","Partial"};
 StableId ids[2];std::string error;
 for(int part=0;part<2;++part) {
 using M=wi::scene::MaterialComponent;
 std::vector<ModelImportTextureRelink> relinks;
 auto add=[&](uint32_t material,uint32_t slot,const std::string& path){
 relinks.push_back({material,slot,(input/path).generic_u8string()});};
 if(part==0) {
 add(0,M::BASECOLORMAP,"Textures/Manny/T_Manny_02_D.PNG");
 add(0,M::NORMALMAP,"Textures/Manny/T_Manny_02_N.PNG");
 add(1,M::BASECOLORMAP,"Textures/Manny/T_Manny_01_D.PNG");
 add(1,M::NORMALMAP,"Textures/Manny/T_Manny_01_N.PNG");
 } else for(uint32_t m=0;m<2;++m) {
 const std::string prefix=m==0?"Meshes/Textures/Weapon/T_Sawed_Off_Shotgun_":"Meshes/Textures/Shell/T_Shell_Red_";
 add(m,M::BASECOLORMAP,prefix+"BaseColor.PNG");add(m,M::NORMALMAP,prefix+"Normal.PNG");
 add(m,M::SURFACEMAP,prefix+"OcclusionRoughnessMetallic.PNG");
 }
 const std::string folder=part==0?"Animations/":"Animations/Weapon/";
 auto candidate=ModelImportCandidateService().PrepareModel((input/(folder+"AS_Shotgun_Idle.FBX")).generic_u8string(),relinks);
 if(!candidate.IsReady()){std::cerr<<candidate.Error()<<"\n";return false;}
 const auto before=candidate.Evidence();
 const size_t count=part==0?armsFiles.size():weaponFiles.size();
 for(size_t i=1;i<count;++i) {
 const std::string clip=part==0?armsFiles[i]:weaponFiles[i];
 if(!ModelImportCandidateService().AppendExternalAnimations(candidate,(input/(folder+"AS_Shotgun_"+clip+".FBX")).generic_u8string(),error)){
 std::cerr<<"Full library "<<clip<<": "<<error<<"\n";return false;}
 }
 if(candidate.Summary().animations!=count||candidate.ExternalAnimations().size()!=count-1||
 candidate.Evidence().skinIndexFingerprint!=before.skinIndexFingerprint||
 candidate.Evidence().inverseBindFingerprint!=before.inverseBindFingerprint||
 std::any_of(candidate.ExternalAnimations().begin(),candidate.ExternalAnimations().end(),[](const auto& a){return !a.matchingRig;}))return false;
 // Preserve descriptive authored names rather than the FBX-wide "Unreal Take".
 auto* native=candidate.PeekMutableScene();
 for(size_t i=0;i<count;++i)native->names.Create(native->animations.GetEntity(i)).name=
 std::string("Shotgun / ")+(part==0?armsFiles[i]:weaponFiles[i]);
 ModelImportCommitRequest request;request.projectRoot=project.generic_u8string();request.projectId=projectId;
 request.assetName=part==0?"Shotgun Arms Full Library":"Shotgun Weapon Full Library";request.characterAsset=true;
 auto saved=ModelImportCommitService().CommitModel(request,candidate);
 if(!saved.succeeded){std::cerr<<saved.error<<"\n";return false;}
 ids[part]=saved.assetId;
 auto reopened=CreatorAssetWorkflowService().PrepareModelPlacement(request.projectRoot,projectId,saved.assetId);
 if(!reopened.IsReady()||reopened.PeekScene()->animations.GetCount()!=count)return false;
 // Rebuild every external clip from retained project-owned sources.
 ReusableModelAssetDocument document;CreatorModelImportRecipe recipe;
 if(!ReadReusableModelAssetDocument((project/fs::u8path(saved.assetProjectRelativePath)).generic_u8string(),document,error)||
 !ParseCreatorModelImportOptions(nlohmann::json::parse(document.manifest.settingsJson).at("options").dump(),recipe,error))return false;
 wi::scene::Scene rebuilt;ImportModel_FBX((project/fs::u8path(saved.sourceProjectRelativePath)).generic_u8string(),rebuilt);
 if(!ApplyCreatorModelImportRecipe(rebuilt,request.projectRoot,projectId,recipe,error)||rebuilt.animations.GetCount()!=count){
 std::cerr<<"Full retained rebuild "<<error<<"\n";return false;}
 std::cout<<"FULL RETAINED PART "<<part<<" clips="<<count<<" matching-rig and retained-source rebuild PASS\n";
 }
 FirstPersonAssemblySettings settings;
 settings.armsAssetId=ids[0];settings.weaponAssetId=ids[1];
 settings.parentBonePath="[\"AS_Shotgun_Idle.FBX\",\"root\",\"ik_hand_root\",\"ik_hand_gun\"]";
 settings.weaponPosition={-0.03466970f,0.27336276f,-0.04505738f};
 settings.weaponRotation={-0.059182247f,0.087738050f,0.994176416f,-0.020316230f};
 settings.cameraPosition={0,-1.65f,0.08f};settings.cameraRotation={0,0.707106781f,-0.707106781f,0};
 const unsigned weaponIndices[]={0,1,0,0,2,0,0,0,0,2,0,0,0,3};
 for(unsigned i=0;i<FirstPersonAssemblyActions.size();++i)
 settings.pairs.push_back({FirstPersonAssemblyActions[i],i,weaponIndices[i]});
 FirstPersonAssemblyService service;StableId asset;
 if(!service.Save(project.generic_u8string(),projectId,"Shotgun Full Library",settings,{},asset,error)){
 std::cerr<<error<<"\n";return false;}
 wi::scene::Scene level;
 if(fs::is_regular_file(project/"Template.wiscene")) {
 wi::Archive archive((project/"Template.wiscene").generic_u8string(),true,false);level.Serialize(archive);
 }
 auto start=ResolvePlayerStart(level);wi::ecs::Entity player;
 if(start.resolution==PlayerStartResolution::Success)player=start.start.entity;
 else {CreatePlayerStartCommand create(level,{});if(!create.Execute())return false;player=create.CreatedEntity();}
 auto playerSettings=CapturePlayerControllerSettings(level,player);playerSettings.firstPersonArmsAssetId=asset;
 if(!SetPlayerControllerSettingsCommand(level,player,playerSettings).Execute())return false;
 fs::create_directories(project/"Content/Scenes");
 const auto scenePath=project/"Content/Scenes/ArmsLibrary.wiscene";
 wi::Archive archive(scenePath.generic_u8string(),false,false);level.Serialize(archive);
 if(!archive.SaveFile(scenePath.generic_u8string()))return false;archive=wi::Archive();
 std::ofstream descriptor(project/"ArmsLibrary.renegade");
 descriptor<<"format = renegade-project\nversion = 1\n\n[project]\nproject_id = "<<projectId
 <<"\nname = Shotgun Full Library\nstartup_scene = Content/Scenes/ArmsLibrary.wiscene\n";
 descriptor.close();
 std::ofstream(project/"assembly-id.txt")<<asset;
 std::cout<<"FULL LIBRARY ASSEMBLY SAVED "<<asset<<"\n";
 return true;
}

static bool FullLibraryReopenProof(const fs::path& descriptor,const fs::path& output)
{
 ProjectMetadata project;std::string error;
 if(!ProjectService().InspectProject(fs::absolute(descriptor).generic_u8string(),project,error))return false;
 SceneService scenes;if(!scenes.LoadScene((fs::u8path(project.rootPath)/fs::u8path(project.startupScene)).generic_u8string()))return false;
 const auto start=ResolvePlayerStart(scenes.GetScene());if(start.resolution!=PlayerStartResolution::Success)return false;
 FirstPersonAssemblyService service;FirstPersonAssemblySettings settings;
 if(!service.ReadSettings(project.rootPath,project.projectId,start.start.settings.firstPersonArmsAssetId,settings,error)||
 settings.pairs.size()!=14)return false;
 wi::scene::Scene assembly;if(!service.Prepare(project.rootPath,project.projectId,settings,assembly,error))return false;
 if(assembly.armatures.GetCount()!=2||assembly.animations.GetCount()!=28)return false;
 unsigned textures=0;
 for(size_t m=0;m<assembly.materials.GetCount();++m)for(const auto& t:assembly.materials[m].textures)
 if(!t.name.empty()){if(!t.resource.IsValid()||!t.resource.GetTexture().IsValid())return false;++textures;}
 if(textures!=10)return false;
 for(const auto& pair:settings.pairs) {
 float duration=0;
 for(size_t i=0;i<assembly.animations.GetCount();++i) {
 const auto* metadata=assembly.metadatas.GetComponent(assembly.animations.GetEntity(i));
 if(metadata&&metadata->string_values.has(CreatorCharacterAnimationActionMetadataKey)&&
 metadata->string_values.get(CreatorCharacterAnimationActionMetadataKey)==pair.action)
 duration=std::max(duration,assembly.animations[i].end-assembly.animations[i].start);
 }
 for(float fraction:{0.0f,0.5f,0.95f}) {
 if(!service.Pose(assembly,pair.action,duration*fraction,error)||
 !Capture(assembly,output/(pair.action+"-"+std::to_string(fraction)+".png"),true,true,
 pair.action=="Equip"||pair.action=="Unequip"))return false;
 }
 std::cout<<"FULL PAIRED PREVIEW "<<pair.action<<" duration="<<duration<<" PASS\n";
 }
 std::cout<<"FULL LIBRARY COLD REOPEN: 14 pairs, 28 tracks, 2 armatures, 10 textures PASS\n";
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
    if(argc==4 && std::string(argv[3])=="--full-library") return FullLibraryProof(input,output)?0:15;
    if(argc==4 && std::string(argv[3])=="--full-library-reopen") return FullLibraryReopenProof(input,output)?0:16;
    if(argc==4 && std::string(argv[3])=="--update-assembly") return UpdateAssemblyProof(input,output)?0:14;
    if(argc==4 && std::string(argv[3])=="--runtime-assembly") return RuntimeAssemblyProof(input,output,false)?0:12;
    if(argc==4 && std::string(argv[3])=="--runtime-package") return RuntimeAssemblyProof(input,output,true)?0:13;
    if(argc==4 && std::string(argv[3])=="--assembly") return ProductAssemblyProof(input,output,false)?0:10;
    if(argc==4 && std::string(argv[3])=="--assembly-reopen") return ProductAssemblyProof(input,output,true)?0:11;
    if(argc==4 && std::string(argv[3])=="--workflow-reopen") return ColdWorkflowProof(input,output)?0:9;
    if(argc==4 && std::string(argv[3])=="--workflow") return WorkflowProof(input,output)?0:8;
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