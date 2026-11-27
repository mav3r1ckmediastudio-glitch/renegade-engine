#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <filesystem>
#include <fstream>
#include "renegade/bridge/WorldNavigationService.h"
#include "renegade/bridge/ScreenThemeStyle.h"
namespace renegade::bridge
{
namespace
{
    bool valid(const ScreenWidget& w,std::string& error)
    {
        if(w.kind!=ScreenWidgetKind::Image||!w.navigation)
        {error="Navigation views must be Image components.";return false;}
        ScreenDocument d;d.usage=ScreenUsage::Hud;d.envelope=CreateDocumentEnvelope(GenerateStableId(),RuntimeScreenDocumentType,"Content/UI/Navigation.renegade-screen","navigation-api");
        auto local=w;local.parentId.clear();d.widgets={local};return ValidateScreenDocument(d,d.envelope.projectId,error);
    }
}
    ScreenWidget MakeWorldNavigationWidget(std::string name)
    {
        ScreenWidget w;w.id=GenerateStableId();w.name=std::move(name);w.kind=ScreenWidgetKind::Image;
        w.rect={320,24,640,104};w.enabled=false;w.navigation.emplace();w.navigation->markerGap=80;w.container.padding=0;
        w.style=MakeScreenWidgetStyleTemplate(w.kind,720);w.style.text.fontSize=18;
        for(auto* v:{&w.style.normal,&w.style.hover,&w.style.pressed,&w.style.focused,&w.style.disabled})
        {v->background={15,19,25,210};v->foreground={230,230,230,255};}
        w.style.borderWidth=1;w.style.borderColor={110,120,132,255};w.style.cornerRadius=4;return w;
    }
    bool WorldNavigationService::ObserveDocument(const ScreenDocument& d,std::string& error)
    {
        size_t additions=0;for(const auto& w:d.widgets)if(w.navigation&&!state_.views.count(w.id))++additions;
        if(state_.views.size()+additions>MaximumViews){error="Navigation view limit reached.";return false;}
        for(const auto& w:d.widgets)if(w.navigation&&!state_.views.count(w.id)){state_.views.emplace(w.id,NavigationView{w,true});++state_.revision;entityIndexDirty_=true;}
        error.clear();return true;
    }
    bool WorldNavigationService::Create(ScreenWidget w,StableId& id,std::string& error)
    {
        if(state_.views.size()>=MaximumViews){error="Navigation view limit reached.";return false;}
        w.id=GenerateStableId();w.parentId.clear();if(!valid(w,error))return false;id=w.id;
        state_.views.emplace(id,NavigationView{std::move(w),false});++state_.revision;entityIndexDirty_=true;error.clear();return true;
    }
    bool WorldNavigationService::Configure(const StableId& id,ScreenWidget w,std::string& error)
    {
        const auto found=state_.views.find(id);if(found==state_.views.end()){error="Navigation view is no longer active.";return false;}
        w.id=id;w.parentId=found->second.widget.parentId;if(!valid(w,error))return false;
        const auto& before=found->second.widget.navigation->targets;const auto& after=w.navigation->targets;
        if(before.size()!=after.size()||!std::equal(before.begin(),before.end(),after.begin(),[](const auto& a,const auto& b){return a.entityId==b.entityId;}))entityIndexDirty_=true;
        found->second.widget=std::move(w);++state_.revision;error.clear();return true;
    }
    bool WorldNavigationService::Remove(const StableId& id,std::string& error)
    {
        const auto found=state_.views.find(id);if(found==state_.views.end()){error="Navigation view is no longer active.";return false;}
        if(found->second.authored)found->second.widget.visible=false;else state_.views.erase(found);
        ++state_.revision;entityIndexDirty_=true;error.clear();return true;
    }
    bool WorldNavigationService::PutTarget(NavigationTarget t,std::string& error)
    {
        if(!ValidateNavigationTarget(t,error))return false;
        if(!targets_.count(t.id)&&targets_.size()>=MaximumTargets){error="Navigation target limit reached.";return false;}
        const auto before=targets_.find(t.id);if(before==targets_.end()||before->second.entityId!=t.entityId)entityIndexDirty_=true;
        targets_[t.id]=std::move(t);state_.targets.clear();for(const auto& item:targets_)state_.targets.push_back(item.second);error.clear();return true;
    }
    bool WorldNavigationService::RemoveTarget(const std::string& id,std::string& error)
    {
        if(!targets_.erase(id)){error="Unknown navigation target.";return false;}
        entityIndexDirty_=true;state_.targets.clear();for(const auto& item:targets_)state_.targets.push_back(item.second);error.clear();return true;
    }
    const NavigationView* WorldNavigationService::View(const StableId& id)const
    {const auto found=state_.views.find(id);return found==state_.views.end()?nullptr:&found->second;}
    void WorldNavigationService::SetObservers(NavigationObserver c,NavigationObserver p){state_.camera=c;state_.player=p;}
    void WorldNavigationService::ResolveEntities(const std::function<std::optional<NavigationPoint>(const std::string&)>& resolver)
    {
        if(entityIndexDirty_)
        {
            entityIds_.clear();for(const auto& t:state_.targets)if(!t.entityId.empty())entityIds_.insert(t.entityId);
            for(const auto& [id,v]:state_.views)if(v.widget.navigation)for(const auto& t:v.widget.navigation->targets)if(!t.entityId.empty())entityIds_.insert(t.entityId);
            entityIndexDirty_=false;
        }
        state_.entities.clear();for(const auto& id:entityIds_)if(const auto p=resolver(id))state_.entities.emplace(id,*p);
    }
    void WorldNavigationService::Reset(){const auto revision=state_.revision+1;state_={};state_.revision=revision;targets_.clear();entityIds_.clear();entityIndexDirty_=false;}
}

namespace renegade::bridge
{
    bool ImportScreenNavigationImage(const std::string& root,const StableId& project,const std::string& source,std::string& output,std::string& error)
    {
        namespace fs=std::filesystem;const auto relative=fs::u8path(source).lexically_relative(fs::u8path(root)).generic_u8string();
        std::string absolute;if(ResolveScreenResourcePath(root,relative,absolute,error)){output=relative;return true;}
        const auto imported=CreatorTextureWorkflowService{}.ImportTexture(root,project,source);
        if(!imported.succeeded){error=imported.error;return false;}
        const auto mirror=fs::path("Content/UI/Images")/fs::u8path(imported.assetId+fs::u8path(source).extension().u8string());
        std::ifstream input(fs::u8path(root)/fs::u8path(imported.sourceProjectRelativePath),std::ios::binary|std::ios::ate);
        const auto size=input?input.tellg():std::streampos(-1);
        if(size<=0||size>128*1024*1024){error="Cannot import navigation image.";return false;}
        ProjectDocumentWrite write;write.destinationPath=(fs::u8path(root)/mirror).generic_u8string();write.content.resize(size_t(size));input.seekg(0);
        if(!input.read(reinterpret_cast<char*>(write.content.data()),size)){error="Cannot read navigation image.";return false;}
        ProjectDocumentTransactionOptions options;options.allowedRoot=root;options.journalDirectory=(fs::u8path(root)/"Saved/Transactions").generic_u8string();
        const auto result=ProjectDocumentTransaction{}.Execute({std::move(write)},options);if(!result.success){error=result.message;return false;}
        output=mirror.generic_u8string();error.clear();return true;
    }
}

namespace renegade::bridge
{
    void WorldNavigationService::RetainAuthoredViews(const std::set<StableId>& active)
    {
        for(auto it=state_.views.begin();it!=state_.views.end();)
            if(it->second.authored&&!active.count(it->first)){it=state_.views.erase(it);++state_.revision;entityIndexDirty_=true;}else ++it;
    }
}


namespace renegade::bridge
{
    bool ApplyNavigationAppearanceDraft(ScreenWidget& target,const ScreenWidget& appearance,std::string& error,bool keepAssignedIcons)
    {
        if(target.kind!=ScreenWidgetKind::Image||!target.navigation||appearance.kind!=ScreenWidgetKind::Image||!appearance.navigation)
        {error="Choose a navigation layer and appearance.";return false;}
        auto candidate=target;
        candidate.resourcePath=appearance.resourcePath;candidate.style=appearance.style;candidate.themeOverrides=63;
        auto icons=appearance.navigation->icons;
        if(keepAssignedIcons)for(const auto& [category,path]:target.navigation->icons)icons[category]=path;
        candidate.navigation->icons=std::move(icons);
        candidate.navigation->bearingStrip=appearance.navigation->bearingStrip;
        candidate.navigation->pointerImage=appearance.navigation->pointerImage;
        if(candidate.component)candidate.component->overrides|=63|512;
        if(!valid(candidate,error))return false;
        target=std::move(candidate);error.clear();return true;
    }
    bool WorldNavigationService::ApplyAppearance(const StableId& id,const ScreenWidget& appearance,std::string& error,bool keepAssignedIcons)
    {
        const auto* view=View(id);
        if(!view){error="Navigation view is no longer active.";return false;}
        auto candidate=view->widget;
        return ApplyNavigationAppearanceDraft(candidate,appearance,error,keepAssignedIcons)&&Configure(id,std::move(candidate),error);
    }
}
