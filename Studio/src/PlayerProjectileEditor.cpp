#include "StudioApplication.h"
#include "renegade/bridge/EquipmentAssetService.h"
#include "renegade/bridge/ProjectileAssetService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include <filesystem>
#include <cmath>

#include <algorithm>
#include <cctype>
#include <map>
#include <iomanip>
#include <sstream>

namespace renegade::studio
{
    namespace
    {
        std::string ProjectileFlightLabel(float value)
        {
            std::ostringstream text;
            text << std::setprecision(4) << value;
            return text.str();
        }
        const char* ProjectileActionLabel(bridge::EquipmentAction action)
        {
            switch(action)
            {
            case bridge::EquipmentAction::PrimaryUse: return "Primary fire / use";
            case bridge::EquipmentAction::AlternateUse: return "Secondary fire / use";
            case bridge::EquipmentAction::Release: return "Release charged action";
            case bridge::EquipmentAction::Cast: return "Cast";
            case bridge::EquipmentAction::Use: return "Use";
            default: return nullptr;
            }
        }
        std::string LowerProjectileSearch(std::string value)
        {
            for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return value;
        }
    }

    void StudioRenderPath::CreateProjectileEditor()
    {
        weaponProjectilePanel_.Create("Weapon projectiles");
        CreateLaunchSocketEditor();
        weaponProjectilePanel_.SetPos({70,80});weaponProjectilePanel_.SetSize({740,630});
        const auto combo=[&](wi::gui::ComboBox& box,const char* name,float y) {
            box.Create(name);box.SetPos({220,y});box.SetSize({480,26});
            weaponProjectilePanel_.AddWidget(&box);
        };
        combo(projectileWeapon_,"Weapon hand",40);
        projectileWeapon_.AddItem("Primary hand");projectileWeapon_.AddItem("Off hand");
        combo(projectileAction_,"Launch on",80);
        projectileSearch_.Create("Search projectiles");
        projectileSearch_.SetDescription("Search (Enter): ");
        projectileSearch_.SetPos({220,120});projectileSearch_.SetSize({480,26});
        projectileSearch_.SetCancelInputEnabled(false);
        weaponProjectilePanel_.AddWidget(&projectileSearch_);
        projectileSearch_.OnInputAccepted([this](const wi::gui::EventArgs&){RefreshProjectileChoices();});
        combo(projectileChoice_,"Projectile",160);
        combo(projectileSocket_,"First PSP",200);
        combo(projectileSecondSocket_,"Second PSP",240);
        combo(projectileSocketPolicy_,"Spawn points",280);
        projectileSocketPolicy_.AddItem("First PSP only");
        projectileSocketPolicy_.AddItem("Alternate first / second PSP");
        projectileSocketPolicy_.AddItem("Both PSPs together");
        projectileReleaseTime_.Create(0,5,0,1000,"Release at (seconds)");
        projectileReleaseTime_.SetPos({220,325});projectileReleaseTime_.SetSize({400,22});
        projectileReleaseTime_.SetTooltip("Seconds from the start of the firing/release animation. 0 fires immediately. Must be before the animation ends.");
        weaponProjectilePanel_.AddWidget(&projectileReleaseTime_);
        projectileSummary_.Create("Projectile summary");
        projectileSummary_.SetPos({20,410});projectileSummary_.SetSize({680,90});
        projectileSummary_.font.params.size=14;weaponProjectilePanel_.AddWidget(&projectileSummary_);
        const auto button=[&](wi::gui::Button& b,const char* label,float x,float y,float width,
                              wi::gui::Window& panel) {
            b.Create(label);b.SetText(label);b.SetPos({x,y});b.SetSize({width,30});panel.AddWidget(&b);
        };
        button(projectileAssign_,"APPLY TO THIS PLAYER",20,540,260,weaponProjectilePanel_);
        button(projectileNew_,"NEW",295,540,105,weaponProjectilePanel_);
        button(projectileEditCopy_,"EDIT COPY",415,540,145,weaponProjectilePanel_);
        button(projectileClose_,"CLOSE",575,540,125,weaponProjectilePanel_);
        button(projectileSocketEdit_,"EDIT PROJECTILE SPAWN POINTS...",220,370,300,weaponProjectilePanel_);
        projectileSocketEdit_.OnClick([this](const wi::gui::EventArgs&){
            if(!session_||!session_->Projects().HasProject())return;
            const auto& project=session_->Projects().CurrentProject();
            const auto settings=bridge::CapturePlayerControllerSettings(session_->Scenes().GetScene(),equipmentPlayer_);
            const auto id=projectileWeapon_.GetSelected()==1?settings.offHandEquipmentAssetId:settings.primaryEquipmentAssetId;
            bridge::EquipmentAssetDocument item;std::string error;
            if(!bridge::LoadEquipmentAsset(project.rootPath,project.projectId,id,item,error)){projectileSummary_.SetText(error);return;}
            weaponProjectilePanel_.SetVisible(false);
            OpenAssemblyEditor(item.equipment.presentationAssetId,true);
        });
        projectileWeapon_.OnSelect([this](const wi::gui::EventArgs&){RefreshWeaponProjectileEditor();});
        projectileAction_.OnSelect([this](const wi::gui::EventArgs&){
            projectilePreferred_.clear();projectileSocketNames_.clear();projectileSocket_.ClearItems();
            if(!session_||!session_->Projects().HasProject())return;
            const auto& project=session_->Projects().CurrentProject();
            if(project.projectId!=equipmentProject_||
               !bridge::IsPlayerStart(session_->Scenes().GetScene(),equipmentPlayer_))return;
            const auto row=projectileAction_.GetSelected();
            if(row<0||static_cast<std::size_t>(row)>=projectileActions_.size())return;
            const auto settings=bridge::CapturePlayerControllerSettings(session_->Scenes().GetScene(),equipmentPlayer_);
            const auto id=projectileWeapon_.GetSelected()==1?settings.offHandEquipmentAssetId:settings.primaryEquipmentAssetId;
            bridge::EquipmentAssetDocument item;std::string error;
            if(bridge::LoadEquipmentAsset(project.rootPath,project.projectId,id,item,error))
                for(const auto& binding:item.equipment.projectiles)
                    if(binding.action==projectileActions_[row])projectilePreferred_=binding.projectileAssetId;
            RefreshProjectileChoices();
        });
        projectileChoice_.OnSelect([this](const wi::gui::EventArgs&){
            projectilePreferred_=projectileChoice_.GetSelectedUserdata()<projectileChoices_.size()?
                projectileChoices_[projectileChoice_.GetSelectedUserdata()]:"";
            RefreshProjectileChoices();
        });
        projectileClose_.OnClick([this](const wi::gui::EventArgs&){
            weaponProjectilePanel_.SetVisible(false);projectileCreatePanel_.SetVisible(false);
        });

        projectileCreatePanel_.Create("New projectile");
        projectileCreatePanel_.SetPos({120,100});projectileCreatePanel_.SetSize({1260,820});
        projectilePreset_.Create("Start with");
        projectilePreset_.SetPos({210,40});projectilePreset_.SetSize({390,26});
        for(unsigned i=0;i<5;++i)
            projectilePreset_.AddItem(bridge::ProjectilePresetName(static_cast<bridge::ProjectilePreset>(i)));
        projectilePreset_.AddItem("Custom / saved projectile");
        projectileCreatePanel_.AddWidget(&projectilePreset_);
        projectileName_.Create("Projectile name");projectileName_.SetDescription("Name: ");
        projectileName_.SetPos({210,80});projectileName_.SetSize({390,26});
        projectileName_.SetCancelInputEnabled(false);projectileCreatePanel_.AddWidget(&projectileName_);
        const auto slider=[&](wi::gui::Slider& value,const char* label,float min,float max,float initial,float y) {
            value.Create(min,max,initial,1000,label);value.SetPos({210,y});value.SetSize({340,22});
            value.SetTooltip("Drag to adjust, or click the number and type a value. Press Enter to apply.");
            value.valueInputField.SetTooltip("Type a value and press Enter. Double-click the number to replace it.");
            projectileCreatePanel_.AddWidget(&value);
        };
        slider(projectileSpeed_,"Speed (m/s)",0.1f,2000,300,125);
        slider(projectileGravity_,"Gravity multiplier",0,10,0,170);
        slider(projectileLifetime_,"Lifetime (seconds)",0.05f,120,5,215);
        projectileMesh_.Create("Visible model");
        projectileMesh_.SetPos({210,255});projectileMesh_.SetSize({390,26});
        projectileCreatePanel_.AddWidget(&projectileMesh_);
        projectileMesh_.OnSelect([this](const wi::gui::EventArgs& args){
            projectileDraftMesh_=args.iValue>0 && static_cast<size_t>(args.iValue)<=projectileMeshChoices_.size()
                ? projectileMeshChoices_[args.iValue-1] : "";
        });
        slider(projectileVisualScale_,"Model scale",0.001f,10,1,305);
        slider(projectileRotationX_,"Model rotation X",-180,180,0,345);
        slider(projectileRotationY_,"Model rotation Y",-180,180,0,385);
        slider(projectileRotationZ_,"Model rotation Z",-180,180,0,425);
        button(projectileImportMesh_,"IMPORT MESH...",20,700,180,projectileCreatePanel_);
        projectileImportMesh_.OnClick([this](const wi::gui::EventArgs&){
            OpenStaticModelImporter(true);
        });
        projectileCreateHelp_.Create("Projectile flight help");
        projectileCreateHelp_.SetText("Gravity 0 flies straight; 1 uses normal gravity.\nProjectile stops on contact or when its lifetime expires.\nSave adds it to this project's reusable projectile list.");
        projectileCreateHelp_.SetPos({20,735});projectileCreateHelp_.SetSize({680,30});
        projectileCreateHelp_.font.params.size=14;projectileCreatePanel_.AddWidget(&projectileCreateHelp_);
        button(projectileSave_,"SAVE AS NEW",20,770,280,projectileCreatePanel_);
        button(projectileCancel_,"CANCEL",320,770,280,projectileCreatePanel_);
        projectilePreviewImage_.Create("Projectile model preview");
        projectilePreviewImage_.SetText("");
        projectilePreviewImage_.SetPos({720,80});projectilePreviewImage_.SetSize({512,320});
        projectilePreviewImage_.SetColor(wi::Color::White());
        projectileCreatePanel_.AddWidget(&projectilePreviewImage_);
        projectilePreviewInfo_.Create("Projectile preview information");
        projectilePreviewInfo_.SetPos({720,550});projectilePreviewInfo_.SetSize({512,72});
        projectilePreviewInfo_.font.params.size=14;
        projectileCreatePanel_.AddWidget(&projectilePreviewInfo_);
        button(projectilePreviewFit_,"FIT / RESET VIEW",720,420,180,projectileCreatePanel_);
        projectilePreviewFit_.OnClick([this](const wi::gui::EventArgs&){
            if (!projectilePreview_) return;
            projectilePreviewDrag_=0;
            projectilePreview_->FitModel();projectilePreview_->SetView(XM_PIDIV2,0);
        });
        projectilePreviewImage_.SetTooltip("Left-drag to orbit; right-drag to pan; mouse wheel to zoom.");
        projectilePreviewInfo_.SetPos({920,410});projectilePreviewInfo_.SetSize({315,46});
        projectilePreviewInfo_.font.params.size=12;
        slider(projectileDamage_,"Damage",0,1000,10,470);
        projectileImpactMode_.Create("On impact");projectileImpactMode_.SetPos({210,515});projectileImpactMode_.SetSize({390,26});
        projectileImpactMode_.AddItem("Disappear");projectileImpactMode_.AddItem("Stick into the object");
        projectileCreatePanel_.AddWidget(&projectileImpactMode_);
        slider(projectileStuckLife_,"Stay for (seconds)",.1f,120,30,560);
        slider(projectileEmbedDepth_,"Embed depth (metres)",0,2,.05f,605);
        const auto effectChoice=[&](wi::gui::ComboBox& box,const char* label,float y) {
            box.Create(label);box.SetPos({930,y});box.SetSize({300,26});
            for(const char* name:{"None","Flame","Smoke","Sparks","Tracer"})box.AddItem(name);
            projectileCreatePanel_.AddWidget(&box);
        };
        effectChoice(projectileEffectA_,"Flight effect 1",470);
        effectChoice(projectileEffectB_,"Flight effect 2",510);
        effectChoice(projectileImpactEffect_,"Impact effect",550);
        const auto effectSlider=[&](wi::gui::Slider& value,const char* label,float min,float max,float initial,float y) {
            value.Create(min,max,initial,1000,label);value.SetPos({930,y});value.SetSize({250,22});
            value.SetTooltip("Applies to both flight effect layers. Type a number and press Enter.");
            projectileCreatePanel_.AddWidget(&value);
        };
        effectSlider(projectileEffectSize_,"Particle size (m)",.005f,2,.08f,595);
        effectSlider(projectileEffectRate_,"Particles / second",1,500,60,640);
        effectSlider(projectileEffectLife_,"Particle life (s)",.02f,5,.3f,685);
        effectSlider(projectileEffectOffset_,"Along arrow (m)",-10,10,.3f,730);
        projectileImpactMode_.SetTooltip("Stick retains the model at contact and follows the hit object until Stay for expires.");
        projectileEffectOffset_.SetTooltip("Forward offset in model space: positive moves effects towards the tip (+Z).");

        projectileCancel_.OnClick([this](const wi::gui::EventArgs&){projectileCreatePanel_.SetVisible(false);});
        projectilePreset_.OnSelect([this](const wi::gui::EventArgs& args){
            if (args.iValue>=5) return;
            const auto d=bridge::MakeProjectilePreset(static_cast<bridge::ProjectilePreset>(args.iValue));
            projectileDraftDamage_=d.damage;SetProjectileEffectControls(d);
            projectileName_.SetText(d.name);projectileSpeed_.SetValue(d.speedMetresPerSecond);
            projectileGravity_.SetValue(d.gravityScale);projectileLifetime_.SetValue(d.lifetimeSeconds);
        });
        projectileNew_.OnClick([this](const wi::gui::EventArgs&){
            const auto d=bridge::MakeProjectilePreset(bridge::ProjectilePreset::Bullet);
            projectileStandaloneEditor_=false;
            projectileEditorProject_=equipmentProject_;
            projectileDraftMesh_.clear(); RefreshProjectileMeshChoices();
            projectileVisualScale_.SetValue(1);
            projectileRotationX_.SetValue(0);projectileRotationY_.SetValue(0);projectileRotationZ_.SetValue(0);
            projectileDraftDamage_=d.damage;SetProjectileEffectControls(d);
            projectilePreset_.SetSelectedWithoutCallback(0);projectileName_.SetText(d.name);
            projectileSpeed_.SetValue(d.speedMetresPerSecond);projectileGravity_.SetValue(d.gravityScale);
            projectileLifetime_.SetValue(d.lifetimeSeconds);projectileCreatePanel_.SetVisible(true);projectileCreatePanel_.Activate();
        });
        projectileEditCopy_.OnClick([this](const wi::gui::EventArgs&){
            if(!session_||!session_->Projects().HasProject()||projectilePreferred_.empty())return;
            const auto& project=session_->Projects().CurrentProject();
            if(project.projectId!=equipmentProject_)return;
            bridge::ProjectileAssetDocument d;std::string error;
            if(!bridge::LoadProjectileAsset(project.rootPath,project.projectId,projectilePreferred_,d,error)) {
                projectileSummary_.SetText(error);return;
            }
            projectileDraftDamage_=d.damage;SetProjectileEffectControls(d);
            projectileStandaloneEditor_=false;
            projectileEditorProject_=project.projectId;
            projectileDraftMesh_=d.meshAssetId;RefreshProjectileMeshChoices();
            projectileVisualScale_.SetValue(d.visualScale);
            projectileRotationX_.SetValue(d.visualRotationDegrees[0]);
            projectileRotationY_.SetValue(d.visualRotationDegrees[1]);
            projectileRotationZ_.SetValue(d.visualRotationDegrees[2]);
            projectilePreset_.SetSelectedWithoutCallback(5);
            projectileName_.SetText(d.name+" copy");projectileSpeed_.SetValue(d.speedMetresPerSecond);
            projectileGravity_.SetValue(d.gravityScale);projectileLifetime_.SetValue(d.lifetimeSeconds);
            projectileCreatePanel_.SetVisible(true);projectileCreatePanel_.Activate();
        });
        projectileSave_.OnClick([this](const wi::gui::EventArgs&){
            bridge::ProjectileAssetDocument d;
            d.name=projectileName_.GetText();d.speedMetresPerSecond=projectileSpeed_.GetValue();
            d.gravityScale=projectileGravity_.GetValue();d.lifetimeSeconds=projectileLifetime_.GetValue();
            d.damage=projectileDamage_.GetValue();d.meshAssetId=projectileDraftMesh_;
            d.flightEffects=ProjectileEffectControls();
            d.impactEffect=static_cast<bridge::ProjectileEffectKind>(projectileImpactEffect_.GetSelected());
            d.stickOnImpact=projectileImpactMode_.GetSelected()==1;
            d.stuckLifetimeSeconds=projectileStuckLife_.GetValue();d.embedDepthMetres=projectileEmbedDepth_.GetValue();
            d.visualScale=projectileVisualScale_.GetValue();
            d.visualRotationDegrees={projectileRotationX_.GetValue(),projectileRotationY_.GetValue(),projectileRotationZ_.GetValue()};
            const auto projectId=projectileEditorProject_;const auto target=equipmentPlayer_;
            const bool standalone=projectileStandaloneEditor_;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this,d,projectId,target,standalone](uint64_t){
                    if(!session_||!session_->Projects().HasProject()||
                       session_->Projects().CurrentProject().projectId!=projectId||
                       (!standalone && (equipmentPlayer_!=target||
                       !bridge::IsPlayerStart(session_->Scenes().GetScene(),target))))return;
                    const auto& project=session_->Projects().CurrentProject();
                    const auto saved=bridge::SaveProjectileAsset(project.rootPath,projectId,d);
                    if(!saved.succeeded){projectileCreateHelp_.SetText(saved.error);return;}
                    projectilePreferred_=saved.document.assetId;projectileSearch_.SetText("");
                    projectileCreatePanel_.SetVisible(false);if(!standalone)RefreshProjectileChoices();
                    RefreshAssetBrowser();
                });
        });
        projectileAssign_.OnClick([this](const wi::gui::EventArgs&){
            const auto row=projectileChoice_.GetSelectedUserdata();
            const auto actionRow=projectileAction_.GetSelected();
            if(row>=projectileChoices_.size()||actionRow<0||
               static_cast<std::size_t>(actionRow)>=projectileActions_.size())return;
            const auto id=projectileChoices_[row];const auto action=projectileActions_[actionRow];
            const bool offHand=projectileWeapon_.GetSelected()==1;
            const auto projectId=equipmentProject_;const auto target=equipmentPlayer_;
            if(!session_||!session_->Projects().HasProject())return;
            const auto settings=bridge::CapturePlayerControllerSettings(session_->Scenes().GetScene(),target);
            const auto weaponId=offHand?settings.offHandEquipmentAssetId:settings.primaryEquipmentAssetId;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this,id,action,offHand,projectId,target,weaponId](uint64_t){
                    if(!session_||!session_->Projects().HasProject()||
                       session_->Projects().CurrentProject().projectId!=projectId||equipmentPlayer_!=target||
                       !bridge::IsPlayerStart(session_->Scenes().GetScene(),target))return;
                    const auto& project=session_->Projects().CurrentProject();std::string error;
                    auto& scene=session_->Scenes().GetScene();
                    auto settings=bridge::CapturePlayerControllerSettings(scene,target);
                    if((offHand?settings.offHandEquipmentAssetId:settings.primaryEquipmentAssetId)!=weaponId) {
                        projectileSummary_.SetText("Weapon changed. Reopen this panel before applying.");return;
                    }
                    bridge::EquipmentAssetDocument item;
                    if(!bridge::LoadEquipmentAsset(project.rootPath,projectId,weaponId,item,error)) {
                        projectileSummary_.SetText(error);return;
                    }
                    auto& bindings=item.equipment.projectiles;
                    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
                        [action](const auto& binding){return binding.action==action;}),bindings.end());
                    const auto socketRow=projectileSocket_.GetSelectedUserdata();
                    const auto secondRow=projectileSecondSocket_.GetSelectedUserdata();
                    if(!id.empty())bindings.push_back({action,id,
                        socketRow<projectileSocketNames_.size()?projectileSocketNames_[socketRow]:std::string{},
                        projectileReleaseTime_.GetValue(),unsigned(std::max(0,projectileSocketPolicy_.GetSelected())),
                        secondRow<projectileSocketNames_.size()?projectileSocketNames_[secondRow]:std::string{}});
                    const auto saved=bridge::SaveEquipmentAsset(project.rootPath,projectId,item.equipment);
                    if(!saved.succeeded){projectileSummary_.SetText(saved.error);return;}
                    if(offHand)settings.offHandEquipmentAssetId=saved.document.equipment.assetId;
                    else settings.primaryEquipmentAssetId=saved.document.equipment.assetId;
                    if(!session_->Commands().Execute(std::make_unique<bridge::SetPlayerControllerSettingsCommand>(
                        scene,target,settings))) {
                        projectileSummary_.SetText("Could not apply the saved weapon setup.");return;
                    }
                    RefreshEquipmentEditor();RefreshWeaponProjectileEditor();RefreshInspector();RefreshAssetBrowser();
                    projectileSummary_.SetText("Projectile assigned to this player's weapon. SAVE LEVEL to retain.\nUndo restores its previous setup. Other players and prefabs are unchanged.\nTest Level: fire the equipped weapon to test flight and impact.");
                });
        });
        weaponProjectilePanel_.SetVisible(false);projectileCreatePanel_.SetVisible(false);
        GetGUI().AddWidget(&weaponProjectilePanel_);GetGUI().AddWidget(&projectileCreatePanel_);
    }

    void StudioRenderPath::SetProjectileEffectControls(const bridge::ProjectileAssetDocument& d)
    {
        projectileDamage_.SetValue(d.damage);
        projectileImpactMode_.SetSelectedWithoutCallback(d.stickOnImpact?1:0);
        projectileStuckLife_.SetValue(d.stuckLifetimeSeconds);projectileEmbedDepth_.SetValue(d.embedDepthMetres);
        projectileImpactEffect_.SetSelectedWithoutCallback(int(d.impactEffect));
        projectileEffectA_.SetSelectedWithoutCallback(d.flightEffects.empty()?0:int(d.flightEffects[0].kind));
        projectileEffectB_.SetSelectedWithoutCallback(d.flightEffects.size()<2?0:int(d.flightEffects[1].kind));
        const auto layer=d.flightEffects.empty()?bridge::ProjectileEffectLayer{}:d.flightEffects[0];
        projectileEffectSize_.SetValue(layer.sizeMetres);projectileEffectRate_.SetValue(layer.particlesPerSecond);
        projectileEffectLife_.SetValue(layer.particleLifeSeconds);projectileEffectOffset_.SetValue(layer.offset[2]);
    }
    std::vector<bridge::ProjectileEffectLayer> StudioRenderPath::ProjectileEffectControls() const
    {
        std::vector<bridge::ProjectileEffectLayer> layers;
        for(const auto kind:{projectileEffectA_.GetSelected(),projectileEffectB_.GetSelected()})
            if(kind>0) {
                bridge::ProjectileEffectLayer layer;layer.kind=static_cast<bridge::ProjectileEffectKind>(kind);
                layer.sizeMetres=projectileEffectSize_.GetValue();layer.particlesPerSecond=projectileEffectRate_.GetValue();
                layer.particleLifeSeconds=projectileEffectLife_.GetValue();layer.offset[2]=projectileEffectOffset_.GetValue();
                layers.push_back(layer);
            }
        return layers;
    }
    void StudioRenderPath::UpdateProjectilePreview(float dt)
    {
        if (!projectileCreatePanel_.IsVisible() || !session_ ||
            !session_->Projects().HasProject() ||
            session_->Projects().CurrentProject().projectId != projectileEditorProject_) {
            projectileCreatePanel_.SetVisible(false);projectilePreviewDrag_=0;
            projectilePreview_.reset();projectilePreviewImage_.SetColor(wi::Color(22,26,33));
            projectilePreviewImage_.SetImage({});
            projectilePreviewMesh_.clear();projectilePreviewProject_.clear();return;
        }
        const auto& project=session_->Projects().CurrentProject();
        if (projectilePreviewMesh_!=projectileDraftMesh_ || projectilePreviewProject_!=project.projectId) {
            projectilePreviewDrag_=0;
            projectilePreview_.reset();projectilePreviewImage_.SetColor(wi::Color(22,26,33));
            projectilePreviewImage_.SetImage({});
            projectilePreviewMesh_=projectileDraftMesh_;projectilePreviewProject_=project.projectId;
            projectilePreviewScale_=-1;
            if (!projectileDraftMesh_.empty()) {
                auto prepared=bridge::ReusableAssetService().PrepareModelAssetPlacement(
                    {project.rootPath,project.projectId,projectileDraftMesh_});
                std::string error;
                if (!prepared.IsReady()) error=prepared.Result().error;
                else {
                    auto model=prepared.ReleaseScene();
                    auto preview=std::make_unique<ModelImportPreview>();
                    if (preview->Prepare(*model,error)) projectilePreview_=std::move(preview);
                }
                if (!projectilePreview_) projectilePreviewInfo_.SetText("Cannot preview model: "+error);
            }
        }
        projectilePreviewFit_.SetEnabled(projectilePreview_!=nullptr);
        if (projectileDraftMesh_.empty()) {
            projectilePreviewInfo_.SetText("Choose a visible model to inspect it here.\nView controls change only your inspection camera.");
            projectileSave_.SetEnabled(true);return;
        }
        if (!projectilePreview_) {projectileSave_.SetEnabled(false);return;}
        const float scale=projectileVisualScale_.GetValue();
        const std::array<float,3> rotation={projectileRotationX_.GetValue(),
            projectileRotationY_.GetValue(),projectileRotationZ_.GetValue()};
        bridge::ProjectileAssetDocument appearance;
        // Validate a transient definition; this identity is never saved.
        appearance.projectId=project.projectId;appearance.assetId=projectileDraftMesh_;
        appearance.name="Preview";appearance.meshAssetId=projectileDraftMesh_;
        appearance.visualScale=scale;appearance.visualRotationDegrees=rotation;
        std::string validationError;
        if (!bridge::ValidateProjectileAsset(appearance,validationError)) {
            projectilePreviewInfo_.SetText("Model scale must be between 0.001 and 100.\nModel rotations must be between -360 and 360 degrees.");
            projectilePreviewScale_=-1;projectilePreviewDrag_=0;
            projectilePreviewImage_.SetColor(wi::Color(22,26,33));
            projectilePreviewImage_.SetImage({});projectileSave_.SetEnabled(false);return;
        }
        if (scale!=projectilePreviewScale_ || rotation!=projectilePreviewRotation_) {
            const bool first=projectilePreviewScale_<0;
            projectilePreviewScale_=scale;projectilePreviewRotation_=rotation;
            projectilePreview_->SetModelAppearance(scale,rotation);
            if (first) {projectilePreview_->FitModel();projectilePreview_->SetView(XM_PIDIV2,0);}
            const auto size=projectilePreview_->ModelSize();
            projectilePreviewInfo_.SetText("Size: "+ProjectileFlightLabel(size.x*scale)+" x "+
                ProjectileFlightLabel(size.y*scale)+" x "+ProjectileFlightLabel(size.z*scale)+" m\n"+
                "LMB orbit / RMB pan / Wheel zoom");
        }
        const auto pointer=wi::input::GetPointer();
        const auto pos=projectilePreviewImage_.GetPos();
        const auto size=projectilePreviewImage_.GetSize();
        const bool inside=pointer.x>=pos.x && pointer.x<pos.x+size.x &&
            pointer.y>=pos.y && pointer.y<pos.y+size.y;
        const auto dragButton=projectilePreviewDrag_==2?wi::input::MOUSE_BUTTON_RIGHT:wi::input::MOUSE_BUTTON_LEFT;
        if (projectilePreviewDrag_ && !wi::input::Down(dragButton)) projectilePreviewDrag_=0;
        if (!projectilePreviewDrag_ && inside) {
            if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) projectilePreviewDrag_=1;
            else if (wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT)) projectilePreviewDrag_=2;
            if (projectilePreviewDrag_) projectilePreviewPointer_={pointer.x,pointer.y};
        }
        if (projectilePreviewDrag_) {
            const float dx=pointer.x-projectilePreviewPointer_.x,dy=pointer.y-projectilePreviewPointer_.y;
            if (dx!=0 || dy!=0) {
                if (projectilePreviewDrag_==1) projectilePreview_->Orbit(-dx*0.008f,dy*0.008f);
                else projectilePreview_->Pan(dx/std::max(1.0f,size.y),dy/std::max(1.0f,size.y));
            }
            projectilePreviewPointer_={pointer.x,pointer.y};
        }
        if (inside && pointer.z!=0) projectilePreview_->Zoom(std::pow(0.85f,std::clamp(pointer.z,-8.0f,8.0f)));
        projectilePreview_->SetProjectileEffects(ProjectileEffectControls());
        if (projectilePreview_->NeedsRender()) {
            projectilePreview_->PreUpdate();projectilePreview_->Update(dt);
        }
        if (projectilePreview_->IsReady()) {
            wi::Resource image;image.SetTexture(projectilePreview_->GetRenderResult3D());
            projectilePreviewImage_.SetColor(wi::Color::White());
            for (auto& sprite:projectilePreviewImage_.sprites) sprite.params.disableBackground();
            projectilePreviewImage_.SetImage(image);
        }
        projectileSave_.SetEnabled(projectilePreview_->IsReady());
    }

    void StudioRenderPath::RefreshProjectileMeshChoices()
    {
        projectileMesh_.ClearItems();projectileMesh_.AddItem("None (flight feedback only)");
        projectileMeshChoices_.clear();
        if(!session_||!session_->Projects().HasProject())return;
        const auto& project=session_->Projects().CurrentProject();
        bridge::AssetRegistry registry;std::string error;
        if(!bridge::ReadAssetRegistry(project.rootPath,project.projectId,registry,error)){
            projectileCreateHelp_.SetText(error);return;
        }
        int selected=0;
        for(const auto& record:registry.records){
            if(!record.sourceAvailable||record.dependencyClass!=bridge::DependencyClass::ImportedContent||
                std::filesystem::u8path(record.projectRelativePath).extension()!=bridge::ReusableAssetExtension)continue;
            projectileMeshChoices_.push_back(record.assetId);
            projectileMesh_.AddItem(std::filesystem::u8path(record.projectRelativePath).stem().u8string());
            if(record.assetId==projectileDraftMesh_)selected=static_cast<int>(projectileMeshChoices_.size());
        }
        projectileMesh_.SetSelectedWithoutCallback(selected);
    }

    void StudioRenderPath::OpenProjectileAssetEditor()
    {
        if(!session_||!session_->Projects().HasProject())return;
        projectileStandaloneEditor_=true;
        projectileEditorProject_=session_->Projects().CurrentProject().projectId;
        const auto d=bridge::MakeProjectilePreset(bridge::ProjectilePreset::Arrow);
        projectileDraftDamage_=d.damage;SetProjectileEffectControls(d);projectileDraftMesh_.clear();
        projectilePreset_.SetSelectedWithoutCallback(1);projectileName_.SetText(d.name);
        projectileSpeed_.SetValue(d.speedMetresPerSecond);projectileGravity_.SetValue(d.gravityScale);
        projectileLifetime_.SetValue(d.lifetimeSeconds);projectileVisualScale_.SetValue(1);
        projectileRotationX_.SetValue(0);projectileRotationY_.SetValue(0);projectileRotationZ_.SetValue(0);
        RefreshProjectileMeshChoices();
        projectileCreateHelp_.SetText("Choose an imported model or use IMPORT MESH. Save creates a reusable asset in Content/Projectiles.");
        projectileCreatePanel_.SetVisible(true);projectileCreatePanel_.Activate();
    }

    void StudioRenderPath::OpenWeaponProjectileEditor()
    {
        projectileSearch_.SetText("");projectilePreferred_.clear();
        equipmentPanel_.SetVisible(false);
        RefreshWeaponProjectileEditor();weaponProjectilePanel_.SetVisible(true);weaponProjectilePanel_.Activate();
    }

    void StudioRenderPath::RefreshWeaponProjectileEditor()
    {
        projectileAction_.ClearItems();projectileActions_.clear();projectilePreferred_.clear();
        projectileSocketNames_.clear();projectileSocket_.ClearItems();
        if(!session_||!session_->Projects().HasProject()||
           session_->Projects().CurrentProject().projectId!=equipmentProject_||
           !bridge::IsPlayerStart(session_->Scenes().GetScene(),equipmentPlayer_))return;
        const auto& project=session_->Projects().CurrentProject();
        const auto settings=bridge::CapturePlayerControllerSettings(session_->Scenes().GetScene(),equipmentPlayer_);
        const auto id=projectileWeapon_.GetSelected()==1?settings.offHandEquipmentAssetId:settings.primaryEquipmentAssetId;
        bridge::EquipmentAssetDocument item;std::string error;
        if(id.empty()||!bridge::LoadEquipmentAsset(project.rootPath,project.projectId,id,item,error)) {
            projectileSummary_.SetText("Choose a saved weapon in Starting Equipment first.");
            projectileAssign_.SetEnabled(false);return;
        }
        int selected=0;
        for(const auto& action:item.equipment.actions) {
            if(action.animationAction=="AimIn"||action.animationAction=="AimOut")continue;
            const auto* label=ProjectileActionLabel(action.action);if(!label)continue;
            projectileActions_.push_back(action.action);projectileAction_.AddItem(label);
            if(action.action==bridge::EquipmentAction::Release)selected=projectileAction_.GetItemCount()-1;
        }
        projectileAction_.SetSelectedWithoutCallback(selected);
        if(!projectileActions_.empty())
            for(const auto& binding:item.equipment.projectiles)
                if(binding.action==projectileActions_[selected])projectilePreferred_=binding.projectileAssetId;
        RefreshProjectileChoices();
    }

    void StudioRenderPath::RefreshProjectileChoices()
    {
        projectileChoice_.ClearItems();projectileChoices_={""};projectileChoice_.AddItem("None",0);
        if(!session_||!session_->Projects().HasProject()||
           session_->Projects().CurrentProject().projectId!=equipmentProject_)return;
        const auto& project=session_->Projects().CurrentProject();std::string error;
        const bool preserveDraft=projectileSocket_.GetItemCount()>0;
        const float previousRelease=projectileReleaseTime_.GetValue();
        const int previousPolicy=projectileSocketPolicy_.GetSelected();
        std::string previousSecond;
        if(projectileSecondSocket_.GetSelectedUserdata()<projectileSocketNames_.size())
            previousSecond=projectileSocketNames_[projectileSecondSocket_.GetSelectedUserdata()];
        std::string previousSocket;
        if(projectileSocket_.GetSelectedUserdata()<projectileSocketNames_.size())
            previousSocket=projectileSocketNames_[projectileSocket_.GetSelectedUserdata()];
        projectileSocket_.ClearItems();projectileSecondSocket_.ClearItems();projectileSocketNames_={""};
        projectileSecondSocket_.AddItem("None",0);
        projectileSocketPolicy_.SetSelectedWithoutCallback(preserveDraft?previousPolicy:0);
        projectileReleaseTime_.SetValue(preserveDraft?previousRelease:0);
        std::string secondSocket=preserveDraft?previousSecond:std::string{};
        projectileSocket_.AddItem("Camera aim (legacy)",0);
        const auto settings=bridge::CapturePlayerControllerSettings(session_->Scenes().GetScene(),equipmentPlayer_);
        const auto equipmentId=projectileWeapon_.GetSelected()==1?settings.offHandEquipmentAssetId:settings.primaryEquipmentAssetId;
        bridge::EquipmentAssetDocument equipment;bridge::FirstPersonAssemblySettings assembly;
        if(bridge::LoadEquipmentAsset(project.rootPath,project.projectId,equipmentId,equipment,error)) {
            if(!projectileActions_.empty())
                for(const auto& binding:equipment.equipment.projectiles)
                    if(!preserveDraft && binding.action==projectileActions_[std::max(0,projectileAction_.GetSelected())])
                    {
                        if(previousSocket.empty())previousSocket=binding.launchSocketName;secondSocket=binding.secondSocketName;
                        projectileReleaseTime_.SetValue(binding.releaseSeconds);
                        projectileSocketPolicy_.SetSelectedWithoutCallback(int(binding.socketPolicy));
                    }
            std::string socketError;
            if(bridge::FirstPersonAssemblyService().ReadSettings(project.rootPath,project.projectId,
                    equipment.equipment.presentationAssetId,assembly,socketError))
                for(const auto& socket:assembly.launchSockets) {
                    projectileSocketNames_.push_back(socket.name);
                    projectileSocket_.AddItem(socket.name,projectileSocketNames_.size()-1);
                    projectileSecondSocket_.AddItem(socket.name,projectileSocketNames_.size()-1);
                }
        }
        if(!previousSocket.empty()&&std::find(projectileSocketNames_.begin(),projectileSocketNames_.end(),previousSocket)==projectileSocketNames_.end()) {
            projectileSocketNames_.push_back(previousSocket);
            projectileSocket_.AddItem("Unavailable: "+previousSocket,projectileSocketNames_.size()-1);
        }
        int socketSelection=0;
        for(size_t i=1;i<projectileSocketNames_.size();++i)
            if(projectileSocketNames_[i]==previousSocket)socketSelection=int(i);
        projectileSocket_.SetSelectedWithoutCallback(socketSelection);
        int secondSelection=0;
        for(size_t i=1;i<projectileSocketNames_.size();++i)
            if(projectileSocketNames_[i]==secondSocket)secondSelection=int(i);
        projectileSecondSocket_.SetSelectedWithoutCallback(secondSelection);
        const auto items=bridge::ListProjectileAssets(project.rootPath,project.projectId,error);
        const auto search=LowerProjectileSearch(projectileSearch_.GetText());int selected=0;
        std::map<std::string,unsigned> labels;
        for(const auto& d:items) {
            if(!search.empty()&&LowerProjectileSearch(d.name).find(search)==std::string::npos)continue;
            const auto index=projectileChoices_.size();projectileChoices_.push_back(d.assetId);
            auto label=d.name+" / "+ProjectileFlightLabel(d.speedMetresPerSecond)+" m/s";
            const auto duplicate=++labels[label];
            if(duplicate>1)label+=" ("+std::to_string(duplicate)+")";
            projectileChoice_.AddItem(label,index);
            if(d.assetId==projectilePreferred_)selected=projectileChoice_.GetItemCount()-1;
        }
        projectileChoice_.SetSelectedWithoutCallback(selected);
        const bool hiddenSelection=!projectilePreferred_.empty()&&selected==0;
        projectileAssign_.SetEnabled(!projectileActions_.empty()&&error.empty()&&!hiddenSelection);
        projectileEditCopy_.SetEnabled(selected!=0);
        std::string summary="Choose a projectile or create one from a preset.\nAssignment changes this player's weapon setup. Other prefabs are unchanged.\nTest Level: fire the equipped weapon to test flight and impact.";
        for(const auto& d:items)if(d.assetId==projectilePreferred_)
            summary=d.name+" / "+ProjectileFlightLabel(d.speedMetresPerSecond)+" m/s / gravity "+
                ProjectileFlightLabel(d.gravityScale)+" / lifetime "+ProjectileFlightLabel(d.lifetimeSeconds)+" s\n"+
                "Applies to this player's weapon; save the level to retain.\nTest Level: fire the equipped weapon to test flight and impact.";
        if(hiddenSelection)summary="The selected projectile is hidden by your search.\nChoose a result or clear the search before applying.";
        projectileSummary_.SetText(error.empty()?summary:error);
    }
}
