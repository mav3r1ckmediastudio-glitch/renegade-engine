#pragma once
#include "renegade/bridge/ScreenService.h"
#include <map>
#include <set>
namespace renegade::bridge
{
    struct NavigationView {ScreenWidget widget;bool authored=false;};
    struct WorldNavigationSnapshot
    {
        NavigationObserver camera,player;
        std::vector<NavigationTarget> targets;
        std::map<std::string,NavigationPoint> entities;
        std::map<StableId,NavigationView> views;
        std::uint64_t revision=0;
    };
    class WorldNavigationService
    {
        WorldNavigationSnapshot state_;
        std::map<std::string,NavigationTarget> targets_;
        std::set<std::string> entityIds_;
        bool entityIndexDirty_=true;
    public:
        static constexpr size_t MaximumViews=32,MaximumTargets=512;
        void RetainAuthoredViews(const std::set<StableId>& active);
        bool ObserveDocument(const ScreenDocument&,std::string& error);
        bool Create(ScreenWidget widget,StableId& id,std::string& error);
        bool Configure(const StableId&,ScreenWidget widget,std::string& error);
        bool ApplyAppearance(const StableId&,const ScreenWidget&,std::string& error,bool keepAssignedIcons=true);
        bool Remove(const StableId&,std::string& error);
        bool PutTarget(NavigationTarget,std::string& error);
        bool RemoveTarget(const std::string&,std::string& error);
        const NavigationView* View(const StableId& id)const;
        void SetObservers(NavigationObserver camera,NavigationObserver player);
        void ResolveEntities(const std::function<std::optional<NavigationPoint>(const std::string&)>& resolver);
        const WorldNavigationSnapshot& Snapshot()const{return state_;}
        void Reset();
    };
    // Appearance is independent of gameplay identity. Existing category assignments
    // win by default; replacing them is an explicit caller policy.
    bool ApplyNavigationAppearanceDraft(ScreenWidget& target,const ScreenWidget& appearance,std::string& error,bool keepAssignedIcons=true);
    bool ImportScreenNavigationImage(const std::string& root,const StableId& project,const std::string& source,std::string& output,std::string& error);
    ScreenWidget MakeWorldNavigationWidget(std::string name="World navigation");
}
