#pragma once
#include "RenegadeStudioChrome.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/InventoryPagedScreenAdapter.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <functional>

namespace renegade::studio {
// A normal Inspector over native Screen widgets. Draft fields are UI state only.
class RenegadeScreenInventoryInspector final : public wi::gui::Widget {
public:
    void Create() {
        columns_.Create("Inventory columns");rows_.Create("Inventory visible rows");
        size_.Create("Inventory slot size");slotHeight_.Create("Inventory slot height");
        gapX_.Create("Inventory horizontal spacing");gapY_.Create("Inventory vertical spacing");
        padding_.Create("Inventory icon padding");
        for(auto* field:{&columns_,&rows_,&size_,&slotHeight_,&gapX_,&gapY_,&padding_})
            {field->SetCancelInputEnabled(false);field->SetClearOnEntry(true);}
        apply_.Create("Apply inventory layout");apply_.SetText("APPLY LAYOUT");
        apply_.OnClick([this](const wi::gui::EventArgs&){Apply();});
        fitSlots_.Create("Fit inventory slots and apply layout");
        fitSlots_.SetText("FIT SLOTS & APPLY");
        fitSlots_.SetTooltip("Keep the requested columns and rows; shrink slots to fit the panels, then apply as one Undo step.");
        fitSlots_.OnClick([this](const wi::gui::EventArgs&){
            auto requested=draft_;std::string error;
            if(!ReadRequested(requested)){statusError_=true;status_="Enter valid numeric values.";return;}
            if(!bridge::FitInventoryPagedGridSlots(requested,error)){statusError_=true;status_=error;return;}
            std::ostringstream w,h;w<<requested.slotWidth;h<<requested.slotHeight;
            size_.SetValue(w.str());slotHeight_.SetValue(h.str());Apply();
        });
        graphic_.Create("Change selected element graphic");graphic_.SetText("CHANGE GRAPHIC");
        graphic_.SetTooltip("Choose PNG/JPG artwork from this computer for the selected element. Its layout and actions stay intact.");
        graphic_.OnClick([this](const wi::gui::EventArgs&){if(graphicRequested_)graphicRequested_(false);});
        slotGraphic_.Create("Change all inventory slot graphics");
        slotGraphic_.SetText("CHANGE ALL SLOT GRAPHICS");
        slotGraphic_.SetTooltip("Choose one image for all slot backgrounds in Backpack and Chest. Item icons are unchanged. One Undo.");
        slotGraphic_.OnClick([this](const wi::gui::EventArgs&){if(graphicRequested_)graphicRequested_(true);});
        appearance_.Create("Inventory Appearance and Spacing");
        appearance_.OnClick([this](const wi::gui::EventArgs&){expanded_=!expanded_;scroll_=0;});
        square_.Create("Square inventory slots");
        square_.OnClick([this](const wi::gui::EventArgs&){draft_.squareSlots=!draft_.squareSlots;});
        fit_.Create("Inventory icon fitting");
        fit_.OnClick([this](const wi::gui::EventArgs&){
            draft_.iconFit=static_cast<bridge::InventoryIconFit>((static_cast<int>(draft_.iconFit)+1)%3);
        });
        scrollbar_.Create("Inventory scrollbar");
        scrollbar_.OnClick([this](const wi::gui::EventArgs&){draft_.showScrollbar=!draft_.showScrollbar;});
        element_.Create("Edit selected inventory element");
        element_.SetText("EDIT SELECTED ELEMENT");
        element_.SetTooltip("Edit native artwork, borders, colours, text or geometry for the selected element.");
        element_.OnClick([this](const wi::gui::EventArgs&){if(edit_)edit_({});});
        slots_.Create("Edit inventory slot appearance");slots_.SetText("SLOT ARTWORK & BORDER");
        slots_.OnClick([this](const wi::gui::EventArgs&){if(edit_)edit_("Paged Slot 0:0");});
        icons_.Create("Edit inventory icon appearance");icons_.SetText("ITEM ICON ARTWORK");
        icons_.OnClick([this](const wi::gui::EventArgs&){if(edit_)edit_("Paged Icon 0:0");});
        background_.Create("Edit inventory background");background_.SetText("INVENTORY BACKGROUND");
        background_.OnClick([this](const wi::gui::EventArgs&){if(edit_)edit_("Inventory Paged Background");});
        peers_.Create("Apply selected inventory appearance to matching elements");
        peers_.SetText("COPY SELECTED STYLE TO ALL MATCHING");
        peers_.SetTooltip("Copies only artwork and styling to matching slots, icons, quantities or scrollbars in both panels. One Undo.");
        peers_.OnClick([this](const wi::gui::EventArgs&){
            std::string error;
            if(!session_->ApplyPagedInventoryAppearance(selected_,error)){status_=error;statusError_=true;return;}
            statusError_=false;status_="Appearance updated in both panels. Undo restores it.";
            if(changed_)changed_();
        });
    }
    void Bind(bridge::ScreenAuthoringSession* session,bridge::StableId selected,
        std::function<void()> changed,std::function<void(std::string)> edit,std::function<void(bool)> graphic) {
        if(session_!=session)revision_=~std::uint64_t(0);
        session_=session;selected_=std::move(selected);changed_=std::move(changed);edit_=std::move(edit);
        graphicRequested_=std::move(graphic);
        if(session_&&session_->IsLoaded()&&revision_!=session_->Revision())Load();
    }
    void ReportGraphicStatus(std::string message,bool error) {
        if(session_&&session_->IsLoaded())Load();
        status_=std::move(message);statusError_=error;
    }
    void Update(const wi::Canvas& canvas,float dt,float x,float y,float width,float height) {
        x_=x;y_=y;width_=width;height_=height;
        clip_={int(x),int(y),int(x+width),int(y+height-64)};
        const auto pointer=wi::input::GetPointer();
        const float content=(expanded_?962.0f:517.0f)+(offerFit_?40.0f:0.0f);
        const float maximum=std::max(0.0f,content-(height-64));
        if(pointer.x>=x&&pointer.x<x+width&&pointer.y>=y&&pointer.y<y+height&&std::isfinite(pointer.z))
            scroll_=std::clamp(scroll_-pointer.z*35.0f,0.0f,maximum);
        scroll_=std::clamp(scroll_,0.0f,maximum);
        fields_.clear();controls_.clear();
        float row=y+64-scroll_;
        Button(graphic_,row);Button(slotGraphic_,row);
        Field(columns_,"Columns",row);Field(rows_,"Visible rows",row);
        Field(size_,draft_.squareSlots?"Slot size (px)":"Slot width (px)",row);
        Button(apply_,row);
        feedbackY_=row;
        row+=72; // Feedback belongs beside the action, never at the panel foot.
        if(offerFit_)Button(fitSlots_,row);

        appearance_.SetText(expanded_?"APPEARANCE & SPACING  [-]":"APPEARANCE & SPACING  [+]");
        Button(appearance_,row);
        if(expanded_) {
            square_.SetText(draft_.squareSlots?"SQUARE SLOTS: ON":"SQUARE SLOTS: OFF");Button(square_,row);
            if(!draft_.squareSlots)Field(slotHeight_,"Slot height (px)",row);
            Field(gapX_,"Horizontal gap (px)",row);Field(gapY_,"Vertical gap (px)",row);
            Field(padding_,"Icon padding (px)",row);
            fit_.SetText(draft_.iconFit==bridge::InventoryIconFit::Contain?"ICON FIT: CONTAIN":
                draft_.iconFit==bridge::InventoryIconFit::Cover?"ICON FIT: COVER":"ICON FIT: STRETCH");
            Button(fit_,row);
            scrollbar_.SetText(draft_.showScrollbar?"SCROLLBAR: VISIBLE":"SCROLLBAR: HIDDEN");Button(scrollbar_,row);
            Button(slots_,row);Button(icons_,row);Button(background_,row);Button(peers_,row);
        }
        Button(element_,row);
        for(auto* c:All())if(std::find(controls_.begin(),controls_.end(),c)==controls_.end())c->SetVisible(false);
        for(auto* c:controls_)if(c->IsVisible())c->Update(canvas,dt);
    }
    void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const {
        wi::gui::Widget::ApplyScissor(canvas,clip_,cmd,false);
        Draw("Applies to Backpack + Chest",x_,y_-scroll_,12,cmd);
        Draw("Applied: "+std::to_string(draft_.columns)+" x "+std::to_string(draft_.visibleRows)+
            " - "+std::to_string(draft_.columns*draft_.visibleRows)+" visible slots per panel",
            x_,y_+20-scroll_,11,cmd);
        const auto* selection=session_?session_->FindWidget(selected_):nullptr;
        std::string selectedName=selection?selection->name:"Select an element";
        if(selectedName.size()>45)selectedName=selectedName.substr(0,42)+"...";
        Draw("Selected: "+selectedName,x_,y_+40-scroll_,11,cmd);
        for(const auto& f:fields_)Draw(f.text,x_,f.y+7,11,cmd);
        wi::font::Params feedback;feedback.posX=x_;feedback.posY=feedbackY_+2;
        feedback.size=14;feedback.bolden=0.12f;feedback.h_wrap=width_;
        feedback.color=statusError_?wi::Color(255,145,125,255):wi::Color(170,205,185,255);
        const wi::graphics::Rect feedbackClip{int(x_),std::max(clip_.top,int(feedbackY_)),
            int(x_+width_),std::min(clip_.bottom,int(feedbackY_+66))};
        if(feedbackClip.bottom>feedbackClip.top) {
            wi::gui::Widget::ApplyScissor(canvas,feedbackClip,cmd,false);
            wi::font::Draw((statusError_?"Cannot apply: ":"")+status_,feedback,cmd);
        }

        for(auto* c:controls_)if(c->IsVisible()) {
            wi::gui::Widget::ApplyScissor(canvas,clip_,cmd,false);c->Render(canvas,cmd);
        }
        wi::graphics::Rect statusClip{int(x_),int(y_+height_-61),int(x_+width_),int(y_+height_)};
        wi::gui::Widget::ApplyScissor(canvas,statusClip,cmd,false);
        wi::font::Params t;t.posX=x_;t.posY=y_+height_-60;t.size=11;t.h_wrap=width_;
        t.color=wi::Color(165,185,195,255);wi::font::Draw("Visible slots only; carrying capacity is unchanged. Scroll for more options.",t,cmd);
    }
private:
    struct FieldLabel{std::string text;float y;};
    void Field(RenegadeTextInputField& field,const char* label,float& y) {
        fields_.push_back({label,y});
        Place(field,x_+width_-92,y,92);y+=40;
    }
    void Button(RenegadeButton& button,float& y){Place(button,x_,y,width_);y+=40;}
    void Place(wi::gui::Widget& c,float x,float y,float width) {
        const bool visible=y>=clip_.top&&y+30<=clip_.bottom;
        c.SetVisible(visible);c.SetEnabled(visible);
        c.SetPos(XMFLOAT2(x,y));c.SetSize(XMFLOAT2(width,30));controls_.push_back(&c);
    }
    static void Draw(const std::string& s,float x,float y,int size,wi::graphics::CommandList cmd) {
        wi::font::Params t;t.posX=x;t.posY=y;t.size=size;t.color=wi::Color(235,240,244,255);
        wi::font::Draw(s,t,cmd);
    }
    template<class T> bool Parse(const std::string& s,T& value) {
        std::istringstream input(s);input.imbue(std::locale::classic());input>>std::noskipws>>value;
        return !input.fail()&&input.peek()==std::char_traits<char>::eof();
    }
    void Load() {
        revision_=session_->Revision();std::string error;
        if(!bridge::ReadInventoryPagedGridLayout(session_->Document(),draft_,error)){status_=error;statusError_=true;return;}
        auto number=[](float n){std::ostringstream s;s.imbue(std::locale::classic());s<<n;return s.str();};
        columns_.SetValue(std::to_string(draft_.columns));rows_.SetValue(std::to_string(draft_.visibleRows));
        size_.SetValue(number(draft_.slotWidth));slotHeight_.SetValue(number(draft_.slotHeight));
        gapX_.SetValue(number(draft_.horizontalGap));gapY_.SetValue(number(draft_.verticalGap));
        padding_.SetValue(number(draft_.iconPadding));
        offerFit_=false;statusError_=false;status_="Up to 40 visible slots per panel. Slots must fit inside the panels.";
    }
    bool ReadRequested(bridge::InventoryGridLayout& next) {
        if(!Parse(columns_.GetValue(),next.columns)||!Parse(rows_.GetValue(),next.visibleRows)||
           !Parse(size_.GetValue(),next.slotWidth)||
           (!next.squareSlots&&!Parse(slotHeight_.GetValue(),next.slotHeight))||
           !Parse(gapX_.GetValue(),next.horizontalGap)||!Parse(gapY_.GetValue(),next.verticalGap)||
           !Parse(padding_.GetValue(),next.iconPadding))return false;
        if(next.squareSlots)next.slotHeight=next.slotWidth;
        return true;
    }
    void Apply() {
        auto next=draft_;offerFit_=false;
        if(!ReadRequested(next)) {
            statusError_=true;status_="Enter whole columns/rows and numeric pixel values.";return;
        }
        std::string error;
        if(!session_->ResizePagedInventoryGrid(next,error)){
            status_=error;statusError_=true;
            auto fitted=next;std::string why;
            offerFit_=bridge::FitInventoryPagedGridSlots(fitted,why)&&
                (std::fabs(fitted.slotWidth-next.slotWidth)>0.01f||std::fabs(fitted.slotHeight-next.slotHeight)>0.01f);
            return;
        }
        Load();statusError_=false;status_="Layout applied to both panels. Undo restores the previous layout.";
        if(changed_)changed_();
    }
    std::vector<wi::gui::Widget*> All() {
        return {&graphic_,&slotGraphic_,&columns_,&rows_,&size_,&slotHeight_,&gapX_,&gapY_,&padding_,&apply_,&fitSlots_,&appearance_,
            &square_,&fit_,&scrollbar_,&element_,&slots_,&icons_,&background_,&peers_};
    }
    bridge::ScreenAuthoringSession* session_=nullptr;
    bridge::StableId selected_;std::uint64_t revision_=~std::uint64_t(0);
    bridge::InventoryGridLayout draft_;std::function<void()> changed_;
    std::function<void(std::string)> edit_;bool expanded_=false;
    std::function<void(bool)> graphicRequested_;
    float x_=0,y_=0,width_=0,height_=0,scroll_=0;wi::graphics::Rect clip_{};
    bool statusError_=false,offerFit_=false;float feedbackY_=0;
    std::string status_;std::vector<FieldLabel> fields_;std::vector<wi::gui::Widget*> controls_;
    RenegadeTextInputField columns_,rows_,size_,slotHeight_,gapX_,gapY_,padding_;
    RenegadeButton graphic_,slotGraphic_,apply_,fitSlots_,appearance_,square_,fit_,scrollbar_,element_,slots_,icons_,background_,peers_;
};
}
