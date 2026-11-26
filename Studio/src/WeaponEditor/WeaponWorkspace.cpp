#include "WeaponWorkspace.h"
#include "renegade/bridge/MaterialTextureCook.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/EquipmentAssetService.h"
#include "renegade/bridge/ProjectileAssetService.h"
#include "renegade/bridge/ProjectileEffectRuntime.h"
#include "renegade/bridge/ModelImportCommitService.h"
#include "renegade/bridge/FirstPersonAssemblyService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/WeaponLoadout.h"
#include "../StudioTheme.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/ModelImportCandidateService.h"
#include "WickedEngine.h"
#include "ModelImporter.h"
#include "renegade/bridge/LaunchSocketService.h"
#include "Translator.h"
#include "json.hpp"
#include "renegade/bridge/WeaponSurface.h"
#include "renegade/bridge/WeaponParticlePlayback.h"
#include "renegade/bridge/MeleeWeaponSettings.h"
#include <commdlg.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <array>
#include <cwctype>
#include <optional>
#include <set>
namespace renegade::studio::weapons {
using namespace DirectX;
using namespace wi::scene;
using wi::ecs::Entity;
using json=nlohmann::json;
namespace fs=std::filesystem;
#include "ArmFit.h"
#include "BoltContact.h"
#include "FingerFit.h"
#include "ContactAudit.h"
HWND editorWindow;
namespace { void saveNativePart(Scene&,const fs::path&); }
fs::path home, projectRoot, requestedModel, presetFile, requestedSetup, requestedArms;
bool enterRequested = false, leaveRequested = false;
static fs::path choose(bool save,const wchar_t* filter,const wchar_t* ext,const fs::path& dialogProject=projectRoot){
    wchar_t path[32768]={};
    OPENFILENAMEW d={sizeof(d)};
    d.hwndOwner=editorWindow; d.lpstrFile=path; d.nMaxFile=32768; d.lpstrFilter=filter; d.lpstrDefExt=ext;
    auto folder = dialogProject / "Content/Player/Weapons";
    if(ext && (std::wstring(ext)==L"weapon"||std::wstring(ext)==L"weaponprefab") && !dialogProject.empty()){fs::create_directories(folder); d.lpstrInitialDir=folder.c_str();}
    d.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    return (save?GetSaveFileNameW(&d):GetOpenFileNameW(&d))?fs::path(path):fs::path();
}
static void readModel(const fs::path& p, Scene& result){
    auto ext=p.extension().wstring();
    std::transform(ext.begin(),ext.end(),ext.begin(),[](wchar_t c){return wchar_t(std::towlower(c));});
    if(ext==L".wiscene"){wi::Archive archive(p.u8string(),true);if(!archive.IsOpen())throw std::runtime_error("Cannot open prefab model.");result.Serialize(archive);return;}
    if(ext==L".rasset"){
        bridge::ReusableModelAssetDocument asset;
        std::string error;
        if(!bridge::ReadReusableModelAssetDocument(p.u8string(),asset,error))throw std::runtime_error(error);
        auto placement=bridge::ReusableAssetService{}.PrepareModelAssetPlacement({projectRoot.u8string(),asset.manifest.projectId,asset.manifest.assetId});
        if(!placement.IsReady())throw std::runtime_error(placement.Result().error);
        result.Merge(*placement.PeekMutableScene());
        return;
    }
    if(ext==L".glb" || ext==L".gltf"){ImportModel_GLTF(p.u8string(),result);return;}
    auto imported=renegade::bridge::ModelImportCandidateService{}.PrepareModel(p.u8string());
    if(!imported.IsReady())throw std::runtime_error(imported.Error());
    result.Merge(*imported.PeekMutableScene());
}
struct Preview:wi::RenderPath3D {
    bool assembled=false;
    json assembledActions=json::object();
    wi::gui::ComboBox actionRole,actionSource;
    static std::string clipKey(std::string name){
        name=name.substr(name.find_last_of('|')+1);
        std::string key;
        for(unsigned char c:name)if(std::isalnum(c))key+=char(std::tolower(c));
        return key;
    }
    void createActionMapping(){
        actionRole.Create("Gameplay action");actionSource.Create("Source animation");
        for(auto role:renegade::bridge::FirstPersonAssemblyActions)actionRole.AddItem(role);
        actionRole.AddItem("FireStart");actionRole.AddItem("FireStop");
        actionSource.AddItem("(Unassigned)");
        for(auto& clip:preset.at("clips")){
            auto source=clip.at("source").get<std::string>();actionSource.AddItem(source);
            auto key=clipKey(source);
            if(key=="fire"||key=="fireloop")key="attack";
            if(key=="sprint")key="run";
            for(auto role:renegade::bridge::FirstPersonAssemblyActions)
                if(clipKey(role)==key&&!assembledActions.contains(role))assembledActions[role]=source;
            for(auto role:{"FireStart","FireStop"})if(clipKey(role)==key)assembledActions[role]=source;
        }
        actionRole.OnSelect([this](auto& a){
            auto source=assembledActions.value(actionRole.GetItemText(a.iValue),std::string{});
            int index=0;
            for(int i=1;i<int(actionSource.GetItemCount());++i)if(actionSource.GetItemText(i)==source)index=i;
            actionSource.SetSelectedWithoutCallback(index);
        });
        actionSource.OnSelect([this](auto& a){
            auto role=actionRole.GetItemText(actionRole.GetSelected());
            if(a.iValue)assembledActions[role]=actionSource.GetItemText(a.iValue);
            else assembledActions.erase(role);
        });
        actionRole.SetSelected(0);
        GetGUI().AddWidget(&actionRole);GetGUI().AddWidget(&actionSource);
    }
    wi::gui::Button backToStudio;
    CameraComponent workspaceCamera;
    Scene arms,prop,placement; json preset;
    std::string fittedMountBone;
    WeaponTranslator gizmo;
    ArmFit armFit;
    BoltContact boltContact;
#include "SupportGrip.h"
#include "SupportGripTimeline.h"
#include "FingerPanel.h"
#include "PosePanel.h"
#include "AuditPanel.h"
#include "EditorHelp.h"
#include "Materials.h"
#include "CleanUI.h"
#include "WeaponMarkers.h"
#include "CasingPreview.h"
#include "MuzzleFlash.h"
#include "FiringPanel.h"
#include "MeleePanel.h"
#include "FreshStart.h"
    wi::gui::Label boltCaption,boltGrabLabel,boltReleaseLabel,reloadStage;
    wi::gui::Button boltGrabHere,boltReleaseHere;
    wi::gui::TextInputField boltGrabInput,boltReleaseInput;
    bool pendingBoltGrab=false,pendingBoltRelease=false;
    float posePhase() const {
        if(!arms.animations.GetCount())return 0;
        auto& a=arms.animations[anim];float time=a.last_update_time>=a.start?a.last_update_time:a.timer;
        return std::clamp((time-a.start)/std::max(.0001f,a.end-a.start),0.f,1.f);
    }
    void orderBoltAfterSeat(){
        if(seatPhase>=boltContact.grab){
            boltContact.captured=false;
            boltContact.grab=seatPhase+(1-seatPhase)*.35f;boltContact.release=seatPhase+(1-seatPhase)*.8f;
            boltGrabInput.SetValue(boltContact.grab*100);boltReleaseInput.SetValue(boltContact.release*100);
        }
    }
    void boltTiming(float grab,float release){
        if(!std::isfinite(grab)||!std::isfinite(release)||grab<seatPhase||grab>=release||release>=1){
            boltGrabInput.SetValue(boltContact.grab*100);boltReleaseInput.SetValue(boltContact.release*100);
            status.SetText("Bolt grab follows magazine seating; release follows grab before the clip ends.");return;
        }
        rememberFit();boltContact.grab=grab;boltContact.release=release;
        boltGrabInput.SetValue(grab*100);boltReleaseInput.SetValue(release*100);
    }
    void requestBolt(bool grab){
        if(!boltContact.bone||!boltContact.hasRail){status.SetText("This weapon needs a bolt with a sliding animation.");return;}
        if(currentClip()!="Reload"){status.SetText("Select Reload to author the bolt contact.");return;}
        if(clipPhase()<seatPhase){status.SetText("Seat the magazine before grabbing the bolt.");return;}
        if(grab&&clipPhase()>=boltContact.release){status.SetText("Choose bolt contact before the release event.");return;}
        if(!grab&&(clipPhase()<=boltContact.grab||clipPhase()>=1)){status.SetText("Release the bolt after grabbing it, before the clip ends.");return;}
        pauseForFit();if(grab)pendingBoltGrab=true;else pendingBoltRelease=true;
    }
    void boltActions(){
        if(pendingBoltGrab){
            pendingBoltGrab=false;
            if(magazineHeld){status.SetText("Seat the magazine before grabbing the bolt.");return;}
            if(boltContact.stroke(clipPhase())>.02f){status.SetText("Choose the forward bolt pose before marking its grab.");return;}
            rememberFit();boltContact.grab=clipPhase();
            boltContact.capture(XMLoadFloat4x4(&arms.transforms.GetComponent(supportHandEntity)->world));
            boltGrabInput.SetValue(boltContact.grab*100);
            status.SetText("Bolt grip captured. The support hand will follow the charging handle.");
        }
        if(pendingBoltRelease){
            pendingBoltRelease=false;rememberFit();boltContact.release=clipPhase();boltReleaseInput.SetValue(boltContact.release*100);
            status.SetText("Bolt release recorded. The hand returns to the foregrip.");
        }
    }
    void showReloadStage(float phase){
        std::string stage="Select Reload to author contacts.";
        if(currentClip()=="Reload"){
            if(phase<pickupPhase)stage="Reach for magazine";
            else if(magazineHeld)stage="Magazine follows support hand";
            else if(phase<boltContact.grab)stage="Magazine seated / reach bolt";
            else if(phase<boltContact.release)stage=boltContact.captured?"Hand pulls bolt on its rail":"Set the bolt grip contact";
            else stage="Release bolt / return to foregrip";
        }
        reloadStage.SetText(stage);
    }
    int fitSelection=0; // 0 weapon, 1/2 hands, 3/4 poles, 5 magazine
    wi::gui::Label armPanel,armHint,eventLabel,pickupLabel,seatLabel;
    wi::gui::Button armSelect[5],armReset,armFrame,pickupHere,seatHere,grabHere,seatNow;
    bool pendingGrab=false,pendingGrabPickup=false,pendingSeat=false,pendingSeatEvent=false,pendingFitMagazine=false;
    void requestGrab(bool requireReload=false){
        if(!magazineBone||editing){status.SetText("Preview a weapon with a magazine first.");return;}
        if(requireReload&&currentClip()!="Reload"){status.SetText("Select Reload and scrub to the contact pose.");return;}
        if(requireReload&&clipPhase()>=seatPhase){status.SetText("Pickup must be before the reseat event.");return;}
        pauseForFit();pendingGrab=true;pendingGrabPickup=currentClip()=="Reload"&&clipPhase()<seatPhase;
    }
    void requestSeat(bool markEvent){
        if(!magazineBone||editing)return;
        if(markEvent&&(currentClip()!="Reload"||clipPhase()<=pickupPhase||clipPhase()>=1)){status.SetText("Choose a Reload pose after pickup.");return;}
        pauseForFit();pendingSeat=true;pendingSeatEvent=markEvent;
    }
    bool magazineActions(){
        bool changed=false;
        if(pendingGrab){
            pendingGrab=false;rememberFit();armFit.hierarchy();
            auto mag=arms.transforms.GetComponent(magazineBone),hand=arms.transforms.GetComponent(supportHandEntity);
            if(mag&&hand){
                // Capture the final visible pose after animation and arm IK.
                XMStoreFloat4x4(&magazineHandOffset,XMLoadFloat4x4(&mag->world)*XMMatrixInverse(nullptr,XMLoadFloat4x4(&hand->world)));
                magazineOffsetValid=true;
                if(pendingGrabPickup){pickupPhase=clipPhase();pickupInput.SetValue(pickupPhase*100);magazineMode=2;}
                else magazineMode=1;
                magazineControl.SetSelectedWithoutCallback(magazineMode);attachMagazine(true);
                mag=arms.transforms.GetComponent(magazineBone);mag->ClearTransform();mag->MatrixTransform(XMLoadFloat4x4(&magazineHandOffset));
                armFit.hierarchy();status.SetText("Magazine grabbed at this pose. It now follows the support hand.");changed=true;
                if(pendingFitMagazine){pendingFitMagazine=false;selectFit(5);}
            }
        }
        if(pendingSeat){
            pendingSeat=false;rememberFit();
            if(pendingSeatEvent){seatPhase=clipPhase();seatInput.SetValue(seatPhase*100);magazineMode=2;orderBoltAfterSeat();}else magazineMode=0;
            magazineControl.SetSelectedWithoutCallback(magazineMode);attachMagazine(false);armFit.hierarchy();
            status.SetText("Magazine seated in the weapon.");changed=true;
        }
        magazineLabel.SetText(magazineHeld?"Magazine: held by support hand":"Magazine: seated in weapon");
        return changed;
    }
    wi::gui::CheckBox applyIK,showIK;
    wi::gui::TextInputField pickupInput,seatInput;
    float pickupPhase=.17f,seatPhase=.77f;
    std::string currentClip() const {
        if(!arms.animations.GetCount())return "Idle";
        auto n=arms.names.GetComponent(arms.animations.GetEntity(anim));return n?n->name:"Idle";
    }
    void selectFit(int index){
        if(draggingFit)return;
        if(index>=16){int k=index-16;selectFingerJoint(k/15,(k/3)%5,k%3);return;}
        if(index>=6){selectFingertip((index-6)/5,(index-6)%5);return;}
        if(editing){if(instance||assembled)setMode(false);else{status.SetText("Place the main grip and preview with arms first.");return;}}
        if(index==5&&!magazineBone){status.SetText("This weapon has no magazine bone.");return;}
        selectWeapon(false);fitSelection=index;pauseForFit();if(index<5){fingerSide=(index-1)%2;fingerMode=true;}
        Entity e=0;gizmo.pivot_offset={};
        if(index<=4)e=armFit.control(index-1);
        else{
            e=magazineBone;
            auto socket=arms.transforms.GetComponent(arms.Entity_FindByName("socket_magazine_grip"));
            auto mag=arms.transforms.GetComponent(magazineBone);
            if(socket&&mag)XMStoreFloat3(&gizmo.pivot_offset,XMVector3TransformCoord(socket->GetPositionV(),XMMatrixInverse(nullptr,XMLoadFloat4x4(&mag->world))));
        }
        if(!e)return;PickResult pick;pick.entity=e;gizmo.selected={pick};gizmo.scene=&arms;
        setTool(0);
        const char* names[]={"Weapon","Left hand IK","Right hand IK","Left elbow pole","Right elbow pole","Magazine"};
        fitLabel.SetText(std::string(names[index])+"\nDrag the viewport handles.");
        status.SetText(index==5?"Move / Rotate adjusts the current magazine fit. Attachment is unchanged.":"Move a target; Rotate sets wrist orientation.");
    }
    void frameArmsControls(){
        auto a=arms.transforms.GetComponent(armFit.chain[0].upper),b=arms.transforms.GetComponent(armFit.chain[1].upper);
        auto l=arms.transforms.GetComponent(armFit.chain[0].wrist),r=arms.transforms.GetComponent(armFit.chain[1].wrist);
        if(!a||!b||!l||!r)return;
        XMStoreFloat3(&center,(a->GetPositionV()+b->GetPositionV()+l->GetPositionV()+r->GetPositionV())*.25f);
        first=false;distance=1.35f;yaw=-.8f;pitch=.18f;
    }
    void timing(float pickup,float seat){
        if(!std::isfinite(pickup)||!std::isfinite(seat)||pickup<0||seat>=1||pickup>=seat){
            pickupInput.SetValue(pickupPhase*100);seatInput.SetValue(seatPhase*100);status.SetText("Pickup must be before reseat, within 0-100%.");return;
        }
        rememberFit();pickupPhase=pickup;seatPhase=seat;orderBoltAfterSeat();pickupInput.SetValue(pickup*100);seatInput.SetValue(seat*100);
    }
    float clipPhase() const {
        if(!arms.animations.GetCount())return 0;
        auto& a=arms.animations[anim];return std::clamp((a.timer-a.start)/std::max(.0001f,a.end-a.start),0.f,1.f);
    }
    void layoutArmPanel(){
        float x=std::max(800.f,GetLogicalWidth()-262.f);
        armPanel.SetPos({x,85});applyIK.SetPos({x+224,121});showIK.SetPos({x+224,150});
        for(int i=0;i<5;i++)armSelect[i].SetPos({x,188.f+35*i});
        grabHere.SetPos({x,363});seatNow.SetPos({x,398});
        armFrame.SetPos({x,443});armReset.SetPos({x,478});armHint.SetPos({x,519});
        eventLabel.SetPos({x,590});pickupLabel.SetPos({x,620});seatLabel.SetPos({x,652});pickupInput.SetPos({x+128,620});seatInput.SetPos({x+128,652});
        pickupHere.SetPos({x,692});seatHere.SetPos({x,727});
        boltCaption.SetPos({x,762});boltGrabHere.SetPos({x,797});boltReleaseHere.SetPos({x,832});
        boltGrabLabel.SetPos({x,867});boltGrabInput.SetPos({x+128,867});boltReleaseLabel.SetPos({x,899});boltReleaseInput.SetPos({x+128,899});reloadStage.SetPos({x,931});
        grabHere.SetColor(magazineHeld?wi::Color(174,83,29):wi::Color(40,45,48));
        if(fitSelection>0){
            tools[2].SetEnabled(false);
            tools[1].SetEnabled(fitSelection<3||fitSelection==5);
        }else{tools[1].SetEnabled(true);tools[2].SetEnabled(true);}
    }
    void drawArmControls(){
        if(!armFit.visible||!armFit.enabled||editing)return;
        for(int side=0;side<2;side++){
            auto& c=armFit.chain[side];if(!c.captured)continue;
            XMFLOAT4 color=side==0?XMFLOAT4{.05f,.9f,1,1}:XMFLOAT4{1,.55f,.08f,1};
            auto target=arms.transforms.GetComponent(c.target),pole=arms.transforms.GetComponent(c.pole);
            float scale=std::max(.007f,wi::math::Distance(camera->Eye,target->GetPosition())*.012f);
            wi::renderer::DrawPoint({target->GetPosition(),scale,color});
            wi::renderer::DrawAxis(XMLoadFloat4x4(&target->world),scale*2);
            wi::renderer::DrawPoint({pole->GetPosition(),scale,XMFLOAT4{.8f,.35f,1,1}});
            wi::renderer::RenderableLine line;line.start=arms.transforms.GetComponent(c.upper)->GetPosition();line.end=arms.transforms.GetComponent(c.lower)->GetPosition();line.color_start=line.color_end=color;wi::renderer::DrawLine(line);
            line.start=line.end;line.end=arms.transforms.GetComponent(c.wrist)->GetPosition();wi::renderer::DrawLine(line);
            line.end=pole->GetPosition();line.color_start=line.color_end={.8f,.35f,1,.7f};wi::renderer::DrawLine(line);
        }
    }
    int pickArmControl(XMFLOAT4 pointer){
        if(!armFit.visible||!armFit.enabled)return 0;float nearest=18*18;int selected=0;
        for(int i=0;i<4;i++){
            auto t=arms.transforms.GetComponent(armFit.control(i));if(!t)continue;
            auto screen=XMVector3Project(t->GetPositionV(),0,0,GetLogicalWidth(),GetLogicalHeight(),0,1,XMLoadFloat4x4(&camera->Projection),XMLoadFloat4x4(&camera->View),XMMatrixIdentity());
            auto p=ArmFit::f3(screen);float d=(p.x-pointer.x)*(p.x-pointer.x)+(p.y-pointer.y)*(p.y-pointer.y);
            if(p.z>=0&&p.z<=1&&d<nearest){nearest=d;selected=i+1;}
        }
        return selected;
    }
    wi::gui::Button tools[3], selectWeaponButton, frameWeaponButton, undoButton, redoButton;
    wi::gui::Label fitLabel;
    wi::graphics::Texture selectionMask, gizmoOverlay, gizmoDepth;
    bool selectedWeapon=false, hasFit=false, draggingFit=false;
    int tool=0;
    wi::gui::ComboBox magazineControl;
    wi::gui::Label magazineLabel;
    Entity magazineBone=0,magazineParent=0,supportHandEntity=0;
    TransformComponent magazineSeat;
    XMFLOAT4X4 magazineHandOffset={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    bool magazineHeld=false,magazineOffsetValid=false;
    int magazineMode=2;
    void attachMagazine(bool toHand){
        if(!magazineBone||magazineHeld==toHand)return;
        auto mag=arms.transforms.GetComponent(magazineBone);
        auto support=arms.transforms.GetComponent(supportHandEntity);
        if(!mag||!support)return;
        if(toHand){
            if(!magazineOffsetValid){
                XMStoreFloat4x4(&magazineHandOffset,XMLoadFloat4x4(&mag->world)*XMMatrixInverse(nullptr,XMLoadFloat4x4(&support->world)));
                magazineOffsetValid=true;
            }
            arms.Component_Attach(magazineBone,supportHandEntity,true);
            mag=arms.transforms.GetComponent(magazineBone);mag->ClearTransform();mag->MatrixTransform(XMLoadFloat4x4(&magazineHandOffset));
        }else{
            arms.Component_Attach(magazineBone,magazineParent,true);
            mag=arms.transforms.GetComponent(magazineBone);*mag=magazineSeat;mag->SetDirty();
        }
        magazineHeld=toHand;
    }
    void updateMagazineAttachment(float poseTime=-1){
        if(!magazineBone)return;
        bool hold=magazineMode==1;
        if(magazineMode==2&&arms.animations.GetCount()){
            auto& a=arms.animations[anim];auto n=arms.names.GetComponent(arms.animations.GetEntity(anim));
            float phase=((poseTime>=0?poseTime:a.timer)-a.start)/std::max(.0001f,a.end-a.start);
            hold=n&&n->name=="Reload"&&phase>=pickupPhase&&phase<seatPhase;
        }
        attachMagazine(hold);magazineLabel.SetText(magazineHeld?"Magazine: held by support hand":"Magazine: seated in weapon");
    }
    void findMagazine(){
        magazineBone=arms.Entity_FindByName("magazine");
        if(!magazineBone||!arms.Entity_IsDescendant(magazineBone,instance)){magazineBone=0;return;}
        auto hier=arms.hierarchy.GetComponent(magazineBone);auto tr=arms.transforms.GetComponent(magazineBone);
        if(!hier||!tr){magazineBone=0;return;}
        magazineParent=hier->parentID;magazineSeat=*tr;
        supportHandEntity=arms.Entity_FindByName(preset.at("left").get<std::string>());
        magazineControl.SetVisible(true);magazineLabel.SetVisible(true);
    }
    wi::vector<Entity> weaponObjects;
    struct FitState {
        XMFLOAT3 position{}, scale={1,1,1};
        XMFLOAT4 rotation={0,0,0,1};
        float modelScale=1;
        json ik,bolt,fingers;
        GripFit grip;
        float pickup=.17f,seat=.77f;
        int magMode=2;
        bool magValid=false;
        XMFLOAT4X4 magOffset; TransformComponent magSeat;
    };
    std::vector<FitState> undoFits,redoFits;
    FitState dragBefore;
    FitState fitState() const {
        FitState f; f.grip=gripFit;auto t=arms.transforms.GetComponent(instance);
        if(t){f.position=t->translation_local;f.scale=t->scale_local;f.rotation=t->rotation_local;}
        f.modelScale=modelScale;f.ik=armFit.write();f.bolt=boltContact.write();f.fingers=fingerFit.write();f.pickup=pickupPhase;f.seat=seatPhase;f.magValid=magazineOffsetValid;f.magMode=magazineMode;f.magOffset=magazineHandOffset;f.magSeat=magazineSeat;return f;
    }
    void restoreFit(const FitState& f){
        auto t=arms.transforms.GetComponent(instance);if(!t)return;
        t->translation_local=f.position;t->scale_local=f.scale;t->rotation_local=f.rotation;t->SetDirty();
        hasFit=true;modelScale=f.modelScale;scaleInput.SetValue(modelScale);updateSizeLabel();
        if(!f.ik.is_null()){
            wristToBind.reset();gripFit=f.grip;armFit.read(f.ik);if(!f.fingers.is_null())fingerFit.read(f.fingers);refreshFingerUI();if(!f.bolt.is_null())boltContact.read(f.bolt);boltGrabInput.SetValue(boltContact.grab*100);boltReleaseInput.SetValue(boltContact.release*100);applyIK.SetCheck(armFit.enabled);showIK.SetCheck(armFit.visible);
            pickupPhase=f.pickup;seatPhase=f.seat;magazineMode=f.magMode;magazineControl.SetSelectedWithoutCallback(magazineMode);magazineOffsetValid=f.magValid;magazineHandOffset=f.magOffset;magazineSeat=f.magSeat;if(magazineBone&&!magazineHeld){*arms.transforms.GetComponent(magazineBone)=magazineSeat;arms.transforms.GetComponent(magazineBone)->SetDirty();}
            pickupInput.SetValue(pickupPhase*100);seatInput.SetValue(seatPhase*100);
            if(magazineHeld&&magazineOffsetValid){auto mag=arms.transforms.GetComponent(magazineBone);mag->ClearTransform();mag->MatrixTransform(XMLoadFloat4x4(&magazineHandOffset));}updateMagazineAttachment();
        }
    }
    void updateSizeLabel(){
        XMFLOAT3 effective{modelScale,modelScale,modelScale};
        if(hasFit&&instance){
            auto t=arms.transforms.GetComponent(instance);auto parent=arms.transforms.GetComponent(hand);
            XMVECTOR sc,rot,pos;if(t&&parent&&XMMatrixDecompose(&sc,&rot,&pos,t->GetLocalMatrix()*XMLoadFloat4x4(&parent->world)))XMStoreFloat3(&effective,XMVectorAbs(sc));
        }
        char buf[160];snprintf(buf,sizeof(buf),"Size (m)   %.3g x %.3g x %.3g",sourceSize.x*effective.x,sourceSize.y*effective.y,sourceSize.z*effective.z);
        sizeLabel.SetText(buf);
    }
    void rememberFit(){if(!instance)return;undoFits.push_back(fitState());if(undoFits.size()>100)undoFits.erase(undoFits.begin());redoFits.clear();}
    void undoFit(bool redo=false){
        if(editing||flashPanel){undoMarker(redo);return;}
        auto& from=redo?redoFits:undoFits;auto& to=redo?undoFits:redoFits;
        if(from.empty()||!instance||draggingFit)return;to.push_back(fitState());auto f=from.back();from.pop_back();restoreFit(f);
        status.SetText(redo?"Fit change redone.":"Fit change undone.");
    }
    void pauseForFit(){
        if(arms.animations.GetCount()){auto& a=arms.animations[anim];if(a.IsPlaying()&&a.last_update_time>=a.start){a.timer=a.last_update_time;a.last_update_time=-1;}a.Pause();play.SetText("Play");}
    }
    void selectWeapon(bool selected){
        meleeOpen=false;firingOpen=false;casingPanel=false;flashPanel=false;
        fitSelection=0;cleanHandleSelected=false;gizmo.pivot_offset=prop.bounds.getCenter();
        selectedWeapon=selected && instance && !editing;
        gizmo.selected.clear();gizmo.scene=&arms;
        if(selectedWeapon){PickResult pick;pick.entity=instance;gizmo.selected.push_back(pick);pauseForFit();}
        for(auto e:weaponObjects)if(auto o=arms.objects.GetComponent(e))o->SetUserStencilRef(selectedWeapon?1:0);
        fitLabel.SetText(selectedWeapon?"Weapon selected\nDrag an axis or centre handle.":"Click the weapon to select it.");
        status.SetText(selectedWeapon?"W Move | E Rotate | R Scale\nCtrl+Z Undo | Ctrl+Y Redo":"Click the weapon to select it.");
    }
    void setTool(int value){
        if(draggingFit)return;
        if(editing&&value==2)return;
        if(fitSelection>0&&value==2){status.SetText("IK targets keep arm lengths fixed. Use Move or Rotate.");return;}
        if((fitSelection==3||fitSelection==4||(fitSelection>=6&&fitSelection<16))&&value==1)value=0;
        if(fitSelection>=16)value=1;
        tool=value;gizmo.isWeaponTranslator=value==0;gizmo.isRotator=value==1;gizmo.isScalator=value==2;
        gizmo.isLocalSpace=value!=0;
        for(int i=0;i<3;i++)tools[i].SetColor(i==value?wi::Color(174,83,29):wi::Color(40,45,48));
    }
    void frameWeapon(){
        if(!instance)return;arms.Update(0);
        auto root=arms.transforms.GetComponent(instance);
        if(root && longest(sourceSize)>0){
            const auto world=XMLoadFloat4x4(&root->world);
            XMStoreFloat3(&center,XMVector3TransformCoord(XMLoadFloat3(&sourceCenter),world));
            // Frame visible geometry; skinned object culling bounds can be much larger.
            const auto size=XMLoadFloat3(&sourceSize);
            float diameter=0;
            for(int i=0;i<8;++i){
                const auto sign=XMVectorSet((i&1)?1.f:-1.f,(i&2)?1.f:-1.f,(i&4)?1.f:-1.f,0);
                diameter=std::max(diameter,XMVectorGetX(XMVector3Length(XMVector3TransformNormal(size*sign,world))));
            }
            distance=std::max(.15f,1.45f*diameter);
            first=false;
            return;
        }
        XMVECTOR lo=XMVectorReplicate(FLT_MAX),hi=XMVectorReplicate(-FLT_MAX);bool found=false;
        for(auto e:weaponObjects){auto i=arms.objects.GetIndex(e);if(i==~size_t(0))continue;
            lo=XMVectorMin(lo,XMLoadFloat3(&arms.aabb_objects[i]._min));hi=XMVectorMax(hi,XMLoadFloat3(&arms.aabb_objects[i]._max));found=true;}
        if(found){XMStoreFloat3(&center,(lo+hi)*.5f);XMFLOAT3 ext;XMStoreFloat3(&ext,(hi-lo)*.5f);distance=std::max(.15f,2.9f*longest(ext));first=false;}
    }
    void ResizeBuffers() override {
        RenderPath3D::ResizeBuffers();auto device=wi::graphics::GetDevice();auto res=GetInternalResolution();
        wi::graphics::TextureDesc d;d.width=res.x;d.height=res.y;d.format=wi::graphics::Format::R8_UNORM;
        d.bind_flags=wi::graphics::BindFlag::RENDER_TARGET|wi::graphics::BindFlag::SHADER_RESOURCE;
        device->CreateTexture(&d,nullptr,&selectionMask);
        d.width=GetPhysicalWidth();d.height=GetPhysicalHeight();d.format=wi::graphics::Format::R8G8B8A8_UNORM;
        device->CreateTexture(&d,nullptr,&gizmoOverlay);
        d.format=wi::graphics::Format::D32_FLOAT;d.bind_flags=wi::graphics::BindFlag::DEPTH_STENCIL;
        d.layout=wi::graphics::ResourceState::DEPTHSTENCIL;
        device->CreateTexture(&d,nullptr,&gizmoDepth);
    }
    void Render() const override {
        RenderPath3D::Render();
        if(gizmo.selected.empty())return;
        using namespace wi::graphics;auto device=GetDevice();auto cmd=device->BeginCommandList();
        Viewport vp;vp.width=(float)selectionMask.desc.width;vp.height=(float)selectionMask.desc.height;device->BindViewports(1,&vp,cmd);
        RenderPassImage maskPass[]={RenderPassImage::RenderTarget(&selectionMask,RenderPassImage::LoadOp::CLEAR),
            RenderPassImage::DepthStencil(GetDepthStencil(),RenderPassImage::LoadOp::LOAD,RenderPassImage::StoreOp::STORE)};
        device->RenderPassBegin(maskPass,arraysize(maskPass),cmd);
        wi::image::Params fx;fx.enableFullScreen();fx.stencilComp=wi::image::STENCILMODE_EQUAL;
        fx.stencilRefMode=wi::image::STENCILREFMODE_USER;fx.stencilRef=1;wi::image::Draw(nullptr,fx,cmd);device->RenderPassEnd(cmd);
        RenderPassImage overlayPass[]={RenderPassImage::RenderTarget(&gizmoOverlay,RenderPassImage::LoadOp::CLEAR),
            RenderPassImage::DepthStencil(&gizmoDepth,RenderPassImage::LoadOp::CLEAR,RenderPassImage::StoreOp::DONTCARE)};
        device->RenderPassBegin(overlayPass,arraysize(overlayPass),cmd);
        vp.width=(float)gizmoOverlay.desc.width;vp.height=(float)gizmoOverlay.desc.height;device->BindViewports(1,&vp,cmd);
        wi::renderer::BindCommonResources(cmd);wi::image::SetCanvas(*this);wi::font::SetCanvas(*this);
        gizmo.Draw(*camera,wi::input::GetPointer(),cmd);device->RenderPassEnd(cmd);
    }
    void Compose(wi::graphics::CommandList cmd) const override {
        wi::image::Params fx;fx.enableFullScreen();fx.blendFlag=wi::enums::BLENDMODE_OPAQUE;
        wi::image::Draw(&GetRenderResult3D(),fx,cmd);
        if(!gizmo.selected.empty()){wi::renderer::BindCommonResources(cmd);
            wi::renderer::Postprocess_Outline(selectionMask,cmd,.1f,2,XMFLOAT4{1,.44f,.06f,1});
            fx.blendFlag=wi::enums::BLENDMODE_PREMULTIPLIED;wi::image::Draw(&gizmoOverlay,fx,cmd);}
        drawWeaponMarkers(cmd);RenderPath2D::Compose(cmd);drawGripTimeline(cmd);
    }
    wi::gui::ComboBox clips;
    wi::gui::Slider timeline;
    wi::gui::Label status, title, clipLabel;
    wi::gui::Button play,view,open,mainGrip,support,assemble,save,load;
    wi::gui::Slider rotation[3];
    std::array<std::optional<XMFLOAT3>,2> points{};
    fs::path model,setupPath;
    Entity instance=0,hand=0;
    Entity previewItem=0;
    std::map<std::string,Entity> itemClips;
    int anim=0,marker=0;
    wi::gui::TextInputField scaleInput;
    wi::gui::Label sizeLabel,scaleLabel;
    wi::gui::Button matchSize;
    float modelScale=1, referenceLength=0;
    XMFLOAT3 sourceSize{}, sourceCenter{};
    static float longest(XMFLOAT3 v){return std::max({v.x,v.y,v.z});}
    static XMFLOAT3 dimensions(Scene& s, XMFLOAT3* meshCenter=nullptr){
        s.Update(0);
        XMVECTOR low=XMVectorReplicate(FLT_MAX),high=XMVectorReplicate(-FLT_MAX);
        bool found=false;
        for(size_t i=0;i<s.objects.GetCount();++i){
            auto mesh=s.meshes.GetComponent(s.objects[i].meshID);
            auto tr=s.transforms.GetComponent(s.objects.GetEntity(i));
            if(!mesh||!tr)continue;
            auto world=XMLoadFloat4x4(&s.matrix_objects[i]);auto rig=s.armatures.GetComponent(mesh->armatureID);
            for(uint32_t k=0;k<mesh->vertex_positions.size();++k){
                auto local=rig && !rig->boneData.empty()?SkinVertex(*mesh,*rig,k):XMLoadFloat3(&mesh->vertex_positions[k]);auto pt=XMVector3TransformCoord(local,world);
                low=XMVectorMin(low,pt);high=XMVectorMax(high,pt);found=true;
            }
        }
        XMFLOAT3 size{};
        if(found){
            XMStoreFloat3(&size,high-low);
            if(meshCenter)XMStoreFloat3(meshCenter,(high+low)*.5f);
        }
        return size;
    }
    void applyScale(float value){
        if(!std::isfinite(value)||value<=0||!std::isfinite(value*longest(sourceSize))){
            scaleInput.SetValue(modelScale);status.SetText("Enter a positive scale.");return;
        }
        if(hasFit && instance){
            rememberFit();float ratio=value/modelScale;auto t=arms.transforms.GetComponent(instance);
            t->Scale(XMFLOAT3{ratio,ratio,ratio});hasFit=true;
        }
        if(!armFit.enabled&&instance&&value!=modelScale)gripFit.bindOnPreview=true;
        modelScale=value;scaleInput.SetValue(value);
        char buf[160];snprintf(buf,sizeof(buf),"Size (m)   %.3g x %.3g x %.3g",sourceSize.x*value,sourceSize.y*value,sourceSize.z*value);
        sizeLabel.SetText(buf);if(!hasFit)align();
    }
    void syncPreviewItem(){
        if(itemClips.empty())return;
        auto& arm=arms.animations[anim];
        auto n=arms.names.GetComponent(arms.animations.GetEntity(anim));
        auto it=itemClips.find(n?n->name:"");if(it==itemClips.end())it=itemClips.find("Idle");
        for(auto& entry:itemClips){
            auto a=arms.animations.GetComponent(entry.second);if(!a)continue;
            if(it==itemClips.end()||entry.second!=it->second){a->Pause();a->last_update_time=a->timer;continue;}
            float phase=(arm.timer-arm.start)/std::max(.0001f,arm.end-arm.start);
            a->Pause();a->timer=a->start+std::clamp(phase,0.f,1.f)*(a->end-a->start);a->last_update_time=-1;
        }
    }
    bool first=true,editing=false,orbitPanActive=false,orbitTurnActive=false;
    float orbitWheelSteps=0;
    struct MarkerView {XMFLOAT3 center;float distance,yaw,pitch;};
    std::optional<MarkerView> lastMarkerView;
    void frameMarkerModel(){
        center=sourceCenter; first=false;
        const float h=std::max(1.f,GetLogicalHeight());
        const float tanY=tanf(camera->fov*.5f);
        const float halfVisibleX=std::max(.01f,tanY*(GetLogicalWidth()-cleanLeft()-cleanRight())/h*.79f);
        const float halfVisibleY=std::max(.01f,tanY*(viewportBottom()-viewportTop)/h*.79f);
        auto outward=XMVectorSet(sinf(yaw)*cosf(pitch),sinf(pitch),-cosf(yaw)*cosf(pitch),0);
        auto right=XMVector3Normalize(XMVector3Cross(XMVectorSet(0,1,0,0),-outward));
        auto up=XMVector3Cross(-outward,right);
        float fit=.05f;
        for(int i=0;i<8;++i){
            auto corner=XMLoadFloat3(&sourceSize)*XMVectorSet(i&1?.5f:-.5f,i&2?.5f:-.5f,i&4?.5f:-.5f,0);
            auto extent=[&](XMVECTOR axis){return XMVectorGetX(XMVector3Dot(corner,axis));};
            fit=std::max(fit,extent(outward)+std::max({std::abs(extent(right))/halfVisibleX,std::abs(extent(up))/halfVisibleY,.025f}));
        }
        distance=fit;
    }
    void onOrbitWheel(HWND window,WPARAM buttons,LPARAM position){
        if(window!=GetForegroundWindow()||first||draggingFit||markerPressed||(LOWORD(buttons)&MK_LBUTTON))return;
        POINT p={static_cast<short>(LOWORD(position)),static_cast<short>(HIWORD(position))};
        ScreenToClient(window,&p);const float x=PhysicalToLogical(float(p.x)),y=PhysicalToLogical(float(p.y));
        if(x>cleanLeft()&&x<GetLogicalWidth()-cleanRight()&&y>viewportTop&&y<viewportBottom()&&!GetGUI().HasFocus())
            orbitWheelSteps+=float(GET_WHEEL_DELTA_WPARAM(buttons))/WHEEL_DELTA;
    }
    float yaw=.5f,pitch=.18f,distance=1;
    XMFLOAT3 center{},eye={0,0,0},forward={0,0,1};
    XMFLOAT4 previous{};
    void light(Scene& s){
        auto& w=s.weathers.Create(wi::ecs::CreateEntity());
        w.ambient={.55f,.55f,.55f};w.horizon={.09f,.11f,.13f};w.zenith={.035f,.045f,.05f};
        auto e=s.Entity_CreateLight("Studio softbox",{0,2,-1},{1,.94f,.86f},3,20,LightComponent::DIRECTIONAL);
        s.transforms.GetComponent(e)->RotateRollPitchYaw(XMFLOAT3{.8f,-.5f,0});
    }
    void frame(Scene& s){
        s.Update(0);
        auto c=s.bounds.getCenter();auto h=s.bounds.getHalfWidth();
        center=c;distance=std::max(.12f,2.9f*std::max({h.x,h.y,h.z}));first=false;
    }
    void frameArms(){
        if(assembled && arms.cameras.GetCount()){
            auto& authored=arms.cameras[0];
            eye=authored.Eye;forward=authored.At;
            return;
        }
        auto joint=[this](const char* n){auto t=arms.transforms.GetComponent(arms.Entity_FindByName(n));return t?t->GetPosition():XMFLOAT3{};};
        auto cameraName=preset.value("camera",std::string{});
        auto cameraBone=arms.Entity_FindByName(cameraName);
        if(!cameraBone)cameraBone=arms.Entity_FindByName("head");
        auto cameraTransform=arms.transforms.GetComponent(cameraBone);
        auto left=joint(preset.at("left").get<std::string>().c_str());
        auto right=joint(preset.at("right").get<std::string>().c_str());
        XMFLOAT3 head=cameraTransform?cameraTransform->GetPosition():XMFLOAT3{(left.x+right.x)*.5f,(left.y+right.y)*.5f+.22f,(left.z+right.z)*.5f};
        auto mid=(XMLoadFloat3(&left)+XMLoadFloat3(&right))*.5f;
        auto h=XMLoadFloat3(&head);
        XMStoreFloat3(&center,XMVectorLerp(h,mid,.68f));distance=1.15f;yaw=-.8f;pitch=.17f;
        auto horizontal=XMVectorSet(mid.m128_f32[0]-head.x,0,mid.m128_f32[2]-head.z,0);
        horizontal=XMVectorGetX(XMVector3LengthSq(horizontal))>1e-8f?XMVector3Normalize(horizontal):XMVectorSet(0,0,1,0);
        XMStoreFloat3(&eye,h+horizontal*.105f+XMVectorSet(0,-.11f,0,0));
        XMStoreFloat3(&forward,XMVector3Normalize(mid-XMLoadFloat3(&eye)+horizontal*.45f));
        if(preset.contains("viewDirection")&&(cameraTransform||preset.contains("viewEye"))){
            eye=head;
            if(preset.contains("viewEye")){auto p=preset.at("viewEye").get<std::array<float,3>>();eye={p[0],p[1],p[2]};}
            auto dir=preset.at("viewDirection").get<std::array<float,3>>();
            forward={dir[0],dir[1],dir[2]};
        }
    }
    void selectClip(int i){
        if(i<0||i>=(int)arms.animations.GetCount())return;
        clearFlashPlayback();clearFlyingCasings();
        for(size_t k=0;k<arms.animations.GetCount();k++)arms.animations[k].Stop();
        fingerOverrides.clear();anim=i;clips.SetSelectedByUserdataWithoutCallback((uint64_t)i);auto& a=arms.animations[i];a.timer=a.start;a.SetLooped();a.Play();
        timeline.SetRange(a.start,std::max(a.start+.001f,a.end));play.SetText("Pause");
    }
    void btn(wi::gui::Button& b,const char* name,float y,std::function<void()> f){
        b.Create(name);b.SetPos({18,y});b.SetSize({214,27});b.OnClick([f](auto&){f();});GetGUI().AddWidget(&b);
        b.SetColor(wi::Color(40,45,48));b.SetColor(wi::Color(110,61,31),wi::gui::FOCUS);
    }
    void setMode(bool edit){
        clearFlashPlayback();clearFlyingCasings();
        clearMarkerPreview();
        if(editing&&!edit)lastMarkerView=MarkerView{center,distance,yaw,pitch};
        selectWeapon(false);editing=edit;scene=edit?&placement:&arms;
        orbitWheelSteps=0;orbitTurnActive=orbitPanActive=false;
        if(edit){
            first=false;
            if(lastMarkerView){center=lastMarkerView->center;distance=lastMarkerView->distance;yaw=lastMarkerView->yaw;pitch=lastMarkerView->pitch;}
            else{yaw=sourceSize.z>=sourceSize.x?-XM_PIDIV2:0;pitch=.1f;frameMarkerModel();}
        }else{frameArms();first=true;marker=-1;}
        for(auto& r:rotation)r.SetVisible(false);
    }
    void positionWeapon(bool undo){
        pauseForFit();
        setFullIK(false,undo);
        if(instance)selectWeapon(true);
        status.SetText("Position the weapon against the paused arms. Choose Preview hand fit when ready.");
    }
    void importWeapon(const fs::path& p){
        setProp(p);
        positionWeapon(false);
        armFit.hierarchy();
        setupPath.clear();
        // Imported socket positions and picked markers share the unmounted model space.
        const char* sockets[]={"socket_grip_main","socket_grip_support"};
        for(int i=0;i<2;++i){
            auto socket=prop.transforms.GetComponent(prop.Entity_FindByName(sockets[i]));
            if(socket)points[i]=socket->GetPosition();
        }
        if(points[0]&&points[1]&&hand){
            mount();
        }else{
            marker=points[0]?1:0;
            status.SetText(points[0]?"Main grip found. Place the support contact or preview with arms.":"Click the main grip on the model.");
        }
    }
    void setProp(const fs::path& p){
        Scene candidate;
        readModel(p,candidate);
        if(!candidate.objects.GetCount())throw std::runtime_error("The file has no mesh.");
        visitSurfaces([&](std::string key,auto& m){if(surfaceEdits.count(key)&&surfaceOriginal.count(key))renegade::bridge::SetWeaponSurface(m,surfaceOriginal.at(key));});
        disabledSurfaceMaps.clear();surfaceEdits.clear();applySurfaces();surfaceOriginal.clear();originalMaps.clear();surfaceOpen=false;
        firingDraft=nullptr;linkedItem.clear();linkedSetup.clear();linkedHash.clear();originalFiring.reset();
        clearFlashPlayback();setShotSound("");audioCues.clear();cueSounds.clear();listedClip.clear();resetFlashTexture({});automaticFire=false;loopShotSound=false;barrelEffects=json::object();flashTimes.clear();flashStyle=0;flashIndex=0;
        clearCasingAsset();casingTimes.clear();casingAsset.clear();casingScale=includedCasingScale;
        gripSocket=0;gripFit={};wristToBind.reset();gripCalibrated=false;gripDragKey.reset();
        fingerFit.restoreBase();fingerFit.poses.clear();armFit.restoreBase();armFit.profiles.clear();armFit.keys.clear();boltContact.clear();
        attachMagazine(false);magazineBone=0;magazineParent=0;magazineHeld=false;magazineOffsetValid=false;
        magazineControl.SetVisible(false);magazineLabel.SetVisible(false);
        selectWeapon(false);weaponObjects.clear();undoFits.clear();redoFits.clear();hasFit=false;
        if(previewItem){arms.Entity_Remove(previewItem);previewItem=0;itemClips.clear();} if(instance){arms.Entity_Remove(instance);instance=0;}
        sourceSize=dimensions(candidate,&sourceCenter);prop.Clear();prop.Merge(candidate);placement.Clear();placement.Instantiate(prop,true);light(placement);
        gizmo.pivot_offset=prop.bounds.getCenter();
        spawnPoints.clear();casingSocket.reset();selectedSpawn=-1;markerHandle=0;markerUndo.clear();markerRedo.clear();refreshMarkerList();
        fittedMountBone.clear();itemClips.clear();
        lastMarkerView.reset();model=p;points={};gripOrientation={0,0,0,1};marker=0;for(auto& r:rotation)r.SetValue(0);
        scaleLabel.SetVisible(true);scaleInput.SetVisible(true);sizeLabel.SetVisible(true);matchSize.SetVisible(referenceLength>0 && longest(sourceSize)>0);applyScale(1);setMode(true);status.SetText("Place Main Grip on the model.");
    }
    XMFLOAT4 gripOrientation={0,0,0,1};
    bool orientFromContacts(){
        if(!points[0]||!points[1])return false;
        auto main=arms.transforms.GetComponent(hand);
        auto supportHand=arms.transforms.GetComponent(arms.Entity_FindByName(preset.at("left").get<std::string>()));
        if(!main||!supportHand)return false;
        auto a=main->GetPosition(),b=supportHand->GetPosition();
        auto local=XMLoadFloat3(&*points[1])-XMLoadFloat3(&*points[0]);
        auto target=XMLoadFloat3(&b)-XMLoadFloat3(&a);
        if(XMVectorGetX(XMVector3LengthSq(local))<1e-10f||XMVectorGetX(XMVector3LengthSq(target))<1e-10f)return false;
        auto basis=[](XMVECTOR dir){
            dir=XMVector3Normalize(dir);
            auto up=std::abs(XMVectorGetY(dir))>.98f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
            return XMMatrixLookToLH(XMVectorZero(),dir,up);
        };
        XMVECTOR scale,rotation,pos;
        if(!XMMatrixDecompose(&scale,&rotation,&pos,XMLoadFloat4x4(&main->world)))return false;
        auto desired=basis(local)*XMMatrixTranspose(basis(target))*XMMatrixTranspose(XMMatrixRotationQuaternion(rotation));
        XMStoreFloat4(&gripOrientation,XMQuaternionNormalize(XMQuaternionRotationMatrix(desired)));return true;
    }
    void align(){
        if(!instance||!points[0])return;
        auto p=*points[0];
        auto m=XMMatrixTranslation(-p.x,-p.y,-p.z)*XMMatrixScaling(modelScale,modelScale,modelScale)*XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotation[0].GetValue()),XMConvertToRadians(rotation[1].GetValue()),XMConvertToRadians(rotation[2].GetValue()))*XMMatrixRotationQuaternion(XMLoadFloat4(&gripOrientation));
        // Keep model units when an imported hand inherits centimetre-to-metre scale.
        XMVECTOR handScale,handRotation,handPosition;
        if(XMMatrixDecompose(&handScale,&handRotation,&handPosition,XMLoadFloat4x4(&arms.transforms.GetComponent(hand)->world))){
            m=m*XMMatrixScalingFromVector(XMVectorReciprocal(handScale));
        }
        auto t=arms.transforms.GetComponent(instance);t->ClearTransform();t->MatrixTransform(m);
    }
    void mount(bool useContacts=true){
        if(assembled){setMode(false);return;}
        if(!points[0]||!hand){status.SetText("A main grip and a preset hand are required.");return;}
        bool firstMount=!instance;
        if(!instance){auto firstAnimation=arms.animations.GetCount();instance=arms.Instantiate(prop,true);auto parent=fittedMountBone.empty()?hand:arms.Entity_FindByName(fittedMountBone);
            arms.Component_Attach(instance,parent,true);
            for(size_t i=firstAnimation;i<arms.animations.GetCount();i++){
                auto e=arms.animations.GetEntity(i);auto n=arms.names.GetComponent(e);if(n)itemClips[n->name]=e;arms.animations[i].Stop();
            }
            for(size_t i=0;i<arms.objects.GetCount();i++){auto e=arms.objects.GetEntity(i);if(e==instance||arms.Entity_IsDescendant(e,instance))weaponObjects.push_back(e);}}
        if(!hasFit){if(useContacts)orientFromContacts();align();}if(!magazineBone)findMagazine();if(!boltContact.bone)boltContact.init(arms,instance,itemClips);setMode(false);selectWeapon(true);frameWeapon();if(firstMount||!gripCalibrated)findSupportGrip();status.SetText("Weapon selected. Fit it with Move / Rotate / Scale; the original arms stay unchanged while IK is off.");
    }
    json lastSaved;
    json describeSetup(const fs::path& file){
            json j={{"version",1},{"model",model.lexically_relative(file.parent_path()).empty()?model.generic_u8string():model.lexically_relative(file.parent_path()).generic_u8string()},{"scale",modelScale},{"gripOrientation",{gripOrientation.x,gripOrientation.y,gripOrientation.z,gripOrientation.w}},{"rotation",{rotation[0].GetValue(),rotation[1].GetValue(),rotation[2].GetValue()}}};
            if(assembled){j["assembled"]=true;j["actions"]=assembledActions;}
            if(!firingDraft.is_null())j["primaryFiring"]=firingDraft;
            if(automaticFire)j["automaticFire"]=true;
            if(loopShotSound)j["loopShotSound"]=true;
            if(!shotSound.empty())j["shotSound"]=shotSound;
            if(!audioCues.empty())j["animationSounds"]=audioCues;
            if(!disabledSurfaceMaps.empty())j["disabledSurfaceMaps"]=disabledSurfaceMaps;
            if(!surfaceEdits.empty())j["materials"]=surfaceEdits;
            if(!fittedMountBone.empty())j["mountBone"]=fittedMountBone;
            if(!presetFile.empty()){const auto rel=presetFile.lexically_relative(home);
                if(!rel.empty()&&*rel.begin()!="..")j["armsPreset"]=rel.generic_u8string();
                else {auto local=presetFile.lexically_relative(file.parent_path());j["localArmsPreset"]=(local.empty()?presetFile:local).generic_u8string();}}
            if(!requestedArms.empty()){
                auto rel=requestedArms.lexically_relative(file.parent_path());
                j["armsModel"]=(rel.empty()?requestedArms:rel).generic_u8string();
            }
            if(instance&&(gripSocket||points[1]))j["supportGrip"]=writeGripFit();
            j["ikEnabled"]=fullIKEnabled();j["fingerGrips"]=fingerFit.write();j["boltContact"]=boltContact.write();
            j["armIK"]=armFit.write();j["reloadPickup"]=pickupPhase;j["reloadSeat"]=seatPhase;
            j["magazineMode"]=magazineMode;
            if(magazineBone){j["magazineSeatTransform"]={{"position",{magazineSeat.translation_local.x,magazineSeat.translation_local.y,magazineSeat.translation_local.z}},{"rotation",{magazineSeat.rotation_local.x,magazineSeat.rotation_local.y,magazineSeat.rotation_local.z,magazineSeat.rotation_local.w}},{"scale",{magazineSeat.scale_local.x,magazineSeat.scale_local.y,magazineSeat.scale_local.z}}};}
            if(magazineOffsetValid){j["magazineHandOffset"]=json::array();for(int row=0;row<4;row++)for(int col=0;col<4;col++)j["magazineHandOffset"].push_back(magazineHandOffset.m[row][col]);}
            if(instance){auto f=fitState();j["fitTransform"]={{"position",{f.position.x,f.position.y,f.position.z}},{"rotation",{f.rotation.x,f.rotation.y,f.rotation.z,f.rotation.w}},{"scale",{f.scale.x,f.scale.y,f.scale.z}}};}
            std::string encoded,error;
            if(!renegade::bridge::SerializeLaunchSockets(spawnPoints,encoded,error))throw std::runtime_error(error);
            j["launchSockets"]=json::parse(encoded);
            if(casingSocket){
                if(!renegade::bridge::SerializeLaunchSockets({*casingSocket},encoded,error))throw std::runtime_error(error);
                j["casingEjectionMarker"]=json::parse(encoded).at("sockets").at(0);
            }
            if(!barrelEffects.empty())j["barrelEffects"]=barrelEffects;
            if(!flashTimes.empty()||flashStyle!=0||!flashTexture.empty())
                j["muzzleFlashPreview"]={{"style",flashStyle},{"barrelClipTimes",flashTimes}};
            if(!flashTexture.empty())j["muzzleFlashPreview"]["texture"]=flashTexture;
            if(!casingTimes.empty()||!casingAsset.empty()||casingScale!=includedCasingScale){
                auto stored=casingAsset.empty()?fs::path{}:casingAsset.lexically_relative(file.parent_path());
                if(stored.empty())stored=casingAsset;
                j["casingPreview"]={{"model",stored.generic_u8string()},{"scale",casingScale},{"clipTimes",casingTimes}};
            }
            for(auto p:points){if(p)j["markers"].push_back({p->x,p->y,p->z});else j["markers"].push_back(nullptr);}
            if(melee){
                j["weaponType"]="melee";j["melee"]=meleeDocument();
                for(auto k:{"primaryFiring","barrelEffects","automaticFire","shotSound","muzzleFlashPreview","casingPreview","launchSockets","casingEjectionMarker"})j.erase(k);
            }
            return j;
    }
    bool unsaved(){
        return !model.empty() && (setupPath.empty() || describeSetup(setupPath)!=lastSaved);
    }
    void setupFile(bool writing,fs::path file={}){
        if(writing && model.empty())throw std::runtime_error("Open a weapon model first.");
        if(file.empty())file=choose(writing,L"Weapon setup (*.weapon)\0*.weapon\0\0",L"weapon");if(file.empty())return;
        if(writing){
            auto j=describeSetup(file);
            auto tmp=file;tmp+=L".tmp-"+std::to_wstring(GetCurrentProcessId());
            std::ofstream out(tmp);out<<j.dump(2);out.close();
            if(!out || !MoveFileExW(tmp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
                std::error_code ec;fs::remove(tmp,ec);throw std::runtime_error("Could not save the weapon setup.");
            }
        }else{
            json j;std::ifstream(file)>>j;
            if(j.contains("fitTransform") && !hand)throw std::runtime_error("This fitted setup needs its original arms rig. Use Open saved setup from the importer.");
            auto savedRig=fs::u8path(j.value("armsModel",std::string{}));
            if(!savedRig.empty())savedRig=(file.parent_path()/savedRig).lexically_normal();
            auto savedPreset=fs::u8path(j.value("armsPreset",std::string{}));
            if(j.contains("localArmsPreset"))savedPreset=(file.parent_path()/fs::u8path(j.at("localArmsPreset").get<std::string>())).lexically_normal();
            else if(!savedPreset.empty())savedPreset=(home/savedPreset).lexically_normal();
            if((!savedRig.empty() && savedRig!=requestedArms.lexically_normal()) ||
               (!savedPreset.empty() && savedPreset!=presetFile.lexically_normal()))
                throw std::runtime_error("Return to the importer and choose Open saved setup to load its arms rig.");
            if(j.at("version")!=1)throw std::runtime_error("Unsupported setup.");
            auto e=j.at("rotation").get<std::array<float,3>>();
            decltype(points) parsed{};for(int i=0;i<2;i++){auto v=j.at("markers").at(i);if(!v.is_null()){auto a=v.get<std::array<float,3>>();parsed[i]=XMFLOAT3(a[0],a[1],a[2]);}}
            std::vector<SpawnPoint> loadedSpawns;std::string socketError;
            if(j.contains("launchSockets")&&!renegade::bridge::ParseLaunchSockets(j.at("launchSockets").dump(),loadedSpawns,socketError))throw std::runtime_error(socketError);
            std::vector<SpawnPoint> loadedCasing;
            if(j.contains("casingEjectionMarker")){
                json wrapper={{"schema_version",1},{"sockets",json::array({j.at("casingEjectionMarker")})}};
                if(!renegade::bridge::ParseLaunchSockets(wrapper.dump(),loadedCasing,socketError))throw std::runtime_error(socketError);
            }
            auto flashes=j.value("muzzleFlashPreview",json::object());
            int style=flashes.value("style",0);
            if(style<0||style>2)throw std::runtime_error("Unknown muzzle flash effect.");
            auto cues=flashes.value("barrelClipTimes",FlashTimes{});
            for(const auto& barrel:cues){
                if(std::none_of(loadedSpawns.begin(),loadedSpawns.end(),[&](const auto& s){return s.name==barrel.first;}))
                    throw std::runtime_error("Muzzle flash refers to a missing PSP.");
                for(const auto& c:barrel.second)if(!std::isfinite(c.second)||c.second<0)throw std::runtime_error("Invalid muzzle flash time.");
            }
            auto loadedEffects=j.value("barrelEffects",json::object());
            if(!loadedEffects.is_object())throw std::runtime_error("Invalid PSP effect settings.");
            for(const auto& entry:loadedEffects.items()){
                const auto& fx=entry.value();
                if(!fx.is_object()||std::none_of(loadedSpawns.begin(),loadedSpawns.end(),[&](const auto& p){return p.name==entry.key();}))
                    throw std::runtime_error("Effect refers to a missing PSP.");
                for(auto key:{"flashSeconds","emitSeconds"}){
                    auto duration=fx.value(key,.075f);
                    if(!std::isfinite(duration)||duration<.01f||duration>60)throw std::runtime_error("Invalid PSP effect duration.");
                }
                if(fx.contains("file")){
                    auto path=fs::u8path(fx.at("file").get<std::string>()).lexically_normal();
                    if(path.has_root_path()||path.generic_u8string().rfind("Content/",0)!=0||path.extension()!=".wiscene")
                        throw std::runtime_error("Invalid particle effect path.");
                }
                auto starts=fx.value("times",std::map<std::string,float>{});
                for(auto& cue:starts)if(!std::isfinite(cue.second)||cue.second<0)throw std::runtime_error("Invalid particle cue time.");
            }
            auto playback=j.value("casingPreview",json::object());
            auto times=playback.value("clipTimes",std::map<std::string,float>{});
            for(const auto& cue:times)if(!std::isfinite(cue.second)||cue.second<0)throw std::runtime_error("Invalid casing ejection time.");
            auto casingPath=fs::u8path(playback.value("model",std::string{}));
            if(!casingPath.empty())casingPath=(file.parent_path()/casingPath).lexically_normal();
            auto visualScale=playback.value("scale",includedCasingScale);
            if(!std::isfinite(visualScale)||visualScale<=0)throw std::runtime_error("Invalid casing scale.");
            const auto mountBone=j.value("mountBone",std::string{});
            if(!mountBone.empty() && !arms.Entity_FindByName(mountBone))throw std::runtime_error("The saved weapon mount is missing from these arms.");
            float savedScale=j.value("scale",1.f);if(!std::isfinite(savedScale)||savedScale<=0)throw std::runtime_error("Invalid saved scale.");setProp((file.parent_path()/fs::u8path(j.at("model").get<std::string>())).lexically_normal());fittedMountBone=mountBone;points=parsed;spawnPoints=std::move(loadedSpawns);if(!loadedCasing.empty())casingSocket=std::move(loadedCasing.front());refreshMarkerList();applyScale(savedScale);
            loopShotSound=j.value("loopShotSound",false);automaticFire=j.value("automaticFire",false);setShotSound(j.value("shotSound",std::string{}));
            audioCues=j.value("animationSounds",AudioCues{});listedClip.clear();resetFlashTexture(flashes.value("texture",std::string{}));
            barrelEffects=std::move(loadedEffects);
            flashTimes=std::move(cues);flashStyle=style;refreshFlashChoices();
            casingTimes=std::move(times);casingAsset=casingPath;casingScale=visualScale;
            for(int i=0;i<3;i++)rotation[i].SetValue(e[i]);if(points[0]&&hand){bool recorded=j.contains("gripOrientation");if(recorded){auto q=j.at("gripOrientation").get<std::array<float,4>>();gripOrientation={q[0],q[1],q[2],q[3]};}mount(!recorded);}
            if(j.contains("fitTransform") && instance){
                auto v=j.at("fitTransform");FitState f;
                auto pos=v.at("position").get<std::array<float,3>>();auto rot=v.at("rotation").get<std::array<float,4>>();auto sc=v.at("scale").get<std::array<float,3>>();
                for(float n:pos)if(!std::isfinite(n))throw std::runtime_error("Invalid fit position.");
                for(float n:rot)if(!std::isfinite(n))throw std::runtime_error("Invalid fit rotation.");
                for(float n:sc)if(!std::isfinite(n)||n<=0)throw std::runtime_error("Invalid fit scale.");
                f.position={pos[0],pos[1],pos[2]};f.rotation={rot[0],rot[1],rot[2],rot[3]};f.scale={sc[0],sc[1],sc[2]};f.modelScale=modelScale;restoreFit(f);
            }
            if(j.contains("magazineSeatTransform")&&magazineBone){auto v=j.at("magazineSeatTransform");auto p=v.at("position").get<std::array<float,3>>();auto q=v.at("rotation").get<std::array<float,4>>();auto sc=v.at("scale").get<std::array<float,3>>();for(float n:p)if(!std::isfinite(n))throw std::runtime_error("Invalid magazine seat.");for(float n:q)if(!std::isfinite(n))throw std::runtime_error("Invalid magazine seat.");for(float n:sc)if(!std::isfinite(n)||n<=0)throw std::runtime_error("Invalid magazine seat.");magazineSeat.translation_local={p[0],p[1],p[2]};magazineSeat.rotation_local={q[0],q[1],q[2],q[3]};magazineSeat.scale_local={sc[0],sc[1],sc[2]};magazineSeat.SetDirty();if(!magazineHeld)*arms.transforms.GetComponent(magazineBone)=magazineSeat;}
            fingerOverrides.clear();for(auto& hand:fingerFit.fingers)for(auto& finger:hand)finger.hasVisible=false;
            fingerFit.read(j.value("fingerGrips",json::object()));refreshFingerUI();
            armFit.read(j.value("armIK",json::object()));applyIK.SetCheck(armFit.enabled);showIK.SetCheck(armFit.visible);
            pickupPhase=j.value("reloadPickup",.17f);seatPhase=j.value("reloadSeat",.77f);
            if(!std::isfinite(pickupPhase)||!std::isfinite(seatPhase)||pickupPhase<0||seatPhase>1||pickupPhase>=seatPhase)throw std::runtime_error("Invalid reload handoff timing.");
            pickupInput.SetValue(pickupPhase*100);seatInput.SetValue(seatPhase*100);
            boltContact.read(j.value("boltContact",json::object()));if(boltContact.captured&&seatPhase>boltContact.grab)throw std::runtime_error("Magazine seating must precede bolt contact.");
            boltGrabInput.SetValue(boltContact.grab*100);boltReleaseInput.SetValue(boltContact.release*100);
            magazineMode=std::clamp(j.value("magazineMode",2),0,2);magazineControl.SetSelectedWithoutCallback(magazineMode);
            if(j.contains("magazineHandOffset")){
                auto v=j.at("magazineHandOffset").get<std::array<float,16>>();for(float n:v)if(!std::isfinite(n))throw std::runtime_error("Invalid magazine grip.");
                for(int row=0;row<4;row++)for(int col=0;col<4;col++)magazineHandOffset.m[row][col]=v[row*4+col];magazineOffsetValid=true;
            }
            if(j.contains("supportGrip"))readGripFit(j.at("supportGrip"));
            else if(hasSupportGrip()){armFit.hierarchy();findSupportGrip();}
            setFullIK(j.value("ikEnabled",armFit.enabled||fingerFit.enabled),false);
            refreshSurfaceList();
            disabledSurfaceMaps=j.value("disabledSurfaceMaps",std::set<std::string>{});
            surfaceEdits=j.value("materials",renegade::bridge::WeaponSurfaces{});
            for(const auto& row:surfaceEdits)for(float v:row.second)if(!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Invalid material setting.");
            applySurfaces();refreshSurfaceValues();
        }if(!writing && assembled){json saved;std::ifstream(file)>>saved;assembledActions=saved.value("actions",assembledActions);actionRole.SetSelected(0);setMode(false);}
        if(!writing){linkEquipment(file);json stored;std::ifstream(file)>>stored;firingDraft=stored.value("primaryFiring",json(nullptr));readMelee(stored);if(melee)refreshMeleeClips();}setupPath=file;lastSaved=describeSetup(file);status.SetText(writing?"Setup saved.":"Setup opened. Click the weapon to select it.");
    }
    void Load() override{
        if(!requestedSetup.empty()){
            json saved;std::ifstream(requestedSetup)>>saved;
            assembled=saved.value("assembled",false);
        }
        camera=&workspaceCamera;
        scene=&arms;RenderPath3D::Load();
        preset = {{"name","Weapon only"},{"left",""},{"right",""},{"camera",""},{"clips",json::array()},{"viewDirection",{0,0,1}}};
        if(!presetFile.empty()){
            std::ifstream(presetFile) >> preset;
            readModel(presetFile.parent_path()/fs::u8path(preset.at("model").get<std::string>()),arms);
            if(!arms.objects.GetCount() || !arms.animations.GetCount())throw std::runtime_error("The selected arms preset has no animated mesh.");
        }
        if(!requestedArms.empty()){
            readModel(requestedArms,arms);
            if(!arms.objects.GetCount() || !arms.armatures.GetCount())throw std::runtime_error("Choose a skinned first-person arms model.");
            if(!assembled && (!arms.Entity_FindByName("hand_r") || !arms.Entity_FindByName("hand_l")))
                throw std::runtime_error("This rig needs a mapped arms preset. The direct arms model option expects hand_l and hand_r.");
            preset["name"]=requestedArms.stem().u8string();
            preset["left"]="hand_l";preset["right"]="hand_r";preset["camera"]="camera_mount";
            if(!arms.Entity_FindByName("camera_mount"))preset.erase("viewDirection");
            for(size_t i=0;i<arms.animations.GetCount();++i){
                auto n=arms.names.GetComponent(arms.animations.GetEntity(i));
                if(n)preset["clips"].push_back({{"source",n->name},{"label",n->name}});
            }
        }
        for(size_t i=0;i<arms.humanoids.GetCount();i++)arms.humanoids[i].SetLookAtEnabled(false);
        armFit.init(arms);fingerFit.init(arms);
        hand=arms.Entity_FindByName(preset.at("right").get<std::string>());light(arms);frame(arms);
        clips.Create("Animation");clips.SetPos({18,35});clips.SetSize({194,28});
        clips.SetText("");clips.SetPos({18,68});
        title.Create("Weapon editor");title.SetText("RENEGADE  /  WEAPON EDITOR");title.SetPos({12,8});title.SetSize({243,24});title.SetColor(wi::Color(37,42,46));GetGUI().AddWidget(&title);
        clipLabel.Create("Arms preset");clipLabel.SetText(preset.at("name").get<std::string>());clipLabel.SetPos({18,40});clipLabel.SetSize({214,22});clipLabel.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&clipLabel);
        GetGUI().AddWidget(&clips);
        int idle=0;
        for(auto& entry:preset.at("clips")){
            for(size_t i=0;i<arms.animations.GetCount();i++){
                auto n=arms.names.GetComponent(arms.animations.GetEntity(i));
                if(n && n->name==entry.at("source").get<std::string>()){
                    if(assembled && clipKey(n->name)=="idle")idle=int(clips.GetItemCount());
                    clips.AddItem(entry.at("label").get<std::string>(),i);
                    break;
                }
            }
        }timeline.Create(0,1,0,1000,"");timeline.SetPos({18,145});timeline.SetSize({160,18});GetGUI().AddWidget(&timeline);
        timeline.OnSlide([this](auto& a){if(!arms.animations.GetCount())return;auto& clip=arms.animations[anim];clip.Pause();clip.timer=a.fValue;clip.last_update_time=-1;fingerOverrides.clear();play.SetText("Play");});
        btn(play,"Play",105,[this]{if(!arms.animations.GetCount())return;auto& a=arms.animations[anim];if(a.IsPlaying())a.Pause();else{fingerOverrides.clear();fingerPoseIndex=0;fingerPoseChoice.SetSelectedWithoutCallback(0);if(fitSelection>=6)selectWeapon(false);a.Play();}play.SetText(a.IsPlaying()?"Pause":"Play");});
        btn(view,"First-person / Orbit",176,[this]{if(!editing)first=!first;});view.SetTooltip("Orbit view: RMB drag rotates, MMB drag pans, wheel zooms. F frames the weapon.");
        btn(backToStudio,"Back to importer",0,[]{leaveRequested=true;});
        backToStudio.SetTooltip("Return to Studio. This weapon session stays open until another setup is selected.");
        btn(open,"Open weapon model...",213,[this]{auto p=choose(false,L"Weapon model\0*.glb;*.gltf;*.fbx;*.obj\0\0",nullptr);if(!p.empty())try{importWeapon(p);}catch(std::exception& e){status.SetText(e.what());}});
        btn(mainGrip,"Place main grip",240,[this]{armMarker(0);});
        btn(support,"Place support hand",272,[this]{armMarker(1);});
        btn(assemble,"Preview with arms",310,[this]{mount();});
        for(int i=0;i<3;i++){auto& r=rotation[i];r.Create(-180,180,0,360,"");r.SetText(std::string(1,"XYZ"[i]));r.SetPos({40,360.f+30*i});r.SetSize({130,16});r.OnSlide([this](auto&){align();});GetGUI().AddWidget(&r);r.SetVisible(false);}
        for(auto& r:rotation)r.SetVisible(false);
        fitLabel.Create("Weapon transform");fitLabel.SetPos({18,351});fitLabel.SetSize({232,53});fitLabel.SetText("Click the weapon to select it.");fitLabel.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&fitLabel);
        btn(selectWeaponButton,"Select weapon",412,[this]{if(editing)mount(false);selectWeapon(true);});
        btn(frameWeaponButton,"Frame weapon  [F]",443,[this]{if(editing)mount(false);frameWeapon();});
        scaleLabel.Create("Scale");scaleLabel.SetText("Uniform scale");scaleLabel.SetPos({18,481});scaleLabel.SetSize({106,24});GetGUI().AddWidget(&scaleLabel);scaleLabel.SetVisible(false);scaleInput.Create("Uniform scale");scaleInput.SetPos({128,481});scaleInput.SetSize({104,24});scaleInput.SetValue(1.f);
        scaleInput.OnInputAccepted([this](auto& e){applyScale(e.fValue);});GetGUI().AddWidget(&scaleInput);scaleInput.SetVisible(false);
        sizeLabel.Create("Dimensions");sizeLabel.SetPos({18,512});sizeLabel.SetSize({230,25});GetGUI().AddWidget(&sizeLabel);sizeLabel.SetVisible(false);
        btn(matchSize,"Match reference size",543,[this]{if(referenceLength>0 && longest(sourceSize)>0)applyScale(referenceLength/longest(sourceSize));});
        matchSize.SetVisible(false);
        btn(save,"Save setup...",580,[this]{if(model.empty()){status.SetText("Open a weapon model first.");return;}try{setupFile(true);}catch(std::exception& e){status.SetText(e.what());}});
        btn(load,"Open setup...",613,[this]{try{setupFile(false);}catch(std::exception& e){status.SetText(e.what());}});
        status.Create("Status");status.SetPos({18,654});status.SetSize({250,54});status.SetText("RMB orbit  |  Wheel zoom");status.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&status);
        const char* toolNames[]={"Move  [W]","Rotate  [E]","Scale  [R]"};
        for(int i=0;i<3;i++){auto& b=tools[i];b.Create(toolNames[i]);b.SetPos({290.f+112*i,15});b.SetSize({106,31});b.OnClick([this,i](auto&){setTool(i);});GetGUI().AddWidget(&b);b.SetColor(wi::Color(110,61,31),wi::gui::FOCUS);}
        btn(undoButton,"Undo",15,[this]{undoFit();});undoButton.SetPos({636,15});undoButton.SetSize({74,31});
        btn(redoButton,"Redo",15,[this]{undoFit(true);});redoButton.SetPos({716,15});redoButton.SetSize({74,31});
        magazineLabel.Create("Magazine attachment");magazineLabel.SetText("Magazine attachment");magazineLabel.SetPos({18,726});magazineLabel.SetSize({232,24});magazineLabel.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&magazineLabel);magazineLabel.SetVisible(false);
        magazineControl.Create("Magazine");magazineControl.SetText("");magazineControl.SetPos({18,757});magazineControl.SetSize({214,28});
        magazineControl.AddItem("In weapon");magazineControl.AddItem("Support hand");magazineControl.AddItem("Auto reload");
        magazineControl.SetSelectedWithoutCallback(2);magazineControl.OnSelect([this](auto& e){if(e.iValue==1){requestGrab();pendingGrabPickup=false;}else{rememberFit();magazineMode=e.iValue;updateMagazineAttachment();}});
        GetGUI().AddWidget(&magazineControl);magazineControl.SetVisible(false);
        armPanel.Create("Arm fitting");armPanel.SetText("ARM FIT  /  IK CONTROLS");armPanel.SetSize({244,27});armPanel.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&armPanel);
        applyIK.Create("Apply arm IK");applyIK.SetCheck(true);applyIK.SetSize({22,22});applyIK.OnClick([this](auto& e){rememberFit();armFit.enabled=e.bValue;});GetGUI().AddWidget(&applyIK);
        showIK.Create("Show hand / elbow controls");showIK.SetCheck(true);showIK.SetSize({22,22});showIK.OnClick([this](auto& e){armFit.visible=e.bValue;});GetGUI().AddWidget(&showIK);
        const char* controlNames[]={"Left hand  /  Move + Rotate","Right hand  /  Move + Rotate","Left elbow  /  Pole","Right elbow  /  Pole","Fit magazine in support hand"};
        for(int i=0;i<5;i++){btn(armSelect[i],controlNames[i],188.f+35*i,[this,i]{selectFit(i+1);});armSelect[i].SetSize({244,29});}
        btn(grabHere,"Grab magazine here",363,[this]{requestGrab();});grabHere.SetSize({244,29});grabHere.SetTooltip("Capture this visible grip and make the magazine follow the support hand.");
        btn(seatNow,"Seat magazine now",398,[this]{requestSeat(currentClip()=="Reload");});seatNow.SetSize({244,29});
        btn(armFrame,"Frame arms and controls",373,[this]{frameArmsControls();});armFrame.SetSize({244,29});
        btn(armReset,"Reset selected IK control",408,[this]{if(fitSelection<1||fitSelection>4)return;rememberFit();armFit.reset(fitSelection-1);});armReset.SetSize({244,29});
        armHint.Create("IK editing");armHint.SetText("Pause or scrub a pose, then drag.\nOffsets save for this animation.\nElbow poles keep arm lengths fixed.");armHint.SetSize({244,61});armHint.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&armHint);
        eventLabel.Create("Reload handoff");eventLabel.SetText("RELOAD HANDOFF");eventLabel.SetSize({244,24});eventLabel.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&eventLabel);
        pickupLabel.Create("Pickup");pickupLabel.SetText("Pickup %");pickupLabel.SetSize({120,24});pickupLabel.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&pickupLabel);
        seatLabel.Create("Reseat");seatLabel.SetText("Reseat %");seatLabel.SetSize({120,24});seatLabel.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&seatLabel);
        pickupInput.Create("Pickup percent");pickupInput.SetSize({108,24});pickupInput.SetValue(17.f);pickupInput.OnInputAccepted([this](auto& e){timing(e.fValue/100,seatPhase);});GetGUI().AddWidget(&pickupInput);
        seatInput.Create("Reseat percent");seatInput.SetSize({108,24});seatInput.SetValue(77.f);seatInput.OnInputAccepted([this](auto& e){timing(pickupPhase,e.fValue/100);});GetGUI().AddWidget(&seatInput);
        btn(pickupHere,"Set pickup at current time",622,[this]{requestGrab(true);});pickupHere.SetSize({244,29});
        btn(seatHere,"Set reseat at current time",657,[this]{requestSeat(true);});seatHere.SetSize({244,29});
        boltCaption.Create("Bolt contacts");boltCaption.SetText("BOLT / CHARGING HANDLE");boltCaption.SetSize({244,24});boltCaption.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&boltCaption);
        btn(boltGrabHere,"Set bolt grab at current time",797,[this]{requestBolt(true);});boltGrabHere.SetSize({244,29});
        btn(boltReleaseHere,"Set bolt release at current time",832,[this]{requestBolt(false);});boltReleaseHere.SetSize({244,29});
        boltGrabLabel.Create("Bolt grab");boltGrabLabel.SetText("Bolt grab %");boltGrabLabel.SetSize({120,24});boltGrabLabel.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&boltGrabLabel);
        boltReleaseLabel.Create("Bolt release");boltReleaseLabel.SetText("Bolt release %");boltReleaseLabel.SetSize({120,24});boltReleaseLabel.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&boltReleaseLabel);
        boltGrabInput.Create("Bolt grab");boltGrabInput.SetSize({108,24});boltGrabInput.SetValue(82.f);boltGrabInput.OnInputAccepted([this](auto& e){boltTiming(e.fValue/100,boltContact.release);});GetGUI().AddWidget(&boltGrabInput);
        boltReleaseInput.Create("Bolt release");boltReleaseInput.SetSize({108,24});boltReleaseInput.SetValue(96.f);boltReleaseInput.OnInputAccepted([this](auto& e){boltTiming(boltContact.grab,e.fValue/100);});GetGUI().AddWidget(&boltReleaseInput);
        reloadStage.Create("Reload stage");reloadStage.SetSize({244,46});reloadStage.SetColor(wi::Color(31,35,38));GetGUI().AddWidget(&reloadStage);
        createFingerPanel();createPosePanel();createAuditPanel();createEditorHelp();createCleanUI();layoutArmPanel();layoutFingerPanel();layoutPosePanel();layoutAuditPanel();layoutEditorHelp();
        setTool(0);gizmo.translate_snap=.01f;gizmo.rotate_snap=XMConvertToRadians(5.f);gizmo.scale_snap=.05f;gizmo.tool_scale=.65f;
        clips.OnSelect([this](auto& a){selectClip((int)a.userdata);});if(arms.animations.GetCount())clips.SetSelected(idle); arms.Update(0.001f); frameArms(); first=true;
        if(assembled){setFullIK(false,false);if(arms.animations.GetCount())pauseForFit();}
        else if(arms.animations.GetCount())startOriginalArms();
        else {setFullIK(false,false);first=false;}
        if(assembled)createActionMapping();
        if(!requestedSetup.empty())setupFile(false,requestedSetup);
        else if(assembled){setProp(requestedArms);setMode(false);}
        else if(!requestedModel.empty())importWeapon(requestedModel);
        else if(preset.contains("projectSetup") && fs::is_regular_file(projectRoot/fs::u8path(preset.at("projectSetup").get<std::string>())))setupFile(false,projectRoot/fs::u8path(preset.at("projectSetup").get<std::string>()));
        else if(preset.contains("setup"))setupFile(false,presetFile.parent_path()/fs::u8path(preset.at("setup").get<std::string>()));
        else if(preset.contains("weapon"))importWeapon(presetFile.parent_path()/fs::u8path(preset.at("weapon").get<std::string>()));
        else if(!hand)setMode(true);
    }
    void Update(float dt) override{
        fingerFit.restoreBase();armFit.restoreBase();armFit.phase=posePhase();refreshPoseValues();layoutEditorHelp();layoutCleanUI();
        auto p=wi::input::GetPointer();handleGripTimeline(p);
        const bool viewport=p.x>cleanLeft() && p.x<GetLogicalWidth()-cleanRight() && p.y>viewportTop && p.y<viewportBottom() && !GetGUI().HasFocus();
        if(!GetGUI().IsTyping()){
            if(wi::input::Press((wi::input::BUTTON)'W'))setTool(0);
            if(wi::input::Press((wi::input::BUTTON)'E'))setTool(1);
            if(wi::input::Press((wi::input::BUTTON)'R'))setTool(2);
            if(wi::input::Press((wi::input::BUTTON)'F'))frameCleanSelection();
            const bool ctrl=wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL)||wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);
            if(ctrl&&wi::input::Press((wi::input::BUTTON)'S')){try{setupFile(true,setupPath);}catch(const std::exception& e){status.SetText(e.what());}}
            if(ctrl&&wi::input::Press((wi::input::BUTTON)'Z'))undoFit();
            if(ctrl&&wi::input::Press((wi::input::BUTTON)'Y'))undoFit(true);
        }
        const bool middleDown=GetForegroundWindow()==editorWindow&&(GetAsyncKeyState(VK_MBUTTON)&0x8000)!=0;
        const bool startPan=viewport&&!first&&!draggingFit&&middleDown&&!orbitPanActive;
        if(startPan)orbitPanActive=true;
        if(!middleDown||first||draggingFit)orbitPanActive=false;
        if(orbitPanActive&&!startPan){
            auto direction=XMVectorSet(-sinf(yaw)*cosf(pitch),-sinf(pitch),cosf(yaw)*cosf(pitch),0);
            auto right=XMVector3Normalize(XMVector3Cross(XMVectorSet(0,1,0,0),direction));
            auto up=XMVector3Normalize(XMVector3Cross(direction,right));
            const float unitsPerPixel=distance*2*tanf(camera->fov*.5f)/std::max(1.f,GetLogicalHeight());
            XMStoreFloat3(&center,XMLoadFloat3(&center)+(right*(previous.x-p.x)+up*(p.y-previous.y))*unitsPerPixel);
        }
        const bool turn=viewport&&!first&&!middleDown&&!draggingFit&&!markerPressed&&GetForegroundWindow()==editorWindow&&
            (GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
        if(turn&&orbitTurnActive){yaw+=(p.x-previous.x)*.008f;pitch=std::clamp(pitch+(p.y-previous.y)*.008f,-1.5f,1.5f);}
        orbitTurnActive=turn;
        if(orbitWheelSteps!=0){
            if(!first&&!draggingFit&&!markerPressed)distance=std::clamp(distance*std::exp(-orbitWheelSteps*.13f),.035f,2000.f);
            orbitWheelSteps=0;
        }
        previous=p;

        XMVECTOR target=XMLoadFloat3(&center);
        XMVECTOR from=target+XMVectorSet(sinf(yaw)*cosf(pitch),sinf(pitch),-cosf(yaw)*cosf(pitch),0)*distance;
        if(first && assembled && arms.cameras.GetCount())frameArms();
        if(first){from=XMLoadFloat3(&eye);target=from+XMLoadFloat3(&forward);}
        if(!first){
            auto dir=XMVector3Normalize(target-from),right=XMVector3Normalize(XMVector3Cross(XMVectorSet(0,1,0,0),dir)),up=XMVector3Normalize(XMVector3Cross(dir,right));
            float dx=(cleanLeft()-cleanRight())*.5f,dy=(viewportTop+viewportBottom()-GetLogicalHeight())*.5f;
            auto offset=(-right*dx+up*dy)*(distance*2*tanf(camera->fov*.5f)/GetLogicalHeight());from+=offset;target+=offset;
        }
        camera->zNearP=.015f;camera->TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(from,target,first&&assembled&&arms.cameras.GetCount()?XMLoadFloat3(&arms.cameras[0].Up):XMVectorSet(0,1,0,0))));camera->UpdateCamera();
        if(arms.animations.GetCount())timeline.SetValue(arms.animations[anim].timer);
        auditBeginFrame();syncPreviewItem();RenderPath3D::Update(dt);
        fingerFit.captureBase();armFit.phase=posePhase();armFit.clip=currentClip();armFit.captureBase();armFit.targets(draggingFit&&fitSelection>0&&fitSelection<5?fitSelection-1:-1);
        armFit.solve(2);boltContact.apply(posePhase(),currentClip()=="Reload");armFit.hierarchy();
        solveSupport(draggingFit&&fitSelection==1);applyFingers();fingerFit.syncTargets(fingerKey(),draggingFit&&fitSelection>=6?fitSelection-6:-1);
        if(editing){
            updateMarkerPlacement(p,viewport);
            if(marker<0&&!wi::input::Release(wi::input::MOUSE_BUTTON_LEFT))updateSpawnHandle(p,viewport);
            else if(draggingFit)updateSpawnHandle(p,viewport);
            drawSpawnDirections();
        }
        else if(!gizmo.selected.empty()){
            auto before=fitState();
            XMFLOAT4 pointer=p;
            if(!viewport&&!draggingFit){pointer.x=-10000;pointer.y=-10000;}
            gizmo.Update(*camera,pointer,*this);
            if(gizmo.IsDragStarted()){dragBefore=before;draggingFit=true;pauseForFit();if(fitSelection>=6){fingerDragGrip=fingerKey();int k=fitSelection>=16?(fitSelection-16)/3:fitSelection-6;fingerOverrides[k]=fingerDragGrip;
                    fingerFit.prepareJoint(fingerDragGrip,k/5,k%5);
                }}
            if(draggingFit && fitSelection>0){
                if(fitSelection==1&&boltContact.captured&&currentClip()=="Reload"&&posePhase()>=boltContact.grab&&posePhase()<=boltContact.release)boltContact.editTarget(armFit);
                else if(fitSelection==1&&activeGripWeight()>0)saveGripDrag();
                else if(fitSelection<5)armFit.edited(fitSelection-1);
                else if(fitSelection==5){auto mag=arms.transforms.GetComponent(magazineBone);if(magazineHeld){XMStoreFloat4x4(&magazineHandOffset,mag->GetLocalMatrix());magazineOffsetValid=true;}else magazineSeat=*mag;}
                else if(fitSelection>=16){int k=(fitSelection-16)/3;fingerFit.editJoint(fingerDragGrip,k/5,k%5,(fitSelection-16)%3);}
                else fingerFit.edited(fingerDragGrip,(fitSelection-6)/5,(fitSelection-6)%5);
            }
            if(draggingFit && fitSelection==0){
                auto current=fitState();bool valid=true;
                for(int i=0;i<3;i++)valid=valid&&std::isfinite((&current.position.x)[i])&&std::isfinite((&current.scale.x)[i])&&((&current.scale.x)[i]>.000001f);
                for(int i=0;i<4;i++)valid=valid&&std::isfinite((&current.rotation.x)[i]);
                if(!valid){restoreFit(before);status.SetText("Scale must stay above zero.");}
                hasFit=true;
                auto t=arms.transforms.GetComponent(instance);auto parent=arms.transforms.GetComponent(hand);
                XMVECTOR ws,wr,wp;
                auto world=t->GetLocalMatrix()*XMLoadFloat4x4(&parent->world);
                if(XMMatrixDecompose(&ws,&wr,&wp,world)){
                    XMFLOAT3 sc;XMStoreFloat3(&sc,ws);modelScale=std::cbrt(std::abs(sc.x*sc.y*sc.z));scaleInput.SetValue(modelScale);updateSizeLabel();
                }
            }
            if(gizmo.IsDragEnded()&&draggingFit){
                if(selectedWeapon&&!armFit.enabled)gripFit.bindOnPreview=true;
                undoFits.push_back(dragBefore);redoFits.clear();draggingFit=false;
                status.SetText("Fit updated. Save setup to keep it.");
            }
        }
        if(!editing&&viewport&&wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)&&!draggingFit&&
           (gizmo.selected.empty()||!gizmo.IsInteracting())){
            auto hit=scene->Intersects(wi::renderer::GetPickRay((long)p.x,(long)p.y,*this,*camera));
            int control=pickFingertip(p);if(!control)control=pickArmControl(p);
            if(control)selectFit(control);else selectHit(hit);
        }
        if(draggingFit&&fitSelection==2)armFit.solve(2);
        boltContact.apply(posePhase(),currentClip()=="Reload");armFit.hierarchy();
        solveSupport();applyFingers();fingerFit.syncTargets(fingerKey(),draggingFit&&fitSelection>=6?fitSelection-6:-1);
        if(fitSelection==1||fitSelection==2){
            auto& c=armFit.chain[fitSelection-1];
            if(c.error>.005f&&draggingFit)status.SetText("Target is beyond reach. Arm lengths stay fixed.");
        }
        if(!editing){
            bool action=magazineActions();
            if(!action){float poseTime=arms.animations.GetCount()?arms.animations[anim].last_update_time:-1;updateMagazineAttachment(poseTime);}
            boltActions();showReloadStage(posePhase());armFit.refreshSkin();fingerFit.cacheVisible();drawArmControls();drawFingerControls();refreshFingerUI();auditEndFrame();
            if(!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT))fingerGesture=false;
        }
        if(!melee)updateCasingPreview(dt);
        updateFlashPlayback(dt);
        if(!melee&&arms.animations.GetCount())updateShotPreview(dt);
    }
};

#include "WeaponPublish.inc"
#include "UpdateWeapon.inc"
#include "WorkspaceSession.inc"
