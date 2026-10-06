#include "StudioApplication.h"
#include "renegade/bridge/EquipmentAssetService.h"
#include "renegade/bridge/ProjectileAssetService.h"

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
        weaponProjectilePanel_.SetPos({70,80});weaponProjectilePanel_.SetSize({740,400});
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
        projectileSummary_.Create("Projectile summary");
        projectileSummary_.SetPos({20,200});projectileSummary_.SetSize({680,90});
        projectileSummary_.font.params.size=14;weaponProjectilePanel_.AddWidget(&projectileSummary_);
        const auto button=[&](wi::gui::Button& b,const char* label,float x,float y,float width,
                              wi::gui::Window& panel) {
            b.Create(label);b.SetText(label);b.SetPos({x,y});b.SetSize({width,30});panel.AddWidget(&b);
        };
        button(projectileAssign_,"APPLY TO THIS PLAYER",20,300,260,weaponProjectilePanel_);
        button(projectileNew_,"NEW",295,300,105,weaponProjectilePanel_);
        button(projectileEditCopy_,"EDIT COPY",415,300,145,weaponProjectilePanel_);
        button(projectileClose_,"CLOSE",575,300,125,weaponProjectilePanel_);
        projectileWeapon_.OnSelect([this](const wi::gui::EventArgs&){RefreshWeaponProjectileEditor();});
        projectileAction_.OnSelect([this](const wi::gui::EventArgs&){
            projectilePreferred_.clear();
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
        projectileCreatePanel_.SetPos({120,100});projectileCreatePanel_.SetSize({640,410});
        projectilePreset_.Create("Start with");
        projectilePreset_.SetPos({210,40});projectilePreset_.SetSize({390,26});
        for(unsigned i=0;i<5;++i)
            projectilePreset_.AddItem(bridge::ProjectilePresetName(static_cast<bridge::ProjectilePreset>(i)));
        projectileCreatePanel_.AddWidget(&projectilePreset_);
        projectileName_.Create("Projectile name");projectileName_.SetDescription("Name: ");
        projectileName_.SetPos({210,80});projectileName_.SetSize({390,26});
        projectileName_.SetCancelInputEnabled(false);projectileCreatePanel_.AddWidget(&projectileName_);
        const auto slider=[&](wi::gui::Slider& value,const char* label,float min,float max,float initial,float y) {
            value.Create(min,max,initial,1000,label);value.SetPos({210,y});value.SetSize({340,22});
            projectileCreatePanel_.AddWidget(&value);
        };
        slider(projectileSpeed_,"Speed (m/s)",0.1f,2000,300,125);
        slider(projectileGravity_,"Gravity multiplier",0,10,0,170);
        slider(projectileLifetime_,"Lifetime (seconds)",0.05f,120,5,215);
        projectileCreateHelp_.Create("Projectile flight help");
        projectileCreateHelp_.SetText("Gravity 0 flies straight; 1 uses normal gravity.\nProjectile stops on contact or when its lifetime expires.\nSave adds it to this project's reusable projectile list.");
        projectileCreateHelp_.SetPos({20,250});projectileCreateHelp_.SetSize({580,75});
        projectileCreateHelp_.font.params.size=14;projectileCreatePanel_.AddWidget(&projectileCreateHelp_);
        button(projectileSave_,"SAVE AS NEW",20,335,280,projectileCreatePanel_);
        button(projectileCancel_,"CANCEL",320,335,280,projectileCreatePanel_);
        projectileCancel_.OnClick([this](const wi::gui::EventArgs&){projectileCreatePanel_.SetVisible(false);});
        projectilePreset_.OnSelect([this](const wi::gui::EventArgs& args){
            const auto d=bridge::MakeProjectilePreset(static_cast<bridge::ProjectilePreset>(args.iValue));
            projectileDraftDamage_=d.damage;
            projectileName_.SetText(d.name);projectileSpeed_.SetValue(d.speedMetresPerSecond);
            projectileGravity_.SetValue(d.gravityScale);projectileLifetime_.SetValue(d.lifetimeSeconds);
        });
        projectileNew_.OnClick([this](const wi::gui::EventArgs&){
            const auto d=bridge::MakeProjectilePreset(bridge::ProjectilePreset::Bullet);
            projectileDraftDamage_=d.damage;
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
            projectileDraftDamage_=d.damage;
            projectileName_.SetText(d.name+" copy");projectileSpeed_.SetValue(d.speedMetresPerSecond);
            projectileGravity_.SetValue(d.gravityScale);projectileLifetime_.SetValue(d.lifetimeSeconds);
            projectileCreatePanel_.SetVisible(true);projectileCreatePanel_.Activate();
        });
        projectileSave_.OnClick([this](const wi::gui::EventArgs&){
            bridge::ProjectileAssetDocument d;
            d.name=projectileName_.GetText();d.speedMetresPerSecond=projectileSpeed_.GetValue();
            d.gravityScale=projectileGravity_.GetValue();d.lifetimeSeconds=projectileLifetime_.GetValue();
            d.damage=projectileDraftDamage_;
            const auto projectId=equipmentProject_;const auto target=equipmentPlayer_;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this,d,projectId,target](uint64_t){
                    if(!session_||!session_->Projects().HasProject()||
                       session_->Projects().CurrentProject().projectId!=projectId||equipmentPlayer_!=target||
                       !bridge::IsPlayerStart(session_->Scenes().GetScene(),target))return;
                    const auto& project=session_->Projects().CurrentProject();
                    const auto saved=bridge::SaveProjectileAsset(project.rootPath,projectId,d);
                    if(!saved.succeeded){projectileCreateHelp_.SetText(saved.error);return;}
                    projectilePreferred_=saved.document.assetId;projectileSearch_.SetText("");
                    projectileCreatePanel_.SetVisible(false);RefreshProjectileChoices();
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
                    if(!id.empty())bindings.push_back({action,id});
                    const auto saved=bridge::SaveEquipmentAsset(project.rootPath,projectId,item.equipment);
                    if(!saved.succeeded){projectileSummary_.SetText(saved.error);return;}
                    if(offHand)settings.offHandEquipmentAssetId=saved.document.equipment.assetId;
                    else settings.primaryEquipmentAssetId=saved.document.equipment.assetId;
                    if(!session_->Commands().Execute(std::make_unique<bridge::SetPlayerControllerSettingsCommand>(
                        scene,target,settings))) {
                        projectileSummary_.SetText("Could not apply the saved weapon setup.");return;
                    }
                    RefreshEquipmentEditor();RefreshWeaponProjectileEditor();RefreshInspector();RefreshAssetBrowser();
                    projectileSummary_.SetText("Projectile assigned to this player's weapon. SAVE LEVEL to retain.\nUndo restores its previous setup. Other players and prefabs are unchanged.\nLive projectile firing is still being integrated.");
                });
        });
        weaponProjectilePanel_.SetVisible(false);projectileCreatePanel_.SetVisible(false);
        GetGUI().AddWidget(&weaponProjectilePanel_);GetGUI().AddWidget(&projectileCreatePanel_);
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
        std::string summary="Choose a projectile or create one from a preset.\nAssignment changes this player's weapon setup. Other prefabs are unchanged.\nLive projectile firing is still being integrated.";
        for(const auto& d:items)if(d.assetId==projectilePreferred_)
            summary=d.name+" / "+ProjectileFlightLabel(d.speedMetresPerSecond)+" m/s / gravity "+
                ProjectileFlightLabel(d.gravityScale)+" / lifetime "+ProjectileFlightLabel(d.lifetimeSeconds)+" s\n"+
                "Applies to this player's weapon; save the level to retain.\nLive projectile firing is still being integrated.";
        if(hiddenSelection)summary="The selected projectile is hidden by your search.\nChoose a result or clear the search before applying.";
        projectileSummary_.SetText(error.empty()?summary:error);
    }
}
