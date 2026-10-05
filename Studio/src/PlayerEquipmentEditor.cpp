#include "StudioApplication.h"
#include "renegade/bridge/EquipmentAssetService.h"
namespace renegade::studio {
void StudioRenderPath::CreateEquipmentEditor() {
    equipmentPanel_.Create("Starting Equipment");
    equipmentPanel_.SetSize({700,410});equipmentPanel_.SetPos({40,80});
    const auto combo=[&](wi::gui::ComboBox& c,const char* name,float y) {
        c.Create(name);c.SetPos({180,y});c.SetSize({490,26});equipmentPanel_.AddWidget(&c);
    };
    combo(equipmentPrimary_,"Primary hand",40);combo(equipmentOffHand_,"Off hand",80);
    equipmentHelp_.Create("Equipment help");
    equipmentHelp_.SetText("Choose equipment for this Player Start. SAVE LEVEL retains the loadout.\nSaved player prefabs include these defaults; changes are local overrides.\nTwo-handed/support items reserve both hands.\nCreate an equipment definition from the player's saved arms assembly below.");
    equipmentHelp_.SetPos({20,125});equipmentHelp_.SetSize({650,80});
    equipmentHelp_.font.params.size=14;equipmentPanel_.AddWidget(&equipmentHelp_);
    equipmentName_.Create("Equipment name");equipmentName_.SetText("");equipmentName_.SetDescription("New item: ");
    equipmentName_.SetPos({180,215});equipmentName_.SetSize({490,26});
    equipmentName_.SetCancelInputEnabled(false);equipmentPanel_.AddWidget(&equipmentName_);
    combo(equipmentHandUse_,"Hand use",250);
    for(const char* name:{"PrimaryOnly","OffHandOnly","EitherHand","TwoHanded","PrimaryWithSupport"})
        equipmentHandUse_.AddItem(name);
    equipmentHandUse_.SetSelectedWithoutCallback(3);
    const auto button=[&](wi::gui::Button& b,const char* name,float x,float width) {
        b.Create(name);b.SetText(name);b.SetPos({x,290});b.SetSize({width,30});equipmentPanel_.AddWidget(&b);
    };
    button(equipmentCreate_,"CREATE FROM ASSEMBLY",20,280);
    button(equipmentApply_,"APPLY LOADOUT",320,210);button(equipmentClose_,"CLOSE",550,120);
    equipmentClose_.OnClick([this](const wi::gui::EventArgs&){equipmentPanel_.SetVisible(false);});
    equipmentStatus_.Create("Equipment status");equipmentStatus_.SetPos({20,335});equipmentStatus_.SetSize({650,50});
    equipmentStatus_.font.params.size=14;equipmentPanel_.AddWidget(&equipmentStatus_);
    equipmentCreate_.OnClick([this](const wi::gui::EventArgs&) {
        const auto target=equipmentPlayer_;const auto projectId=equipmentProject_;
        const auto name=equipmentName_.GetText();
        const auto policy=equipmentHandUse_.GetSelected();
        wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
            [this,name,policy,target,projectId](uint64_t) {
                if(!session_||!session_->Projects().HasProject()||
                   session_->Projects().CurrentProject().projectId!=projectId||equipmentPlayer_!=target||
                   !bridge::IsPlayerStart(session_->Scenes().GetScene(),equipmentPlayer_))return;
                const auto& project=session_->Projects().CurrentProject();
                const auto settings=bridge::CapturePlayerControllerSettings(session_->Scenes().GetScene(),equipmentPlayer_);
                bridge::EquipmentDefinition item;std::string error;
                if(policy<0||!bridge::PrepareEquipmentFromAssembly(project.rootPath,project.projectId,
                    settings.firstPersonArmsAssetId,name,static_cast<bridge::EquipmentHandUse>(policy),item,error)) {
                    equipmentStatus_.SetText("EQUIPMENT // "+error);return;
                }
                const auto saved=bridge::SaveEquipmentAsset(project.rootPath,project.projectId,item);
                if(!saved.succeeded){equipmentStatus_.SetText("EQUIPMENT // "+saved.error);return;}
                RefreshEquipmentEditor();RefreshAssetBrowser();
                equipmentStatus_.SetText("Created "+name+". Choose it above, then APPLY LOADOUT.");
            });
    });
    equipmentApply_.OnClick([this](const wi::gui::EventArgs&) {
        const auto p=equipmentPrimary_.GetSelectedUserdata(),o=equipmentOffHand_.GetSelectedUserdata();
        if(p>=equipmentChoices_.size()||o>=equipmentChoices_.size())return;
        const auto primary=equipmentChoices_[p],offHand=equipmentChoices_[o];
        const auto target=equipmentPlayer_;const auto projectId=equipmentProject_;
        wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
            [this,primary,offHand,target,projectId](uint64_t) {
                if(!session_||!session_->Projects().HasProject()||
                   session_->Projects().CurrentProject().projectId!=projectId||equipmentPlayer_!=target||
                   !bridge::IsPlayerStart(session_->Scenes().GetScene(),equipmentPlayer_))return;
                const auto& project=session_->Projects().CurrentProject();std::string error;
                if(!bridge::ValidateStartingEquipment(project.rootPath,project.projectId,primary,offHand,error)) {
                    equipmentStatus_.SetText("LOADOUT // "+error);return;
                }
                auto& scene=session_->Scenes().GetScene();auto settings=bridge::CapturePlayerControllerSettings(scene,equipmentPlayer_);
                settings.primaryEquipmentAssetId=primary;settings.offHandEquipmentAssetId=offHand;
                (void)session_->Commands().Execute(std::make_unique<bridge::SetPlayerControllerSettingsCommand>(scene,equipmentPlayer_,settings));
                RefreshInspector();RefreshStatus();
                equipmentStatus_.SetText("Loadout applied. SAVE LEVEL to retain; Undo restores the previous loadout.");
            });
    });
    equipmentPanel_.SetVisible(false);GetGUI().AddWidget(&equipmentPanel_);
}
void StudioRenderPath::OpenEquipmentEditor() {
    if(!session_||!session_->Projects().HasProject())return;
    const auto entity=session_->Selection().SelectedEntity();
    if(!bridge::IsPlayerStart(session_->Scenes().GetScene(),entity))return;
    equipmentPlayer_=entity;equipmentProject_=session_->Projects().CurrentProject().projectId;
    RefreshEquipmentEditor();equipmentPanel_.SetVisible(true);
}
void StudioRenderPath::RefreshEquipmentEditor() {
    if(!session_||!session_->Projects().HasProject()||
       session_->Projects().CurrentProject().projectId!=equipmentProject_)return;
    const auto& project=session_->Projects().CurrentProject();
    const auto settings=bridge::CapturePlayerControllerSettings(session_->Scenes().GetScene(),equipmentPlayer_);
    std::string error;const auto items=bridge::ListEquipmentAssets(project.rootPath,project.projectId,error);
    equipmentPrimary_.ClearItems();equipmentOffHand_.ClearItems();equipmentChoices_={""};
    equipmentPrimary_.AddItem("NONE",0);equipmentOffHand_.AddItem("NONE",0);
    int p=0,o=0;
    for(const auto& d:items) {
        const auto& item=d.equipment;const auto index=equipmentChoices_.size();equipmentChoices_.push_back(item.assetId);
        const auto label=item.name+" // "+item.assetId.substr(0,8);
        if(item.handUse!=bridge::EquipmentHandUse::OffHandOnly) {
            equipmentPrimary_.AddItem(label,index);
            if(item.assetId==settings.primaryEquipmentAssetId)p=equipmentPrimary_.GetItemCount()-1;
        }
        if(item.handUse==bridge::EquipmentHandUse::OffHandOnly||item.handUse==bridge::EquipmentHandUse::EitherHand) {
            equipmentOffHand_.AddItem(label,index);
            if(item.assetId==settings.offHandEquipmentAssetId)o=equipmentOffHand_.GetItemCount()-1;
        }
    }
    const auto missing=[&](wi::gui::ComboBox& box,const bridge::StableId& id,int& selected) {
        if(id.empty()||selected!=0)return;
        const auto index=equipmentChoices_.size();equipmentChoices_.push_back(id);
        box.AddItem("MISSING / INCOMPATIBLE // "+id.substr(0,8),index);selected=box.GetItemCount()-1;
    };
    missing(equipmentPrimary_,settings.primaryEquipmentAssetId,p);missing(equipmentOffHand_,settings.offHandEquipmentAssetId,o);
    equipmentPrimary_.SetSelectedWithoutCallback(p);equipmentOffHand_.SetSelectedWithoutCallback(o);
    equipmentStatus_.SetText(error.empty()?"Choose primary and off-hand equipment, then APPLY LOADOUT.":error);
}
}
