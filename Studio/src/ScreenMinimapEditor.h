#pragma once
#include "StudioModalWindow.h"
#include "RenegadeStudioChrome.h"
#include "RenegadeScreenColorPicker.h"
#include "renegade/bridge/WorldNavigationService.h"
#include "renegade/bridge/CreatorScreenFontImportService.h"
#include "renegade/screen/ScreenRenderer.h"
#include <memory>
#include <filesystem>
#include <cstdlib>
namespace renegade::studio
{
class ScreenMinimapEditor final:public StudioModalWindow,public std::enable_shared_from_this<ScreenMinimapEditor>
{
    bridge::ScreenWidget widget_;
    std::string root_,status_;bridge::StableId project_;unsigned generation_=0;
    std::function<bool(bridge::ScreenWidget,std::string&)> accept_;
    std::function<void()> closed_;
    wi::RenderPath2D previewPath_;screen::ScreenRenderer renderer_;bool previewLoaded_=false;
    std::vector<wi::gui::Widget*> controls_;
    RenegadeScreenColorPicker colourPicker_;
    RenegadeButton font_,textColour_,backingColour_;
    RenegadeTextInputField fontSize_;
    RenegadeButton mode_,shape_,rotation_,image_,worldArt_,playerIcon_,enemyIcon_,questIcon_,vendorIcon_,locationIcon_,resetIcons_,quality_,rate_,save_,cancel_;
    RenegadeTextInputField range_,height_,markerSize_,playerSize_,west_,east_,south_,north_;
    RenegadeCheckBox mapKey_,enemies_,quests_,vendors_,names_,distances_,edges_;
    void Add(wi::gui::Widget& w,float x,float y,float width,float height){w.SetPos({x,y});w.SetSize({width,height});w.SetShadowRadius(0);AddWidget(&w);controls_.push_back(&w);}
    void Button(RenegadeButton& b,const char* text,float x,float y,float width,const char* tip){b.Create(text);b.SetText(text);b.SetRenderTextSize(14);b.SetAccentOutlineStyle(true);b.SetTooltip(tip);Add(b,x,y,width,32);}
    void Field(RenegadeTextInputField& f,const char* name,float x,float y,float width,const char* tip){f.Create(name);f.SetDescription("");f.SetRenderTextSize(14);f.SetTooltip(tip);Add(f,x,y,width,28);f.OnInputAccepted([this](const auto&){Preview();});}
    void Check(RenegadeCheckBox& c,const char* text,float x,float y,const char* tip){c.Create(text);c.SetRenderTextSize(13);c.SetTooltip(tip);Add(c,x,y,20,22);c.OnClick([this](const auto&){Preview();});}
    std::vector<wi::gui::TextInputField*> Fields(){return {&fontSize_,&range_,&height_,&markerSize_,&playerSize_,&west_,&east_,&south_,&north_};}
    void Drafts(){for(auto* f:Fields())if(f->GetState()==wi::gui::ACTIVE){auto text=f->GetCurrentInputValue();f->Deactivate();f->SetValue(text);}}
    static bool Number(const std::string& text,float& n){char* e=nullptr;n=std::strtof(text.c_str(),&e);return e!=text.c_str()&&!*e&&std::isfinite(n);}
    bool Capture(){auto& c=*widget_.navigation;auto& m=*c.minimap;
        if(!Number(fontSize_.GetValue(),widget_.style.text.fontSize)||widget_.style.text.fontSize<4||widget_.style.text.fontSize>128||!Number(range_.GetValue(),m.range)||!Number(height_.GetValue(),m.cameraHeight)||!Number(markerSize_.GetValue(),c.markerSize)||!Number(playerSize_.GetValue(),m.playerSize)||
            !Number(west_.GetValue(),m.west)||!Number(east_.GetValue(),m.east)||!Number(south_.GetValue(),m.south)||!Number(north_.GetValue(),m.north)){status_="Enter valid numbers.";return false;}
        m.mapKeyEnabled=mapKey_.GetCheck();m.showEnemies=enemies_.GetCheck();m.showQuestGivers=quests_.GetCheck();m.showVendors=vendors_.GetCheck();
        c.showNames=names_.GetCheck();c.showDistance=distances_.GetCheck();m.clampEdges=edges_.GetCheck();
        return bridge::ValidateScreenNavigation(c,status_);
    }
    void Labels(){const auto& m=*widget_.navigation->minimap;
        mode_.SetText(m.live?"Live overhead view":"Custom map image");shape_.SetText(m.round?"Round":"Square / rectangle");rotation_.SetText(m.rotate?"Rotate with player":"North stays up");
        quality_.SetText("Live image: "+std::to_string(m.resolution)+" px");rate_.SetText("Updates: "+std::to_string(int(m.updateHz))+" / second");}
    void Preview(){if(!IsVisible()||!Capture())return;Labels();std::string error;
        if(previewLoaded_){renderer_.Reset(previewPath_);previewLoaded_=false;}
        auto w=widget_;w.parentId.clear();w.component.reset();w.rect={0,0,400,400};w.layoutMode=bridge::ScreenLayoutMode::Absolute;w.animations.clear();w.navigation->minimap->fullMap=false;
        bridge::ScreenDocument doc;doc.usage=bridge::ScreenUsage::Hud;doc.designWidth=doc.designHeight=400;
        doc.envelope=bridge::CreateDocumentEnvelope(project_,bridge::RuntimeScreenDocumentType,"Content/UI/MinimapPreview.renegade-screen","minimap-preview");doc.widgets={w};
        if(!renderer_.Load(doc,root_,previewPath_,{},error)){status_=error;return;}renderer_.SetInputEnabled(false);previewLoaded_=true;
        if(!renderer_.PreviewNavigationDraft(w.id,*w.navigation,0,error))status_=error;
    }
    void ChooseColour(bool background){Drafts();if(!Capture())return;
        colourPicker_.Open(root_,background?"Map backing colour":"Map text colour",background?widget_.style.normal.background:widget_.style.normal.foreground,[this,background](auto colour){
            for(auto* v:{&widget_.style.normal,&widget_.style.hover,&widget_.style.pressed,&widget_.style.focused,&widget_.style.disabled})
                if(background)v->background=colour;else v->foreground=colour;Preview();});
    }
    void ChooseFont(){Drafts();const auto weak=weak_from_this();const auto generation=generation_;
        wi::helper::FileDialogParams d;d.type=wi::helper::FileDialogParams::OPEN;d.description="Choose map TrueType font";d.extensions={"ttf"};
        wi::helper::FileDialog(d,[weak,generation](const std::string& path){if(path.empty())return;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[weak,generation,path](std::uint64_t){
                const auto self=weak.lock();if(!self||!self->IsVisible()||self->generation_!=generation)return;
                const auto font=bridge::CreatorScreenFontImportService::Import(self->root_,self->project_,path);
                if(!font.succeeded){self->status_=font.error;return;}self->widget_.style.text.fontResource=font.runtimePath;self->Preview();
            });});
    }
    void Choose(int which){Drafts();if(!Capture())return;const auto weak=weak_from_this();const auto generation=generation_;
        wi::helper::FileDialogParams d;d.type=wi::helper::FileDialogParams::OPEN;d.description="Choose map artwork or icon";d.extensions={"png","jpg","jpeg","dds","tga","webp"};
        wi::helper::FileDialog(d,[weak,generation,which](const std::string& path){if(path.empty())return;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[weak,generation,which,path](std::uint64_t){
                const auto self=weak.lock();if(!self||!self->IsVisible()||self->generation_!=generation)return;std::string output,error;
                if(!bridge::ImportScreenNavigationImage(self->root_,self->project_,path,output,error)){self->status_=error;return;}
                auto& c=*self->widget_.navigation;auto& m=*c.minimap;
                if(which==0){m.image=output;m.live=false;}else if(which==1)m.worldMapImage=output;else if(which==2)m.playerIcon=output;
                else c.icons[which==3?"enemy":which==4?"questgiver":which==5?"vendor":"default"]=output;
                self->Preview();self->status_="Image imported. Save applies this draft.";
            });
        });
    }
    static void Label(const std::string& text,float x,float y,int size,wi::Color colour,wi::graphics::CommandList cmd)
    {wi::font::Params p(x,y,size);p.color=colour;p.bolden=.12f;wi::font::Draw(text,p,cmd);}
public:
    ~ScreenMinimapEditor(){renderer_.Reset(previewPath_);RemoveWidgets();}
    void Create(){StudioModalWindow::Create("Minimap",wi::gui::Window::WindowControls::CLOSE|wi::gui::Window::WindowControls::MOVE);SetSize({1000,850});SetControlSize(32);SetShadowRadius(0);
        Button(mode_,"Live overhead view",24,40,220,"Choose a live native overhead camera or imported artwork.");mode_.OnClick([this](const auto&){widget_.navigation->minimap->live=!widget_.navigation->minimap->live;Preview();});
        Button(shape_,"Round",260,40,220,"Choose a round minimap or a rectangular map surface.");shape_.OnClick([this](const auto&){widget_.navigation->minimap->round=!widget_.navigation->minimap->round;Preview();});
        Button(rotation_,"North stays up",24,86,220,"Keep north fixed or rotate map and markers with player heading.");rotation_.OnClick([this](const auto&){widget_.navigation->minimap->rotate=!widget_.navigation->minimap->rotate;Preview();});
        Button(image_,"Minimap artwork...",260,86,220,"Import the image used when Custom map image is selected.");image_.OnClick([this](const auto&){Choose(0);});
        Field(range_,"Visible range",24,155,220,"Metres from the centre to the top edge. Controls minimap zoom.");Field(height_,"Camera height",260,155,220,"Height above the player for the live overhead camera.");
        Field(markerSize_,"Marker size",24,220,220,"Size of target icons in design pixels.");Field(playerSize_,"Player size",260,220,220,"Size of the independent player arrow or custom player icon.");
        Check(mapKey_,"M opens full map",24,267,"Enable the configurable Map input action, M by default.");Check(edges_,"Edge markers",260,267,"Pin distant location/objective markers to the edge. Nearby NPC markers stay range-limited.");
        Check(enemies_,"Nearby enemies",24,310,"Show live hostile Characters in range. Dead/despawned actors are removed.");Check(quests_,"Quest givers",260,310,"Show questgiver markers from Location Zones or registered entity targets.");Check(vendors_,"Vendors",24,350,"Show vendor markers from Location Zones or registered entity targets.");
        Check(names_,"Marker names",260,350,"Display target names where they fit.");Check(distances_,"Distances",24,390,"Display horizontal world distances beside target names.");
        Button(worldArt_,"Full map artwork...",24,450,456,"Independent illustrated map opened by M. Drag and zoom in game; shared world bounds below place icons on the artwork.");worldArt_.OnClick([this](const auto&){Choose(1);});
        Field(west_,"West / min X",24,520,220,"World X at the left edge of either map image.");Field(east_,"East / max X",260,520,220,"World X at the right edge of either map image.");
        Field(south_,"South / min Z",24,583,220,"World Z at the bottom edge of the artwork.");Field(north_,"North / max Z",260,583,220,"World Z at the top edge of the artwork. +Z is north.");
        Button(quality_,"Live image: 256 px",24,640,220,"Cycle 128/256/512 pixels. Does not change HUD size.");quality_.OnClick([this](const auto&){auto& m=*widget_.navigation->minimap;m.resolution=m.resolution==128?256:m.resolution==256?512:128;Labels();});
        Button(rate_,"Updates: 20 / second",260,640,220,"Cycle live camera refresh rate: 10/20/30/60. Native scene and physics update only once.");rate_.OnClick([this](const auto&){auto& m=*widget_.navigation->minimap;m.updateHz=m.updateHz==10?20:m.updateHz==20?30:m.updateHz==30?60:10;Labels();});
        Button(playerIcon_,"Player icon...",516,450,204,"Swap the player arrow for custom artwork.");playerIcon_.OnClick([this](const auto&){Choose(2);});
        Button(enemyIcon_,"Enemy icon...",744,450,204,"Swap all generic enemy markers; per-target icons still take precedence.");enemyIcon_.OnClick([this](const auto&){Choose(3);});
        Button(questIcon_,"Quest-giver icon...",516,496,204,"Swap the quest-giver icon for this minimap.");questIcon_.OnClick([this](const auto&){Choose(4);});
        Button(vendorIcon_,"Vendor icon...",744,496,204,"Swap the vendor icon for this minimap.");vendorIcon_.OnClick([this](const auto&){Choose(5);});
        Button(locationIcon_,"Default location icon...",516,542,432,"Choose a fallback for locations with no individual Zone icon.");locationIcon_.OnClick([this](const auto&){Choose(6);});
        Button(resetIcons_,"Use generic icons",516,588,432,"Clear this minimap's category/player overrides. Individual Location Zone icons remain assigned.");resetIcons_.OnClick([this](const auto&){widget_.navigation->icons.clear();widget_.navigation->minimap->playerIcon.clear();Preview();});
        Button(font_,"Map font...",516,640,204,"Import a TrueType font for map labels and hints.");font_.OnClick([this](const auto&){ChooseFont();});
        Field(fontSize_,"Label size",744,640,204,"Base label size in design pixels, 4-128.");
        Button(textColour_,"Text colour...",516,686,204,"Set map direction and label colour.");textColour_.OnClick([this](const auto&){ChooseColour(false);});
        Button(backingColour_,"Backing colour...",744,686,204,"Set the backing and its opacity independently of the artwork.");backingColour_.OnClick([this](const auto&){ChooseColour(true);});
        Button(save_,"Save",0,0,116,"Commit all minimap changes in one Undo operation.");PinFooter(save_,260,12,true);save_.OnClick([this](const auto&){Drafts();if(!Capture())return;std::string error;if(accept_&&accept_(widget_,error))Close();else status_=error;});
        Button(cancel_,"Cancel",0,0,116,"Discard this draft and keep the saved minimap.");PinFooter(cancel_,132,12,true);cancel_.OnClick([this](const auto&){Close();});OnClose([this](const auto&){Close();});SetVisible(false);
    }
    void Open(bridge::ScreenWidget w,std::string root,bridge::StableId project,std::function<bool(bridge::ScreenWidget,std::string&)> accept,std::function<void()> closed){
        Close();widget_=std::move(w);root_=std::move(root);project_=std::move(project);accept_=std::move(accept);closed_=std::move(closed);
        const auto& c=*widget_.navigation;const auto& m=*c.minimap;
        fontSize_.SetValue(widget_.style.text.fontSize);range_.SetValue(m.range);height_.SetValue(m.cameraHeight);markerSize_.SetValue(c.markerSize);playerSize_.SetValue(m.playerSize);
        west_.SetValue(m.west);east_.SetValue(m.east);south_.SetValue(m.south);north_.SetValue(m.north);
        mapKey_.SetCheck(m.mapKeyEnabled);enemies_.SetCheck(m.showEnemies);quests_.SetCheck(m.showQuestGivers);vendors_.SetCheck(m.showVendors);names_.SetCheck(c.showNames);distances_.SetCheck(c.showDistance);edges_.SetCheck(m.clampEdges);
        status_="Location Zones supply locations. Use type questgiver or vendor for those markers. All icons can be swapped.";Labels();SetVisible(true);Preview();
    }
    void Close(){++generation_;for(auto* f:Fields())f->Deactivate();renderer_.Reset(previewPath_);previewLoaded_=false;colourPicker_.Close();SetVisible(false);accept_={};if(closed_){auto cb=std::move(closed_);cb();}}
    void Update(const wi::Canvas& canvas,float dt)override{
        if(!IsVisible())return;
        if(colourPicker_.IsOpen()){const auto p=GetPos(),s=GetSize();colourPicker_.SetLayout(p.x+(s.x-400)*.5f,p.y+50,400,std::min(500.f,s.y-90));colourPicker_.Update(canvas,dt);return;}
        StudioModalWindow::Update(canvas,dt);
        if(previewLoaded_){const auto o=ContentOrigin();renderer_.SetViewport({o.x+534,o.y+38,396,396});
            auto clip=scissorRect;clip.top=std::max(clip.top,int(GetPos().y+38));clip.bottom=std::min(clip.bottom,int(GetPos().y+GetSize().y-76));
            renderer_.SetClipViewport({float(clip.left),float(clip.top),float(clip.right-clip.left),float(clip.bottom-clip.top)});
            renderer_.ApplyLayout(previewPath_);renderer_.UpdateCameraFeed(dt);previewPath_.GetGUI().Update(canvas,dt);}
    }
    void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd)const override{
        if(!IsVisible())return;const auto p=GetPos(),s=GetSize();ApplyScissor(canvas,scissorRect,cmd);
        theme::ButtonRect(p.x,p.y,s.x,s.y,theme::Border,cmd,8);theme::ButtonRect(p.x+1,p.y+1,s.x-2,s.y-2,theme::Canvas,cmd,7);
        theme::ButtonRect(p.x+1,p.y+1,s.x-2,38,theme::Panel,cmd,6);Label("MINIMAP / WORLD MAP",p.x+18,p.y+9,18,theme::Text,cmd);
        auto clip=scissorRect;clip.top=std::max(clip.top,int(p.y+38));clip.bottom=std::min(clip.bottom,int(p.y+s.y-76));ApplyScissor(canvas,clip,cmd);const auto o=ContentOrigin();
        const auto label=[&](const char* text,float x,float y){Label(text,o.x+x,o.y+y,13,theme::Accent,cmd);};
        label("VIEW",24,16);label("EDITOR SAMPLE",534,16);label("RANGE (METRES)",24,132);label("LIVE CAMERA HEIGHT",260,132);
        label("TARGET ICON SIZE",24,197);label("PLAYER ICON SIZE",260,197);label("ILLUSTRATED FULL MAP",24,426);label("CUSTOM ICONS",516,426);label("LABEL SIZE",744,617);
        label("WEST / MIN X",24,497);label("EAST / MAX X",260,497);label("SOUTH / MIN Z",24,560);label("NORTH / MAX Z",260,560);
        for(auto* c:controls_){ApplyScissor(canvas,c==&save_||c==&cancel_?scissorRect:clip,cmd);c->Render(canvas,cmd);}
        if(previewLoaded_)previewPath_.GetGUI().Render(canvas,cmd);
        ApplyScissor(canvas,scissorRect,cmd);Label(status_.substr(0,130),p.x+18,p.y+s.y-24,12,theme::Muted,cmd);
        const auto cp=closeButton.GetPos(),cs=closeButton.GetSize();theme::DrawButton(cp.x,cp.y,cs.x,cs.y,"X",{true,closeButton.GetState()==wi::gui::FOCUS,false},14,cmd);
        for(auto* c:controls_)c->RenderTooltip(canvas,cmd);colourPicker_.Render(canvas,cmd);
    }
};
}
