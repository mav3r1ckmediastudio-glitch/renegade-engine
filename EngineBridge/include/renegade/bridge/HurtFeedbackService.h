#pragma once
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ScreenThemeStyle.h"
#include <map>
#include <set>
namespace renegade::bridge
{
    inline ScreenWidget MakePlayerHurtWidget(float width=1280,float height=720)
    {
        ScreenWidget w;w.id=GenerateStableId();w.kind=ScreenWidgetKind::Image;w.name="Player hurt feedback";
        w.rect={0,0,width,height};w.enabled=false;w.hurt.emplace();
        w.style=MakeScreenWidgetStyleTemplate(w.kind,height);w.style.borderWidth=0;w.style.cornerRadius=0;
        for(auto* v:{&w.style.normal,&w.style.hover,&w.style.pressed,&w.style.focused,&w.style.disabled})v->background={0,0,0,0};
        return w;
    }
    struct HurtFeedbackView {ScreenWidget widget;bool authored=true;};
    struct HurtFeedbackSnapshot {std::map<StableId,HurtFeedbackView> views;std::uint64_t revision=1;HurtFeedbackFrame frame;};
    class HurtFeedbackService
    {
        HurtFeedbackSnapshot state_;
    public:
        const HurtFeedbackSnapshot& Snapshot()const{return state_;}
        void SetFrame(HurtFeedbackFrame frame){state_.frame=std::move(frame);}
        void Reset(){const auto next=state_.revision+1;state_={};state_.revision=next;}
        void Observe(const ScreenDocument& doc){for(const auto& w:doc.widgets)if(w.hurt&&!state_.views.count(w.id)&&state_.views.size()<16){state_.views.emplace(w.id,HurtFeedbackView{w,true});++state_.revision;}}
        void RetainAuthored(const std::set<StableId>& active){for(auto i=state_.views.begin();i!=state_.views.end();)if(i->second.authored&&!active.count(i->first)){i=state_.views.erase(i);++state_.revision;}else ++i;}
        const HurtFeedbackView* View(const StableId& id)const{const auto i=state_.views.find(id);return i==state_.views.end()?nullptr:&i->second;}
        bool Create(ScreenWidget w,StableId& id,std::string& error)
        {
            if(state_.views.size()>=16){error="Hurt feedback view limit reached.";return false;}
            w.id=GenerateStableId();w.parentId.clear();if(!w.hurt||!ValidateHurtFeedback(*w.hurt,error))return false;
            ScreenDocument d;d.schemaVersion=3;d.usage=ScreenUsage::Hud;d.envelope=CreateDocumentEnvelope(GenerateStableId(),RuntimeScreenDocumentType,"Content/UI/Hurt.renegade-screen","hurt-api");d.widgets={w};
            if(!ValidateScreenDocument(d,d.envelope.projectId,error))return false;
            id=w.id;state_.views.emplace(id,HurtFeedbackView{std::move(w),false});++state_.revision;return true;
        }
        bool Configure(const StableId& id,ScreenHurtFeedback config,std::string& error)
        {
            const auto i=state_.views.find(id);if(i==state_.views.end()){error="Unknown hurt feedback view.";return false;}
            if(!ValidateHurtFeedback(config,error))return false;i->second.widget.hurt=std::move(config);++state_.revision;return true;
        }
        bool ConfigureWidget(const StableId& id,ScreenWidget w,std::string& error)
        {
            const auto found=state_.views.find(id);if(found==state_.views.end()){error="Unknown hurt feedback view.";return false;}
            if(!w.hurt||!ValidateHurtFeedback(*w.hurt,error))return false;
            auto local=w;local.parentId.clear();
            ScreenDocument d;d.schemaVersion=3;d.usage=ScreenUsage::Hud;d.envelope=CreateDocumentEnvelope(GenerateStableId(),RuntimeScreenDocumentType,"Content/UI/Hurt.renegade-screen","hurt-api");d.widgets={local};
            if(!ValidateScreenDocument(d,d.envelope.projectId,error))return false;
            w.id=id;w.parentId=found->second.widget.parentId;found->second.widget=std::move(w);++state_.revision;return true;
        }
        bool Remove(const StableId& id,std::string& error)
        {
            auto i=state_.views.find(id);if(i==state_.views.end()){error="Unknown hurt feedback view.";return false;}
            if(i->second.authored)i->second.widget.hurt->enabled=false;else state_.views.erase(i);++state_.revision;error.clear();return true;
        }
    };
}
