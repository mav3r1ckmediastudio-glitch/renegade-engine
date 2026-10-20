#pragma once
#include "RenegadeStudioChrome.h"
#include "RenegadeScreenChoicePicker.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/ScreenLibraryService.h"
#include "renegade/bridge/ScreenThemeResolver.h"
namespace renegade::studio
{
    class RenegadeScreenThemeInspector final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("UI theme inheritance");source_.Create("Choose project theme source");
            all_.Create("Inherit theme for all");all_.SetText("INHERIT THEME // ALL ELEMENTS");
            clear_.Create("Clear UI theme");clear_.SetText("CLEAR THEME // LOCAL FALLBACKS");
            apply_.Create("Apply UI theme overrides");apply_.SetText("APPLY SELECTED OVERRIDES");
            for(int i=0;i<6;++i)
            {
                groups_[i].Create("Theme override group "+std::to_string(i));
                groups_[i].OnClick([this,i](const wi::gui::EventArgs&){mask_^=1u<<i;Labels();});
            }
            source_.OnClick([this](const wi::gui::EventArgs&){Pick();});
            all_.OnClick([this](const wi::gui::EventArgs&){Change(true,false);});
            clear_.OnClick([this](const wi::gui::EventArgs&){Change(false,true);});
            apply_.OnClick([this](const wi::gui::EventArgs&){
                if(!session_)return;auto candidate=session_->Document();
                auto w=std::find_if(candidate.widgets.begin(),candidate.widgets.end(),[&](const auto& v){return v.id==id_;});
                if(w==candidate.widgets.end())return;w->themeOverrides=mask_;
                bridge::ScreenDocument effective;std::string error;
                if(!bridge::ResolveScreenTheme(root_,candidate,effective,error)||
                   !session_->UpdateWidgetThemeOverrides(id_,mask_,error)){status_=error;return;}
                Load();if(changed_)changed_();
            });
        }
        void Bind(bridge::ScreenAuthoringSession* session,bridge::StableId id,std::string root,std::function<void()> changed)
        {session_=session;id_=std::move(id);root_=std::move(root);changed_=std::move(changed);Load();}
        bool HasOpenPopup() const noexcept{return picker_.IsOpen();}
        void ClosePopup(){picker_.Close();}
        void SetActive(bool active){SetVisible(active);if(!active){picker_.Close();for(auto* c:Controls())c->SetVisible(false);}}
        void SetLayout(float x,float y,float width,float height)
        {x_=x;y_=y;width_=width;height_=height;SetPos(XMFLOAT2(x,y));SetSize(XMFLOAT2(width,height));picker_.SetLayout(x,y,width,height);}
        void Update(const wi::Canvas& canvas,float dt) override
        {
            Widget::Update(canvas,dt);if(!IsVisible()||!session_||!session_->IsLoaded())return;
            if(revision_!=session_->Revision())Load();
            if(picker_.IsOpen()){picker_.Update();return;}
            float y=y_;for(auto* c:Controls())
            {c->SetVisible(true);c->SetEnabled(true);c->SetPos(XMFLOAT2(x_,y));c->SetSize(XMFLOAT2(width_,28));y+=35;c->Update(canvas,dt);}
            const bool selected=session_->FindWidget(id_)!=nullptr;
            apply_.SetEnabled(selected);for(auto& group:groups_)group.SetEnabled(selected);
            all_.SetEnabled(session_->Document().theme.has_value());
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override
        {
            if(!IsVisible()||!session_||!session_->IsLoaded())return;
            for(auto* c:const_cast<RenegadeScreenThemeInspector*>(this)->Controls())
            {ApplyScissor(canvas,scissorRect,cmd);c->Render(canvas,cmd);}
            ApplyScissor(canvas,scissorRect,cmd);wi::font::Params t;t.posX=x_;t.posY=y_+365;t.size=10;t.h_wrap=width_;
            t.color=wi::Color(145,170,181,255);wi::font::Draw(status_,t,cmd);picker_.Render(cmd);
        }
    private:
        void Labels()
        {
            const char* names[]={"COLORS","BORDER / CORNERS","TYPOGRAPHY","STATE IMAGES","OPACITY","ANIMATION"};
            for(int i=0;i<6;++i)groups_[i].SetText(std::string(names[i])+((mask_&(1u<<i))?" // LOCAL":" // INHERIT"));
        }
        void Load()
        {
            if(!session_||!session_->IsLoaded())return;revision_=session_->Revision();
            const auto* w=session_->FindWidget(id_);mask_=w?w->themeOverrides:63;
            source_.SetText(session_->Document().theme?"THEME: "+session_->Document().theme->pathHint:"CHOOSE PROJECT THEME SOURCE");
            Labels();status_="Shared Screen source: first style of each widget kind. LOCAL preserves fallback; INHERIT follows the source. Edit source through the UI Library.";
        }
        void Change(bool all,bool clear)
        {
            if(!session_)return;auto candidate=session_->Document();
            if(clear)candidate.theme.reset();if(all)for(auto& w:candidate.widgets)w.themeOverrides=0;
            bridge::ScreenDocument effective;std::string error;
            if(!bridge::ResolveScreenTheme(root_,candidate,effective,error)||
               !session_->UpdateScreenTheme(candidate.theme,all,error)){status_=error;return;}
            Load();if(changed_)changed_();
        }
        void Pick()
        {
            if(!session_)return;
            const auto catalogue=bridge::EnumerateProjectScreenLibrary(root_,session_->ProjectId());
            std::vector<RenegadeScreenChoicePicker::Choice> choices;
            for(const auto& entry:catalogue.screens)
                if(entry.available&&entry.documentId!=session_->Document().envelope.documentId)
                    choices.push_back({entry.name+" // "+entry.pathHint,entry.documentId});
            picker_.Open("PROJECT THEME SOURCE",std::move(choices),"",[this,catalogue](const std::string& value){
                const auto found=std::find_if(catalogue.screens.begin(),catalogue.screens.end(),[&](const auto& v){return v.documentId==value;});
                if(found==catalogue.screens.end()||!session_)return;
                auto candidate=session_->Document();candidate.theme=bridge::ScreenThemeReference{found->documentId,found->pathHint};
                bridge::ScreenDocument effective;std::string error;
                if(!bridge::ResolveScreenTheme(root_,candidate,effective,error)||
                   !session_->UpdateScreenTheme(candidate.theme,false,error)){status_=error;return;}
                Load();if(changed_)changed_();
            });
        }
        std::vector<wi::gui::Widget*> Controls()
        {std::vector<wi::gui::Widget*> c{&source_,&all_,&clear_};for(auto& group:groups_)c.push_back(&group);c.push_back(&apply_);return c;}
        bridge::ScreenAuthoringSession* session_=nullptr;bridge::StableId id_;std::string root_,status_;
        std::function<void()> changed_;std::uint64_t revision_=0;unsigned mask_=63;
        RenegadeButton source_,all_,clear_,apply_,groups_[6];RenegadeScreenChoicePicker picker_;
        float x_=0,y_=0,width_=320,height_=450;
    };
}
