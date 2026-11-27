#pragma once
#include "StudioModalWindow.h"
#include "RenegadeStudioChrome.h"
#include "renegade/screen/ScreenRenderer.h"
#include "RenegadeScreenColorPicker.h"
#include "renegade/bridge/CreatorScreenFontImportService.h"
#include "renegade/bridge/WorldNavigationService.h"
#include "renegade/bridge/ScreenLibraryService.h"
#include <memory>
#include <filesystem>
#include <cstdlib>
namespace renegade::studio
{
class ScreenNavigationEditor final:public StudioModalWindow,public std::enable_shared_from_this<ScreenNavigationEditor>
{
    wi::RenderPath2D previewPath_;
    screen::ScreenRenderer previewRenderer_;
    bool previewLoaded_=false;
    bridge::ScreenWidget widget_;
    std::string root_,status_;bridge::StableId project_;
    std::uint64_t generation_=0;
    std::function<bool(bridge::ScreenWidget,std::string&)> accept_;
    std::function<bool(const bridge::ScreenWidget&,float,std::string&)> preview_;
    std::function<void()> closed_;
    RenegadeScreenColorPicker colourPicker_;
    RenegadeButton font_,textColour_,backingColour_;
    RenegadeTextInputField fontSize_,targetPriority_;
    RenegadeCheckBox targetVisible_;
    RenegadeButton source_,neutral_,fantasy_,appearance_,backing_,strip_,pointer_,clearArt_,icon_,clearIcon_,add_,remove_,save_,cancel_;
    RenegadeTextInputField span_,heading_,size_,gap_,range_,filter_,iconCategory_,targetName_,targetCategory_,targetX_,targetY_,targetZ_,targetEntity_;
    RenegadeCheckBox names_,distances_,ticks_;
    RenegadeSlider previewHeading_;
    wi::gui::TreeList targets_;
    std::vector<wi::gui::Widget*> controls_;
    int selected_=-1,pending_=-1;
    void Add(wi::gui::Widget& w,float x,float y,float width,float height){w.SetPos({x,y});w.SetSize({width,height});w.SetShadowRadius(0);AddWidget(&w);controls_.push_back(&w);}
    void Button(RenegadeButton& b,const char* text,float x,float y,float width,const char* help)
    {b.Create(text);b.SetText(text);b.SetRenderTextSize(14);b.SetAccentOutlineStyle(true);b.SetTooltip(help);Add(b,x,y,width,32);}
    void Field(RenegadeTextInputField& f,const char* name,float x,float y,float width,const char* help)
    {f.Create(name);f.SetDescription("");f.SetRenderTextSize(14);f.SetTooltip(help);Add(f,x,y,width,28);f.OnInputAccepted([this](const auto&){Preview();});}
    std::vector<wi::gui::TextInputField*> Fields(){return {&fontSize_,&targetPriority_,&span_,&heading_,&size_,&gap_,&range_,&filter_,&iconCategory_,&targetName_,&targetCategory_,&targetX_,&targetY_,&targetZ_,&targetEntity_,&previewHeading_.valueInputField};}
    void Drafts(){for(auto* f:Fields())if(f->GetState()==wi::gui::ACTIVE){const auto text=f->GetCurrentInputValue();f->Deactivate();f->SetValue(text);}}
    static bool Number(const std::string& s,float& n){char* end=nullptr;n=std::strtof(s.c_str(),&end);return end!=s.c_str()&&!*end&&std::isfinite(n);}
    void TargetFields()
    {
        const bool valid=selected_>=0&&selected_<int(widget_.navigation->targets.size());
        for(auto* f:{&targetPriority_,&targetName_,&targetCategory_,&targetX_,&targetY_,&targetZ_,&targetEntity_})f->SetEnabled(valid);
        remove_.SetEnabled(valid);targetVisible_.SetEnabled(valid);
        if(!valid){for(auto* f:{&targetPriority_,&targetName_,&targetCategory_,&targetX_,&targetY_,&targetZ_,&targetEntity_})f->SetValue("");return;}
        const auto& t=widget_.navigation->targets[selected_];targetName_.SetValue(t.label);targetCategory_.SetValue(t.category);
        targetX_.SetValue(t.position.x);targetY_.SetValue(t.position.y);targetZ_.SetValue(t.position.z);targetEntity_.SetValue(t.entityId);targetPriority_.SetValue(t.priority);targetVisible_.SetCheck(t.visible);
    }
    void List()
    {
        targets_.ClearItems();for(size_t i=0;i<widget_.navigation->targets.size();++i){wi::gui::TreeList::Item row;
            row.name=widget_.navigation->targets[i].label;row.userdata=i;row.selected=int(i)==selected_;targets_.AddItem(row);}
    }
    bool Capture()
    {
        auto& c=*widget_.navigation;
        if(!Number(fontSize_.GetValue(),widget_.style.text.fontSize)||widget_.style.text.fontSize<4||widget_.style.text.fontSize>128){status_="Font size must be4-128 design pixels.";return false;}
        if(!Number(span_.GetValue(),c.fieldOfView)||!Number(heading_.GetValue(),c.manualHeading)||
            !Number(size_.GetValue(),c.markerSize)||!Number(gap_.GetValue(),c.markerGap)||!Number(range_.GetValue(),c.maxDistance))
        {status_="Enter valid numeric settings.";return false;}
        c.categoryFilter=filter_.GetValue();c.showNames=names_.GetCheck();c.showDistance=distances_.GetCheck();c.showTicks=ticks_.GetCheck();
        if(selected_>=0&&selected_<int(c.targets.size()))
        {
            float priority=0;if(!Number(targetPriority_.GetValue(),priority)||priority!=std::floor(priority)||priority< -1000||priority>1000){status_="Target priority must be a whole number from-1000 to1000.";return false;}
            auto& t=c.targets[selected_];t.priority=int(priority);t.visible=targetVisible_.GetCheck();t.label=targetName_.GetValue();t.category=targetCategory_.GetValue();t.entityId=targetEntity_.GetValue();
            if(!Number(targetX_.GetValue(),t.position.x)||!Number(targetY_.GetValue(),t.position.y)||!Number(targetZ_.GetValue(),t.position.z))
            {status_="Enter valid world target coordinates.";return false;}
        }
        return bridge::ValidateScreenNavigation(c,status_);
    }
    void Preview()
    {
        if(!preview_||!Capture())return;std::string error;
        if(!preview_(widget_,previewHeading_.GetValue(),error)){status_=error;return;}
        if(!previewLoaded_)
        {
            auto w=widget_;w.parentId.clear();w.component.reset();w.layoutMode=bridge::ScreenLayoutMode::Absolute;
            w.rect={0,0,640,104};w.animations.clear();w.video.reset();w.cameraSource.reset();w.carousel.reset();w.transitionOverride.reset();
            bridge::ScreenDocument doc;doc.usage=bridge::ScreenUsage::Hud;doc.designWidth=640;doc.designHeight=180;
            doc.envelope=bridge::CreateDocumentEnvelope(project_,bridge::RuntimeScreenDocumentType,"Content/UI/NavigationPreview.renegade-screen","navigation-private-preview");doc.widgets={w};
            if(!previewRenderer_.Load(doc,root_,previewPath_,{},error)){status_=error;return;}previewRenderer_.SetInputEnabled(false);previewLoaded_=true;
        }
        if(!previewRenderer_.PreviewNavigationDraft(widget_.id,*widget_.navigation,previewHeading_.GetValue(),error,&widget_))status_=error;
    }
    void UpdatePreview(const wi::Canvas& canvas,float dt)
    {
        if(!previewLoaded_)return;const auto o=ContentOrigin();
        previewRenderer_.SetViewport({o.x+520,o.y+722,492,138.375f});
        auto clip=scissorRect;clip.top=std::max(clip.top,int(GetPos().y+38));clip.bottom=std::min(clip.bottom,int(GetPos().y+GetSize().y-76));
        previewRenderer_.SetClipViewport({float(clip.left),float(clip.top),float(clip.right-clip.left),float(clip.bottom-clip.top)});
        previewRenderer_.ApplyLayout(previewPath_);previewPath_.GetGUI().Update(canvas,dt);
    }
    void Source(){const char* names[]={"Follow camera","Follow player","Manual heading"};source_.SetText(names[int(widget_.navigation->headingSource)]);}
    void ChooseFont()
    {
        Drafts();const auto weak=weak_from_this();const auto generation=generation_;
        wi::helper::FileDialogParams d;d.type=wi::helper::FileDialogParams::OPEN;d.description="Choose TrueType navigation font";d.extensions={"ttf"};
        wi::helper::FileDialog(d,[weak,generation](const std::string& path){if(path.empty())return;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[weak,generation,path](std::uint64_t){
                const auto self=weak.lock();if(!self||!self->IsVisible()||self->generation_!=generation)return;
                const auto result=bridge::CreatorScreenFontImportService::Import(self->root_,self->project_,path);
                if(!result.succeeded){self->status_=result.error;return;}
                self->widget_.style.text.fontResource=result.runtimePath;self->Preview();self->status_="Font imported. Save applies it to this navigation layer.";
            });
        });
    }
    void ChooseColour(bool backing)
    {
        Drafts();if(!Capture())return;
        const auto initial=backing?(widget_.resourcePath.empty()?widget_.style.normal.background:widget_.style.normal.imageTint):widget_.style.normal.foreground;
        colourPicker_.Open(root_,backing?"Navigation backing colour":"Navigation text colour",initial,[this,backing](auto colour){
            for(auto* v:{&widget_.style.normal,&widget_.style.hover,&widget_.style.pressed,&widget_.style.focused,&widget_.style.disabled})
                if(!backing)v->foreground=colour;else if(widget_.resourcePath.empty())v->background=colour;else v->imageTint=colour;
            Preview();
        });
    }
    void ChooseImage(int slot)
    {
        Drafts();if(!Capture())return;const auto category=iconCategory_.GetValue();
        if(slot==3&&(category.empty()||category.size()>48)){status_="Enter the marker category whose icon you want to replace.";return;}
        const auto weak=weak_from_this();const auto generation=generation_;
        wi::helper::FileDialogParams d;d.type=wi::helper::FileDialogParams::OPEN;d.description="Choose navigation artwork";d.extensions={"png","jpg","jpeg","bmp"};
        wi::helper::FileDialog(d,[weak,generation,slot,category](const std::string& path){if(path.empty())return;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[weak,generation,slot,category,path](std::uint64_t){
                const auto self=weak.lock();if(!self||!self->IsVisible()||self->generation_!=generation)return;
                std::string resource,error;if(!bridge::ImportScreenNavigationImage(self->root_,self->project_,path,resource,error)){self->status_=error;return;}
                if(slot==0)self->widget_.resourcePath=resource;else if(slot==1)self->widget_.navigation->bearingStrip=resource;
                else if(slot==2)self->widget_.navigation->pointerImage=resource;else self->widget_.navigation->icons[category]=resource;
                self->Preview();self->status_="Artwork imported. Save applies the change; Cancel keeps the layer unchanged.";
            });
        });
    }
    void ApplyAppearance(const std::string& package)
    {
        Drafts();if(!Capture())return;bridge::ScreenPortableAdoptionResult adopted;std::string error;
        if(!bridge::AdoptPortableScreenPackage(root_,project_,package,adopted,error)){status_=error;return;}
        bridge::ScreenDocument doc;if(!bridge::ReadScreenDocument((std::filesystem::u8path(root_)/std::filesystem::u8path(adopted.pathHint)).generic_u8string(),project_,doc,error)){status_=error;return;}
        const auto source=std::find_if(doc.widgets.begin(),doc.widgets.end(),[](const auto& w){return w.navigation.has_value();});
        if(source==doc.widgets.end()){status_="Choose a portable UI package containing a navigation component.";return;}
        widget_.resourcePath=source->resourcePath;widget_.style=source->style;widget_.themeOverrides=63;fontSize_.SetValue(widget_.style.text.fontSize);
        auto& c=*widget_.navigation;c.bearingStrip=source->navigation->bearingStrip;c.pointerImage=source->navigation->pointerImage;c.icons=source->navigation->icons;
        Preview();status_="Appearance changed. Targets, heading settings and placement are retained.";
    }
    void ChooseAppearance()
    {
        Drafts();const auto weak=weak_from_this();const auto generation=generation_;
        wi::helper::FileDialogParams d;d.type=wi::helper::FileDialogParams::OPEN;d.description="Choose a navigation UI package manifest";d.extensions={"json"};
        wi::helper::FileDialog(d,[weak,generation](const std::string& path){if(path.empty())return;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[weak,generation,path](std::uint64_t){
                const auto self=weak.lock();if(self&&self->IsVisible()&&self->generation_==generation)self->ApplyAppearance(std::filesystem::u8path(path).parent_path().generic_u8string());
            });
        });
    }
    static void Label(const std::string& text,float x,float y,int size,wi::Color colour,wi::graphics::CommandList cmd)
    {wi::font::Params p(x,y,size);p.color=colour;p.bolden=.12f;wi::font::Draw(text,p,cmd);}
public:
    ~ScreenNavigationEditor(){previewRenderer_.Reset(previewPath_);RemoveWidgets();}
    void Create()
    {
        StudioModalWindow::Create("World navigation",WindowControls::CLOSE|WindowControls::MOVE);SetSize({1040,920});SetControlSize(32);SetShadowRadius(0);
        Button(source_,"Follow camera",28,42,444,"Choose the heading source. Camera follows the view; Player follows body yaw; Manual can be driven from scripts.");
        source_.OnClick([this](const auto&){widget_.navigation->headingSource=static_cast<bridge::NavigationHeadingSource>((int(widget_.navigation->headingSource)+1)%3);Source();Preview();});
        Field(span_,"Visible compass span",28,112,206,"Visible angular span in degrees, from30 to360. Smaller values show a narrower direction range.");
        Field(heading_,"Manual heading",266,112,206,"Clockwise degrees from north when the heading source is Manual.");
        Field(size_,"Marker size",28,174,206,"Marker icon size in design pixels, from4 to128.");
        Field(gap_,"Marker spacing",266,174,206,"Minimum space between markers in design pixels. Priority and distance decide which overlapping marker is kept.");
        Field(range_,"Maximum distance",28,236,206,"Hide targets farther than this world distance. Zero means unlimited.");
        Field(filter_,"Visible categories",266,236,206,"Comma-separated marker categories to show. Leave blank to show all categories.");
        names_.Create("Target names");names_.SetRenderTextSize(14);names_.SetTooltip("Show target names beneath icons.");Add(names_,28,285,20,24);
        distances_.Create("Distances");distances_.SetRenderTextSize(14);distances_.SetTooltip("Show planar target distance in metres.");Add(distances_,185,285,20,24);
        ticks_.Create("Direction ticks");ticks_.SetRenderTextSize(14);ticks_.SetTooltip("Show moving direction labels and bearing ticks.");Add(ticks_,325,285,20,24);
        for(auto* c:{&names_,&distances_,&ticks_})c->OnClick([this](const auto&){Preview();});
        previewHeading_.Create(0,360,0,360,"Preview heading","Sample heading");previewHeading_.SetRenderTextSizes(14,14);
        previewHeading_.SetTooltip("Turn the editor sample view. This does not rotate the real player or camera.");Add(previewHeading_,28,352,444,40);
        previewHeading_.OnValuePreview([this](float){Preview();});previewHeading_.OnValueCommitted([this](float){Preview();});
        Button(neutral_,"Neutral",520,42,140,"Use the generic navigation appearance, retaining targets and behaviour.");
        neutral_.OnClick([this](const auto&){ApplyAppearance((std::filesystem::u8path(wi::helper::GetExecutablePath()).parent_path()/"Content/UI/Prefabs/WorldNavigation").generic_u8string());});
        Button(fantasy_,"Fantasy example",676,42,148,"Use the supplied fantasy artwork as an interchangeable example appearance.");
        fantasy_.OnClick([this](const auto&){ApplyAppearance((std::filesystem::u8path(wi::helper::GetExecutablePath()).parent_path()/"Content/UI/Prefabs/WorldNavigationFantasy").generic_u8string());});
        Button(appearance_,"Import appearance...",840,42,172,"Choose a portable navigation UI package; appearance changes keep existing targets and placement.");appearance_.OnClick([this](const auto&){ChooseAppearance();});
        Button(backing_,"Change backing...",520,104,232,"Replace only this navigation layer's backing artwork.");backing_.OnClick([this](const auto&){ChooseImage(0);});
        Button(strip_,"Change direction strip...",776,104,236,"Optional 360-degree bearing strip, north at both ends. Clear artwork uses editable font-based directions.");strip_.OnClick([this](const auto&){ChooseImage(1);});
        Button(pointer_,"Change centre pointer...",520,154,232,"Replace the fixed centre pointer independently of the backing and icons.");pointer_.OnClick([this](const auto&){ChooseImage(2);});
        Button(clearArt_,"Clear artwork",776,154,236,"Clear backing, strip, pointer and category images to use generic drawing. Font and colour settings remain.");clearArt_.OnClick([this](const auto&){widget_.resourcePath.clear();auto& c=*widget_.navigation;c.bearingStrip.clear();c.pointerImage.clear();c.icons.clear();Preview();});
        Field(iconCategory_,"Icon category",520,236,232,"Category whose icon is being edited, such as quest, town, enemy or your own category.");
        Button(icon_,"Choose category icon...",776,232,236,"Assign custom artwork to the category entered on the left.");icon_.OnClick([this](const auto&){ChooseImage(3);});
        Button(clearIcon_,"Clear category icon",776,282,236,"Use the generic marker for this category.");clearIcon_.OnClick([this](const auto&){widget_.navigation->icons.erase(iconCategory_.GetValue());Preview();});
        Button(font_,"Choose font...",520,332,148,"Import a TrueType font for the generated compass labels and marker names.");font_.OnClick([this](const auto&){ChooseFont();});
        Button(textColour_,"Text colour...",692,332,148,"Set generated directions and marker label colour.");textColour_.OnClick([this](const auto&){ChooseColour(false);});
        Button(backingColour_,"Backing colour...",864,332,148,"Set plain backing colour, or tint the chosen backing image.");backingColour_.OnClick([this](const auto&){ChooseColour(true);});
        Field(fontSize_,"Font size",520,392,148,"Generated direction label size in design pixels, from4 to128. Marker labels use a smaller matching size.");
        targets_.Create("World targets");targets_.SetText("");Add(targets_,28,456,444,242);
        targets_.OnSelect([this](const wi::gui::EventArgs& a){if(a.userdata<widget_.navigation->targets.size()&&int(a.userdata)!=selected_)pending_=int(a.userdata);});
        Button(add_,"Add target",28,714,206,"Add an authored world target. Scripts may also register targets at Runtime.");add_.OnClick([this](const auto&){Drafts();if(!Capture())return;if(widget_.navigation->targets.size()>=64){status_="This layer already has64 authored targets.";return;}
            widget_.navigation->targets.push_back({bridge::GenerateStableId(),"New target","waypoint","",{0,0,20},0,true});selected_=int(widget_.navigation->targets.size())-1;List();TargetFields();Preview();});
        Button(remove_,"Remove target",266,714,206,"Remove only the selected authored target.");remove_.OnClick([this](const auto&){if(selected_<0)return;widget_.navigation->targets.erase(widget_.navigation->targets.begin()+selected_);selected_=widget_.navigation->targets.empty()?-1:std::min(selected_,int(widget_.navigation->targets.size())-1);List();TargetFields();Preview();});
        Field(targetName_,"Target name",520,456,232,"Name shown for the selected target.");
        Field(targetCategory_,"Target category",776,456,236,"Marker category used for icon selection and category filtering.");
        Field(targetX_,"Target world X",520,526,148,"World X in metres; +X is east.");
        Field(targetY_,"Target world Y",692,526,148,"World Y in metres. Compass bearings and distances use the horizontal X/Z plane.");
        Field(targetZ_,"Target world Z",864,526,148,"World Z in metres; +Z is north.");
        Field(targetEntity_,"Follow entity ID (advanced)",520,596,492,"Optional persistent entity ID in this Level. Leave blank for fixed world coordinates; scripts can safely supply an EntityRef. Cross-project imports explicitly unassign this reference.");
        Field(targetPriority_,"Target priority",520,656,148,"Higher priority targets are retained when markers overlap; range-1000 to1000.");
        targetVisible_.Create("Show target");targetVisible_.SetRenderTextSize(14);targetVisible_.SetTooltip("Show or hide this authored target without deleting its settings.");Add(targetVisible_,692,656,20,24);targetVisible_.OnClick([this](const auto&){Preview();});
        targetName_.OnInputAccepted([this](const auto&){Preview();List();});
        Button(save_,"Save",0,0,116,"Apply navigation settings as one Undo operation.");PinFooter(save_,260,12,true);
        save_.OnClick([this](const auto&){Drafts();if(!Capture())return;std::string error;if(widget_.component)widget_.component->overrides|=63|512;
            if(accept_&&accept_(widget_,error))Close();else status_=error;});
        Button(cancel_,"Cancel",0,0,116,"Discard this draft and restore the saved presentation.");PinFooter(cancel_,132,12,true);cancel_.OnClick([this](const auto&){Close();});
        OnClose([this](const auto&){Close();});SetVisible(false);
    }
    void Open(bridge::ScreenWidget widget,std::string root,bridge::StableId project,
        std::function<bool(bridge::ScreenWidget,std::string&)> accept,std::function<void()> closed,
        std::function<bool(const bridge::ScreenWidget&,float,std::string&)> preview)
    {
        Close();widget_=std::move(widget);root_=std::move(root);project_=std::move(project);accept_=std::move(accept);closed_=std::move(closed);preview_=std::move(preview);
        fontSize_.SetValue(widget_.style.text.fontSize);auto& c=*widget_.navigation;span_.SetValue(c.fieldOfView);heading_.SetValue(c.manualHeading);size_.SetValue(c.markerSize);gap_.SetValue(c.markerGap);range_.SetValue(c.maxDistance);filter_.SetValue(c.categoryFilter);
        names_.SetCheck(c.showNames);distances_.SetCheck(c.showDistance);ticks_.SetCheck(c.showTicks);previewHeading_.SetValue(bridge::WrapNavigationHeading(c.manualHeading));iconCategory_.SetValue("quest");
        selected_=c.targets.empty()?-1:0;pending_=-1;List();TargetFields();Source();status_="EDITOR SAMPLE: preview heading and demonstration targets never change gameplay.";SetVisible(true);Preview();
    }
    void Close()
    {++generation_;previewRenderer_.Reset(previewPath_);previewLoaded_=false;colourPicker_.Close();for(auto* f:Fields())f->Deactivate();SetVisible(false);accept_={};preview_={};if(closed_){auto cb=std::move(closed_);cb();}}
    void Update(const wi::Canvas& canvas,float dt)override
    {
        if(!IsVisible())return;
        if(colourPicker_.IsOpen()){const auto p=GetPos(),s=GetSize();colourPicker_.SetLayout(p.x+(s.x-400)*.5f,p.y+50,400,std::min(500.f,s.y-90));colourPicker_.Update(canvas,dt);return;}
        if(wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)){const auto pointer=wi::input::GetPointer();for(auto* f:Fields()){const auto p=f->GetPos(),s=f->GetSize();
            if(f->GetState()==wi::gui::ACTIVE&&(pointer.x<p.x||pointer.x>=p.x+s.x||pointer.y<p.y||pointer.y>=p.y+s.y)){const auto text=f->GetCurrentInputValue();f->Deactivate();f->SetValue(text);}}}
        StudioModalWindow::Update(canvas,dt);
        UpdatePreview(canvas,dt);
        if(pending_>=0){Drafts();if(Capture()){selected_=pending_;TargetFields();List();}pending_=-1;Preview();}
    }
    void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd)const override
    {
        if(!IsVisible())return;const auto p=GetPos(),s=GetSize();ApplyScissor(canvas,scissorRect,cmd);
        theme::ButtonRect(p.x,p.y,s.x,s.y,theme::Border,cmd,8);theme::ButtonRect(p.x+1,p.y+1,s.x-2,s.y-2,theme::Canvas,cmd,7);
        theme::ButtonRect(p.x+1,p.y+1,s.x-2,38,theme::Panel,cmd,6);Label("WORLD NAVIGATION",p.x+18,p.y+9,18,theme::Text,cmd);
        auto clip=scissorRect;clip.top=std::max(clip.top,int(p.y+38));clip.bottom=std::min(clip.bottom,int(p.y+s.y-76));ApplyScissor(canvas,clip,cmd);const auto o=ContentOrigin();
        const auto label=[&](const char* text,float x,float y){Label(text,o.x+x,o.y+y,14,theme::Accent,cmd);};
        label("BEHAVIOUR",28,16);label("APPEARANCE",520,16);label("VISIBLE SPAN (DEGREES)",28,88);label("MANUAL HEADING (DEGREES)",266,88);
        label("MARKER SIZE (PX)",28,150);label("MARKER SPACING (PX)",266,150);label("MAX DISTANCE (0 = UNLIMITED)",28,212);label("CATEGORIES (BLANK = ALL)",266,212);
        label("CATEGORY TO RESTYLE",520,212);label("EDITOR SAMPLE HEADING",28,326);label("AUTHORED WORLD TARGETS",28,426);label("TARGET NAME",520,426);label("CATEGORY",776,426);
        label("WORLD X",520,502);label("WORLD Y",692,502);label("WORLD Z",864,502);label("FOLLOW ENTITY ID (OPTIONAL / ADVANCED)",520,572);
        label("FONT SIZE (DESIGN PX)",520,368);
        label("TARGET PRIORITY",520,632);
        for(auto* w:controls_){ApplyScissor(canvas,w==&save_||w==&cancel_?scissorRect:clip,cmd);w->Render(canvas,cmd);}
        label("PREVIEW / SAMPLE DATA",520,700);
        if(previewLoaded_)previewPath_.GetGUI().Render(canvas,cmd);
        ApplyScissor(canvas,scissorRect,cmd);auto message=status_;if(message.size()>130)message=message.substr(0,127)+"...";Label(message,p.x+18,p.y+s.y-24,12,theme::Muted,cmd);
        const auto cp=closeButton.GetPos(),cs=closeButton.GetSize();theme::DrawButton(cp.x,cp.y,cs.x,cs.y,"X",{true,closeButton.GetState()==wi::gui::FOCUS,false},14,cmd);
        for(auto* w:controls_){ApplyScissor(canvas,scissorRect,cmd);w->RenderTooltip(canvas,cmd);}colourPicker_.Render(canvas,cmd);
    }
};
}
