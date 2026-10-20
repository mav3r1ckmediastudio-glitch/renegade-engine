#pragma once
#include "RenegadeStudioChrome.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include <functional>
#include <sstream>
namespace renegade::studio
{
    class RenegadeScreenLayoutInspector final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Screen container layout Inspector");
            page_.Create("Responsive layout authoring tab");
            page_.OnClick([this](const wi::gui::EventArgs&){
                pageIndex_=(pageIndex_+1)%3;Labels();});
            compactWidth_.Create("Compact layout viewport width");compactWidth_.SetDescription("BELOW WIDTH (0=OFF)  ");
            compactColumns_.Create("Compact grid columns");compactColumns_.SetDescription("COMPACT GRID COLUMNS  ");
            compactMode_.Create("Compact child layout mode");compactApply_.Create("Apply compact child layout");
            compactApply_.SetText("APPLY COMPACT LAYOUT");
            compactMode_.OnClick([this](const wi::gui::EventArgs&){
                draft_.compactMode=static_cast<bridge::ScreenChildLayout>((static_cast<int>(draft_.compactMode)+1)%4);Labels();});
            compactApply_.OnClick([this](const wi::gui::EventArgs&){ApplyCompact();});
            mode_.Create("Screen child layout");apply_.Create("Apply child layout");free_.Create("Use manual child layout");
            padding_.Create("Container padding");padding_.SetDescription("PADDING (PX)  ");
            gap_.Create("Container spacing");gap_.SetDescription("SPACING (PX)  ");
            columns_.Create("Container grid columns");columns_.SetDescription("GRID COLUMNS  ");
            safeLeft_.Create("Screen safe-area left");safeTop_.Create("Screen safe-area top");
            safeRight_.Create("Screen safe-area right");safeBottom_.Create("Screen safe-area bottom");
            safeLeft_.SetDescription("SAFE LEFT (PX)  ");safeTop_.SetDescription("SAFE TOP (PX)  ");
            safeRight_.SetDescription("SAFE RIGHT (PX)  ");safeBottom_.SetDescription("SAFE BOTTOM (PX)  ");
            safeApply_.Create("Apply root-anchored Screen safe area");safeApply_.SetText("APPLY SCREEN SAFE AREA");
            minWidth_.Create("Widget minimum width");minHeight_.Create("Widget minimum height");
            maxWidth_.Create("Widget maximum width");maxHeight_.Create("Widget maximum height");
            minWidth_.SetDescription("MIN WIDTH (PX)  ");minHeight_.SetDescription("MIN HEIGHT (PX)  ");
            maxWidth_.SetDescription("MAX WIDTH (0=ANY)  ");maxHeight_.SetDescription("MAX HEIGHT (0=ANY)  ");
            constraintsApply_.Create("Apply min/max widget geometry");
            constraintsApply_.SetText("APPLY SIZE CONSTRAINTS");
            for(auto* input:{&padding_,&gap_,&columns_,&safeLeft_,&safeTop_,&safeRight_,&safeBottom_,
                &minWidth_,&minHeight_,&maxWidth_,&maxHeight_,&compactWidth_,&compactColumns_})
                input->SetCancelInputEnabled(false);
            apply_.SetText("APPLY CHILD LAYOUT");free_.SetText("RESTORE MANUAL CHILD GEOMETRY");
            mode_.OnClick([this](const wi::gui::EventArgs&){
                draft_.mode=static_cast<bridge::ScreenChildLayout>((static_cast<int>(draft_.mode)+1)%4);Labels();});
            apply_.OnClick([this](const wi::gui::EventArgs&){Apply(false);});
            free_.OnClick([this](const wi::gui::EventArgs&){Apply(true);});
            safeApply_.OnClick([this](const wi::gui::EventArgs&){ApplySafeArea();});
            constraintsApply_.OnClick([this](const wi::gui::EventArgs&){ApplyConstraints();});
            Labels();
        }
        void Bind(bridge::ScreenAuthoringSession* session,bridge::StableId id,std::function<void()> changed)
        {session_=session;id_=std::move(id);changed_=std::move(changed);Load();}
        void SetActive(bool active)
        {SetVisible(active);if(!active)for(auto* c:AllControls())c->SetVisible(false);}
        void SetLayout(float x,float y,float width,float height)
        {x_=x;y_=y;width_=width;height_=height;SetPos(XMFLOAT2(x,y));SetSize(XMFLOAT2(width,height));}
        void Update(const wi::Canvas& canvas,float dt) override
        {
            Widget::Update(canvas,dt);
            if(!IsVisible()||!session_||!session_->IsLoaded())return;
            if(revision_!=session_->Revision())Load();
            const auto* widget=session_->FindWidget(id_);
            const bool capable=widget&&widget->kind==bridge::ScreenWidgetKind::Image;
            const auto active=Controls();
            for(auto* c:AllControls())if(std::find(active.begin(),active.end(),c)==active.end()&&c->IsVisible())
                c->SetVisible(false);
            float y=y_;
            for(auto* control:active)
            {
                const bool groupField=control==&mode_||control==&padding_||control==&gap_||
                    control==&columns_||control==&apply_||control==&free_||pageIndex_==2;
                const bool gridColumns=control==&columns_;
                const bool compactColumns=control==&compactColumns_;
                const bool enabled=(!groupField||capable)&&
                    (!gridColumns||draft_.mode==bridge::ScreenChildLayout::Grid)&&
                    (!compactColumns||draft_.compactMode==bridge::ScreenChildLayout::Grid);
                // Set input state before its Update to avoid processing a
                // hidden/disabled field for one stale frame.
                control->SetEnabled(enabled);
                if(!control->IsVisible())control->SetVisible(true);
                control->SetPos(XMFLOAT2(x_,y));control->SetSize(XMFLOAT2(width_,28));
                y+=35;control->Update(canvas,dt);
            }
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override
        {
            if(!IsVisible()||!session_||!session_->IsLoaded())return;
            for(auto* control:const_cast<RenegadeScreenLayoutInspector*>(this)->Controls())
            {ApplyScissor(canvas,scissorRect,cmd);control->Render(canvas,cmd);}
            ApplyScissor(canvas,scissorRect,cmd);
            wi::font::Params t;t.posX=x_;t.posY=y_+403;t.size=10;t.h_wrap=width_;
            t.color=wi::Color(145,170,181,255);wi::font::Draw(status_,t,cmd);
        }
    private:
        void Labels()
        {
            mode_.SetText(std::string("CHILDREN: ")+bridge::ScreenChildLayoutName(draft_.mode));
            page_.SetText(pageIndex_==0?"LAYOUT // SAFE AREA + SIZE":pageIndex_==1?"LAYOUT // COMPACT BREAKPOINT":"LAYOUT // BACK TO CONTAINERS");
            compactMode_.SetText(std::string("COMPACT CHILDREN: ")+bridge::ScreenChildLayoutName(draft_.compactMode));
        }
        void Load()
        {
            if(!session_||!session_->IsLoaded())return;
            revision_=session_->Revision();const auto* widget=session_->FindWidget(id_);
            draft_=widget?widget->container:bridge::ScreenContainerLayout{};
            size_=widget?widget->sizeConstraints:bridge::ScreenSizeConstraints{};
            safe_=session_->Document().safeArea;
            minWidth_.SetValue(std::to_string(size_.minimumWidth));
            minHeight_.SetValue(std::to_string(size_.minimumHeight));
            maxWidth_.SetValue(std::to_string(size_.maximumWidth));
            maxHeight_.SetValue(std::to_string(size_.maximumHeight));
            safeLeft_.SetValue(std::to_string(safe_.left));
            safeTop_.SetValue(std::to_string(safe_.top));
            safeRight_.SetValue(std::to_string(safe_.right));
            safeBottom_.SetValue(std::to_string(safe_.bottom));
            padding_.SetValue(std::to_string(draft_.padding));gap_.SetValue(std::to_string(draft_.gap));
            columns_.SetValue(std::to_string(draft_.columns));
            compactWidth_.SetValue(std::to_string(draft_.compactBelowWidth));
            compactColumns_.SetValue(std::to_string(draft_.compactColumns));Labels();
            status_="Safe-area margins inset root ANCHORED widgets; fixed ABSOLUTE art stays unchanged. Child layout requires an Image/Stack/Grid.";
        }
        template<class T> bool Parse(const std::string& text,T& value)
        {std::istringstream s(text);s.imbue(std::locale::classic());s>>std::noskipws>>value;
            return !s.fail()&&s.peek()==std::char_traits<char>::eof();}
        void Apply(bool manual)
        {
            if(!session_)return;
            auto layout=draft_;
            if(manual){layout.mode=bridge::ScreenChildLayout::Free;layout.compactBelowWidth=0;}
            else if(!Parse(padding_.GetValue(),layout.padding)||!Parse(gap_.GetValue(),layout.gap)||
                !Parse(columns_.GetValue(),layout.columns))
            {status_="Padding/spacing must be numbers and columns an integer.";return;}
            std::string error;
            if(!session_->UpdateWidgetContainer(id_,layout,error)){status_=error;return;}
            Load();if(changed_)changed_();
        }
        void ApplyCompact()
        {
            if(!session_)return;
            auto layout=draft_;
            if(!Parse(compactWidth_.GetValue(),layout.compactBelowWidth)||
                !Parse(compactColumns_.GetValue(),layout.compactColumns))
            {status_="Breakpoint width must be a number; grid columns must be an integer.";return;}
            std::string error;
            if(!session_->UpdateWidgetContainer(id_,layout,error)){status_=error;return;}
            Load();status_="Compact arrangement uses logical output width. Canvas uses authored design width; zoom does not change layout. Zero disables the override.";
            if(changed_)changed_();
        }
        void ApplySafeArea()
        {
            if(!session_)return;
            auto area=safe_;
            if(!Parse(safeLeft_.GetValue(),area.left)||!Parse(safeTop_.GetValue(),area.top)||
               !Parse(safeRight_.GetValue(),area.right)||!Parse(safeBottom_.GetValue(),area.bottom))
            {status_="Safe-area insets must be finite numbers in design pixels.";return;}
            std::string error;
            if(!session_->UpdateScreenSafeArea(area,error)){status_=error;return;}
            Load();if(changed_)changed_();
        }
        void ApplyConstraints()
        {
            if(!session_)return;
            auto requested=size_;
            if(!Parse(minWidth_.GetValue(),requested.minimumWidth)||
               !Parse(minHeight_.GetValue(),requested.minimumHeight)||
               !Parse(maxWidth_.GetValue(),requested.maximumWidth)||
               !Parse(maxHeight_.GetValue(),requested.maximumHeight))
            {status_="Width and height bounds must be finite design-pixel numbers (0 maximum = unbounded).";return;}
            std::string error;
            if(!session_->UpdateWidgetSizeConstraints(id_,requested,error))
            {status_=error;return;}
            Load();if(changed_)changed_();
        }
        std::vector<wi::gui::Widget*> Controls()
        {
            if(pageIndex_==2)return {&page_,&compactWidth_,&compactMode_,&compactColumns_,&compactApply_};
            if(pageIndex_==1)return {&page_,&safeLeft_,&safeTop_,&safeRight_,&safeBottom_,&safeApply_,
                &minWidth_,&minHeight_,&maxWidth_,&maxHeight_,&constraintsApply_};
            return {&page_,&mode_,&padding_,&gap_,&columns_,&apply_,&free_};
        }
        std::vector<wi::gui::Widget*> AllControls()
        {return {&page_,&mode_,&padding_,&gap_,&columns_,&apply_,&free_,&safeLeft_,
            &safeTop_,&safeRight_,&safeBottom_,&safeApply_,&minWidth_,&minHeight_,
            &maxWidth_,&maxHeight_,&constraintsApply_,&compactWidth_,&compactMode_,&compactColumns_,&compactApply_};}
        bridge::ScreenAuthoringSession* session_=nullptr;
        bridge::StableId id_;std::function<void()> changed_;
        bridge::ScreenContainerLayout draft_;bridge::ScreenSafeArea safe_;
        bridge::ScreenSizeConstraints size_;int pageIndex_=0;std::uint64_t revision_=0;
        float x_=0,y_=0,width_=320,height_=500;std::string status_;
        RenegadeButton page_,mode_,apply_,free_,safeApply_,constraintsApply_,compactMode_,compactApply_;
        RenegadeTextInputField padding_,gap_,columns_,safeLeft_,safeTop_,safeRight_,safeBottom_;
        RenegadeTextInputField minWidth_,minHeight_,maxWidth_,maxHeight_,compactWidth_,compactColumns_;
    };
}
