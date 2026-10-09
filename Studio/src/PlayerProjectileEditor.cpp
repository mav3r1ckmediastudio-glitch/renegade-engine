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
        weaponProjectilePanel_.Create("Weapon firing");
        CreateLaunchSocketEditor();
        weaponProjectilePanel_.SetPos({70,80});weaponProjectilePanel_.SetSize({740,630});
        const auto combo=[&](wi::gui::ComboBox& box,const char* name,float y) {
            box.Create(name);box.SetPos({220,y});box.SetSize({480,26});
            weaponProjectilePanel_.AddWidget(&box);
        };
        combo(projectileWeapon_,"Weapon hand",40);
        projectileWeapon_.AddItem("Primary hand");projectileWeapon_.AddItem("Off hand");
        combo(projectileAction_,"Launch on",80);
        combo(projectileFireMode_,"Fire mode",120);
        projectileFireMode_.AddItem("Projectile / travelling");
        projectileFireMode_.AddItem("Hitscan / instant ray");
        projectileFireMode_.SetTooltip("Projectile launches a travelling authored asset. Hitscan checks the aim line instantly.");
        projectileFireMode_.OnSelect([this](const wi::gui::EventArgs&){
            RefreshProjectileChoices();
            RefreshWeaponProjectileLayout();
        });
        projectileSearch_.Create("Search projectiles");
        projectileSearch_.SetDescription("Search (Enter): ");
        projectileSearch_.SetPos({220,160});projectileSearch_.SetSize({480,26});
        projectileSearch_.SetCancelInputEnabled(false);
        weaponProjectilePanel_.AddWidget(&projectileSearch_);
        projectileSearch_.OnInputAccepted([this](const wi::gui::EventArgs&){RefreshProjectileChoices();});
        combo(projectileChoice_,"Projectile",200);
        projectileHitscanRange_.Create(.1f,5000,100,1000,"Range (metres)");
        projectileHitscanRange_.SetPos({220,160});projectileHitscanRange_.SetSize({400,22});
        projectileHitscanRange_.SetTooltip("Maximum instant-ray distance. The first valid surface or Character blocks the shot.");
        weaponProjectilePanel_.AddWidget(&projectileHitscanRange_);
        projectileHitscanRange_.OnSlide([this](const wi::gui::EventArgs&){
            if(projectileFireMode_.GetSelected()==1)RefreshProjectileChoices();
        });
        projectileHitscanDamage_.Create(0,100000,10,1000,"Damage");
        projectileHitscanDamage_.SetPos({220,200});projectileHitscanDamage_.SetSize({400,22});
        projectileHitscanDamage_.SetTooltip("Attributed damage payload for a governed Character hit. World impacts do not require health.");
        weaponProjectilePanel_.AddWidget(&projectileHitscanDamage_);
        projectileHitscanDamage_.OnSlide([this](const wi::gui::EventArgs&){
            if(projectileFireMode_.GetSelected()==1)RefreshProjectileChoices();
        });
        combo(projectileSocket_,"Spawn point",240);
        combo(projectileSecondSocket_,"Second spawn point",320);
        combo(projectileSocketPolicy_,"Firing",280);
        projectileSocketPolicy_.AddItem("Selected spawn point");
        projectileSocketPolicy_.AddItem("Alternate between two");
        projectileSocketPolicy_.AddItem("Both together");
        combo(projectileReleaseMode_,"Fire at",360);
        projectileReleaseMode_.AddItem("Animation start");
        projectileReleaseMode_.AddItem("After a delay");
        projectileReleaseMode_.OnSelect([this](const wi::gui::EventArgs&){
            if(projectileReleaseMode_.GetSelected()==0)projectileReleaseTime_.SetValue(0);
            RefreshWeaponProjectileLayout();
        });
        projectileSocketPolicy_.OnSelect([this](const wi::gui::EventArgs&){RefreshWeaponProjectileLayout();});
        projectileReleaseTime_.Create(0,5,0,1000,"Delay (seconds)");
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
        button(projectileSocketEdit_,"EDIT SPAWN POINTS...",220,370,300,weaponProjectilePanel_);
        button(projectileTimingOpen_,"SET FIRE TIME IN ANIMATION...",220,405,300,weaponProjectilePanel_);
        projectileTimingOpen_.OnClick([this](const wi::gui::EventArgs&){OpenProjectileTiming();});
        projectileTimingPanel_.Create("Firing timing");
        projectileTimingPanel_.SetPos({120,90});projectileTimingPanel_.SetSize({740,650});
        projectileTimingImage_.Create("Animation preview");projectileTimingImage_.SetText("");
        projectileTimingImage_.SetPos({110,20});projectileTimingImage_.SetSize({512,320});
        projectileTimingPanel_.AddWidget(&projectileTimingImage_);
        projectileTimingCursor_.Create(0,1,0,1000,"Preview time (s)");
        projectileTimingCursor_.SetPos({180,360});projectileTimingCursor_.SetSize({400,22});
        projectileTimingPanel_.AddWidget(&projectileTimingCursor_);
        projectileTimingCursor_.OnSlide([this](const wi::gui::EventArgs& a){
            if(projectileTimingPreview_)projectileTimingPreview_->Scrub(a.fValue);
        });
        projectileTimingMarker_.Create(0,1,0,1000,"Fire marker (s)");
        projectileTimingMarker_.SetPos({180,400});projectileTimingMarker_.SetSize({400,22});
        projectileTimingMarker_.SetTooltip("Drag the fire marker to inspect the matching animation pose.");
        projectileTimingPanel_.AddWidget(&projectileTimingMarker_);
        projectileTimingMarker_.OnSlide([this](const wi::gui::EventArgs& a){
            if(projectileTimingPreview_)projectileTimingPreview_->Scrub(a.fValue);
        });
        button(projectileTimingPlay_,"PLAY / PAUSE",20,445,160,projectileTimingPanel_);
        button(projectileTimingMark_,"MARK CURRENT POSE",195,445,230,projectileTimingPanel_);
        button(projectileTimingUse_,"USE FIRE TIME",20,550,260,projectileTimingPanel_);
        button(projectileTimingClose_,"CANCEL",300,550,200,projectileTimingPanel_);
        projectileTimingPlay_.OnClick([this](const wi::gui::EventArgs&){
            if(projectileTimingPreview_)projectileTimingPreview_->PlayPause();
        });
        projectileTimingMark_.OnClick([this](const wi::gui::EventArgs&){
            if(projectileTimingPreview_)projectileTimingMarker_.SetValue(projectileTimingPreview_->ClipTime());
        });
        projectileTimingUse_.OnClick([this](const wi::gui::EventArgs&){
            const float t=projectileTimingMarker_.GetValue();
            if(!projectileTimingPreview_||!std::isfinite(t)||t<0||t>=projectileTimingDuration_||t>5)return;
            projectileReleaseTime_.SetValue(t);
            projectileReleaseMode_.SetSelectedWithoutCallback(t>0?1:0);
            RefreshWeaponProjectileLayout();projectileTimingPanel_.SetVisible(false);
        });
        projectileTimingClose_.OnClick([this](const wi::gui::EventArgs&){projectileTimingPanel_.SetVisible(false);});
        projectileTimingInfo_.Create("Timing help");projectileTimingInfo_.SetPos({20,490});
        projectileTimingInfo_.SetSize({680,50});projectileTimingInfo_.font.params.size=14;
        projectileTimingPanel_.AddWidget(&projectileTimingInfo_);
        projectileTimingPanel_.SetVisible(false);GetGUI().AddWidget(&projectileTimingPanel_);
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
            projectileFireMode_.SetSelectedWithoutCallback(0);
            projectileHitscanRange_.SetValue(100);projectileHitscanDamage_.SetValue(10);
            if(bridge::LoadEquipmentAsset(project.rootPath,project.projectId,id,item,error))
                for(const auto& binding:item.equipment.projectiles)
                    if(binding.action==projectileActions_[row]) {
                        projectilePreferred_=binding.projectileAssetId;
                        projectileFireMode_.SetSelectedWithoutCallback(int(binding.fireMode));
                        projectileHitscanRange_.SetValue(binding.hitscanRangeMetres);
                        projectileHitscanDamage_.SetValue(binding.hitscanDamage);
                    }
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
        projectilePreset_.SetTooltip("Applies flight, impact and effect defaults to this draft. Keeps your chosen model, scale and rotation. Saved assets are unchanged.");
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
        slider(projectileSpeed_,"Speed (m/s)",0.1f,2000,300,215);
        slider(projectileGravity_,"Gravity",0,10,0,260);
        projectileGravity_.SetTooltip("0 flies straight; 1 uses normal gravity. Values between scale its strength.");
        slider(projectileLifetime_,"Lifetime (seconds)",0.05f,120,5,395);
        projectileMesh_.Create("Visible model");
        projectileMesh_.SetPos({210,125});projectileMesh_.SetSize({390,26});
        projectileCreatePanel_.AddWidget(&projectileMesh_);
        projectileMesh_.OnSelect([this](const wi::gui::EventArgs& args){
            projectileDraftMesh_=args.iValue>0 && static_cast<size_t>(args.iValue)<=projectileMeshChoices_.size()
                ? projectileMeshChoices_[args.iValue-1] : "";
        });
        slider(projectileVisualScale_,"Model scale",0.001f,10,1,170);
        slider(projectileRotationX_,"Model rotation X",-180,180,0,435);
        slider(projectileRotationY_,"Model rotation Y",-180,180,0,475);
        slider(projectileRotationZ_,"Model rotation Z",-180,180,0,515);
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
        slider(projectileDamage_,"Damage",0,1000,10,560);
        projectileImpactMode_.Create("On impact");projectileImpactMode_.SetPos({210,305});projectileImpactMode_.SetSize({390,26});
        projectileImpactMode_.AddItem("Disappear");projectileImpactMode_.AddItem("Stick into the object");
        projectileCreatePanel_.AddWidget(&projectileImpactMode_);
        slider(projectileStuckLife_,"Stay for (seconds)",.1f,120,30,605);
        slider(projectileEmbedDepth_,"Embed depth (metres)",0,2,.05f,650);
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
        projectileImpactMode_.SetTooltip("Stick requires a visible model. Retains it at contact and follows the hit object until Stay for expires.");
        projectileEffectOffset_.SetTooltip("Forward offset in model space: positive moves effects towards the tip (+Z).");

        button(projectileAdvanced_,"SHOW ADVANCED",20,350,180,projectileCreatePanel_);
        projectileAdvanced_.OnClick([this](const wi::gui::EventArgs&){
            projectileAdvancedVisible_=!projectileAdvancedVisible_;
            RefreshProjectileEditorLayout();
        });
        projectileCancel_.OnClick([this](const wi::gui::EventArgs&){projectileCreatePanel_.SetVisible(false);});
        projectilePreset_.OnSelect([this](const wi::gui::EventArgs& args){
            if (args.iValue>=5) return;
            const auto d=bridge::MakeProjectilePreset(static_cast<bridge::ProjectilePreset>(args.iValue));
            projectileDraftDamage_=d.damage;SetProjectileEffectControls(d);
            projectileName_.SetText(d.name);projectileSpeed_.SetValue(d.speedMetresPerSecond);
            projectileGravity_.SetValue(d.gravityScale);projectileLifetime_.SetValue(d.lifetimeSeconds);
            const char* help[]={
                "Bullet: fast, straight flight with a tracer and impact sparks.\nThis is a travelling projectile. Choose a model if wanted.",
                "Arrow: arcing flight under normal gravity.\nChoose an arrow model; select Stick into the object to retain it on impact.",
                "Bolt: faster arcing flight under normal gravity.\nChoose a bolt model; select Stick into the object to retain it on impact.",
                "Thrown projectile: slower, arcing flight under normal gravity.\nChoose a model. Disappears on contact; no bounce or explosion is included.",
                "Spell projectile: straight flight with flame and impact sparks.\nEffects work without a model. No homing or area damage is included."
            };
            projectileCreateHelp_.SetText(help[args.iValue]);
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
            projectileLifetime_.SetValue(d.lifetimeSeconds);projectileAdvancedVisible_=false;projectileCreatePanel_.SetVisible(true);RefreshProjectileEditorLayout();projectileCreatePanel_.Activate();
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
            projectileAdvancedVisible_=false;projectileCreatePanel_.SetVisible(true);RefreshProjectileEditorLayout();projectileCreatePanel_.Activate();
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
            const bool hitscan=projectileFireMode_.GetSelected()==1;
            const auto row=projectileChoice_.GetSelectedUserdata();
            const auto actionRow=projectileAction_.GetSelected();
            if(actionRow<0||static_cast<std::size_t>(actionRow)>=projectileActions_.size()||
               (!hitscan&&row>=projectileChoices_.size()))return;
            const auto id=hitscan?bridge::StableId{}:projectileChoices_[row];
            const auto action=projectileActions_[actionRow];
            const auto fireMode=hitscan?bridge::EquipmentFireMode::Hitscan:bridge::EquipmentFireMode::Projectile;
            const float hitscanRange=projectileHitscanRange_.GetValue();
            const float hitscanDamage=projectileHitscanDamage_.GetValue();
            const bool offHand=projectileWeapon_.GetSelected()==1;
            const auto projectId=equipmentProject_;const auto target=equipmentPlayer_;
            if(!session_||!session_->Projects().HasProject())return;
            const auto settings=bridge::CapturePlayerControllerSettings(session_->Scenes().GetScene(),target);
            const auto weaponId=offHand?settings.offHandEquipmentAssetId:settings.primaryEquipmentAssetId;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this,id,action,fireMode,hitscanRange,hitscanDamage,offHand,projectId,target,weaponId](uint64_t){
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
                    if(fireMode==bridge::EquipmentFireMode::Hitscan||!id.empty())
                        bindings.push_back({action,id,
                            socketRow<projectileSocketNames_.size()?projectileSocketNames_[socketRow]:std::string{},
                            projectileReleaseTime_.GetValue(),unsigned(std::max(0,projectileSocketPolicy_.GetSelected())),
                            secondRow<projectileSocketNames_.size()?projectileSocketNames_[secondRow]:std::string{},
                            fireMode,hitscanRange,hitscanDamage});
                    const auto saved=bridge::SaveEquipmentAsset(project.rootPath,projectId,item.equipment);
                    if(!saved.succeeded){projectileSummary_.SetText(saved.error);return;}
                    if(offHand)settings.offHandEquipmentAssetId=saved.document.equipment.assetId;
                    else settings.primaryEquipmentAssetId=saved.document.equipment.assetId;
                    if(!session_->Commands().Execute(std::make_unique<bridge::SetPlayerControllerSettingsCommand>(
                        scene,target,settings))) {
                        projectileSummary_.SetText("Could not apply the saved weapon setup.");return;
                    }
                    RefreshEquipmentEditor();RefreshWeaponProjectileEditor();RefreshInspector();RefreshAssetBrowser();
                    projectileSummary_.SetText(fireMode==bridge::EquipmentFireMode::Hitscan?
                        "Hitscan assigned to this player's weapon. SAVE LEVEL to retain.\nUndo restores its previous setup. Other players and prefabs are unchanged.\nTest Level: fire to test the instant ray and target hit confirmation.":
                        "Projectile assigned to this player's weapon. SAVE LEVEL to retain.\nUndo restores its previous setup. Other players and prefabs are unchanged.\nTest Level: fire the equipped weapon to test flight and impact.");
                });
        });
        weaponProjectilePanel_.SetVisible(false);projectileCreatePanel_.SetVisible(false);
        GetGUI().AddWidget(&weaponProjectilePanel_);GetGUI().AddWidget(&projectileCreatePanel_);
    }


    void StudioRenderPath::OpenProjectileTiming()
    {
        projectileTimingPreview_.reset();projectileTimingImage_.SetImage({});
        if(!session_||!session_->Projects().HasProject())return;
        const auto& project=session_->Projects().CurrentProject();
        if(project.projectId!=equipmentProject_||!bridge::IsPlayerStart(session_->Scenes().GetScene(),equipmentPlayer_))return;
        const int row=projectileAction_.GetSelected();
        if(row<0||size_t(row)>=projectileActions_.size())return;
        const auto settings=bridge::CapturePlayerControllerSettings(session_->Scenes().GetScene(),equipmentPlayer_);
        const auto id=projectileWeapon_.GetSelected()==1?settings.offHandEquipmentAssetId:settings.primaryEquipmentAssetId;
        bridge::EquipmentAssetDocument item;bridge::FirstPersonAssemblySettings assembly;std::string error;
        if(!bridge::LoadEquipmentAsset(project.rootPath,project.projectId,id,item,error)) {
            projectileSummary_.SetText(error);return;
        }
        const auto action=std::find_if(item.equipment.actions.begin(),item.equipment.actions.end(),
            [&](const auto& a){return a.action==projectileActions_[row];});
        if(action==item.equipment.actions.end()||action->animationAction.empty()) {
            projectileSummary_.SetText("This action has no animation to preview. Use the numeric delay.");return;
        }
        wi::scene::Scene composed;
        const bridge::FirstPersonAssemblyService service;
        if(!service.ReadSettings(project.rootPath,project.projectId,item.equipment.presentationAssetId,assembly,error)||
           !service.Prepare(project.rootPath,project.projectId,assembly,composed,error)) {
            projectileSummary_.SetText("Cannot preview saved assembly: "+error);return;
        }
        auto preview=std::make_unique<ModelImportPreview>();
        if(!preview->Prepare(composed,error)||!preview->SetPairedAction(action->animationAction)) {
            projectileSummary_.SetText("Cannot preview this animation: "+error);return;
        }
        preview->UseFirstPersonCamera();
        projectileTimingDuration_=preview->ClipDuration();
        if(projectileTimingDuration_<=0) {
            projectileSummary_.SetText("This animation has no timed range. Use the numeric delay.");return;
        }
        projectileTimingCursor_.SetRange(0,projectileTimingDuration_);
        projectileTimingMarker_.SetRange(0,projectileTimingDuration_);
        const float t=projectileReleaseTime_.GetValue();
        projectileTimingMarker_.SetValue(t);preview->Scrub(t);
        projectileTimingPreview_=std::move(preview);
        projectileTimingPanel_.SetVisible(true);projectileTimingPanel_.Activate();
    }
    void StudioRenderPath::UpdateProjectileTiming(float dt)
    {
        if(!projectileTimingPanel_.IsVisible()||!weaponProjectilePanel_.IsVisible()||
           !session_||!session_->Projects().HasProject()||
           session_->Projects().CurrentProject().projectId!=equipmentProject_) {
            projectileTimingPanel_.SetVisible(false);projectileTimingPreview_.reset();return;
        }
        if(!projectileTimingPreview_)return;
        auto& preview=*projectileTimingPreview_;
        if(preview.NeedsRender()){preview.PreUpdate();preview.Update(dt);}
        projectileTimingCursor_.SetValue(preview.ClipTime());
        const float t=projectileTimingMarker_.GetValue();
        const bool valid=std::isfinite(t)&&t>=0&&t<projectileTimingDuration_&&t<=5;
        projectileTimingUse_.SetEnabled(valid&&preview.IsReady());
        projectileTimingInfo_.SetText(valid?
            "Scrub the animation, then MARK CURRENT POSE or drag the fire marker.\nFire: "+ProjectileFlightLabel(t)+" s / "+ProjectileFlightLabel(projectileTimingDuration_)+" s. Use time, then APPLY TO THIS PLAYER.":
            "Release must be before the animation ends and within 0-5 seconds.\nChoose an earlier pose. Cancel leaves your original timing unchanged.");
        if(preview.IsReady()){
            wi::Resource img;img.SetTexture(preview.GetRenderResult3D());
            projectileTimingImage_.SetColor(wi::Color::White());projectileTimingImage_.SetImage(img);
        }
    }

    void StudioRenderPath::RefreshProjectileEditorLayout()
    {
        // Window::SetVisible propagates to children. Reapply disclosure after every open.
        const bool advanced=projectileAdvancedVisible_;
        wi::gui::Widget* advancedControls[]={&projectileLifetime_,
            &projectileRotationX_,&projectileRotationY_,&projectileRotationZ_,&projectileDamage_,
            &projectileEffectSize_,&projectileEffectRate_,&projectileEffectLife_,&projectileEffectOffset_};
        for(auto* control:advancedControls)
            control->SetVisible(advanced);
        const bool stick=projectileImpactMode_.GetSelected()==1;
        projectileStuckLife_.SetVisible(advanced&&stick);
        projectileEmbedDepth_.SetVisible(advanced&&stick);
        if(projectileEditorLayout_==int(advanced))return;
        projectileEditorLayout_=int(advanced);
        projectileAdvanced_.SetText(advanced?"HIDE ADVANCED":"SHOW ADVANCED");
        projectileCreatePanel_.SetSize({1260,advanced?860.f:660.f});
        projectileImportMesh_.SetPos({20,advanced?700.f:540.f});
        projectileCreateHelp_.SetPos({20,advanced?735.f:475.f});
        projectileCreateHelp_.SetSize({680,advanced?30.f:55.f});
        projectileSave_.SetPos({20,advanced?770.f:580.f});
        projectileCancel_.SetPos({320,advanced?770.f:580.f});
    }

    void StudioRenderPath::RefreshWeaponProjectileLayout()
    {
        const bool hitscan=projectileFireMode_.GetSelected()==1;
        projectileSearch_.SetVisible(!hitscan);projectileChoice_.SetVisible(!hitscan);
        projectileHitscanRange_.SetVisible(hitscan);projectileHitscanDamage_.SetVisible(hitscan);
        projectileNew_.SetVisible(!hitscan);projectileEditCopy_.SetVisible(!hitscan);
        const bool multiple=projectileAvailableSocketCount_>1;
        // Keep a saved invalid multi-point policy visible so it can be repaired, never silently downgrade it.
        const bool policyVisible=multiple||projectileSocketPolicy_.GetSelected()>0;
        const bool second=policyVisible&&projectileSocketPolicy_.GetSelected()>0;
        projectileSocketPolicy_.SetVisible(policyVisible);
        projectileSecondSocket_.SetVisible(second);
        const bool delayed=projectileReleaseMode_.GetSelected()==1;
        projectileReleaseTime_.SetVisible(delayed);
        const int layout=int(hitscan)|(int(policyVisible)<<1)|(int(second)<<2)|(int(delayed)<<3);
        if(projectileWeaponLayout_==layout)return;
        projectileWeaponLayout_=layout;
        float y=280;
        if(policyVisible){projectileSocketPolicy_.SetPos({220,y});y+=40;}
        if(second){projectileSecondSocket_.SetPos({220,y});y+=40;}
        projectileReleaseMode_.SetPos({220,y});y+=40;
        projectileReleaseTime_.SetPos({220,y});
        if(delayed)y+=40;
        projectileSocketEdit_.SetPos({220,y});y+=40;
        projectileTimingOpen_.SetPos({220,y});y+=40;
        projectileSummary_.SetPos({20,y});
        projectileSummary_.SetSize({680,64});y+=78;
        projectileAssign_.SetPos({20,y});
        if(hitscan) {
            projectileClose_.SetPos({295,y});projectileClose_.SetSize({260,30});
        } else {
            projectileNew_.SetPos({295,y});projectileEditCopy_.SetPos({415,y});
            projectileClose_.SetPos({575,y});projectileClose_.SetSize({125,30});
        }
        weaponProjectilePanel_.SetSize({740,y+70});
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
        UpdateProjectileTiming(dt);
        if(weaponProjectilePanel_.IsVisible())RefreshWeaponProjectileLayout();
        if (!projectileCreatePanel_.IsVisible() || !session_ ||
            !session_->Projects().HasProject() ||
            session_->Projects().CurrentProject().projectId != projectileEditorProject_) {
            projectileCreatePanel_.SetVisible(false);projectilePreviewDrag_=0;
            projectilePreview_.reset();projectilePreviewImage_.SetColor(wi::Color(22,26,33));
            projectilePreviewImage_.SetImage({});
            projectilePreviewMesh_.clear();projectilePreviewProject_.clear();return;
        }
        RefreshProjectileEditorLayout();
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
        projectileAdvancedVisible_=false;projectileCreatePanel_.SetVisible(true);RefreshProjectileEditorLayout();projectileCreatePanel_.Activate();
    }

    void StudioRenderPath::OpenWeaponProjectileEditor()
    {
        projectileSearch_.SetText("");projectilePreferred_.clear();
        equipmentPanel_.SetVisible(false);
        RefreshWeaponProjectileEditor();weaponProjectilePanel_.SetVisible(true);RefreshWeaponProjectileLayout();weaponProjectilePanel_.Activate();
    }

    void StudioRenderPath::RefreshWeaponProjectileEditor()
    {
        projectileAction_.ClearItems();projectileActions_.clear();projectilePreferred_.clear();
        projectileFireMode_.SetSelectedWithoutCallback(0);
        projectileHitscanRange_.SetValue(100);projectileHitscanDamage_.SetValue(10);
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
                if(binding.action==projectileActions_[selected]) {
                    projectilePreferred_=binding.projectileAssetId;
                    projectileFireMode_.SetSelectedWithoutCallback(int(binding.fireMode));
                    projectileHitscanRange_.SetValue(binding.hitscanRangeMetres);
                    projectileHitscanDamage_.SetValue(binding.hitscanDamage);
                }
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
        const int previousReleaseMode=projectileReleaseMode_.GetSelected();
        const int previousFireMode=projectileFireMode_.GetSelected();
        const float previousHitscanRange=projectileHitscanRange_.GetValue();
        const float previousHitscanDamage=projectileHitscanDamage_.GetValue();
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
        if(preserveDraft) {
            projectileFireMode_.SetSelectedWithoutCallback(previousFireMode);
            projectileHitscanRange_.SetValue(previousHitscanRange);
            projectileHitscanDamage_.SetValue(previousHitscanDamage);
        }
        std::string secondSocket=preserveDraft?previousSecond:std::string{};
        bool existingBinding=false;
        projectileAvailableSocketCount_=0;
        projectileSocket_.AddItem("Camera aim (legacy)",0);
        const auto settings=bridge::CapturePlayerControllerSettings(session_->Scenes().GetScene(),equipmentPlayer_);
        const auto equipmentId=projectileWeapon_.GetSelected()==1?settings.offHandEquipmentAssetId:settings.primaryEquipmentAssetId;
        bridge::EquipmentAssetDocument equipment;bridge::FirstPersonAssemblySettings assembly;
        if(bridge::LoadEquipmentAsset(project.rootPath,project.projectId,equipmentId,equipment,error)) {
            if(!projectileActions_.empty())
                for(const auto& binding:equipment.equipment.projectiles)
                    if(!preserveDraft && binding.action==projectileActions_[std::max(0,projectileAction_.GetSelected())])
                    {
                        existingBinding=true;
                        if(previousSocket.empty())previousSocket=binding.launchSocketName;secondSocket=binding.secondSocketName;
                        projectileReleaseTime_.SetValue(binding.releaseSeconds);
                        projectileSocketPolicy_.SetSelectedWithoutCallback(int(binding.socketPolicy));
                        projectileFireMode_.SetSelectedWithoutCallback(int(binding.fireMode));
                        projectileHitscanRange_.SetValue(binding.hitscanRangeMetres);
                        projectileHitscanDamage_.SetValue(binding.hitscanDamage);
                    }
            std::string socketError;
            if(bridge::FirstPersonAssemblyService().ReadSettings(project.rootPath,project.projectId,
                    equipment.equipment.presentationAssetId,assembly,socketError))
                for(const auto& socket:assembly.launchSockets) {
                    ++projectileAvailableSocketCount_;
                    projectileSocketNames_.push_back(socket.name);
                    projectileSocket_.AddItem(socket.name,projectileSocketNames_.size()-1);
                    projectileSecondSocket_.AddItem(socket.name,projectileSocketNames_.size()-1);
                }
        }
        for(const auto& name:{previousSocket,secondSocket})
            if(!name.empty()&&std::find(projectileSocketNames_.begin(),projectileSocketNames_.end(),name)==projectileSocketNames_.end()) {
                projectileSocketNames_.push_back(name);
                const auto index=projectileSocketNames_.size()-1;
                projectileSocket_.AddItem("Unavailable: "+name,index);
                projectileSecondSocket_.AddItem("Unavailable: "+name,index);
            }
        // New bindings use the sole authored PSP. Existing camera-origin bindings remain explicit.
        if(!preserveDraft&&!existingBinding&&projectileAvailableSocketCount_==1)
            previousSocket=projectileSocketNames_[1];
        int socketSelection=0;
        for(size_t i=1;i<projectileSocketNames_.size();++i)
            if(projectileSocketNames_[i]==previousSocket)socketSelection=int(i);
        projectileSocket_.SetSelectedWithoutCallback(socketSelection);
        int secondSelection=0;
        for(size_t i=1;i<projectileSocketNames_.size();++i)
            if(projectileSocketNames_[i]==secondSocket)secondSelection=int(i);
        projectileSecondSocket_.SetSelectedWithoutCallback(secondSelection);
        projectileReleaseMode_.SetSelectedWithoutCallback(preserveDraft?previousReleaseMode:(projectileReleaseTime_.GetValue()>0?1:0));
        RefreshWeaponProjectileLayout();
        const bool hitscan=projectileFireMode_.GetSelected()==1;
        std::vector<bridge::ProjectileAssetDocument> items;
        if(!hitscan)items=bridge::ListProjectileAssets(project.rootPath,project.projectId,error);
        const auto search=LowerProjectileSearch(projectileSearch_.GetText());int selected=0;
        std::map<std::string,unsigned> labels;
        if(!hitscan)for(const auto& d:items) {
            if(!search.empty()&&LowerProjectileSearch(d.name).find(search)==std::string::npos)continue;
            const auto index=projectileChoices_.size();projectileChoices_.push_back(d.assetId);
            auto label=d.name+" / "+ProjectileFlightLabel(d.speedMetresPerSecond)+" m/s";
            const auto duplicate=++labels[label];
            if(duplicate>1)label+=" ("+std::to_string(duplicate)+")";
            projectileChoice_.AddItem(label,index);
            if(d.assetId==projectilePreferred_)selected=projectileChoice_.GetItemCount()-1;
        }
        projectileChoice_.SetSelectedWithoutCallback(selected);
        const bool hiddenSelection=!hitscan&&!projectilePreferred_.empty()&&selected==0;
        projectileAssign_.SetEnabled(!projectileActions_.empty()&&error.empty()&&!hiddenSelection);
        projectileNew_.SetEnabled(!hitscan);projectileEditCopy_.SetEnabled(!hitscan&&selected!=0);
        std::string summary;
        if(hitscan) {
            summary="Hitscan / "+ProjectileFlightLabel(projectileHitscanRange_.GetValue())+
                " m range / damage "+ProjectileFlightLabel(projectileHitscanDamage_.GetValue())+
                "\nInstant ray from the chosen spawn point; first valid contact wins."+
                "\nCharacter hits show a brief centre hit confirmation.";
        } else {
            summary="Choose a projectile or create one from a preset.\nAssignment changes this player's weapon setup. Other prefabs are unchanged.\nTest Level: fire the equipped weapon to test flight and impact.";
            for(const auto& d:items)if(d.assetId==projectilePreferred_)
                summary=d.name+" / "+ProjectileFlightLabel(d.speedMetresPerSecond)+" m/s / gravity "+
                    ProjectileFlightLabel(d.gravityScale)+" / lifetime "+ProjectileFlightLabel(d.lifetimeSeconds)+" s\n"+
                    "Applies to this player's weapon; save the level to retain.\nTest Level: fire the equipped weapon to test flight and impact.";
            if(hiddenSelection)summary="The selected projectile is hidden by your search.\nChoose a result or clear the search before applying.";
        }
        projectileSummary_.SetText(error.empty()?summary:error);
        RefreshWeaponProjectileLayout();
    }
}
