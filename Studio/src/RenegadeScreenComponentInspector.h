#pragma once
#include "RenegadeStudioChrome.h"
#include "RenegadeScreenChoicePicker.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/ScreenLibraryService.h"
namespace renegade::studio
{
    class RenegadeScreenComponentInspector final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Linked UI component");
            source_.Create("Choose linked component source");source_.SetText("LINK TO PROJECT ELEMENT");
            source_.OnClick([this](const wi::gui::EventArgs&){PickScreen(false);});
            importTree_.Create("Import linked UI hierarchy");
            importTree_.SetText("IMPORT LINKED SUBTREE");
            importTree_.SetTooltip("Import a complete project Screen element hierarchy with independent Button actions and linked source overrides.");
            importTree_.OnClick([this](const wi::gui::EventArgs&){PickScreen(true);});
            detach_.Create("Detach linked component");detach_.SetText("DETACH // BAKE CURRENT VALUES");
            detach_.OnClick([this](const wi::gui::EventArgs&){Commit(std::nullopt);});
            recover_.Create("Detach unavailable linked source explicitly");
            recover_.SetText("SOURCE MISSING // KEEP LOCAL FALLBACK");
            recover_.SetTooltip("Recovery only: discard broken source links for the whole hierarchy. Inherited appearance cannot be reconstructed and may change. One Undo step.");
            recover_.OnClick([this](const wi::gui::EventArgs&){RecoverBrokenSource();});
            refresh_.Create("Refresh linked component source");
            refresh_.SetText("REFRESH // PROJECT SOURCE");
            refresh_.SetTooltip("Revalidate the saved linked source and rebuild shared preview without altering this Screen's draft, local overrides or Undo history.");
            refresh_.OnClick([this](const wi::gui::EventArgs&){RefreshSource();});
            apply_.Create("Apply component overrides");apply_.SetText("APPLY INSTANCE OVERRIDES");
            apply_.OnClick([this](const wi::gui::EventArgs&){
                const auto* w=Selected();if(!w||!w->component)return;
                auto link=*w->component;link.overrides=mask_;Commit(link);
            });
            for(unsigned i=0;i<10;++i)
            {
                groups_[i].Create("Component override "+std::to_string(i));
                groups_[i].OnClick([this,i](const wi::gui::EventArgs&){mask_^=1u<<i;Labels();});
            }
        }
        void Bind(bridge::ScreenAuthoringSession* session,bridge::StableId id,std::string root,std::function<void()> changed)
        {session_=session;id_=std::move(id);root_=std::move(root);changed_=std::move(changed);Load();}
        bool HasOpenPopup() const noexcept{return picker_.IsOpen();}
        void ClosePopup(){picker_.Close();}
        void SetActive(bool active)
        {SetVisible(active);if(!active){picker_.Close();for(auto* c:Controls())c->SetVisible(false);}}
        void SetLayout(float x,float y,float width,float height)
        {x_=x;y_=y;width_=width;height_=height;SetPos(XMFLOAT2(x,y));SetSize(XMFLOAT2(width,height));picker_.SetLayout(x,y,width,height);}
        void Update(const wi::Canvas& canvas,float dt) override
        {
            Widget::Update(canvas,dt);if(!IsVisible()||!session_||!session_->IsLoaded())return;
            if(revision_!=session_->Revision())Load();
            if(picker_.IsOpen()){picker_.Update();return;}
            const auto* w=Selected();const bool linked=w&&w->component;
            const auto place=[&](wi::gui::Widget& c,float x,float y,float width,bool enabled){
                c.SetVisible(true);c.SetEnabled(enabled);c.SetPos(XMFLOAT2(x,y));c.SetSize(XMFLOAT2(width,26));c.Update(canvas,dt);
            };
            place(source_,x_,y_,width_,w!=nullptr);
            place(detach_,x_,y_+32,width_,linked);
            const float cell=(width_-6)*.5f;
            for(int i=0;i<10;++i)place(groups_[i],x_+(i%2)*(cell+6),y_+68+(i/2)*31,cell,linked);
            place(apply_,x_,y_+227,width_,linked);
            place(refresh_,x_,y_+258,width_,linked);
            place(importTree_,x_,y_+289,width_,true);
            if(linked&&canRecover_)place(recover_,x_,y_+321,width_,true);
            else recover_.SetVisible(false);
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override
        {
            if(!IsVisible()||!session_||!session_->IsLoaded())return;
            for(auto* c:const_cast<RenegadeScreenComponentInspector*>(this)->Controls())
            {ApplyScissor(canvas,scissorRect,cmd);c->Render(canvas,cmd);}
            ApplyScissor(canvas,scissorRect,cmd);wi::font::Params t;t.posX=x_;t.posY=y_+358;t.size=10;t.h_wrap=width_;
            t.color=wi::Color(170,190,200,255);wi::font::Draw(status_,t,cmd);picker_.Render(cmd);
        }
    private:
        const bridge::ScreenWidget* Selected() const{return session_?session_->FindWidget(id_):nullptr;}
        void Labels()
        {
            const char* names[]={"COLORS","BORDER","FONT","IMAGES","OPACITY","ANIMATION","TEXT","CONTROL","DATA BINDINGS","PROGRESS"};
            for(unsigned i=0;i<10;++i)groups_[i].SetText(std::string(names[i])+((mask_&(1u<<i))?" // LOCAL":" // SOURCE"));
        }
        void Load()
        {
            if(!session_||!session_->IsLoaded())return;revision_=session_->Revision();
            canRecover_=false;
            const auto* w=Selected();mask_=w&&w->component?w->component->overrides:0;Labels();
            status_=w&&w->component?"SOURCE: "+w->component->pathHint+" // "+w->component->widgetId:
                "Choose an exact same-kind project element. Geometry, identity, focus and Flow actions stay local. Refresh accepts only a valid saved source.";
        }
        void RefreshSource()
        {
            if(!session_||!session_->IsLoaded())return;
            const auto* selected=Selected();
            if(!selected||!selected->component)
            {status_="No selected linked source to refresh.";return;}
            if(root_.empty())
            {status_="Cannot refresh a linked component without its owning project.";return;}
            // Linked hierarchy refresh performs a separate, validated
            // authoring transaction only when new source children appear.
            // Existing local IDs, values and overrides stay untouched.
            if(!selected->component->treeRootId.empty())
            {
                std::string conflict;
                if(!session_->RefreshLinkedWidgetTree(selected->component->treeRootId,conflict))
                {canRecover_=true;status_="SOURCE CONFLICT // "+conflict;return;}
            }
            // Resolve into a temporary copy before touching the renderer.
            bridge::ScreenDocument effective;
            std::string error;
            if(!bridge::ResolveScreenTheme(root_,session_->Document(),effective,error))
            {canRecover_=true;status_="SOURCE CONFLICT // "+error;return;}
            const auto current=std::find_if(effective.widgets.begin(),effective.widgets.end(),
                [&](const auto& widget){return widget.id==id_;});
            if(current==effective.widgets.end())
            {canRecover_=true;status_="SOURCE CONFLICT // Selected instance disappeared.";return;}
            if(changed_)changed_(); // Rebuilds the shared renderer on its owner frame.
            status_="PROJECT SOURCE REFRESH REQUESTED // LOCAL OVERRIDES PRESERVED";
        }

        void Commit(std::optional<bridge::ScreenComponentReference> link)
        {
            if(!session_)return;std::string error;
            if(!session_->UpdateWidgetComponent(id_,std::move(link),error))
            {canRecover_=Selected()&&Selected()->component;status_="SOURCE NOT RESOLVED // "+error;return;}
            Load();if(changed_)changed_();
        }
        void RecoverBrokenSource()
        {
            if(!canRecover_||!session_||!Selected()||!Selected()->component)return;
            std::string failure;
            if(!session_->DetachUnavailableWidgetSource(id_,failure))
            {status_="RECOVERY REJECTED // "+failure;return;}
            Load();if(changed_)changed_();
            status_="SOURCE RECOVERY // LOCAL FALLBACK KEPT; INHERITED APPEARANCE MAY DIFFER";
        }
        void PickScreen(bool importTree)
        {
            if(!session_||!session_->IsLoaded()||(!importTree&&!Selected()))return;
            const auto catalogue=bridge::EnumerateProjectScreenLibrary(root_,session_->ProjectId());
            std::vector<RenegadeScreenChoicePicker::Choice> choices;
            for(const auto& entry:catalogue.screens)
                if(entry.available&&entry.documentId!=session_->Document().envelope.documentId)
                    choices.push_back({entry.name+" // "+entry.pathHint,entry.documentId});
            picker_.Open(importTree?"LINKED SUBTREE SOURCE SCREEN":"COMPONENT SOURCE SCREEN",std::move(choices),"",[this,catalogue,importTree](const std::string& value){
                const auto entry=std::find_if(catalogue.screens.begin(),catalogue.screens.end(),[&](const auto& v){return v.documentId==value;});
                if(entry==catalogue.screens.end()||(!importTree&&!Selected()))return;
                std::string path,error;bridge::ScreenDocument source;
                if(!bridge::ResolveRuntimeScreenDocumentPath(root_,session_->ProjectId(),entry->documentId,entry->pathHint,path,error)||
                   !bridge::ReadScreenDocument(path,session_->ProjectId(),source,error)){status_=error;return;}
                std::vector<RenegadeScreenChoicePicker::Choice> widgets;
                for(const auto& w:source.widgets)
                    if(importTree||w.kind==Selected()->kind)widgets.push_back({w.name+" // "+w.id,w.id});
                const auto documentId=entry->documentId,pathHint=entry->pathHint;
                picker_.Open(importTree?"LINKED SUBTREE ROOT":"EXACT SOURCE ELEMENT",std::move(widgets),"",[this,documentId,pathHint,importTree](const std::string& id){
                    if(!importTree){Commit(bridge::ScreenComponentReference{documentId,pathHint,id,0});return;}
                    bridge::StableId imported;
                    std::string failure;
                    if(!session_->ImportLinkedWidgetTree(
                        bridge::ScreenComponentReference{documentId,pathHint,id,0},imported,failure))
                    {status_="IMPORT FAILED // "+failure;return;}
                    if(changed_)changed_();
                    status_="LINKED SUBTREE IMPORTED // SELECT THE NEW ROOT IN LAYERS";
                });
            });
        }
        std::vector<wi::gui::Widget*> Controls()
        {std::vector<wi::gui::Widget*> c{&source_,&detach_};for(auto& group:groups_)c.push_back(&group);c.push_back(&apply_);c.push_back(&refresh_);c.push_back(&importTree_);c.push_back(&recover_);return c;}
        bridge::ScreenAuthoringSession* session_=nullptr;bridge::StableId id_;std::string root_,status_;
        std::function<void()> changed_;std::uint64_t revision_=0;unsigned mask_=0;
        bool canRecover_=false;
        RenegadeButton source_,detach_,apply_,refresh_,importTree_,recover_,groups_[10];RenegadeScreenChoicePicker picker_;
        float x_=0,y_=0,width_=320,height_=450;
    };
}
