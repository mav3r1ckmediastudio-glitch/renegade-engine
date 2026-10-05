#include "StudioApplication.h"
#include "renegade/bridge/PlayerPrefabService.h"

namespace renegade::studio
{
    void StudioRenderPath::SaveSelectedPlayerPrefab()
    {
        if(!session_||!session_->Projects().HasProject())return;
        const auto entity=session_->Selection().SelectedEntity();
        auto& scene=session_->Scenes().GetScene();
        if(!bridge::IsPlayerStart(scene,entity))return;
        const auto project=session_->Projects().CurrentProject();
        const auto settings=bridge::CapturePlayerControllerSettings(scene,entity);
        const auto* name=scene.names.GetComponent(entity);
        const std::string title=name&&!name->name.empty()?name->name:"Player";
        // Defer mutations until the widget update stack has returned.
        wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
            [this,project,entity,settings,title](std::uint64_t) {
                if(!session_||!session_->Projects().HasProject()||
                    session_->Projects().CurrentProject().projectId!=project.projectId||
                    !bridge::IsPlayerStart(session_->Scenes().GetScene(),entity))return;
                const auto result=bridge::SavePlayerPrefab(project.rootPath,project.projectId,title,settings);
                if(!result.succeeded)
                {studioChrome_.SetStatusText("PLAYER PREFAB // "+result.error);return;}
                (void)session_->Commands().Execute(std::make_unique<bridge::ApplyPlayerPrefabCommand>(
                    session_->Scenes().GetScene(),entity,result.document));
                RefreshInspector();RefreshStatus();
                studioChrome_.SetStatusText("PLAYER PREFAB SAVED // "+result.document.name);
            });
    }
    void StudioRenderPath::ApplySelectedPlayerPrefab(std::string assetId)
    {
        if(assetId.empty()||!session_||!session_->Projects().HasProject())return;
        const auto project=session_->Projects().CurrentProject();
        const auto entity=session_->Selection().SelectedEntity();
        wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
            [this,project,entity,assetId](std::uint64_t) {
                if(!session_||!session_->Projects().HasProject()||
                    session_->Projects().CurrentProject().projectId!=project.projectId)return;
                bridge::PlayerPrefabDocument d;std::string error;
                if(!bridge::LoadPlayerPrefab(project.rootPath,project.projectId,assetId,d,error))
                {studioChrome_.SetStatusText("PLAYER PREFAB // "+error);return;}
                (void)session_->Commands().Execute(std::make_unique<bridge::ApplyPlayerPrefabCommand>(
                    session_->Scenes().GetScene(),entity,d));
                RefreshInspector();RefreshStatus();
            });
    }
    void StudioRenderPath::ResetSelectedPlayerPrefab()
    {
        if(!session_||!session_->Projects().HasProject())return;
        const auto projectId=session_->Projects().CurrentProject().projectId;
        const auto entity=session_->Selection().SelectedEntity();
        bridge::PlayerPrefabDocument d;std::string error;
        if(!bridge::CapturePlayerPrefabBaseline(session_->Scenes().GetScene(),entity,d,error))
        {studioChrome_.SetStatusText("PLAYER PREFAB // "+error);return;}
        wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,
            [this,entity,d,projectId](std::uint64_t) {
                if(!session_||!session_->Projects().HasProject()||
                    session_->Projects().CurrentProject().projectId!=projectId)return;
                (void)session_->Commands().Execute(std::make_unique<bridge::ApplyPlayerPrefabCommand>(
                    session_->Scenes().GetScene(),entity,d));
                RefreshInspector();RefreshStatus();
            });
    }
    void StudioRenderPath::RefreshPlayerPrefabInspector()
    {
        if(!session_||!session_->Projects().HasProject())return;
        const auto& project=session_->Projects().CurrentProject();
        const auto entity=session_->Selection().SelectedEntity();
        const auto& scene=session_->Scenes().GetScene();
        const auto origin=bridge::CapturePlayerPrefabOrigin(scene,entity);
        playerPrefab_.ClearItems();playerPrefabChoices_={""};
        playerPrefab_.AddItem("PLAYER PREFAB // CUSTOM",0);
        std::string error;
        const auto prefabs=bridge::ListPlayerPrefabs(project.rootPath,project.projectId,error);
        int selected=0;
        for(const auto& d:prefabs)
        {
            const int index=static_cast<int>(playerPrefabChoices_.size());
            playerPrefabChoices_.push_back(d.assetId);
            playerPrefab_.AddItem(d.name+" // "+d.assetId.substr(0,8),index);
            if(d.assetId==origin)selected=index;
        }
        if(!origin.empty()&&selected==0)
        {
            selected=static_cast<int>(playerPrefabChoices_.size());
            playerPrefabChoices_.push_back(origin);
            playerPrefab_.AddItem("MISSING PREFAB",selected);
        }
        // Inspector refresh must not enqueue another apply while the safe-point
        // event is dispatching its callback vector. Only user selection applies.
        playerPrefab_.SetSelectedWithoutCallback(selected);
        bridge::PlayerPrefabDocument baseline;
        const bool hasBaseline=bridge::CapturePlayerPrefabBaseline(scene,entity,baseline,error);
        const bool overridden=hasBaseline&&!bridge::PlayerSettingsEqual(
            bridge::CapturePlayerControllerSettings(scene,entity),baseline.settings);
        playerPrefabReset_.SetEnabled(overridden);
        playerPrefabStatus_.SetText(hasBaseline
            ? (overridden?"PREFAB // LOCAL OVERRIDES":"PREFAB // DEFAULTS")
            : "CUSTOM PLAYER // NO PREFAB");
        playerPrefabStatus_.SetTooltip("Saved prefab defaults are copied into this level. Controller/arms edits are local overrides; reset restores the assigned defaults. Spawn transform is always level-specific. Save creates a new reusable prefab.");
    }
}
